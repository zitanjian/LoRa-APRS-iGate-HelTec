#include <NTPClient.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>

#include <APRS-IS.h>
#include <heltec.h>

// 以github的 peterus/LoRa_APRS_iGate 为范例
// https://github.com/peterus/LoRa_APRS_iGate
// 主板为HelTec的ESP32，带1278射频模块和LED显示屏，带盒子和弹簧天线

#define WIFI_NAME "29603"          //WiFi用户名
#define WIFI_KEY "29603296031"     //WiFi密码

#define USER "BH4BCT-10"              //自己的呼号
#define PASS "18108"               //呼号对应的APRS计算密码 http://js.lhham.com/index.php/passcode
#define TOOL "Heltec-APRS-IS"      //主板-用途
#define VERS "0.1"                 //版本号
#define SERVER "202.141.176.2"     //APRS中国服务器地址
//#define SERVER "euro.aprs2.net"  //APRS国外服务器地址
#define PORT 14580                 //APRS服务器端口

#define BEACON_TIMEOUT (15)        //超时
#define BEACON_LAT_POS "3110.18N"  //iGate网关坐标，经度
#define BEACON_LONG_POS "12126.87E"//iGate网关坐标，纬度
#define BEACON_MESSAGE "433MHz LoRa IGATE(RX)"//APRS地图显示的附加信息

#define BAND    433E6              //LoRa APRS频率433.0MHz

APRS_IS aprs_is(USER, PASS, TOOL, VERS);
String BeaconMsg;
WiFiMulti WiFiMulti;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, 60*60);
int next_update = -1;

// 本段内容仅为示例，说明Heltec主板的ESP32与1278的关系
//            SX1278    ESP32     HelTec的ESP32主板，管脚对应
// NSS，CS:     19     GPIO 18        片选*
// DIO0:        8      GPIO 26        中断*，支持模拟
// RESET:       7      GPIO 14        复位*，（支持模拟）
// DIO1:        9      GPIO 35       数据输入输出，支持模拟
// DIO2:        10     GPIO 34       数据输入输出，支持模拟，FSK模拟使用
// MOSI：       18     GPIO 27       主出从进，SPI数据输入，（支持模拟）
// MISO：       17     GPIO 19       主进从出，SPI数据输出
// SCK：        16     GPIO 5         SPI时钟
// RXTX：       20                   Rx/Tx开关，Tx高
// BOOST：      27                    大功率输出
// CS=18片选,irq=26中断,rst=14复位,gpio=35数据输出
// SPI.begin(5, 19, 27, 18); //示例
// LoRa.setPins(18, 14, 26); //示例
// SX1278A fsk = new Module(18, 26, 14, 35); //示例

void setup() {

//  串口速率在heltec.cpp文件第120行已定义为 115200。
//  初始化显示屏、串口，LoRa射频模块，低功率，频率值。
//  Heltec.begin(true /*DisplayEnable Enable*/, true /*LoRa Disable*/, true /*Serial Enable*/, true /*PABOOST Enable*/, BAND /*long BAND*/);
  Heltec.begin(true, false, true, false, BAND);
  Heltec.display->init();
  Heltec.display->flipScreenVertically();  
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "LCD, Serial, LoRa Initial success!");
  Heltec.display->display();
  delay(500);
  
//  WiFi 连接
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(USER);
  WiFiMulti.addAP(WIFI_NAME, WIFI_KEY);
  while(WiFiMulti.run() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Heltec.display->drawString(0, 10, "WiFi init done！");
  Heltec.display->display();
  
// NTP 连接
  timeClient.begin();
  if(!timeClient.forceUpdate())
  {
    Serial.println("[WARN] NTP Client force update issue!");
  }
  Heltec.display->drawString(0, 20, "NTP Client init done!");
  Heltec.display->display();

// LoRa 参数设置
// LoRa.setPins(18, 14, 26);  //SS片选，RESET复位，DIO0中断，Heltec.begin已声明
  LoRa.setSpreadingFactor(12);   //扩散因子12
  LoRa.setSignalBandwidth(125E3);//带宽125KHz
  LoRa.setCodingRate4(5);        //编码率5
  LoRa.enableCrc();              //允许CRC校验
//  LoRa.setSyncWord(0xF3【int sw】);        //同步字
//  LoRa.setPreambleLength(long length);    //前导码长度
// LoRa注册：中断调用函数
//  LoRa.onReceive(onReceive);
// LoRa设置：接收模式
  LoRa.receive();

// APRS 参数设置
  APRSMessage msg;
  msg.setSource(USER);
  msg.setDestination("APVRT7");
  msg.getAPRSBody()->setData(String("=") + BEACON_LAT_POS + "I" + BEACON_LONG_POS + "&" + BEACON_MESSAGE);
  BeaconMsg = msg.encode();

  delay(500);
}

void loop() {

  timeClient.update();

  if(WiFiMulti.run() != WL_CONNECTED)
  {
    Serial.println("[ERROR] WiFi not connected!");
    delay(1000);
    return;
  }

  if(!aprs_is.connected())
  {
    Serial.print("[INFO] connecting to server: ");
    Serial.print(SERVER);
    Serial.print(" on port: ");
    Serial.println(PORT);
    Serial.println("INFO Connecting to server");
    if(!aprs_is.connect(SERVER, PORT))
    {
      Serial.println("[ERROR] Connection failed.");
      Serial.println("[INFO] Waiting 5 seconds before retrying...");
      Serial.println("ERROR Server connection failed! waiting 5 sec");
      delay(5000);
      return;
    }
    Serial.println("[INFO] Connected to server!");
  }

  if(next_update == timeClient.getMinutes() || next_update == -1)
  {
//    show_display(USER, "Beacon to Server...");
    Serial.print("[" + timeClient.getFormattedTime() + "] ");
    aprs_is.sendMessage(BeaconMsg);
    next_update = (timeClient.getMinutes() + BEACON_TIMEOUT) % 60;
  }

  if(aprs_is.available() > 0)
  {
    Heltec.display->clear();
    String timestr = timeClient.getFormattedTime();
    String str = aprs_is.getMessage();
    Heltec.display->drawString(0, 0, timestr);
    Heltec.display->drawStringMaxWidth(0, 10, 128, str);
    Heltec.display->display();

#ifdef SEND_MESSAGES_FROM_IS_TO_LORA
    std::shared_ptr<APRSMessage> msg = std::shared_ptr<APRSMessage>(new APRSMessage());
    msg->decode(str);
    lora_aprs.sendMessage(msg);
#endif
  }

}

void onReceive(int packetSize)
{
  // received a packet
  Serial.print("Received packet '");
  // read packet
  for (int i = 0; i < packetSize; i++)
  {
    Serial.print((char)LoRa.read());
  }
  // print RSSI of packet
  Serial.print("' with RSSI ");
  Serial.println(LoRa.packetRssi());
}
