#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

const char* ssid = "PS-4";
const char* password = "liza250374liza";

String BOTtoken = "8780042934:AAFQvPUDZu2ctRhD9vgzq885XJoulTUVu64";  
String CHAT_ID = "707808229";
bool ledcwr = false;
bool sendPhoto = false;
WiFiClientSecure clientTCP;

#define TRIGGER_PIN 13
#define SIREN_PIN 14
static unsigned long sirenStart = 0;
unsigned long lastPhotoTime = 0;   
const unsigned long photoDelay = 3000; 

//CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0 
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


void configInitCamera(){
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;   
    config.jpeg_quality = 30;           
    config.fb_count = 1;                 

  } else {
      config.frame_size = FRAMESIZE_QVGA;
      config.jpeg_quality = 12;
      config.fb_count = 1;
  }

  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }
}

String sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char timeString[30];
  strftime(timeString, sizeof(timeString), "%d.%m.%Y %H:%M:%S", timeinfo);
  String caption = "📸 Фото з ESP32-CAM\nДата та час: " + String(timeString);

  //Dispose first picture because of bad quality
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb); // dispose the buffered image
  
  // Take a new photo
  fb = NULL;  
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }  
  
  Serial.println("Connect to " + String(myDomain));


  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Connection successful");
    
        String head = "--myboundary\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID +
                  "\r\n--myboundary\r\nContent-Disposition: form-data; name=\"caption\"; \r\n\r\n" + caption +
                  "\r\n--myboundary\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";

    String tail = "\r\n--myboundary--\r\n";

    size_t imageLen = fb->len;
    size_t extraLen = head.length() + tail.length();
    size_t totalLen = imageLen + extraLen;
  
    clientTCP.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=myboundary");
    clientTCP.println();
    clientTCP.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0;n<fbLen;n=n+1024) {
      if (n+1024<fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  
    
    clientTCP.print(tail);
    
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;   // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;
    while ((startTimer + waitTime) > millis()){
      Serial.print(".");
      delay(100);      
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state==true) getBody += String(c);        
        if (c == '\n') {
          if (getAll.length()==0) state=true; 
          getAll = "";
        } 
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length()>0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  }
  else {
    getBody="Connected to api.telegram.org failed.";
    Serial.println("Connected to api.telegram.org failed.");
  }
  return getBody;
}

void setup(){
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);

  ledcSetup(0, 2000, 8);    
  ledcAttachPin(SIREN_PIN, 0); 

  WiFi.mode(WIFI_STA);
  IPAddress local_IP(0,0,0,0);   
  IPAddress gateway(0,0,0,0);    
  IPAddress subnet(0,0,0,0);     
  IPAddress primaryDNS(8,8,8,8); 
  IPAddress secondaryDNS(8,8,4,4);
  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);

  pinMode(TRIGGER_PIN, INPUT_PULLUP); 

  configInitCamera();
  Serial.println();
  Serial.print("Connecting to  ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add .root certificate for api.telegramorg
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP()); 
  configTime(3 * 3600, 0, "pool.ntp.org"); // GMT+3 для Украины
}

void loop() {
  if (sendPhoto) {
    Serial.println("Preparing photo");
    sendPhotoTelegram(); 
    sendPhoto = false; 
  }
  
    // В loop() вместо tone()
  if (digitalRead(TRIGGER_PIN) != LOW && !sendPhoto) {
    if (millis() - lastPhotoTime >= photoDelay) {
      Serial.println("TRIGGER");
      ledcWriteTone(0, 430);   // включаем звук 420 Гц
      ledcwr = true;
      sendPhoto = true;
      lastPhotoTime = millis();
    }
  }

  // Для выключения через 5 секунд
  
  if (ledcwr && sirenStart == 0) {
    sirenStart = millis();
  }
  if (sirenStart > 0 && millis() - sirenStart >= 1000) {
    ledcWriteTone(0, 0);       // выключаем звук
    sirenStart = 0;
    ledcwr = false;
  }
}