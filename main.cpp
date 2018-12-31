#include "main.h"
#include <stdio.h>

int initialized = 0; // Only can call the Initialize method once
int isFocused = 1; // If game window is currently in focus
int delta = 0; // Number of mouse wheel scrolls since last checked (+ UP | - DOWN)
int dragged = 0; // If window is being dragged or resized; do not register mouse wheels if so
int alwaysShow = 0; // Always show cursor over the game

CURSORINFO cinfo;
HWND window_handle;

typedef void WinEventProc(int* hWinEventHook, UINT eventType, int* hwnd, UINT idObject, int idChild, UINT dwEventThread, UINT dwmsEventTime);

// Listen for mouse wheel messages
LRESULT CALLBACK GetMsgProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code >= 0)
	{
		MSG * msg = (MSG*)lParam;
		unsigned int msgid = msg->message;
		// Only accept the mouse wheel message if the game window is in focus.
		// PM_REMOVE check is necessary since PM_NOREMOVE is called afterwards.
		if (msgid == WM_MOUSEWHEEL && wParam == PM_REMOVE && isFocused != dragged) // this would be processing some stuff regarding WM_CLOSE
		{
            short delt = msg->wParam >> 16;
            delta += delt / 120;
		}
	}
	return CallNextHookEx(NULL, code, wParam, lParam);
}

// When player sets focus back onto the game window
void EventHookCallback(int* hWinEventHook, UINT eventType, int* hwnd, UINT idObject, int idChild, UINT dwEventThread, UINT dwmsEventTime)
{
    // If the foreground window is not the game window, then game is not in focus
    if ((HWND)hwnd != window_handle && eventType == 0x0003)
    {
        //SetFocus(window_handle); // Uncomment for game window to be active when not in focus
        isFocused = 0;
    }
    else if ((HWND)hwnd == window_handle && eventType == 0x000A)
    {
        dragged = 1;
    }
    else if ((HWND)hwnd == window_handle && eventType == 0x000B)
    {
        dragged = 0;
    }
}

// Checks if the window is in focus
DWORD WINAPI CheckGame(LPVOID lpParam)
{
    while (true)
    {
        // If window is gone, end thread
        if (IsWindow(window_handle) == 0)
            return 0;
        // If game window is front window, enable input
        if (window_handle == GetForegroundWindow())
        {
            isFocused = 1;
        }
        // Keep cursor visible
        if (alwaysShow > 0)
        {
            PCURSORINFO pic = &cinfo;
            pic->cbSize = sizeof(CURSORINFO);
            GetCursorInfo(pic);
            if (pic->flags == 0)
            {
                POINT p;
                GetCursorPos(&p);
                SetCursorPos(p.x, p.y);
            }
        }
    }
}

// Activate thread for watching when the game window gains/loses focus
void Start()
{
    typedef int (*EVENTHOOK)(UINT min, UINT max, HMODULE eventProc, WinEventProc wineventproc, DWORD idproc, DWORD idthread, UINT flags);
    // Get Module handle
    HMODULE module_handle = GetModuleHandle(NULL);
    // Set event hook (sends event when system foreground window changes)
    HMODULE hm = LoadLibrary(TEXT("User32.dll"));
    EVENTHOOK SetWinEventHook = (EVENTHOOK) GetProcAddress(hm, "SetWinEventHook");
    // 0x0003 = EVENT_SYSTEM_FOREGROUND
    // 0x000A = EVENT_SYSTEM_MOVESIZESTART
    // 0x000B = EVENT_SYSTEM_MOVESIZEEND
    SetWinEventHook(0x0003, 0x000B, module_handle, EventHookCallback, 0, 0, 0);
    // Create new thread to keep track of when game is in focus
    CreateThread(NULL, 0, CheckGame, NULL, 0, 0);
}

// DLL method that needs to be called first
int DLL_EXPORT Initialize(int asc)
{
	if (!initialized) // Call only once
	{
	    // Get Window handle
        char buffer[256];
        GetPrivateProfileStringA("Game", "Title", "", buffer, 256, ".\\Game.ini");
        window_handle = FindWindowA("RGSS Player", buffer);

		HINSTANCE hinstance = (HINSTANCE)GetWindowLongW(window_handle, GWL_HINSTANCE);
		DWORD threadId = GetCurrentThreadId();
		// Create the Hook to listen for mouse wheel messages
		if (hinstance != 0 && threadId != 0 && SetWindowsHookExW(WH_GETMESSAGE, GetMsgProc, hinstance, threadId) != 0)
		{
		    Start(); // Activate thread to check if game is in focus
			initialized = 1;
			alwaysShow = asc;
			return 1; // 1 for successful
		}
	}
	return 0; // Unsuccessful

}

// Returns the number of times the wheel scrolled. Positive for up, Negative for down
int DLL_EXPORT WheelDelta()
{
    int copydelta = delta;
    delta = 0;
    return copydelta;
}
