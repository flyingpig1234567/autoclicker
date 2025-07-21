#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <gdiplus.h>
#include <thread>
#include <atomic>
#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <functional>
#include <CommCtrl.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")

using namespace Gdiplus;

// 热键ID定义
#define HOTKEY_ACTIVATE   1
#define HOTKEY_DEACTIVATE 2
#define HOTKEY_CONFIG     3
#define HOTKEY_SAVE       4

// 配置结构
struct Config {
    int leftClickCount = 3;
    int rightClickCount = 3;
    int delayMs = 50;
    int activationKey = VK_F6;
    int deactivationKey = VK_F7;
    int configModeKey = VK_F5;
    int saveConfigKey = VK_F8;
};

// 全局变量
Config g_config;
std::atomic<bool> g_running(false);
std::atomic<bool> g_exit(false);
HWND g_hWnd = nullptr;
HWND g_hEditLeft = nullptr, g_hEditRight = nullptr, g_hEditDelay = nullptr;
HWND g_hBtnSave = nullptr;
ULONG_PTR g_gdiToken;
bool g_configMode = false;
bool g_unsavedChanges = false;

// 函数前置声明
void SaveConfig();
void InitCommonControlsEx();
bool LoadConfig();
void CreateConfigControls(HWND hWnd);
void DestroyConfigControls();
void UpdateConfigFromUI();
void SimulateClick(bool left);
void RegisterAppHotKeys();
void UnregisterAppHotKeys();
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void CreateMainWindow(HINSTANCE hInst);
void ClickerThread();

// 保存配置
void SaveConfig() {
    std::ofstream file("config.ini");
    if (!file) {
        MessageBoxA(nullptr, "无法保存配置文件", "错误", MB_ICONERROR);
        return;
    }

    file << "; 鼠标连点器配置文件\n";
    file << "; 格式: 键=值\n\n";
    file << "leftClickCount=" << g_config.leftClickCount << "\n";
    file << "rightClickCount=" << g_config.rightClickCount << "\n";
    file << "delayMs=" << g_config.delayMs << "\n";
    file << "activationKey=" << g_config.activationKey << "\n";
    file << "deactivationKey=" << g_config.deactivationKey << "\n";
    file << "configModeKey=" << g_config.configModeKey << "\n";
    file << "saveConfigKey=" << g_config.saveConfigKey << "\n";

    g_unsavedChanges = false;
}

// 初始化通用控件
void InitCommonControlsEx() {
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);
}

// 加载配置
bool LoadConfig() {
    std::ifstream file("config.ini");
    if (!file) {
        g_config = Config();
        SaveConfig();
        return false;
    }

    std::map<std::string, std::function<void(int)>> setters = {
        {"leftClickCount", [](int v) { g_config.leftClickCount = std::max(1, v); }},
        {"rightClickCount", [](int v) { g_config.rightClickCount = std::max(1, v); }},
        {"delayMs", [](int v) { g_config.delayMs = std::max(10, v); }},
        {"activationKey", [](int v) { g_config.activationKey = v; }},
        {"deactivationKey", [](int v) { g_config.deactivationKey = v; }},
        {"configModeKey", [](int v) { g_config.configModeKey = v; }},
        {"saveConfigKey", [](int v) { g_config.saveConfigKey = v; }}
    };

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == ';') continue;

        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            try {
                int value = std::stoi(line.substr(pos + 1));
                if (setters.count(key)) setters[key](value);
            }
            catch (...) {}
        }
    }
    return true;
}

// 注册全局热键
void RegisterAppHotKeys() {
    RegisterHotKey(g_hWnd, HOTKEY_ACTIVATE, 0, g_config.activationKey);
    RegisterHotKey(g_hWnd, HOTKEY_DEACTIVATE, 0, g_config.deactivationKey);
    RegisterHotKey(g_hWnd, HOTKEY_CONFIG, 0, g_config.configModeKey);
    RegisterHotKey(g_hWnd, HOTKEY_SAVE, 0, g_config.saveConfigKey);
}

// 注销全局热键
void UnregisterAppHotKeys() {
    UnregisterHotKey(g_hWnd, HOTKEY_ACTIVATE);
    UnregisterHotKey(g_hWnd, HOTKEY_DEACTIVATE);
    UnregisterHotKey(g_hWnd, HOTKEY_CONFIG);
    UnregisterHotKey(g_hWnd, HOTKEY_SAVE);
}

// 模拟点击
void SimulateClick(bool left) {
    INPUT inputs[2] = {};
    inputs[0].type = inputs[1].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = left ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
    inputs[1].mi.dwFlags = left ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;
    SendInput(2, inputs, sizeof(INPUT));
}

// 连点线程
void ClickerThread() {
    while (!g_exit) {
        if (g_running && !g_configMode) {
            bool left = (GetAsyncKeyState(VK_LBUTTON) & 0x8000);
            bool right = (GetAsyncKeyState(VK_RBUTTON) & 0x8000);

            if (left || right) {
                int count = left ? g_config.leftClickCount : g_config.rightClickCount;
                for (int i = 0; i < count && g_running; ++i) {
                    SimulateClick(left);
                    Sleep(g_config.delayMs);
                }
                while ((left ? GetAsyncKeyState(VK_LBUTTON) : GetAsyncKeyState(VK_RBUTTON)) & 0x8000) {
                    Sleep(10);
                }
            }
        }
        Sleep(1);
    }
}

// 创建配置控件
void CreateConfigControls(HWND hWnd) {
    g_hEditLeft = CreateWindowW(L"EDIT", std::to_wstring(g_config.leftClickCount).c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        150, 40, 60, 25, hWnd, nullptr, nullptr, nullptr);

    g_hEditRight = CreateWindowW(L"EDIT", std::to_wstring(g_config.rightClickCount).c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        150, 75, 60, 25, hWnd, nullptr, nullptr, nullptr);

    g_hEditDelay = CreateWindowW(L"EDIT", std::to_wstring(g_config.delayMs).c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        150, 110, 60, 25, hWnd, nullptr, nullptr, nullptr);

    g_hBtnSave = CreateWindowW(L"BUTTON", L"保存配置 (F8)",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        100, 150, 120, 30, hWnd, (HMENU)1, nullptr, nullptr);

    HFONT hFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");

    SendMessage(g_hEditLeft, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(g_hEditRight, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(g_hEditDelay, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(g_hBtnSave, WM_SETFONT, (WPARAM)hFont, TRUE);
}

// 销毁配置控件
void DestroyConfigControls() {
    if (g_hEditLeft) DestroyWindow(g_hEditLeft);
    if (g_hEditRight) DestroyWindow(g_hEditRight);
    if (g_hEditDelay) DestroyWindow(g_hEditDelay);
    if (g_hBtnSave) DestroyWindow(g_hBtnSave);
    g_hEditLeft = g_hEditRight = g_hEditDelay = g_hBtnSave = nullptr;
}

// 从UI更新配置
void UpdateConfigFromUI() {
    wchar_t buffer[32];

    GetWindowText(g_hEditLeft, buffer, 32);
    g_config.leftClickCount = std::max(1, _wtoi(buffer));

    GetWindowText(g_hEditRight, buffer, 32);
    g_config.rightClickCount = std::max(1, _wtoi(buffer));

    GetWindowText(g_hEditDelay, buffer, 32);
    g_config.delayMs = std::max(10, _wtoi(buffer));

    g_unsavedChanges = true;
}

// 窗口过程
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HFONT hFont = nullptr;

    switch (msg) {
    case WM_CREATE:
        hFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1 && HIWORD(wParam) == BN_CLICKED) {
            UpdateConfigFromUI();
            SaveConfig();
            g_configMode = false;
            DestroyConfigControls();
            InvalidateRect(hWnd, nullptr, TRUE);
        }
        return 0;

        // 处理全局热键
    case WM_HOTKEY:
        switch (wParam) {
        case HOTKEY_ACTIVATE:
            if (!g_configMode) {
                g_running = true;
                InvalidateRect(hWnd, nullptr, TRUE);
            }
            break;

        case HOTKEY_DEACTIVATE:
            if (!g_configMode) {
                g_running = false;
                InvalidateRect(hWnd, nullptr, TRUE);
            }
            break;

        case HOTKEY_CONFIG:
            g_configMode = !g_configMode;
            if (g_configMode) {
                CreateConfigControls(hWnd);
            }
            else {
                if (g_unsavedChanges) {
                    int result = MessageBoxW(hWnd,
                        L"有未保存的更改，是否保存？",
                        L"确认",
                        MB_YESNOCANCEL | MB_ICONQUESTION);

                    if (result == IDYES) {
                        UpdateConfigFromUI();
                        SaveConfig();
                    }
                    else if (result == IDCANCEL) {
                        g_configMode = true;
                        return 0;
                    }
                }
                DestroyConfigControls();
            }
            InvalidateRect(hWnd, nullptr, TRUE);
            break;

        case HOTKEY_SAVE:
            if (g_configMode) {
                UpdateConfigFromUI();
                SaveConfig();
                MessageBoxW(hWnd, L"配置已保存！", L"成功", MB_ICONINFORMATION);
            }
            break;
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            if (g_configMode) {
                int result = MessageBoxW(hWnd,
                    L"放弃未保存的更改并退出配置模式？",
                    L"确认",
                    MB_YESNO | MB_ICONQUESTION);

                if (result == IDYES) {
                    g_configMode = false;
                    g_unsavedChanges = false;
                    DestroyConfigControls();
                    InvalidateRect(hWnd, nullptr, TRUE);
                }
            }
            else {
                g_exit = true;
                DestroyWindow(hWnd);
            }
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        HDC hdcMem = CreateCompatibleDC(hdc);
        RECT rc;
        GetClientRect(hWnd, &rc);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        SelectObject(hdcMem, hBitmap);

        HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(hdcMem, &rc, hBrush);
        DeleteObject(hBrush);

        HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);
        SetTextColor(hdcMem, RGB(255, 255, 255));
        SetBkMode(hdcMem, TRANSPARENT);

        RECT textRect = { 10, 10, rc.right - 10, rc.bottom - 10 };
        if (g_configMode) {
            std::wstring title = L"配置模式 (ESC取消)";
            DrawTextW(hdcMem, title.c_str(), -1, &textRect, DT_LEFT);

            textRect.top += 30;
            DrawTextW(hdcMem, L"左键点击次数:", -1, &textRect, DT_LEFT);
            textRect.top += 35;
            DrawTextW(hdcMem, L"右键点击次数:", -1, &textRect, DT_LEFT);
            textRect.top += 35;
            DrawTextW(hdcMem, L"点击间隔(ms):", -1, &textRect, DT_LEFT);
        }
        else {
            std::wstring status = g_running ? L"状态: ▶ 运行中" : L"状态: ❚❚ 已停止";
            DrawTextW(hdcMem, status.c_str(), -1, &textRect, DT_LEFT);

            textRect.top += 30;
            std::wstring configText = L"左键连点: " + std::to_wstring(g_config.leftClickCount) + L" 次\n";
            configText += L"右键连点: " + std::to_wstring(g_config.rightClickCount) + L" 次\n";
            configText += L"点击间隔: " + std::to_wstring(g_config.delayMs) + L" ms\n\n";
            configText += L"快捷键:\n";
            configText += L"F6: 启动连点\n";
            configText += L"F7: 停止连点\n";
            configText += L"F5: 配置模式\n";
            configText += L"ESC: 退出程序";
            DrawTextW(hdcMem, configText.c_str(), -1, &textRect, DT_LEFT);
        }

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, hOldFont);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);
        return (hit == HTCLIENT && !g_configMode) ? HTCAPTION : hit;
    }

    case WM_DESTROY:
        UnregisterAppHotKeys(); // 注销热键
        if (hFont) DeleteObject(hFont);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

// 创建主窗口
void CreateMainWindow(HINSTANCE hInst) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"AutoClickerWindow";
    RegisterClassEx(&wc);

    g_hWnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        L"AutoClickerWindow",
        L"鼠标连点器",
        WS_POPUP | WS_VISIBLE,
        100, 100, 300, 250,
        nullptr, nullptr, hInst, nullptr);

    SetLayeredWindowAttributes(g_hWnd, 0, 220, LWA_ALPHA);
    ShowWindow(g_hWnd, SW_SHOW);

    // 注册全局热键
    RegisterAppHotKeys();
}

// 主函数
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    GdiplusStartupInput gdiInput;
    GdiplusStartup(&g_gdiToken, &gdiInput, nullptr);
    InitCommonControlsEx();

    LoadConfig();

    CreateMainWindow(hInst);

    std::thread clicker(ClickerThread);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_exit = true;
    clicker.join();
    GdiplusShutdown(g_gdiToken);
    return 0;
}