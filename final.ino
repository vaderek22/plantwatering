#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ThingSpeak.h>
#include <NewPing.h>
#include <TimeLib.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define DHTPIN D8
#define DHTTYPE DHT11
#define SOIL_MOISTURE_PIN A0
#define PUMP_PIN D0
#define TRIGGER_PIN D10
#define ECHO_PIN D11
#define MAX_WATER_LEVEL 2
#define SENSOR_HEIGHT 8.4

NewPing sonar(TRIGGER_PIN, ECHO_PIN, 340);
DHT dht(DHTPIN, DHTTYPE);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
WiFiClient client;

const char *ssid = "WIFI SSID";
const char *password = "PASSWORD";
const char *server = "api.thingspeak.com";
const char *apiKey = "klucz";
const unsigned long CHANNEL_ID = 123;

unsigned long previousMillis = 0;  
unsigned long previousMillisThingspeak = 0; 
const unsigned long interval = 1000;
const unsigned long thingspeakInterval = 60000;

unsigned long displayStartTime = 0; 
unsigned long displayDuration = 30000; 
unsigned long displayDateDuration = 15000;
bool displayingTime = true;

float my_min(float a, float b) {
  return a < b ? a : b;
}
float my_max(float a, float b) {
  return a > b ? a : b;
}

    
void setup() {
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();
  display.clearDisplay();

  dht.begin();
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Łączenie z WIFI...");
  }

  ThingSpeak.begin(client);
  timeClient.begin();
  timeClient.setTimeOffset(3600);
}

void loop() {
    unsigned long currentMillis = millis();
    
    timeClient.update();
    
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      if (currentMillis - displayStartTime >= (displayingTime ? displayDuration : displayDateDuration)) {
            displayStartTime = currentMillis;
            displayingTime = !displayingTime;
        }
        
        display.clearDisplay();

        if (displayingTime) {
            String timeString = timeClient.getFormattedTime().substring(0, 5);
            int textWidth = timeString.length() * 12;
            int xPos = (SCREEN_WIDTH - textWidth) / 2;

            display.setTextSize(2);
            display.setTextColor(WHITE);
            display.setCursor(xPos, 0);
            display.println(timeString);
        } else {
            time_t epochTime = timeClient.getEpochTime();
            tmElements_t tm;
            breakTime(epochTime, tm);
            
            int currentYear = tm.Year + 1970;
            int currentMonth = tm.Month;
            int currentDay = tm.Day;
            
            String currentDate = String(currentDay) + "." + String(currentMonth) + "." + String(currentYear);
            int textWidth = currentDate.length() * 12;
            int xPos = (SCREEN_WIDTH - textWidth) / 2;
            
            display.setTextSize(2);
            display.setTextColor(WHITE);
            display.setCursor(xPos, 0);
            display.println(currentDate);
        }

    int valueX = (int)(SCREEN_WIDTH * 0.65);
    
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int soilMoisture = analogRead(SOIL_MOISTURE_PIN);
    float distance = sonar.ping_cm();
    float waterLevel = SENSOR_HEIGHT - distance;
    float percentage = (waterLevel / (SENSOR_HEIGHT - MAX_WATER_LEVEL)) * 100; 

    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("Temperatura:");
    display.setCursor(valueX, 20);
    display.println(String(temperature) + "C");

    display.println("w. powietrza:");
    display.setCursor(valueX, 30);
    display.println(String(humidity) + "%");

    display.println("w. gleby:");
    display.setCursor(valueX, 40);
    display.println(String(soilMoisture));
  
    percentage = my_max(0, my_min(100, percentage));
    display.println("p. wody: ");
    display.setCursor(valueX, 50);
    display.println(String(percentage) + "%");
    display.display();

    if (currentMillis - previousMillisThingspeak >= thingspeakInterval) {
    previousMillisThingspeak = currentMillis;

    ThingSpeak.setField(1, dht.readTemperature());
    ThingSpeak.setField(2, dht.readHumidity());
    ThingSpeak.setField(3, analogRead(SOIL_MOISTURE_PIN));
    ThingSpeak.setField(4, percentage);
    int statusCode = ThingSpeak.writeFields(CHANNEL_ID, apiKey);

    if (statusCode == 200) {
      Serial.println("Dane wysłane do ThingSpeak!");
    } else {
      Serial.println("Błąd podczas wysyłania danych do ThingSpeak. Kod statusu: " + String(statusCode));
    }
  }
    if (soilMoisture < 650) {
      digitalWrite(PUMP_PIN, LOW);  
    } else {
      digitalWrite(PUMP_PIN, HIGH);
    }
  }
}
