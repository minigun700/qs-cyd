#define LGFX_USE_V1

#if defined(ESP32)
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#endif

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
// #include <TAMC_GT911.h>
#include <Wire.h>

// ==========================================
// 1. ตั้งค่าขาพินหน้าจอ LovyanGFX
// ==========================================
class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_RGB _panel_instance;
    lgfx::Bus_RGB _bus_instance;
    lgfx::Light_PWM _light_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;
            cfg.pin_d0 = GPIO_NUM_8;
            cfg.pin_d1 = GPIO_NUM_3;
            cfg.pin_d2 = GPIO_NUM_46;
            cfg.pin_d3 = GPIO_NUM_9;
            cfg.pin_d4 = GPIO_NUM_1;
            cfg.pin_d5 = GPIO_NUM_5;
            cfg.pin_d6 = GPIO_NUM_6;
            cfg.pin_d7 = GPIO_NUM_7;
            cfg.pin_d8 = GPIO_NUM_15;
            cfg.pin_d9 = GPIO_NUM_16;
            cfg.pin_d10 = GPIO_NUM_4;
            cfg.pin_d11 = GPIO_NUM_45;
            cfg.pin_d12 = GPIO_NUM_48;
            cfg.pin_d13 = GPIO_NUM_47;
            cfg.pin_d14 = GPIO_NUM_21;
            cfg.pin_d15 = GPIO_NUM_14;

            cfg.pin_henable = GPIO_NUM_40;
            cfg.pin_vsync = GPIO_NUM_41;
            cfg.pin_hsync = GPIO_NUM_39;
            cfg.pin_pclk = GPIO_NUM_42;

            cfg.freq_write = 14000000;
            cfg.hsync_polarity = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4;
            cfg.hsync_back_porch = 8;
            cfg.vsync_polarity = 0;
            cfg.vsync_front_porch = 8;
            cfg.vsync_pulse_width = 4;
            cfg.vsync_back_porch = 8;
            cfg.pclk_idle_high = 1;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width = 800;
            cfg.memory_height = 480;
            cfg.panel_width = 800;
            cfg.panel_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = GPIO_NUM_2;
            cfg.freq = 1000;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        setPanel(&_panel_instance);
    }
};

LGFX lcd;

// ==========================================
// 2. ตั้งค่าทัชสกรีน (TAMC_GT911)
// ==========================================
#define TOUCH_SDA 19
#define TOUCH_SCL 20
#define TOUCH_INT 18
#define TOUCH_RST 38
#define TOUCH_WIDTH 800
#define TOUCH_HEIGHT 480

#define GT_MAX_X 480 // ความละเอียดที่ชิปรายงาน (จากค่า raw สูงสุดที่เห็น)
#define GT_MAX_Y 272
#define GT_FLIP_X 1 // ถ้านิ้วซ้าย-ขวากลับด้าน เปลี่ยนเป็น 1
#define GT_FLIP_Y 1 // ถ้านิ้วบน-ล่างกลับด้าน เปลี่ยนเป็น 1

// TAMC_GT911 ts = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH, TOUCH_HEIGHT);
static uint8_t gtAddr = 0; // 0 = ยังไม่เจอชิป

static bool gtRead(uint16_t reg, uint8_t *buf, uint8_t len)
{
    Wire.beginTransmission(gtAddr);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    if (Wire.endTransmission(false) != 0)
        return false;
    if (Wire.requestFrom((int)gtAddr, (int)len) != len)
        return false;
    for (uint8_t i = 0; i < len; i++)
        buf[i] = Wire.read();
    return true;
}

static void gtWrite(uint16_t reg, uint8_t v)
{
    Wire.beginTransmission(gtAddr);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    Wire.write(v);
    Wire.endTransmission();
}

// หาชิปที่ 0x5D แล้วลอง 0x14 พิมพ์ผลทาง Serial
static void gtInit()
{
    Wire.setClock(400000);
    const uint8_t cand[2] = {0x5D, 0x14};
    uint8_t id[4] = {0};

    for (uint8_t k = 0; k < 2; k++)
    {
        gtAddr = cand[k];
        if (gtRead(0x8140, id, 4))
        {
            Serial.printf("GT911 addr 0x%02X id %c%c%c%c\n", gtAddr, id[0], id[1], id[2], id[3]);
            return;
        }
    }
    gtAddr = 0;
    Serial.println("GT911 not found");
}

// คืน true ถ้านิ้วกดอยู่ (ค่าดิบ x, y); ถ้าชิปยังไม่มีเฟรมใหม่ คงสถานะเดิมไว้
static bool gtGetPoint(int &x, int &y)
{
    static bool touched = false;
    static int lx = 0, ly = 0;

    if (gtAddr != 0)
    {
        uint8_t st;
        if (gtRead(0x814E, &st, 1) && (st & 0x80))
        {
            uint8_t n = st & 0x0F;
            if (n > 0 && n <= 5)
            {
                uint8_t p[4];
                if (gtRead(0x8150, p, 4))
                {
                    lx = p[0] | (p[1] << 8);
                    ly = p[2] | (p[3] << 8);
                    touched = true;
                }
            }
            else
            {
                touched = false;
            }
            gtWrite(0x814E, 0); // บอกชิปว่าอ่านแล้ว
        }
    }

    x = lx;
    y = ly;
    return touched;
}
// ==========================================
// 3. Bluetooth + Modbus ASCII
//   อ่าน rpm : :010320020001D9      (สั่งซ้ำ = สลับเปิด/หยุดส่งทุก 100 ms)
//   อ่านองศา : :010340000010AC      องศา ×10 (300 = 30.0)
//   อ่านพัลส์ : :010350000010 9C
//   เขียน    : :01 10 addrH addrL byteCnt data... LRC   ตอบ :0110 addr 0010 LRC
// ==========================================
static NimBLEAdvertisedDevice *advDevice;
static bool doConnect = false;
static volatile bool connected = false;
static NimBLEClient *pClient = nullptr;

NimBLERemoteCharacteristic *pNotifyChar = nullptr;
NimBLERemoteCharacteristic *pWriteChar = nullptr;

// ---- รายการอุปกรณ์ที่สแกนเจอ (ปุ่ม SCAN BT) ----
#define MAX_DEV 12
#define SCAN_SECONDS 8
static char devName[MAX_DEV][24];
static char devAddr[MAX_DEV][20];
static int devRssi[MAX_DEV];
static NimBLEAdvertisedDevice *devPtr[MAX_DEV];
static volatile int devCount = 0;
static int shownCount = 0; // จำนวนที่แสดงในลิสต์แล้ว
static volatile bool scanning = false;
static char selName[24] = ""; // ชื่ออุปกรณ์ที่เลือกเชื่อมต่อ
volatile int selIdx = -1;

#define BINS 16
#define MB_ID 0x01
#define ADDR_RPM 0x2002
#define ADDR_SENS 0x2004
#define ADDR_DEG 0x4000
#define ADDR_PLS 0x5000
#define DEG_MAX_X10 1000 // องศา 0-100
#define PLS_MIN 0        // เวลาหน่วง 0-200 ms
#define PLS_MAX 200
#define SENS_MIN 0       // ความไวเซนเซอร์ 0-100%
#define SENS_MAX 100
#define OP_TIMEOUT_MS 3000
#define RPM_RETRY_MS 1000 // ไม่มี rpm เข้ามานานเกินนี้ ส่งคำขอใหม่
#define BLE_CHUNK 20      // ส่งทีละ 20 ไบต์ ให้เข้ากับโมดูล BLE-UART ทั่วไป

volatile int currentRPM = 0;
volatile uint32_t lastRpmMs = 0;
static uint32_t lastRpmReqMs = 0;

// ข้อมูลที่ callback แกะได้ (loop เป็นคนเอาไปใช้เมื่อ flag ขึ้น)
static int rxDeg[BINS];
static int rxPls[BINS];
static int rxSens = 50;
volatile bool evDegRead = false;
volatile bool evPlsRead = false;
volatile bool evSensRead = false;
volatile bool evDegAck = false;
volatile bool evPlsAck = false;
volatile bool evSensAck = false;

enum Op
{
    OP_IDLE,
    OP_LOAD_DEG,
    OP_LOAD_PLS,
    OP_LOAD_SENS,
    OP_SAVE_DEG,
    OP_SAVE_PLS,
    OP_SAVE_SENS
};
static Op op = OP_IDLE;
static uint32_t opStartMs = 0;
static int saveDeg[BINS];
static int savePls[BINS];
static int saveSens = 50;

volatile bool uiLoadReq = false;
volatile bool uiSaveReq = false;
volatile bool uiScanReq = false;
volatile bool uiSelectReq = false;
volatile bool uiCloseReq = false;

// ==========================================
// 4. ตัวแปรสำหรับ LVGL
// ==========================================
static const uint16_t screenWidth = 800;
static const uint16_t screenHeight = 480;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf;

lv_obj_t *label_rpm;
lv_obj_t *label_status;
lv_obj_t *label_msg;
lv_obj_t *btn_load;
lv_obj_t *btn_save;
lv_obj_t *btn_scan;
lv_obj_t *dev_panel;
lv_obj_t *dev_list;
lv_obj_t *right_panel;
lv_obj_t *ta_deg[BINS];
lv_obj_t *ta_pls[BINS];
lv_obj_t *ta_sens;
lv_obj_t *kb;

#define KB_HEIGHT 200

// ==========================================
// 5. ฟังก์ชัน Bridge ให้ LVGL คุยกับหน้าจอ
// ==========================================
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    lcd.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t *)&color_p->full);
    lv_disp_flush_ready(disp_drv);
}

void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    int raw_x, raw_y;

    if (gtGetPoint(raw_x, raw_y))
    {
        data->state = LV_INDEV_STATE_PR;

        // แมปค่าดิบให้อยู่ในสเกล 800x480 ตามที่วัดมา
        int mapped_x = (long)raw_x * 800 / GT_MAX_X;
        int mapped_y = (long)raw_y * 480 / GT_MAX_Y;
        if (GT_FLIP_X)
            mapped_x = 799 - mapped_x;
        if (GT_FLIP_Y)
            mapped_y = 479 - mapped_y;

        // ล็อกค่าไม่ให้ทะลุขอบจอ
        data->point.x = constrain(mapped_x, 0, 800);
        data->point.y = constrain(mapped_y, 0, 480);

        Serial.printf("raw %d,%d -> X: %d, Y: %d\n", raw_x, raw_y, data->point.x, data->point.y);
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

// ==========================================
// 6. Modbus ASCII
// ==========================================
static uint8_t hexv(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return 0xFF;
}

// b = id func ... (ไม่รวม LRC) ฟังก์ชันนี้ต่อ LRC + CRLF ให้เอง
static bool sendFrame(const uint8_t *b, size_t n)
{
    if (!connected || pWriteChar == nullptr)
        return false;

    static const char H[] = "0123456789ABCDEF";
    char out[96];
    size_t k = 0;
    uint8_t sum = 0;

    out[k++] = ':';
    for (size_t i = 0; i < n; i++)
    {
        out[k++] = H[b[i] >> 4];
        out[k++] = H[b[i] & 0x0F];
        sum += b[i];
    }
    uint8_t lrc = (uint8_t)(0 - sum);
    out[k++] = H[lrc >> 4];
    out[k++] = H[lrc & 0x0F];
    out[k++] = '\r';
    out[k++] = '\n';

    bool resp = !pWriteChar->canWriteNoResponse();

    for (size_t off = 0; off < k; off += BLE_CHUNK)
    {
        size_t len = (k - off < BLE_CHUNK) ? (k - off) : BLE_CHUNK;
        if (!pWriteChar->writeValue((const uint8_t *)out + off, len, resp))
            return false;
        if (off + len < k)
            delay(5);
    }
    Serial.printf("TX: %.*s\n", (int)k - 2, out);
    return true;
}

static void sendRpmReq()
{
    uint8_t b[6] = {MB_ID, 0x03, ADDR_RPM >> 8, ADDR_RPM & 0xFF, 0x00, 0x01};
    sendFrame(b, 6);
    lastRpmReqMs = millis();
}

static void sendRead(uint16_t addr, uint8_t qty)
{
    uint8_t b[6] = {MB_ID, 0x03, (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF), 0x00, qty};
    sendFrame(b, 6);
}

static void sendWrite(uint16_t addr, const int *v, uint8_t qty = BINS)
{
    uint8_t b[7 + BINS * 2];
    b[0] = MB_ID;
    b[1] = 0x10;
    b[2] = addr >> 8;
    b[3] = addr & 0xFF;
    b[4] = 0x00;
    b[5] = qty;
    b[6] = qty * 2;
    for (int i = 0; i < qty; i++)
    {
        b[7 + i * 2] = (uint8_t)(v[i] >> 8);
        b[8 + i * 2] = (uint8_t)(v[i] & 0xFF);
    }
    sendFrame(b, 7 + qty * 2);
}

// s = ตัวอักษร hex หลัง ':' ก่อน CRLF
static void handleFrame(const char *s, size_t n)
{
    if (n < 14 || (n & 1))
        return;

    uint8_t b[48];
    size_t nb = n / 2;
    if (nb > sizeof(b))
        return;

    uint8_t sum = 0;
    for (size_t i = 0; i < nb; i++)
    {
        uint8_t h = hexv(s[i * 2]);
        uint8_t l = hexv(s[i * 2 + 1]);
        if (h == 0xFF || l == 0xFF)
            return;
        b[i] = (h << 4) | l;
        sum += b[i];
    }
    if (sum != 0)
        return; // LRC ผิด
    if (b[0] != MB_ID)
        return;

    uint16_t addr = ((uint16_t)b[2] << 8) | b[3];

    if (b[1] == 0x03)
    { // id func addrH addrL byteCnt data... lrc
        uint8_t bc = b[4];
        if (nb != (size_t)(6 + bc))
            return;

        if (addr == ADDR_RPM && bc == 2)
        {
            currentRPM = ((int)b[5] << 8) | b[6];
            lastRpmMs = millis();
        }
        else if (addr == ADDR_SENS && bc == 2)
        {
            rxSens = ((int)b[5] << 8) | b[6];
            evSensRead = true;
        }
        else if ((addr == ADDR_DEG || addr == ADDR_PLS) && bc == BINS * 2)
        {
            int *dst = (addr == ADDR_DEG) ? rxDeg : rxPls;
            for (int i = 0; i < BINS; i++)
            {
                dst[i] = ((int)b[5 + i * 2] << 8) | b[6 + i * 2];
            }
            if (addr == ADDR_DEG)
                evDegRead = true;
            else
                evPlsRead = true;
        }
    }
    else if (b[1] == 0x10)
    { // id func addrH addrL qtyH qtyL lrc
        if (nb == 7 && b[4] == 0x00)
        {
            if (addr == ADDR_DEG && b[5] == BINS)
                evDegAck = true;
            else if (addr == ADDR_PLS && b[5] == BINS)
                evPlsAck = true;
            else if (addr == ADDR_SENS && b[5] == 1)
                evSensAck = true;
        }
    }
}

// ==========================================
// 7. UI
// ==========================================
static void setStatus(const char *s, uint32_t color)
{
    lv_label_set_text(label_status, s);
    lv_obj_set_style_text_color(label_status, lv_color_hex(color), 0);
}

static void setMsg(const char *s, uint32_t color = 0xFF5555)
{
    lv_label_set_text(label_msg, s);
    lv_obj_set_style_text_color(label_msg, lv_color_hex(color), 0);
}

static void ta_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED)
    {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(kb);

        if (ta != ta_sens)
        {
            // ย่นตารางไม่ให้คีย์บอร์ดทับ แล้วเลื่อนช่องที่แก้ให้อยู่ในมุมมอง
            lv_obj_set_height(right_panel, screenHeight - KB_HEIGHT - 10);
            lv_obj_update_layout(right_panel);
            lv_obj_scroll_to_view(ta, LV_ANIM_OFF);
        }
    }
    else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
    {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(ta, LV_STATE_FOCUSED);
        lv_obj_set_height(right_panel, 470);
    }
}

static void btn_load_event_cb(lv_event_t *e) { uiLoadReq = true; }
static void btn_save_event_cb(lv_event_t *e) { uiSaveReq = true; }
static void btn_scan_event_cb(lv_event_t *e) { uiScanReq = true; }
static void btn_close_event_cb(lv_event_t *e) { uiCloseReq = true; }

static void dev_item_cb(lv_event_t *e)
{
    selIdx = (int)(intptr_t)lv_event_get_user_data(e);
    uiSelectReq = true;
}

static void rpmLabel(int i, char *out, size_t sz)
{
    if (i < BINS - 1)
        snprintf(out, sz, "<=%d", 3000 + i * 500);
    else
        snprintf(out, sz, ">%d", 3000 + (BINS - 2) * 500);
}

static void fillDegUI()
{
    char t[12];
    for (int i = 0; i < BINS; i++)
    {
        snprintf(t, sizeof(t), "%d.%d", rxDeg[i] / 10, rxDeg[i] % 10);
        lv_textarea_set_text(ta_deg[i], t);
    }
}

static void fillPlsUI()
{
    char t[12];
    for (int i = 0; i < BINS; i++)
    {
        snprintf(t, sizeof(t), "%d", rxPls[i]);
        lv_textarea_set_text(ta_pls[i], t);
    }
}

static void fillSensUI()
{
    char t[12];
    snprintf(t, sizeof(t), "%d", rxSens);
    lv_textarea_set_text(ta_sens, t);
}

// อ่านค่าจากช่องกรอก ตรวจช่วง; คืน false พร้อมข้อความ ถ้ามีช่องผิด
static bool collectSave()
{
    char m[48];

    const char *ss = lv_textarea_get_text(ta_sens);
    if (ss[0] == 0)
    {
        setMsg("Sens % is empty");
        return false;
    }
    int s = atoi(ss);
    if (s < SENS_MIN || s > SENS_MAX)
    {
        snprintf(m, sizeof(m), "Sens: %d-%d%%", SENS_MIN, SENS_MAX);
        setMsg(m);
        return false;
    }
    saveSens = s;

    for (int i = 0; i < BINS; i++)
    {
        const char *sd = lv_textarea_get_text(ta_deg[i]);
        const char *sp = lv_textarea_get_text(ta_pls[i]);

        if (sd[0] == 0 || sp[0] == 0)
        {
            snprintf(m, sizeof(m), "Row %d is empty", i + 1);
            setMsg(m);
            return false;
        }

        int d = (int)(atof(sd) * 10.0f + 0.5f);
        int p = atoi(sp);

        if (d < 0 || d > DEG_MAX_X10)
        {
            snprintf(m, sizeof(m), "Row %d: degree 0-100", i + 1);
            setMsg(m);
            return false;
        }
        if (p < PLS_MIN || p > PLS_MAX)
        {
            snprintf(m, sizeof(m), "Row %d: time %d-%d ms", i + 1, PLS_MIN, PLS_MAX);
            setMsg(m);
            return false;
        }

        saveDeg[i] = d;
        savePls[i] = p;
    }
    return true;
}

void build_ui()
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x222222), 0);

    // --- แผงซ้าย: rpm / สถานะ / ปุ่ม ---
    lv_obj_t *left_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(left_panel, 215, 470);
    lv_obj_align(left_panel, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_pad_all(left_panel, 6, 0);
    lv_obj_set_style_bg_color(left_panel, lv_color_hex(0x111111), 0);
    lv_obj_clear_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE);

    label_rpm = lv_label_create(left_panel);
    lv_obj_set_style_text_color(label_rpm, lv_color_hex(0xFFDD00), 0);
    lv_obj_set_style_text_font(label_rpm, &lv_font_montserrat_48, 0);
    lv_label_set_text(label_rpm, "RPM\n0");
    lv_obj_align(label_rpm, LV_ALIGN_TOP_MID, 0, 5);

    label_status = lv_label_create(left_panel);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_28, 0);
    lv_label_set_long_mode(label_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_status, 190);
    lv_obj_set_style_text_align(label_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_status, LV_ALIGN_TOP_MID, 0, 110);
    setStatus("Not connected", 0xFFAA00);

    label_msg = lv_label_create(left_panel);
    lv_label_set_long_mode(label_msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_msg, 190);
    lv_obj_set_style_text_align(label_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_msg, LV_ALIGN_TOP_MID, 0, 178);
    setMsg("Press SCAN BT", 0xAAAAAA);

    // --- ช่องปรับความไวเซนเซอร์ (Sens %) ---
    lv_obj_t *lbl_sens = lv_label_create(left_panel);
    lv_label_set_text(lbl_sens, "Sens (%)");
    lv_obj_set_style_text_color(lbl_sens, lv_color_white(), 0);
    lv_obj_align(lbl_sens, LV_ALIGN_TOP_LEFT, 10, 226);

    ta_sens = lv_textarea_create(left_panel);
    lv_obj_set_size(ta_sens, 95, 42);
    lv_obj_align(ta_sens, LV_ALIGN_TOP_RIGHT, -6, 212);
    lv_textarea_set_one_line(ta_sens, true);
    lv_textarea_set_accepted_chars(ta_sens, "0123456789");
    lv_textarea_set_max_length(ta_sens, 3);
    lv_textarea_set_placeholder_text(ta_sens, "0-100");
    lv_textarea_set_text(ta_sens, "50");
    lv_obj_set_style_text_color(ta_sens, lv_color_black(), 0);
    lv_obj_add_event_cb(ta_sens, ta_event_cb, LV_EVENT_ALL, NULL);

    btn_scan = lv_btn_create(left_panel);
    lv_obj_set_size(btn_scan, 190, 50);
    lv_obj_align(btn_scan, LV_ALIGN_BOTTOM_MID, 0, -118);
    lv_obj_add_event_cb(btn_scan, btn_scan_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_scan = lv_label_create(btn_scan);
    lv_label_set_text(lbl_scan, "SCAN BT");
    lv_obj_center(lbl_scan);

    btn_load = lv_btn_create(left_panel);
    lv_obj_set_size(btn_load, 190, 50);
    lv_obj_align(btn_load, LV_ALIGN_BOTTOM_MID, 0, -59);
    lv_obj_add_event_cb(btn_load, btn_load_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_load = lv_label_create(btn_load);
    lv_label_set_text(lbl_load, "LOAD TUNE");
    lv_obj_center(lbl_load);

    btn_save = lv_btn_create(left_panel);
    lv_obj_set_size(btn_save, 190, 50);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(0xCC0000), 0);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(btn_save, btn_save_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "SAVE TUNE");
    lv_obj_center(lbl_save);

    // --- แผงขวา: ตารางจูน 16 แถว แบ่ง 2 คอลัมน์ (8 + 8) ---
    right_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(right_panel, 570, 470);
    lv_obj_align(right_panel, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_style_pad_all(right_panel, 4, 0);
    lv_obj_set_style_bg_color(right_panel, lv_color_hex(0x111111), 0);
    lv_obj_set_style_text_color(right_panel, lv_color_white(), 0);
    lv_obj_set_scroll_dir(right_panel, LV_DIR_VER);

    for (int g = 0; g < 2; g++)
    {
        int x0 = g * 280;

        lv_obj_t *h1 = lv_label_create(right_panel);
        lv_label_set_text(h1, "RPM");
        lv_obj_align(h1, LV_ALIGN_TOP_LEFT, x0 + 4, 4);
        lv_obj_t *h2 = lv_label_create(right_panel);
        lv_label_set_text(h2, "Degree");
        lv_obj_align(h2, LV_ALIGN_TOP_LEFT, x0 + 90, 4);
        lv_obj_t *h3 = lv_label_create(right_panel);
        lv_label_set_text(h3, "ms");
        lv_obj_align(h3, LV_ALIGN_TOP_LEFT, x0 + 200, 4);
    }

    for (int i = 0; i < BINS; i++)
    {
        int x0 = (i / 8) * 280;
        int y = 28 + (i % 8) * 52;
        char t[12];

        lv_obj_t *lbl = lv_label_create(right_panel);
        rpmLabel(i, t, sizeof(t));
        lv_label_set_text(lbl, t);
        lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, x0 + 4, y + 14);

        ta_deg[i] = lv_textarea_create(right_panel);
        lv_obj_set_size(ta_deg[i], 100, 44);
        lv_obj_align(ta_deg[i], LV_ALIGN_TOP_LEFT, x0 + 72, y);
        lv_textarea_set_one_line(ta_deg[i], true);
        lv_textarea_set_accepted_chars(ta_deg[i], "0123456789.");
        lv_textarea_set_max_length(ta_deg[i], 5);
        lv_textarea_set_placeholder_text(ta_deg[i], "-");
        lv_obj_set_style_text_color(ta_deg[i], lv_color_black(), 0);
        lv_obj_add_event_cb(ta_deg[i], ta_event_cb, LV_EVENT_ALL, NULL);

        ta_pls[i] = lv_textarea_create(right_panel);
        lv_obj_set_size(ta_pls[i], 70, 44);
        lv_obj_align(ta_pls[i], LV_ALIGN_TOP_LEFT, x0 + 180, y);
        lv_textarea_set_one_line(ta_pls[i], true);
        lv_textarea_set_accepted_chars(ta_pls[i], "0123456789");
        lv_textarea_set_max_length(ta_pls[i], 3);
        lv_textarea_set_placeholder_text(ta_pls[i], "-");
        lv_obj_set_style_text_color(ta_pls[i], lv_color_black(), 0);
        lv_obj_add_event_cb(ta_pls[i], ta_event_cb, LV_EVENT_ALL, NULL);
    }

    kb = lv_keyboard_create(lv_scr_act());
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(kb, 800, KB_HEIGHT);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);

    // --- หน้าต่างเลือกอุปกรณ์ BT (ซ่อนไว้จนกดปุ่ม SCAN BT) ---
    dev_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(dev_panel, 460, 360);
    lv_obj_align(dev_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(dev_panel, 8, 0);
    lv_obj_set_style_bg_color(dev_panel, lv_color_hex(0x333333), 0);
    lv_obj_clear_flag(dev_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dev_title = lv_label_create(dev_panel);
    lv_label_set_text(dev_title, "Select Bluetooth device");
    lv_obj_set_style_text_color(dev_title, lv_color_white(), 0);
    lv_obj_align(dev_title, LV_ALIGN_TOP_MID, 0, 0);

    dev_list = lv_list_create(dev_panel);
    lv_obj_set_size(dev_list, 440, 250);
    lv_obj_align(dev_list, LV_ALIGN_TOP_MID, 0, 28);

    lv_obj_t *btn_close = lv_btn_create(dev_panel);
    lv_obj_set_size(btn_close, 140, 44);
    lv_obj_align(btn_close, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(btn_close, btn_close_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, "CLOSE");
    lv_obj_center(lbl_close);

    lv_obj_add_flag(dev_panel, LV_OBJ_FLAG_HIDDEN);
}

// ==========================================
// 8. Bluetooth Callbacks
// ==========================================
void notifyCallback(NimBLERemoteCharacteristic *pRemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    Serial.printf("RX %u: %.*s\n", (unsigned)length, (int)length, (const char *)pData);
    static char fb[100];
    static size_t fl = 0;
    static bool in = false;

    for (size_t i = 0; i < length; i++)
    {
        char c = (char)pData[i];

        if (c == ':')
        {
            in = true;
            fl = 0;
        }
        else if (!in || c == '\r')
        {
            continue;
        }
        else if (c == '\n')
        {
            handleFrame(fb, fl);
            in = false;
            fl = 0;
        }
        else if (fl < sizeof(fb))
        {
            fb[fl++] = c;
        }
        else
        {
            in = false;
        }
    }
}

class ClientCallbacks : public NimBLEClientCallbacks
{
    void onConnect(NimBLEClient *pClient)
    {
        Serial.println("เชื่อมต่อบลูทูธสำเร็จ!");
    }
    void onDisconnect(NimBLEClient *pClient)
    {
        connected = false;
        Serial.println("บลูทูธหลุด!");
    }
};

static ClientCallbacks cliCb;

// เก็บอุปกรณ์ที่มีชื่อเข้าลิสต์ (ไม่ซ้ำที่อยู่) loop เป็นคนเอาไปแสดงบนจอ
class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
    void onResult(NimBLEAdvertisedDevice *advertisedDevice)
    {
        std::string name = advertisedDevice->getName();
        if (name.empty())
            return;

        std::string addr = advertisedDevice->getAddress().toString();
        int n = devCount;

        for (int i = 0; i < n; i++)
        {
            if (strcmp(devAddr[i], addr.c_str()) == 0)
                return;
        }
        if (n >= MAX_DEV)
            return;

        strncpy(devName[n], name.c_str(), sizeof(devName[n]) - 1);
        devName[n][sizeof(devName[n]) - 1] = 0;
        strncpy(devAddr[n], addr.c_str(), sizeof(devAddr[n]) - 1);
        devAddr[n][sizeof(devAddr[n]) - 1] = 0;
        devRssi[n] = advertisedDevice->getRSSI();
        devPtr[n] = advertisedDevice;
        devCount = n + 1;
    }
};

static void scanEnded(NimBLEScanResults results)
{
    scanning = false;
}

bool connectToServer()
{
    // ล้าง client ของอุปกรณ์ก่อนหน้า (กันเต็มโควตา client เวลาสลับบอร์ดหลายตัว)
    if (pClient != nullptr)
    {
        if (pClient->isConnected())
            pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
    }

    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(&cliCb, false);

    if (!pClient->connect(advDevice))
    {
        return false;
    }

    pNotifyChar = nullptr;
    pWriteChar = nullptr;

    std::vector<NimBLERemoteService *> *pServices = pClient->getServices(true);
    for (auto pService : *pServices)
    {
        if (pService->getUUID().bitSize() == 16 && (pService->getUUID() == NimBLEUUID((uint16_t)0x1800) || pService->getUUID() == NimBLEUUID((uint16_t)0x1801)))
        {
            continue;
        }

        std::vector<NimBLERemoteCharacteristic *> *pChars = pService->getCharacteristics(true);
        for (auto pChar : *pChars)
        {
            if (pChar->canNotify())
                pNotifyChar = pChar;
            if (pChar->canWrite() || pChar->canWriteNoResponse())
                pWriteChar = pChar;
        }
    }

    if (pNotifyChar != nullptr)
    {
        pNotifyChar->subscribe(true, notifyCallback, true);
        NimBLERemoteDescriptor *pCCCD = pNotifyChar->getDescriptor(NimBLEUUID((uint16_t)0x2902));
        if (pCCCD)
        {
            uint8_t v[2] = {0x01, 0x00};
            pCCCD->writeValue(v, 2, true);
        }
        Serial.printf("notify=%s write=%s\n",
                      pNotifyChar->getUUID().toString().c_str(),
                      pWriteChar ? pWriteChar->getUUID().toString().c_str() : "NULL");
        connected = true;
        return true;
    }
    else
    {
        pClient->disconnect();
        return false;
    }
}

// ปุ่ม SCAN BT: ตัดการเชื่อมต่อเดิม ล้างลิสต์ เปิดหน้าต่างเลือก แล้วเริ่มสแกน
static void startScan()
{
    NimBLEScan *pScan = NimBLEDevice::getScan();

    if (pScan->isScanning())
        pScan->stop();

    if (pClient != nullptr && pClient->isConnected())
        pClient->disconnect();

    connected = false;
    op = OP_IDLE;

    devCount = 0;
    shownCount = 0;
    lv_obj_clean(dev_list);
    lv_obj_clear_flag(dev_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(dev_panel);

    scanning = true;
    setStatus("Scanning...", 0xFFAA00);
    setMsg("");

    pScan->start(SCAN_SECONDS, scanEnded, false); // ไม่บล็อก
}

static void selectDevice(int i)
{
    if (i < 0 || i >= devCount)
        return;

    NimBLEDevice::getScan()->stop();
    scanning = false;

    advDevice = devPtr[i];
    strncpy(selName, devName[i], sizeof(selName) - 1);
    selName[sizeof(selName) - 1] = 0;

    lv_obj_add_flag(dev_panel, LV_OBJ_FLAG_HIDDEN);
    doConnect = true;
}

static void closeDevPanel()
{
    if (scanning)
    {
        NimBLEDevice::getScan()->stop();
        scanning = false;
    }

    lv_obj_add_flag(dev_panel, LV_OBJ_FLAG_HIDDEN);

    if (!connected)
    {
        setStatus("Not connected", 0xFFAA00);
        setMsg("Press SCAN BT", 0xAAAAAA);
    }
}

#define HC(tag) Serial.printf("%s heap_ok=%d free=%u psram=%u\n", tag, (int)heap_caps_check_integrity_all(true), (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getPsramSize())

void setup()
{
    Serial.begin(115200);
    HC("start");

    // ---- หน้าจอ ----
    lcd.init();
    lcd.setRotation(2);
    lcd.setBrightness(128);
    HC("after lcd");

    // ---- ทัชสกรีน ----
    pinMode(TOUCH_RST, OUTPUT);
    pinMode(TOUCH_INT, OUTPUT);
    digitalWrite(TOUCH_INT, LOW);
    digitalWrite(TOUCH_RST, LOW);
    delay(20);
    digitalWrite(TOUCH_RST, HIGH);
    delay(20);
    digitalWrite(TOUCH_INT, LOW);
    delay(50);
    pinMode(TOUCH_INT, INPUT);
    HC("touch pins");
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    HC("wire");
    gtInit();
    HC("ts.begin");
    HC("after touch");

    // ---- LVGL ----
    lv_init();
    buf = (lv_color_t *)heap_caps_malloc(screenWidth * screenHeight / 10 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    HC("after lvgl");

    build_ui();
    HC("after ui");

    // ---- Bluetooth (ไม่สแกนเอง รอกดปุ่ม SCAN BT) ----
    NimBLEDevice::init("");
    HC("after ble init");
    NimBLEDevice::setMTU(247);
    NimBLEScan *pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new ScanCallbacks());
    pScan->setActiveScan(true);
    Serial.println("setup done");
}

void loop()
{
    static bool once = false;
    if (!once)
    {
        once = true;
        Serial.println("loop running");
    }
    uint32_t now = millis();

    // ---- ปุ่ม SCAN BT / เลือกอุปกรณ์ / ปิดรายการ ----
    if (uiScanReq)
    {
        uiScanReq = false;
        startScan();
    }
    if (uiCloseReq)
    {
        uiCloseReq = false;
        closeDevPanel();
    }
    if (uiSelectReq)
    {
        uiSelectReq = false;
        selectDevice(selIdx);
    }

    // ---- เติมรายการอุปกรณ์ที่สแกนเจอลงลิสต์ ----
    int dc = devCount;
    while (shownCount < dc)
    {
        char t[56];
        snprintf(t, sizeof(t), "%s  (%d dBm)", devName[shownCount], devRssi[shownCount]);
        lv_obj_t *item = lv_list_add_btn(dev_list, NULL, t);
        lv_obj_add_event_cb(item, dev_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)shownCount);
        shownCount++;
    }

    // ---- สแกนจบเอง (ครบเวลา) ----
    static bool lastScanning = false;
    bool sc = scanning;
    if (lastScanning && !sc && !connected && !doConnect && !lv_obj_has_flag(dev_panel, LV_OBJ_FLAG_HIDDEN))
    {
        if (devCount > 0)
            setStatus("Select device", 0x00BFFF);
        else
            setStatus("No device", 0xFF5555);
    }
    lastScanning = sc;

    if (doConnect)
    {
        setStatus("Connecting...", 0x00BFFF);
        setMsg("");
        lv_timer_handler(); // วาดข้อความก่อนเข้าฟังก์ชันเชื่อมต่อที่ใช้เวลา

        if (connectToServer())
        {
            Serial.println("พร้อมใช้งาน!");
            lastRpmMs = now;
            sendRpmReq(); // เริ่มให้ GD32 ส่ง rpm ทุก 100 ms
        }
        else
        {
            Serial.println("ไม่สามารถเชื่อมต่อได้");
            setStatus("Connect failed", 0xFF5555);
            setMsg("Press SCAN BT", 0xAAAAAA);
        }
        doConnect = false;
    }

    // ---- สถานะการเชื่อมต่อเปลี่ยน ----
    static bool lastConn = false;
    if (connected != lastConn)
    {
        lastConn = connected;
        if (connected)
        {
            setStatus("Connected", 0x00FF00);
            setMsg(selName, 0xFFFFFF);
        }
        else
        {
            op = OP_IDLE;
            currentRPM = 0;
            if (!scanning && !doConnect)
            {
                setStatus("Not connected", 0xFFAA00);
                setMsg("Press SCAN BT", 0xAAAAAA);
            }
            else
            {
                setMsg("");
            }
        }
    }

    // ---- ถ้า rpm เงียบนานเกิน ส่งคำขอใหม่ (คำขอสลับเปิด/หยุดสตรีม) ----
    if (connected && op == OP_IDLE && (now - lastRpmMs) > RPM_RETRY_MS && (now - lastRpmReqMs) > RPM_RETRY_MS)
    {
        sendRpmReq();
    }

    // ---- แสดง rpm ----
    static int lastShownRPM = -1;
    int rpmNow = currentRPM;
    if (rpmNow != lastShownRPM)
    {
        lv_label_set_text_fmt(label_rpm, "RPM\n%d", rpmNow);
        lastShownRPM = rpmNow;
    }

    // ---- ปุ่ม LOAD / SAVE ----
    if (uiLoadReq)
    {
        uiLoadReq = false;
        if (!connected)
            setMsg("Not connected");
        else if (op != OP_IDLE)
            setMsg("Busy");
        else
        {
            setMsg("");
            setStatus("Loading...", 0x00BFFF);
            op = OP_LOAD_DEG;
            opStartMs = now;
            sendRead(ADDR_DEG, BINS);
        }
    }

    if (uiSaveReq)
    {
        uiSaveReq = false;
        if (!connected)
            setMsg("Not connected");
        else if (op != OP_IDLE)
            setMsg("Busy");
        else if (collectSave())
        {
            setMsg("");
            setStatus("Saving...", 0x00BFFF);
            op = OP_SAVE_DEG;
            opStartMs = now;
            sendWrite(ADDR_DEG, saveDeg);
        }
    }

    // ---- ผลตอบกลับจาก GD32 ----
    if (evDegRead)
    {
        evDegRead = false;
        if (op == OP_LOAD_DEG)
        {
            fillDegUI();
            op = OP_LOAD_PLS;
            opStartMs = now;
            sendRead(ADDR_PLS, BINS);
        }
    }
    if (evPlsRead)
    {
        evPlsRead = false;
        if (op == OP_LOAD_PLS)
        {
            fillPlsUI();
            op = OP_LOAD_SENS;
            opStartMs = now;
            sendRead(ADDR_SENS, 1);
        }
    }
    if (evSensRead)
    {
        evSensRead = false;
        if (op == OP_LOAD_SENS)
        {
            fillSensUI();
            op = OP_IDLE;
            setStatus("Loaded", 0x00FF00);
        }
    }
    if (evDegAck)
    {
        evDegAck = false;
        if (op == OP_SAVE_DEG)
        {
            op = OP_SAVE_PLS;
            opStartMs = now;
            sendWrite(ADDR_PLS, savePls, BINS);
        }
    }
    if (evPlsAck)
    {
        evPlsAck = false;
        if (op == OP_SAVE_PLS)
        {
            op = OP_SAVE_SENS;
            opStartMs = now;
            sendWrite(ADDR_SENS, &saveSens, 1);
        }
    }
    if (evSensAck)
    {
        evSensAck = false;
        if (op == OP_SAVE_SENS)
        {
            op = OP_IDLE;
            setStatus("Saved", 0x00FF00);
        }
    }

    // ---- ไม่มีตอบกลับ ----
    if (op != OP_IDLE && (now - opStartMs) > OP_TIMEOUT_MS)
    {
        op = OP_IDLE;
        setStatus("Timeout", 0xFF5555);
    }

    lv_timer_handler();
    delay(5);
}