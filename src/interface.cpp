#include "main.h"

#include "ui.h"
#include "basicui.h"
#include "EmbUI.h"
#include "interface.h"

// Photo File Name to save in LittleFS
#define FILE_PHOTO "/photo.jpg"

/**
 * Define configuration variables and controls handlers
 * variables has literal names and are kept within json-configuration file on flash
 * 
 * Control handlers are bound by literal name with a particular method. This method is invoked
 * by manipulating controls
 * 
 */
void create_parameters(){
    LOG(println, F("Создание конфигурационных переменных по-умолчанию"));
    BasicUI::add_sections();

    //embui.var(FPSTR(P_hostname), "esp32cam", true);        // device hostname (autogenerated on first-run)

    //embui.section_handle_add("esp32camdemo", block_cam);
    embui.section_handle_add("esp32cam", block_cam);
    embui.section_handle_add("stream", block_stream);

    embui.section_handle_add("ledBtn", led_toggle);
    embui.section_handle_add("ledBright", set_led_bright);
    embui.section_handle_add("refresh", set_refresh);
}

/**
 * This code builds UI section with menu block on the left
 * 
 */
void block_menu(Interface *interf, JsonObject *data){
    if (!interf) return;
    // создаем меню
    interf->json_section_menu();

    interf->option("esp32cam", F("esp32cam"));
    interf->option("stream", F("stream"));

    /**
     * добавляем в меню пункт - настройки,
     * это автоматически даст доступ ко всем связанным секциям с интерфейсом для системных настроек
     * 
     */
    BasicUI::opt_setup(interf, data);       // пункт меню "настройки"
    interf->json_section_end();
}

/**
 * Headlile section
 * this is an overriden weak method that builds our WebUI from the top
 * ==
 * Головная секция
 * переопределенный метод фреймфорка, который начинает строить корень нашего WebUI
 * 
 */
void section_main_frame(Interface *interf, JsonObject *data){
    if (!interf) return;

    interf->json_frame_interface("esp32cam");  // HEADLINE for WebUI
    block_menu(interf, data);
    //block_cam(interf, data);
    interf->json_frame_flush();

    if(!embui.sysData.wifi_sta && embui.param(FPSTR(P_APonly))=="0"){
        // форсируем выбор вкладки настройки WiFi если контроллер не подключен к внешней AP и не задан режим принудительного AP
        BasicUI::block_settings_netw(interf, data);
    } else {
        // главное окно
        block_cam(interf, data);
    }
}

uint8_t ledbright = 1;

// обработчик кнопки "Переключение светодиода"
void led_toggle(Interface *interf, JsonObject *data){
    if (!interf || !data) return;
    btask->toggle();
}

void set_led_bright(Interface *interf, JsonObject *data){
    if (!interf || !data) return;
    btask->setBright((*data)["ledBright"].as<int8_t>());
}

// Check if photo capture was successful
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

// Capture Photo and Save it to LittleFS
void capturePhotoSaveLittleFS() {
  camera_fb_t * fb = NULL; // pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  do {
    // Take a photo with the camera
    Serial.println("Taking a photo...");

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    // Photo file name
    Serial.printf("Picture file name: %s\n", FILE_PHOTO);
    File file = LittleFS.open(FILE_PHOTO, FILE_WRITE);

    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(fb->len);
      Serial.println(" bytes");
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);

    // check if file has been correctly saved in LittleFS
    ok = checkPhoto(LittleFS);
  } while ( !ok );
}

Task *_offtask = nullptr;
void set_refresh(Interface *interf, JsonObject *data)
{
    if(_offtask) return;
    
    btask->setBright(7);
    delay(200);
    capturePhotoSaveLittleFS();
    btask->setLedOffAfterMS(200);
    if(!_offtask){
        _offtask = new Task(TASK_SECOND,TASK_ONCE, []{
            LOG(println, "Update WebUI");
            Interface *interf =  embui.ws.count()? new Interface(&embui, &embui.ws, 512) : nullptr;
            //interf->json_frame_custom("xload");
            //interf->json_section_content();
            interf->json_frame_interface();
            interf->json_section_begin("photo");
            //interf->frame2("jpgf", "jpg");
            //interf->frame2("jpgf", String(FILE_PHOTO) + "?" + micros());
            interf->image("jpgf", String(FILE_PHOTO) + "?" + micros());
            interf->json_section_end();
            interf->json_frame_flush();
            delete interf;
            _offtask = nullptr;
            TASK_RECYCLE;
        },&ts,true);
        _offtask->enableDelayed();
    } else
        _offtask->restartDelayed();
}

void block_cam(Interface *interf, JsonObject *data){
    if (!interf) return;

    btask->off();
    interf->json_frame_interface();
    interf->json_section_main(String("jpg"), String("ESP32CAM"));
    interf->json_section_begin("photo");
    if(!checkPhoto(LittleFS)){
        btask->setBright(7);
        delay(50);
        //interf->frame2("jpgf", "jpg");
        interf->image("jpgf", String(FILE_PHOTO) + "?" + micros());
        btask->setLedOffAfterMS(500);
    } else
        //interf->frame2("jpgf", String(FILE_PHOTO) + "?" + micros());
        interf->image("jpgf", String(FILE_PHOTO) + "?" + micros());
    interf->json_section_end();
    interf->button("refresh","Обновить");
    interf->json_section_end();
    interf->json_frame_flush();
}

void block_stream(Interface *interf, JsonObject *data){
    if (!interf) return;
    //LOG(println, "Here");

    interf->json_frame_interface();
    interf->json_section_main(String("esp32cam"), String("ESP32CAM STREAM"));

    //interf->frame("jpgframe", "<iframe class=\"iframe\" src=\"jpg\"></iframe>"); // marginheight=\"0\" marginwidth=\"0\" width=\"100%\" height=\"100%\" frameborder=\"0\" scrolling=\"yes\"
    //interf->frame2("jpgframe", "jpg");
    //interf->frame2("mjpgframe", "mjpeg/1");
    interf->image("stream", "mjpeg/1");
    interf->spacer();
    interf->range("ledBright",String(ledbright),String(0),String(15),String(1),"Уровень светимости светодиода", true);
    interf->button("ledBtn","Переключение светодиода");
    
    interf->json_section_end();
    interf->json_frame_flush();
}

void pubCallback(Interface *interf){
    if (!interf) return;
    interf->json_frame_value();
    interf->value(F("pTime"), embui.timeProcessor.getFormattedShortTime(), true);
    interf->value(F("pMem"), String(ESP.getFreeHeap())+" / "+String(ESP.getFreePsram()), true);
    //interf->value(F("pUptime"), String(millis()/1000), true);
    char fuptime[16];
    uint32_t tm = millis()/1000;
    sprintf_P(fuptime, PSTR("%u.%02u:%02u:%02u"),tm/86400,(tm/3600)%24,(tm/60)%60,tm%60);
    interf->value("pUptime", String(fuptime), true);
    //interf->value("fsfreespace", String(myLamp.getLampState().fsfreespace), true);
    int32_t rssi = WiFi.RSSI();
    interf->value("pRSSI", String(constrain(map(rssi, -85, -40, 0, 100),0,100)) + F("% (") + String(rssi) + F("dBm)"), true);
    interf->json_frame_flush();
}
