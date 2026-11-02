#pragma execution_character_set("utf-8")
#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cwchar>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:wWinMainCRTStartup")

// ============================================================
// СОСТОЯНИЯ ИГРЫ
// ============================================================
enum GameState {
    STATE_DISCLAIMER = 0,
    STATE_MAIN_MENU = 1,
    STATE_OPTIONS = 2
};

// ============================================================
// ГЛОБАЛЫ
// ============================================================
HINSTANCE g_hInst = nullptr;
HWND g_hWnd = nullptr;
GameState g_state = STATE_DISCLAIMER;
HFONT g_fontMain = nullptr;
HFONT g_fontSmall = nullptr;

// Настройки
bool g_musicOn = true;
bool g_soundOn = true;

// Наведение мыши (для подсветки)
int g_hoveredItem = -1; // 0=Play, 1=Options, 2=Exit, 3=Understood, 4=Music, 5=Sound, 6=Back

// Цвета
const COLORREF BG_COLOR = RGB(255, 255, 255); // Белый фон
const COLORREF TEXT_COLOR = RGB(0, 0, 0);     // Черный текст
const COLORREF HOVER_COLOR = RGB(0, 150, 0);  // Темно-зеленый при наведении (отсылка к Марку)
const COLORREF SQUARE_COLOR = RGB(0, 0, 0);   // Черный куб

// ============================================================
// УТИЛИТЫ ОТРИСОВКИ
// ============================================================
void DrawTextCentered(HDC dc, const wchar_t* text, RECT rc, COLORREF color, HFONT font) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HFONT oldFont = (HFONT)SelectObject(dc, font);
    DrawTextW(dc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, oldFont);
}

void DrawTextRight(HDC dc, const wchar_t* text, RECT rc, COLORREF color, HFONT font) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HFONT oldFont = (HFONT)SelectObject(dc, font);
    DrawTextW(dc, text, -1, &rc, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE);
    SelectObject(dc, oldFont);
}

// ============================================================
// ОКНО
// ============================================================
LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_hWnd = h;
        
        // --- ИСПРАВЛЕНИЕ ЗАГРУЗКИ ШРИФТА ---
        // 1. Получаем полный путь к нашей .exe программе
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        
        // 2. Отрезаем имя файла .exe, оставляя только путь к папке
        wchar_t* lastSlash = wcsrchr(exePath, L'\\');
        if (lastSlash) *(lastSlash + 1) = L'\0';
        
        // 3. Склеиваем путь к папке и имя шрифта
        std::wstring fontPath = std::wstring(exePath) + L"Monodetermination.ttf";

        // 4. Загружаем шрифт и проверяем результат
        int fontsAdded = AddFontResourceExW(fontPath.c_str(), FR_PRIVATE, 0);
        if (fontsAdded == 0) {
            std::wstring errorMsg = L"Не удалось загрузить шрифт!\nУбедитесь, что файл Monodetermination.ttf лежит рядом с .exe файлом.\nОжидаемый путь: " + fontPath;
            MessageBoxW(h, errorMsg.c_str(), L"Ошибка шрифта", MB_ICONERROR);
        }
        
        // 5. Создаем шрифты. 
        // ВАЖНО: "Monodetermination" - это имя шрифта ВНУТРИ файла (см. свойства TTF).
        g_fontMain = CreateFontW(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Monodetermination");
        g_fontSmall = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Monodetermination");
        
        // Включаем таймер для анимации (если понадобится)
        SetTimer(h, 1, 16, nullptr);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);

        // Заливаем белым
        HBRUSH bg = CreateSolidBrush(BG_COLOR);
        FillRect(dc, &rc, bg);
        DeleteObject(bg);

        int cx = rc.right / 2;
        int cy = rc.bottom / 2;

        if (g_state == STATE_DISCLAIMER) {
            // --- ЭКРАН 1: ДИСКЛЕЙМЕР ---
            // Сделал RECT чуть больше по высоте, чтобы текст точно влез
            RECT textRc = { 50, cy - 150, rc.right - 50, cy + 50 };
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, TEXT_COLOR);
            HFONT oldFont = (HFONT)SelectObject(dc, g_fontMain);
            
            // Текст предупреждения (используем \n для переноса)
            DrawTextW(dc, L"ВНИМАНИЕ\n\nЭта игра содержит элементы психологического хоррора,\nрезкие звуки и пугающие изображения.\n\nПожалуйста, будьте осторожны.", -1, &textRc, DT_CENTER | DT_TOP | DT_WORDBREAK);
            SelectObject(dc, oldFont);

            // Кнопка "Понятно"
            RECT btn = { cx - 100, rc.bottom - 100, cx + 100, rc.bottom - 50 };
            DrawTextCentered(dc, L"[ Я ПОНИМАЮ ]", btn, (g_hoveredItem == 3) ? HOVER_COLOR : TEXT_COLOR, g_fontMain);

        } else if (g_state == STATE_MAIN_MENU) {
            // --- ЭКРАН 2: ГЛАВНОЕ МЕНЮ ---
            
            // Черный квадрат сверху (Персонаж)
            int sqSize = 80;
            RECT square = { cx - sqSize/2, 80, cx + sqSize/2, 80 + sqSize };
            HBRUSH sqBrush = CreateSolidBrush(SQUARE_COLOR);
            FillRect(dc, &square, sqBrush);
            DeleteObject(sqBrush);

            // Кнопки меню
            RECT btnPlay = { cx - 100, cy, cx + 100, cy + 40 };
            RECT btnOpt  = { cx - 100, cy + 60, cx + 100, cy + 100 };
            RECT btnExit = { cx - 100, cy + 120, cx + 100, cy + 160 };

            DrawTextCentered(dc, L"PLAY", btnPlay, (g_hoveredItem == 0) ? HOVER_COLOR : TEXT_COLOR, g_fontMain);
            DrawTextCentered(dc, L"OPTIONS", btnOpt, (g_hoveredItem == 1) ? HOVER_COLOR : TEXT_COLOR, g_fontMain);
            DrawTextCentered(dc, L"EXIT", btnExit, (g_hoveredItem == 2) ? HOVER_COLOR : TEXT_COLOR, g_fontMain);

            // Версия в правом нижнем углу (тихим шрифтом)
            RECT verRc = { rc.right - 150, rc.bottom - 40, rc.right - 10, rc.bottom - 10 };
            DrawTextRight(dc, L"Version 1.0", verRc, RGB(150, 150, 150), g_fontSmall);

        } else if (g_state == STATE_OPTIONS) {
            // --- ЭКРАН 3: НАСТРОЙКИ ---
            RECT titleRc = { 0, 50, rc.right, 100 };
            DrawTextCentered(dc, L"OPTIONS", titleRc, TEXT_COLOR, g_fontMain);

            RECT btnMusic = { cx - 100, cy - 40, cx + 100, cy };
            RECT btnSound = { cx - 100, cy + 20, cx + 100, cy + 60 };
            RECT btnBack  = { cx - 100, cy + 100, cx + 100, cy + 140 };

            std::wstring musicText = L"MUSIC: " + std::wstring(g_musicOn ? L"ON" : L"OFF");
            std::wstring soundText = L"SOUND: " + std::wstring(g_soundOn ? L"ON" : L"OFF");

            DrawTextCentered(dc, musicText.c_str(), btnMusic, (g_hoveredItem == 4) ? HOVER_COLOR : TEXT_COLOR, g_fontMain);
            DrawTextCentered(dc, soundText.c_str(), btnSound, (g_hoveredItem == 5) ? HOVER_COLOR : TEXT_COLOR, g_fontMain);
            DrawTextCentered(dc, L"BACK", btnBack, (g_hoveredItem == 6) ? HOVER_COLOR : TEXT_COLOR, g_fontMain);
        }

        EndPaint(h, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = LOWORD(l), y = HIWORD(l);
        RECT rc; GetClientRect(h, &rc);
        int cx = rc.right / 2;
        int cy = rc.bottom / 2;
        int oldHover = g_hoveredItem;
        g_hoveredItem = -1; // Сбрасываем

        auto ins = [](RECT r, int px, int py) { return px >= r.left && px <= r.right && py >= r.top && py <= r.bottom; };

        if (g_state == STATE_DISCLAIMER) {
            RECT btn = { cx - 100, rc.bottom - 100, cx + 100, rc.bottom - 50 };
            if (ins(btn, x, y)) g_hoveredItem = 3;
        } else if (g_state == STATE_MAIN_MENU) {
            RECT btnPlay = { cx - 100, cy, cx + 100, cy + 40 };
            RECT btnOpt  = { cx - 100, cy + 60, cx + 100, cy + 100 };
            RECT btnExit = { cx - 100, cy + 120, cx + 100, cy + 160 };
            if (ins(btnPlay, x, y)) g_hoveredItem = 0;
            else if (ins(btnOpt, x, y)) g_hoveredItem = 1;
            else if (ins(btnExit, x, y)) g_hoveredItem = 2;
        } else if (g_state == STATE_OPTIONS) {
            RECT btnMusic = { cx - 100, cy - 40, cx + 100, cy };
            RECT btnSound = { cx - 100, cy + 20, cx + 100, cy + 60 };
            RECT btnBack  = { cx - 100, cy + 100, cx + 100, cy + 140 };
            if (ins(btnMusic, x, y)) g_hoveredItem = 4;
            else if (ins(btnSound, x, y)) g_hoveredItem = 5;
            else if (ins(btnBack, x, y)) g_hoveredItem = 6;
        }

        if (oldHover != g_hoveredItem) {
            InvalidateRect(h, nullptr, FALSE); // Перерисовываем при смене наведения
        }

        if (g_hoveredItem != -1) SetCursor(LoadCursor(nullptr, IDC_HAND));
        else SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(l), y = HIWORD(l);
        RECT rc; GetClientRect(h, &rc);
        int cx = rc.right / 2;
        int cy = rc.bottom / 2;

        auto ins = [](RECT r, int px, int py) { return px >= r.left && px <= r.right && py >= r.top && py <= r.bottom; };

        if (g_state == STATE_DISCLAIMER) {
            RECT btn = { cx - 100, rc.bottom - 100, cx + 100, rc.bottom - 50 };
            if (ins(btn, x, y)) { g_state = STATE_MAIN_MENU; InvalidateRect(h, nullptr, TRUE); }
        } 
        else if (g_state == STATE_MAIN_MENU) {
            RECT btnPlay = { cx - 100, cy, cx + 100, cy + 40 };
            RECT btnOpt  = { cx - 100, cy + 60, cx + 100, cy + 100 };
            RECT btnExit = { cx - 100, cy + 120, cx + 100, cy + 160 };
            
            if (ins(btnPlay, x, y)) { 
                MessageBoxW(h, L"Тут начнется игра...", L"Square", MB_OK); 
            }
            else if (ins(btnOpt, x, y)) { g_state = STATE_OPTIONS; InvalidateRect(h, nullptr, TRUE); }
            else if (ins(btnExit, x, y)) { PostQuitMessage(0); }
        }
        else if (g_state == STATE_OPTIONS) {
            RECT btnMusic = { cx - 100, cy - 40, cx + 100, cy };
            RECT btnSound = { cx - 100, cy + 20, cx + 100, cy + 60 };
            RECT btnBack  = { cx - 100, cy + 100, cx + 100, cy + 140 };

            if (ins(btnMusic, x, y)) { g_musicOn = !g_musicOn; InvalidateRect(h, nullptr, TRUE); }
            else if (ins(btnSound, x, y)) { g_soundOn = !g_soundOn; InvalidateRect(h, nullptr, TRUE); }
            else if (ins(btnBack, x, y)) { g_state = STATE_MAIN_MENU; InvalidateRect(h, nullptr, TRUE); }
        }
        return 0;
    }

    case WM_DESTROY:
        RemoveFontResourceExW(L"Monodetermination.ttf", FR_PRIVATE, 0);
        DeleteObject(g_fontMain);
        DeleteObject(g_fontSmall);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ============================================================
// ТОЧКА ВХОДА
// ============================================================
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    g_hInst = hInst;

    WNDCLASSW wc = {};
    wc.hInstance = hInst;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = L"SquareGameWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH); // Белый фон окна
    RegisterClassW(&wc);

    // Создаем окно (800x600, фиксированного размера для пиксель-арта)
    DWORD style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
    int w = 800, h = 600;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    
    g_hWnd = CreateWindowExW(0, L"SquareGameWnd", L"Square Black and Green", style, 
        (sw-w)/2, (sh-h)/2, w, h, nullptr, nullptr, hInst, nullptr);
    
    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
