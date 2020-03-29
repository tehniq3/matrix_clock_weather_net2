/*
 * original from https://www.youtube.com/watch?v=2I_us9NhIvM
 * rotate feature: https://www.youtube.com/watch?v=PUOhUyLCIU8
 * 
 * small changes by Nicu FLORICA (niq_ro)
 * 
  ESP-01 pinout from top:
  
  GND    GP2 GP0 RX/GP3
  TX/GP1 CH  RST VCC

  MAX7219
  ESP-1 from rear
  Re Br Or Ye
  Gr -- -- --

  USB to Serial programming
  ESP-1 from rear, FF to GND, RR to GND before upload
  Gr FF -- Bl
  Wh -- RR Vi

  GPIO 2 - DataIn
  GPIO 1 - LOAD/CS
  GPIO 0 - CLK
  GPIO 3 - time selection
  ------------------------
  NodeMCU 1.0 pinout:

  D8 - DataIn
  D7 - LOAD/CS
  D6 - CLK
  
*/


#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

WiFiClient client;

String weatherMain = "";
String weatherDescription = "";
String weatherLocation = "";
String country;
int humidity;
int pressure;
int presiune;
float temp;
float tempMin, tempMax;
int clouds;
float windSpeed;
float vant;
String date;

String currencyRates;
String weatherString;

#define NUM_MAX 4
#define ROTATE 90  // must test with 0, 90, 180 or 270

// for ESP-01 module
//#define DIN_PIN 2 // D4
//#define CS_PIN  1 // 3 // D9/RX
//#define CLK_PIN 0 // D3

//#define orainplus 3 // RX

// for NodeMCU 1.0
#define DIN_PIN 15  // D8
#define CS_PIN  13  // D7
#define CLK_PIN 12  // D6

#define orainplus 14 // GPIO14 = D5

#include "max7219.h"
#include "fonts.h"

// =======================================================================
// CHANGE YOUR CONFIG HERE:
// =======================================================================
const char* ssid     = "bere";     // SSID of local network
const char* password = "bererece";   // Password on network
String weatherKey = "0APIkey";
String weatherLang = "&lang=en";
String cityID = "680332"; //Craiova
// read OpenWeather api description for more info
// =======================================================================

void setup() 
{
  Serial.begin(115200);
  pinMode(orainplus, INPUT);
  initMAX7219();
  sendCmdAll(CMD_SHUTDOWN,1);
  sendCmdAll(CMD_INTENSITY,0);
  Serial.print("Connecting WiFi ");
  WiFi.begin(ssid, password);
  printStringWithShift("Connecting",15);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected: "); Serial.println(WiFi.localIP());
}
// =======================================================================
#define MAX_DIGITS 4
byte dig[MAX_DIGITS]={0};
byte digold[MAX_DIGITS]={0};
byte digtrans[MAX_DIGITS]={0};
int updCnt = 0;
int dots = 0;
long dotTime = 0;
long clkTime = 0;
int dx=0;
int dy=0;
byte del=0;
int h,m,s;
// =======================================================================

void loop()
{
  if(updCnt<=0) { // every 10 scrolls, ~450s=7.5m
    updCnt = 5;
    Serial.println("Getting data ...");
 //   printStringWithShift("   Getting dat",15);
    getWeatherData();
 //   getCurrencyRates();
    getTime();
    Serial.println("Data loaded");
    clkTime = millis();
  }
 
  if(millis()-clkTime > 15000 && !del && dots) { // clock for 45s, then scrolls for about 30s
    printStringWithShift(date.c_str(),80);
 //   printStringWithShift(currencyRates.c_str(),70);
    printStringWithShift(weatherString.c_str(),80);
    updCnt--;
    clkTime = millis();
  }
  if(millis()-dotTime > 500) {
    dotTime = millis();
    dots = !dots;
  }
  updateTime();
  showAnimClock();
}

// =======================================================================

void showSimpleClock()
{
  dx=dy=0;
  clr();
  showDigit(h/10,  0, dig6x8);
  showDigit(h%10,  8, dig6x8);
  showDigit(m/10, 17, dig6x8);
  showDigit(m%10, 25, dig6x8);
  showDigit(s/10, 34, dig6x8);
  showDigit(s%10, 42, dig6x8);
  setCol(15,dots ? B00100100 : 0);
  setCol(32,dots ? B00100100 : 0);
  refreshAll();
}

// =======================================================================

void showAnimClock()
{
  byte digPos[6]={0,8,17,25,34,42};
  int digHt = 12;
  int num = 6; 
  int i;
  if(del==0) {
    del = digHt;
    for(i=0; i<num; i++) digold[i] = dig[i];
    dig[0] = h/10 ? h/10 : 10;
    dig[1] = h%10;
    dig[2] = m/10;
    dig[3] = m%10;
    dig[4] = s/10;
    dig[5] = s%10;
    for(i=0; i<num; i++)  digtrans[i] = (dig[i]==digold[i]) ? 0 : digHt;
  } else
    del--;
  
  clr();
  for(i=0; i<num; i++) {
    if(digtrans[i]==0) {
      dy=0;
      showDigit(dig[i], digPos[i], dig6x8);
    } else {
      dy = digHt-digtrans[i];
      showDigit(digold[i], digPos[i], dig6x8);
      dy = -digtrans[i];
      showDigit(dig[i], digPos[i], dig6x8);
      digtrans[i]--;
    }
  }
  dy=0;
  setCol(15,dots ? B00100100 : 0);
  setCol(32,dots ? B00100100 : 0);
  refreshAll();
  delay(30);
}

// =======================================================================

void showDigit(char ch, int col, const uint8_t *data)
{
  if(dy<-8 | dy>8) return;
  int len = pgm_read_byte(data);
  int w = pgm_read_byte(data + 1 + ch * len);
  col += dx;
  for (int i = 0; i < w; i++)
    if(col+i>=0 && col+i<8*NUM_MAX) {
      byte v = pgm_read_byte(data + 1 + ch * len + 1 + i);
      if(!dy) scr[col + i] = v; else scr[col + i] |= dy>0 ? v>>dy : v<<-dy;
    }
}

// =======================================================================

void setCol(int col, byte v)
{
  if(dy<-8 | dy>8) return;
  col += dx;
  if(col>=0 && col<8*NUM_MAX)
    if(!dy) scr[col] = v; else scr[col] |= dy>0 ? v>>dy : v<<-dy;
}

// =======================================================================

int showChar(char ch, const uint8_t *data)
{
  int len = pgm_read_byte(data);
  int i,w = pgm_read_byte(data + 1 + ch * len);
  for (i = 0; i < w; i++)
    scr[NUM_MAX*8 + i] = pgm_read_byte(data + 1 + ch * len + 1 + i);
  scr[NUM_MAX*8 + i] = 0;
  return w;
}

// =======================================================================
int dualChar = 0;

unsigned char convertPolish(unsigned char _c)
{
  unsigned char c = _c;
  if(c==196 || c==197 || c==195) {
    dualChar = c;
    return 0;
  }
  if(dualChar) {
    switch(_c) {
      case 133: c = 1+'~'; break; // 'ą'
      case 135: c = 2+'~'; break; // 'ć'
      case 153: c = 3+'~'; break; // 'ę'
      case 130: c = 4+'~'; break; // 'ł'
      case 132: c = dualChar==197 ? 5+'~' : 10+'~'; break; // 'ń' and 'Ą'
      case 179: c = 6+'~'; break; // 'ó'
      case 155: c = 7+'~'; break; // 'ś'
      case 186: c = 8+'~'; break; // 'ź'
      case 188: c = 9+'~'; break; // 'ż'
      //case 132: c = 10+'~'; break; // 'Ą'
      case 134: c = 11+'~'; break; // 'Ć'
      case 152: c = 12+'~'; break; // 'Ę'
      case 129: c = 13+'~'; break; // 'Ł'
      case 131: c = 14+'~'; break; // 'Ń'
      case 147: c = 15+'~'; break; // 'Ó'
      case 154: c = 16+'~'; break; // 'Ś'
      case 185: c = 17+'~'; break; // 'Ź'
      case 187: c = 18+'~'; break; // 'Ż'
      default:  break;
    }
    dualChar = 0;
    return c;
  }    
  switch(_c) {
    case 185: c = 1+'~'; break;
    case 230: c = 2+'~'; break;
    case 234: c = 3+'~'; break;
    case 179: c = 4+'~'; break;
    case 241: c = 5+'~'; break;
    case 243: c = 6+'~'; break;
    case 156: c = 7+'~'; break;
    case 159: c = 8+'~'; break;
    case 191: c = 9+'~'; break;
    case 165: c = 10+'~'; break;
    case 198: c = 11+'~'; break;
    case 202: c = 12+'~'; break;
    case 163: c = 13+'~'; break;
    case 209: c = 14+'~'; break;
    case 211: c = 15+'~'; break;
    case 140: c = 16+'~'; break;
    case 143: c = 17+'~'; break;
    case 175: c = 18+'~'; break;
    default:  break;
  }
  return c;
}

// =======================================================================

void printCharWithShift(unsigned char c, int shiftDelay) {
  c = convertPolish(c);
  if (c < ' ' || c > '~'+25) return;
  c -= 32;
  int w = showChar(c, font);
  for (int i=0; i<w+1; i++) {
    delay(shiftDelay);
    scrollLeft();
    refreshAll();
  }
}

// =======================================================================

void printStringWithShift(const char* s, int shiftDelay){
  while (*s) {
    printCharWithShift(*s, shiftDelay);
    s++;
  }
}

// =======================================================================

const char *weatherHost = "api.openweathermap.org";

void getWeatherData()
{
  Serial.print("connecting to "); Serial.println(weatherHost);
  if (client.connect(weatherHost, 80)) {
    client.println("GET /data/2.5/weather?id=" + cityID + "&units=metric&appid=" + weatherKey + weatherLang + "\r\n" +
                "Host: " + weatherHost + "\r\nUser-Agent: ArduinoWiFi/1.1\r\n" +
                "Connection: close\r\n\r\n");
  } else {
    Serial.println("connection failed");
    return;
  }
  String line;
  int repeatCounter = 0;
  /*
  while (!client.available() && repeatCounter < 100) {
    delay(50);
    Serial.println("w.");
    repeatCounter++;    
  }
 */
  while(client.connected() && !client.available()) 
  delay(1);                                          //waits for data
 
  while (client.connected() && client.available()) {
    char c = client.read(); 
    if (c == '[' || c == ']') c = ' ';
    line += c;
  }

  client.stop();
  Serial.println(line);
 /* 
  DynamicJsonBuffer jsonBuf;
  JsonObject &root = jsonBuf.parseObject(line);
*/
char jsonBuf [line.length()+1];
line.toCharArray(jsonBuf,sizeof(jsonBuf));
jsonBuf[line.length() + 1] = '\0';
StaticJsonBuffer<1024> json_buf;
JsonObject &root = json_buf.parseObject(jsonBuf);
  
  if (!root.success())
  {
    Serial.println("parseObject() failed");
    return;
  }
  //weatherMain = root["weather"]["main"].as<String>();
  weatherDescription = root["weather"]["description"].as<String>();
  weatherDescription.toLowerCase();
  //  weatherLocation = root["name"].as<String>();
  //  country = root["sys"]["country"].as<String>();
  temp = root["main"]["temp"];
  humidity = root["main"]["humidity"];
  pressure = root["main"]["pressure"];
  presiune = 0.75*pressure;
  tempMin = root["main"]["temp_min"];
  tempMax = root["main"]["temp_max"];
  windSpeed = root["wind"]["speed"];
  vant = (float)windSpeed*3.6;
  clouds = root["clouds"]["all"];
  String deg = String(char('~'+25));
  weatherString = "         Temp: " + String(temp,1) + deg + "C (" + String(tempMin,1) + deg + "-" + String(tempMax,1) + deg + ")  ";
  weatherString += weatherDescription;
  weatherString += "  Humidity: " + String(humidity) + "%  ";
//  weatherString += "  Pressure: " + String(pressure) + "hPa  ";
  weatherString += "  Pressure: " + String(presiune) + "mmHg  ";
  weatherString += "  Clouds: " + String(clouds) + "%  ";
//  weatherString += "  Wind: " + String(windSpeed,1) + "m/s                 ";
  weatherString += "  Wind: " + String(vant,1) + "km/h                 ";
}

// =======================================================================

const char* currencyHost = "cinkciarz.pl";
void getCurrencyRates()
{
  WiFiClientSecure client;
  Serial.print("connecting to "); Serial.println(currencyHost);
  if (!client.connect(currencyHost, 443)) {
    Serial.println("connection failed");
    return;
  }
  client.print(String("GET / HTTP/1.1\r\n") +
               "Host: " + currencyHost + "\r\nConnection: close\r\n\r\n");

  //Serial.print("request sent");
  int repeatCounter = 0;
  while (!client.available() && repeatCounter < 10) {
    delay(500);
    Serial.println("c.");
    repeatCounter++;
  }
  Serial.println("connected");
  while (client.connected() && client.available()) {
    String line = client.readStringUntil('\n');
    //      Serial.println(line);
    int currIdx = line.indexOf("/kantor/kursy-walut-cinkciarz-pl/usd");
    if (currIdx > 0) {
      String curr = line.substring(currIdx + 33, currIdx + 33 + 3);
      curr.toUpperCase();
      line = client.readStringUntil('\n');
      int rateIdx = line.indexOf("\">");
      if (rateIdx <= 0) {
        Serial.println("Found rate but wrong structure!");
        return;
      }
      currencyRates = "        PLN/" + curr + ": ";
      if (line[rateIdx - 1] == 'n') currencyRates += char('~'+24); else currencyRates += char('~'+23); // down/up
      currencyRates += line.substring(rateIdx + 2, rateIdx + 8) + " ";

      line = client.readStringUntil('\n');
      rateIdx = line.indexOf("\">");
      if (rateIdx <= 0) {
        Serial.println("Found rate but wrong structure!");
        return;
      }
      if (line[rateIdx - 1] == 'n') currencyRates += char('~'+24); else currencyRates += char('~'+23); // down/up
      currencyRates += line.substring(rateIdx + 2, rateIdx + 8);
      currencyRates.replace(',', '.');
      break;
    }
  }
  client.stop();
}

// =======================================================================

float utcOffset = 2;
long localEpoc = 0;
long localMillisAtUpdate = 0;

void getTime()
{
  WiFiClient client;
  if (!client.connect("www.google.com", 80)) {
    Serial.println("connection to google failed");
    return;
  }

  client.print(String("GET / HTTP/1.1\r\n") +
               String("Host: www.google.com\r\n") +
               String("Connection: close\r\n\r\n"));
  int repeatCounter = 0;
  while (!client.available() && repeatCounter < 10) {
    delay(500);
    //Serial.println(".");
    repeatCounter++;
  }

  String line;
  client.setNoDelay(false);
  while(client.connected() && client.available()) {
    line = client.readStringUntil('\n');
    line.toUpperCase();
    if (line.startsWith("DATE: ")) {
      date = "     "+line.substring(6, 22);
      h = line.substring(23, 25).toInt();
      m = line.substring(26, 28).toInt();
      s = line.substring(29, 31).toInt();
      localMillisAtUpdate = millis();
      localEpoc = (h * 60 * 60 + m * 60 + s);
    }
  }
  client.stop();
}

// =======================================================================

void updateTime()
{
  long curEpoch = localEpoc + ((millis() - localMillisAtUpdate) / 1000);
//  long epoch = (long)(round(curEpoch + 3600 * utcOffset + 86400L) % 86400L);
if (digitalRead(orainplus) == LOW)
{
  utcOffset = 2;
}
else
 utcOffset = 3;
  long epoch = (long)(round(curEpoch + 3600 * utcOffset + 86400L)) % 86400L; // https://community.platformio.org/t/time-function-error/7345/2
  h = ((epoch  % 86400L) / 3600) % 24;
  m = (epoch % 3600) / 60;
  s = epoch % 60;
}

// =======================================================================
