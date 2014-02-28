/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CEHHelper.h"
#include <tlhelp32.h>
#include "mozilla/widget/MetroD3DCheckHelper.h"

#ifdef SHOW_CONSOLE
#include <io.h> // _open_osfhandle
#endif

HANDLE sCon;
LPCWSTR metroDX10Available = L"MetroD3DAvailable";
LPCWSTR metroLastAHE = L"MetroLastAHE";
LPCWSTR cehDumpDebugStrings = L"CEHDump";
extern const WCHAR* kFirefoxExe;

void
Log(const wchar_t *fmt, ...)
{
#if !defined(SHOW_CONSOLE)
  DWORD dwRes = 0;
  if (!GetDWORDRegKey(cehDumpDebugStrings, dwRes) || !dwRes) {
    return;
  }
#endif
  va_list a = nullptr;
  wchar_t szDebugString[1024];
  if(!lstrlenW(fmt))
    return;
  va_start(a,fmt);
  vswprintf(szDebugString, 1024, fmt, a);
  va_end(a);
  if(!lstrlenW(szDebugString))
    return;

  DWORD len;
  WriteConsoleW(sCon, szDebugString, lstrlenW(szDebugString), &len, nullptr);
  WriteConsoleW(sCon, L"\n", 1, &len, nullptr);

  OutputDebugStringW(szDebugString);
  OutputDebugStringW(L"\n");
}

#if defined(SHOW_CONSOLE)
void
SetupConsole()
{
  FILE *fp;
  AllocConsole();
  sCon = GetStdHandle(STD_OUTPUT_HANDLE); 
  int fd = _open_osfhandle(reinterpret_cast<intptr_t>(sCon), 0);
  fp = _fdopen(fd, "w");
  *stdout = *fp;
  setvbuf(stdout, nullptr, _IONBF, 0);
}
#endif

bool
IsImmersiveProcessDynamic(HANDLE process)
{
  HMODULE user32DLL = LoadLibraryW(L"user32.dll");
  if (!user32DLL) {
    return false;
  }

  decltype(IsImmersiveProcess)* IsImmersiveProcessPtr =
    (decltype(IsImmersiveProcess)*) GetProcAddress(user32DLL,
                                                   "IsImmersiveProcess");
  if (!IsImmersiveProcessPtr) {
    FreeLibrary(user32DLL);
    return false;
  }

  BOOL bImmersiveProcess = IsImmersiveProcessPtr(process);
  FreeLibrary(user32DLL);
  return bImmersiveProcess;
}

bool
IsProcessRunning(const wchar_t *processName, bool bCheckIfMetro)
{
  bool exists = false;
  PROCESSENTRY32W entry;
  entry.dwSize = sizeof(PROCESSENTRY32W);

  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

  if (Process32First(snapshot, &entry)) {
    while (!exists && Process32Next(snapshot, &entry)) {
      if (!_wcsicmp(entry.szExeFile, processName)) {
        HANDLE process = OpenProcess(GENERIC_READ, FALSE, entry.th32ProcessID);
        bool isImmersiveProcess = IsImmersiveProcessDynamic(process);
        if ((bCheckIfMetro && isImmersiveProcess) ||
            (!bCheckIfMetro && !isImmersiveProcess)) {
          exists = true;
        }
        CloseHandle(process);
      }
    }
  }

  CloseHandle(snapshot);
  return exists;
}

bool
IsMetroProcessRunning()
{
  return IsProcessRunning(kFirefoxExe, true);
}

bool
IsDesktopProcessRunning()
{
  return IsProcessRunning(kFirefoxExe, false);
}

/*
 * Retrieve the last front end ui we launched so we can target it
 * again. This value is updated down in nsAppRunner when the browser
 * starts up.
 */
AHE_TYPE
GetLastAHE()
{
  DWORD ahe;
  if (GetDWORDRegKey(metroLastAHE, ahe)) {
    return (AHE_TYPE) ahe;
  }
  return AHE_DESKTOP;
}

bool
IsDX10Available()
{
  DWORD isDX10Available;
  if (GetDWORDRegKey(metroDX10Available, isDX10Available)) {
    return isDX10Available;
  }
  bool check = D3DFeatureLevelCheck();
  SetDWORDRegKey(metroDX10Available, check);
  return check;
}

bool
GetDWORDRegKey(LPCWSTR name, DWORD &value)
{
  HKEY key;
  LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Mozilla\\Firefox",
                              0, KEY_READ, &key);
  if (result != ERROR_SUCCESS) {
    return false;
  }

  DWORD bufferSize = sizeof(DWORD);
  DWORD type;
  result = RegQueryValueExW(key, name, nullptr, &type,
                            reinterpret_cast<LPBYTE>(&value),
                            &bufferSize);
  RegCloseKey(key);
  return result == ERROR_SUCCESS;
}

bool
SetDWORDRegKey(LPCWSTR name, DWORD value)
{
  HKEY key;
  LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Mozilla\\Firefox",
                              0, KEY_WRITE, &key);
  if (result != ERROR_SUCCESS) {
    return false;
  }

  result = RegSetValueEx(key, name, 0, REG_DWORD,
                         reinterpret_cast<LPBYTE>(&value),
                         sizeof(DWORD));
  RegCloseKey(key);
  return result == ERROR_SUCCESS;
}
