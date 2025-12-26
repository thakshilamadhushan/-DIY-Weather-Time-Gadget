#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <LittleFS.h>
#include "time.h"

#define API_KEY ""
#define CITY "Kegalle"
#define COUNTRY "LK"
#define BUZZER_PIN 5
#define LED1_PIN 19
#define LED2_PIN 26

WiFiMulti wifiMulti;
TFT_eSPI tft = TFT_eSPI();
String greeting = "";

const char* ntpServer = "pool.ntp.org";

String lastTimeStr = "";
String lastSecStr = "";
float temp;
int humidity;
String weather = "N/A";
int lastHour = -1;
unsigned long lastLEDBlink = 0;
int ledBlinkStep = 0;

unsigned long lastWeatherUpdate = 0;
unsigned long lastClockUpdate = 0;

void setup() {
  Serial.begin(115200);
  wifiMulti.addAP("", "");
  wifiMulti.addAP("", ""); // if there is more than 1 wifi

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED1_PIN, OUTPUT);       // <-- Initialize LED pin
  pinMode(LED2_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);

  Serial.println("Connecting to WiFi...");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  configTime(19800, 0, ntpServer); // GMT+5:30
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  showGreeting();
  drawStaticUI(); // draw icons and layout once
  getWeather();   // initial weather
  updateDynamicUI(); // draw initial values
}

void loop() {
  unsigned long now = millis();

   // LED blinking pattern: LED1 blinks 2x, then LED2 blinks 2x
  switch (ledBlinkStep) {
    case 0: if (now - lastLEDBlink >= 0) {
      digitalWrite(LED1_PIN, HIGH); lastLEDBlink = now; ledBlinkStep++; }
      break;
    case 1: if (now - lastLEDBlink >= 50) {
      digitalWrite(LED1_PIN, LOW); lastLEDBlink = now; ledBlinkStep++; }
      break;
    case 2: if (now - lastLEDBlink >= 50) {
      digitalWrite(LED1_PIN, HIGH); lastLEDBlink = now; ledBlinkStep++; }
      break;
    case 3: if (now - lastLEDBlink >= 50) {
      digitalWrite(LED1_PIN, LOW); lastLEDBlink = now; ledBlinkStep++; }
      break;
    case 4: if (now - lastLEDBlink >= 50) {
      digitalWrite(LED2_PIN, HIGH); lastLEDBlink = now; ledBlinkStep++; }
      break;
    case 5: if (now - lastLEDBlink >= 50) {
      digitalWrite(LED2_PIN, LOW); lastLEDBlink = now; ledBlinkStep++; }
      break;
    case 6: if (now - lastLEDBlink >= 50) {
      digitalWrite(LED2_PIN, HIGH); lastLEDBlink = now; ledBlinkStep++; }
      break;
    case 7: if (now - lastLEDBlink >= 50) {
      digitalWrite(LED2_PIN, LOW); lastLEDBlink = now; ledBlinkStep++; }
      break;
    case 8: if (now - lastLEDBlink >= 2000) {
      ledBlinkStep = 0; } // Small gap before restarting
      break;
  }

  if (now - lastClockUpdate > 1000) {
    updateTime(); // update time every second
    lastClockUpdate = now;

    // Hourly buzzer check
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    int currentHour = timeinfo.tm_hour;

    if (currentHour != lastHour) {
      buzzOnHour();
      lastHour = currentHour;
    }
  }

  if (now - lastWeatherUpdate > 300000) { // update weather every 5 min
    getWeather();
    updateWeather();
    lastWeatherUpdate = now;
  }
}

void showGreeting() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  int hour = timeinfo.tm_hour;

  if (hour < 12) greeting = "Good Morning!";
  else if (hour < 18) greeting = "Good Afternoon!";
  else greeting = "Good Evening!";

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Hello!", 60, 80, 4);
  tft.drawString(greeting, 20, 120, 2);
  delay(3000);
  tft.fillScreen(TFT_BLACK);
}

void drawStaticUI() {
  // draw fixed icons and layout
  drawBMP("/spaceman.bmp", 165, 165);
  drawBMP("/location.bmp", 0, 5);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(CITY, 30, 10, 1);
  drawBMP("/C.bmp", 110, 35);
  drawBMP("/Humidity.bmp", 185, 30);
}

void updateDynamicUI() {
  updateTime();
  updateWeather();
}

void updateTime() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeStrBuf[6];
  strftime(timeStrBuf, sizeof(timeStrBuf), "%H:%M", &timeinfo);
  String timeStr = String(timeStrBuf);

  char secStrBuf[3];   // SS
  strftime(secStrBuf, sizeof(secStrBuf), "%S", &timeinfo);
  String secStr = String(secStrBuf);

  char dateStr[20];
  strftime(dateStr, sizeof(dateStr), "%Y/%m/%d", &timeinfo);

  char dayStr[10];
  strftime(dayStr, sizeof(dayStr), "%A", &timeinfo);

  tft.setTextDatum(MC_DATUM);

  // Clear previous time by overwriting with background color
  tft.setTextColor(TFT_BLACK); // Set to background color
  tft.drawString(lastTimeStr, 90, 145, 4); // Clear old time
  tft.drawString(lastSecStr, 180, 145, 2); //Clear old secod
 
  // Draw new time
  tft.setTextColor(TFT_WHITE);
  tft.drawString(timeStr, 90, 145, 4);

   // Draw SS (smaller font)
  tft.setTextSize(2);
  tft.drawString(secStr, 180, 145, 2); // Seconds beside HH:MM

  lastTimeStr = timeStr; // Update last drawn time string
  lastSecStr = secStr; // Update last drawn second string

  // Date
  //tft.fillRect(60, 180, 120, 20, TFT_BLACK);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString(dateStr, 90, 190, 1);

  // Day
  //tft.fillRect(60, 210, 120, 20, TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  tft.drawString(dayStr, 90, 220, 1);
}



void updateWeather() {
  // Temperature
  tft.fillRect(60, 35, 50, 40, TFT_BLACK);
  tft.setCursor(60, 43);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(4);
  tft.print(int(temp));

  // Weather description
  tft.fillRect(10, 87, 100, 20, TFT_BLACK);
  tft.setCursor(10, 87);
  tft.setTextSize(2);
  tft.print(weather);

  if (weather == "Clouds"){
    drawBMP("/Clouds.bmp", 0, 30);
  }else if(weather == "Clear"){
    drawBMP("/Clear.bmp", 0, 30);
  }else if(weather == "Rain"){
    drawBMP("/Rain.bmp", 0, 30);
  }else if(weather == "Thunderstorm"){
    drawBMP("/Thunderstorm.bmp", 0, 30);
  }

  // Humidity
  tft.fillRect(180, 87, 60, 20, TFT_BLACK);
  tft.setCursor(185, 87);
  tft.setTextSize(2);
  tft.print(humidity); tft.print("%");
}

void getWeather() {
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + String(CITY) + "," + String(COUNTRY) + "&units=metric&appid=" + String(API_KEY);
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    temp = doc["main"]["temp"];
    humidity = doc["main"]["humidity"];
    weather = doc["weather"][0]["main"].as<String>();
  }
  http.end();
}

void buzzOnHour() {
  for (int i = 0; i < 3; i++) { // 3 short beeps
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

// Draw BMP image from LittleFS
void drawBMP(const char *filename, int x, int y) {
  fs::File bmpFile = LittleFS.open(filename, "r");
  if (!bmpFile) {
    Serial.println("Image file not found: " + String(filename));
    return;
  }

  uint16_t magic = read16(bmpFile);
  if (magic != 0x4D42) {
    Serial.println("Not a BMP file");
    bmpFile.close();
    return;
  }

  (void)read32(bmpFile); // file size
  (void)read32(bmpFile); // reserved
  uint32_t bmpImageoffset = read32(bmpFile);
  (void)read32(bmpFile); // header size
  int bmpWidth = read32(bmpFile);
  int bmpHeight = read32(bmpFile);
  if (read16(bmpFile) != 1) return;
  uint8_t bmpDepth = read16(bmpFile);
  if (bmpDepth != 24 || read32(bmpFile) != 0) {
    Serial.println("Unsupported BMP format");
    bmpFile.close();
    return;
  }

  uint32_t rowSize = (bmpWidth * 3 + 3) & ~3;
  bool flip = true;
  if (bmpHeight < 0) {
    bmpHeight = -bmpHeight;
    flip = false;
  }

  tft.startWrite();
  for (int row = 0; row < bmpHeight; row++) {
    uint32_t pos = bmpImageoffset + (flip ? (bmpHeight - 1 - row) : row) * rowSize;
    bmpFile.seek(pos);
    for (int col = 0; col < bmpWidth; col++) {
      uint8_t b = bmpFile.read();
      uint8_t g = bmpFile.read();
      uint8_t r = bmpFile.read();
      tft.drawPixel(x + col, y + row, tft.color565(r, g, b));
    }
  }
  tft.endWrite();
  bmpFile.close();
}

uint16_t read16(fs::File &f) {
  return f.read() | (f.read() << 8);
}

uint32_t read32(fs::File &f) {
  return f.read() | (f.read() << 8) | (f.read() << 16) | (f.read() << 24);
}
