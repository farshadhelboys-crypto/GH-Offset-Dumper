// GH Offset Dumper - Simple GUI + CLI support
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>

#include <GHDumper.h>
#include <GHFileHelp.h>
#include "resource.h"

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// Control handles
static HWND g_hEditConfig = nullptr;
static HWND g_hEditLog = nullptr;
static HWND g_hBtnDump = nullptr;
static HWND g_hMainWnd = nullptr;

// Append text to the log edit control
void AppendLog(const std::wstring& text)
{
    if (!g_hEditLog) return;

    int len = GetWindowTextLengthW(g_hEditLog);
    SendMessageW(g_hEditLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(g_hEditLog, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    SendMessageW(g_hEditLog, EM_SCROLLCARET, 0, 0);
}

void AppendLogA(const std::string& text)
{
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (size <= 0) return;
    std::wstring wtext(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wtext[0], size);
    // remove null terminator if present
    if (!wtext.empty() && wtext.back() == L'\0') wtext.pop_back();
    AppendLog(wtext);
}

// Open file dialog for config.json
std::wstring BrowseForConfig()
{
    wchar_t filename[MAX_PATH] = { 0 };

    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMainWnd;
    ofn.lpstrFilter = L"JSON Config (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrTitle = L"Select config.json";

    if (GetOpenFileNameW(&ofn))
        return filename;
    return L"";
}

// Run the dumper using the library
void RunDump()
{
    wchar_t configPathW[MAX_PATH] = { 0 };
    GetWindowTextW(g_hEditConfig, configPathW, MAX_PATH);

    if (wcslen(configPathW) == 0)
    {
        MessageBoxW(g_hMainWnd, L"Please select a config.json file first.", L"GH Offset Dumper", MB_OK | MB_ICONWARNING);
        return;
    }

    // Convert to narrow string (UTF-8)
    int size = WideCharToMultiByte(CP_UTF8, 0, configPathW, -1, nullptr, 0, nullptr, nullptr);
    std::string configPath(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, configPathW, -1, &configPath[0], size, nullptr, nullptr);
    if (!configPath.empty() && configPath.back() == '\0') configPath.pop_back();

    EnableWindow(g_hBtnDump, FALSE);
    AppendLog(L"\r\n========================================\r\n");
    AppendLog(L"Starting dump...\r\n");
    AppendLog(L"Config: " + std::wstring(configPathW) + L"\r\n");
    AppendLog(L"========================================\r\n\r\n");

    // Redirect stdout/stderr to a temporary file so we can show the logs in GUI
    char tempPath[MAX_PATH] = { 0 };
    char tempFile[MAX_PATH] = { 0 };
    GetTempPathA(MAX_PATH, tempPath);
    GetTempFileNameA(tempPath, "ghd", 0, tempFile);

    FILE* oldStdout = stdout;
    FILE* oldStderr = stderr;
    FILE* f = freopen(tempFile, "w", stdout);
    freopen(tempFile, "a", stderr);

    // Build fake argv for ParseCommandLine
    // argv[0] = program name, argv[1] = path to config.json
    std::string exeName = "GH-Offset-Dumper.exe";
    const char* argv[] = { exeName.c_str(), configPath.c_str() };

    bool success = false;
    try
    {
        success = gh::ParseCommandLine(2, argv);
    }
    catch (...)
    {
        success = false;
    }

    // Restore stdout
    fflush(stdout);
    fflush(stderr);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    // Read the captured log
    std::ifstream logFile(tempFile);
    if (logFile)
    {
        std::stringstream buffer;
        buffer << logFile.rdbuf();
        std::string logContent = buffer.str();
        AppendLogA(logContent);
    }
    logFile.close();
    DeleteFileA(tempFile);

    if (success)
    {
        AppendLog(L"\r\n[+] Dump completed successfully!\r\n");
        MessageBoxW(g_hMainWnd,
            L"Dump finished successfully!\n\nOutput files were saved next to the config / in the folder named after the executable.",
            L"GH Offset Dumper - Success",
            MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        AppendLog(L"\r\n[-] Dump failed. Check the log above.\r\n");
        MessageBoxW(g_hMainWnd,
            L"Dump failed.\n\nPlease check the log for details.\nMake sure the game is running (if not using file-only mode) and the signatures are up to date.",
            L"GH Offset Dumper - Error",
            MB_OK | MB_ICONERROR);
    }

    EnableWindow(g_hBtnDump, TRUE);
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        // Font
        HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        HFONT hFontBold = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        // Config label
        HWND hLabel = CreateWindowW(L"STATIC", L"Config JSON:",
            WS_CHILD | WS_VISIBLE,
            16, 16, 100, 22, hwnd, (HMENU)IDC_STATIC_CONFIG, nullptr, nullptr);
        SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Config path edit
        g_hEditConfig = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            16, 40, 420, 26, hwnd, (HMENU)IDC_EDIT_CONFIG, nullptr, nullptr);
        SendMessageW(g_hEditConfig, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Browse button
        HWND hBrowse = CreateWindowW(L"BUTTON", L"Browse...",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            446, 38, 90, 30, hwnd, (HMENU)IDC_BTN_BROWSE, nullptr, nullptr);
        SendMessageW(hBrowse, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Dump button
        g_hBtnDump = CreateWindowW(L"BUTTON", L"Start Dump",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            16, 80, 150, 36, hwnd, (HMENU)IDC_BTN_DUMP, nullptr, nullptr);
        SendMessageW(g_hBtnDump, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        // Clear log button
        HWND hClear = CreateWindowW(L"BUTTON", L"Clear Log",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            176, 80, 100, 36, hwnd, (HMENU)IDC_BTN_CLEAR, nullptr, nullptr);
        SendMessageW(hClear, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Log label
        HWND hLogLabel = CreateWindowW(L"STATIC", L"Log:",
            WS_CHILD | WS_VISIBLE,
            16, 128, 60, 20, hwnd, (HMENU)IDC_STATIC_LOG, nullptr, nullptr);
        SendMessageW(hLogLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Log edit (multiline, readonly, scroll)
        g_hEditLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
            16, 150, 520, 280, hwnd, (HMENU)IDC_EDIT_LOG, nullptr, nullptr);
        SendMessageW(g_hEditLog, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Welcome message
        AppendLog(L"GH Offset Dumper - Simple GUI\r\n");
        AppendLog(L"1. Click Browse and select your config.json\r\n");
        AppendLog(L"2. Make sure the game is running (unless file-only mode)\r\n");
        AppendLog(L"3. Click Start Dump\r\n\r\n");
        AppendLog(L"You can also drag & drop a .json file onto this window.\r\n");

        // Accept drag & drop
        DragAcceptFiles(hwnd, TRUE);

        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_BTN_BROWSE:
        {
            std::wstring path = BrowseForConfig();
            if (!path.empty())
                SetWindowTextW(g_hEditConfig, path.c_str());
            break;
        }
        case IDC_BTN_DUMP:
            RunDump();
            break;
        case IDC_BTN_CLEAR:
            SetWindowTextW(g_hEditLog, L"");
            break;
        }
        break;
    }

    case WM_DROPFILES:
    {
        HDROP hDrop = (HDROP)wParam;
        wchar_t path[MAX_PATH] = { 0 };
        if (DragQueryFileW(hDrop, 0, path, MAX_PATH))
        {
            // Check if it ends with .json
            std::wstring p(path);
            if (p.size() > 5)
            {
                std::wstring ext = p.substr(p.size() - 5);
                for (auto& c : ext) c = towlower(c);
                if (ext == L".json")
                {
                    SetWindowTextW(g_hEditConfig, path);
                    AppendLog(L"\r\nLoaded config via drag & drop: " + p + L"\r\n");
                }
                else
                {
                    MessageBoxW(hwnd, L"Please drop a .json config file.", L"GH Offset Dumper", MB_OK | MB_ICONWARNING);
                }
            }
        }
        DragFinish(hDrop);
        break;
    }

    case WM_SIZE:
    {
        // Optional: simple resize of log area
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (g_hEditLog && width > 50 && height > 200)
        {
            MoveWindow(g_hEditLog, 16, 150, width - 32, height - 170, TRUE);
        }
        break;
    }

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 560;
        mmi->ptMinTrackSize.y = 480;
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // If launched with a .json argument (drag onto exe from explorer), run CLI style then exit
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc >= 2)
    {
        // Convert args to narrow and call original logic
        std::vector<std::string> args;
        std::vector<const char*> argv;
        for (int i = 0; i < argc; ++i)
        {
            int size = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, nullptr, 0, nullptr, nullptr);
            std::string s(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, &s[0], size, nullptr, nullptr);
            if (!s.empty() && s.back() == '\0') s.pop_back();
            args.push_back(s);
        }
        for (auto& s : args) argv.push_back(s.c_str());

        // Show a console for CLI mode
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);

        bool ok = gh::ParseCommandLine(argc, argv.data());
        LocalFree(argvW);
        return ok ? 0 : 1;
    }
    LocalFree(argvW);

    // Register window class
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"GHOffsetDumperGUI";
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON1));
    wc.hIconSm = wc.hIcon;

    if (!RegisterClassExW(&wc))
    {
        MessageBoxW(nullptr, L"Failed to register window class.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    g_hMainWnd = CreateWindowExW(
        0,
        L"GHOffsetDumperGUI",
        L"GH Offset Dumper",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 580, 520,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_hMainWnd)
    {
        MessageBoxW(nullptr, L"Failed to create window.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

// Keep a classic main for compatibility if someone links differently
int main(int argc, const char** argv)
{
    if (argc >= 2)
    {
        return gh::ParseCommandLine(argc, argv) ? 0 : 1;
    }
    // No args → start GUI
    return wWinMain(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), SW_SHOW);
}
