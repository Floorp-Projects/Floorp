/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CEHHelper.h"
#include <tlhelp32.h>

#ifdef SHOW_CONSOLE
#include <io.h> // _open_osfhandle
#endif

HANDLE sCon;
LPCWSTR metroDX10Available = L"MetroD3DAvailable";

typedef HRESULT (WINAPI*D3D10CreateDevice1Func)
  (IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT,
   D3D10_FEATURE_LEVEL1, UINT, ID3D10Device1 **);
typedef HRESULT(WINAPI*CreateDXGIFactory1Func)(REFIID , void **);

void
Log(const wchar_t *fmt, ...)
{
#if !defined(SHOW_CONSOLE)
  return;
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

  if (IsDebuggerPresent()) {  
    OutputDebugStringW(szDebugString);
    OutputDebugStringW(L"\n");
  }
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

  typedef BOOL (WINAPI* IsImmersiveProcessFunc)(HANDLE process);
  IsImmersiveProcessFunc IsImmersiveProcessPtr =
    (IsImmersiveProcessFunc)GetProcAddress(user32DLL,
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
IsImmersiveProcessRunning(const wchar_t *processName)
{
  bool exists = false;
  PROCESSENTRY32W entry;
  entry.dwSize = sizeof(PROCESSENTRY32W);

  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

  if (Process32First(snapshot, &entry)) {
    while (!exists && Process32Next(snapshot, &entry)) {
      if (!_wcsicmp(entry.szExeFile, processName)) {
        HANDLE process = OpenProcess(GENERIC_READ, FALSE, entry.th32ProcessID);
        if (IsImmersiveProcessDynamic(process)) {
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
IsDX10Available()
{
  DWORD isDX10Available;
  if (GetDWORDRegKey(metroDX10Available, isDX10Available)) {
    return isDX10Available;
  }

  HMODULE dxgiModule = LoadLibraryA("dxgi.dll");
  if (!dxgiModule) {
    SetDWORDRegKey(metroDX10Available, 0);
    return false;
  }
  CreateDXGIFactory1Func createDXGIFactory1 =
    (CreateDXGIFactory1Func) GetProcAddress(dxgiModule, "CreateDXGIFactory1");
  if (!createDXGIFactory1) {
    SetDWORDRegKey(metroDX10Available, 0);
    return false;
  }

  HMODULE d3d10module = LoadLibraryA("d3d10_1.dll");
  if (!d3d10module) {
    SetDWORDRegKey(metroDX10Available, 0);
    return false;
  }
  D3D10CreateDevice1Func createD3DDevice =
    (D3D10CreateDevice1Func) GetProcAddress(d3d10module,
                                            "D3D10CreateDevice1");
  if (!createD3DDevice) {
    SetDWORDRegKey(metroDX10Available, 0);
    return false;
  }

  CComPtr<IDXGIFactory1> factory1;
  if (FAILED(createDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory1))) {
    SetDWORDRegKey(metroDX10Available, 0);
    return false;
  }

  CComPtr<IDXGIAdapter1> adapter1;
  if (FAILED(factory1->EnumAdapters1(0, &adapter1))) {
    SetDWORDRegKey(metroDX10Available, 0);
    return false;
  }

  CComPtr<ID3D10Device1> device;
  // Try for DX10.1
  if (FAILED(createD3DDevice(adapter1, D3D10_DRIVER_TYPE_HARDWARE, nullptr,
                             D3D10_CREATE_DEVICE_BGRA_SUPPORT |
                             D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
                             D3D10_FEATURE_LEVEL_10_1,
                             D3D10_1_SDK_VERSION, &device))) {
    // Try for DX10
    if (FAILED(createD3DDevice(adapter1, D3D10_DRIVER_TYPE_HARDWARE, nullptr,
                               D3D10_CREATE_DEVICE_BGRA_SUPPORT |
                               D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
                               D3D10_FEATURE_LEVEL_10_0,
                               D3D10_1_SDK_VERSION, &device))) {
      // Try for DX9.3 (we fall back to cairo and cairo has support for D3D 9.3)
      if (FAILED(createD3DDevice(adapter1, D3D10_DRIVER_TYPE_HARDWARE, nullptr,
                                 D3D10_CREATE_DEVICE_BGRA_SUPPORT |
                                 D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
                                 D3D10_FEATURE_LEVEL_9_3,
                                 D3D10_1_SDK_VERSION, &device))) {

        SetDWORDRegKey(metroDX10Available, 0);
        return false;
      }
    }
  }
  

  SetDWORDRegKey(metroDX10Available, 1);
  return true;
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
