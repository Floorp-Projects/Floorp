/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file exists so that LaunchModernSettingsDialogDefaultApps can be called
 * without linking to libxul.
 */
#include "WindowsDefaultBrowser.h"

#include "city.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/WinHeaderOnlyUtils.h"

// Needed for access to IApplicationActivationManager
// This must be before any other includes that might include shlobj.h
#ifdef _WIN32_WINNT
#  undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600
#define INITGUID
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN8
#include <shlobj.h>

#include <lm.h>
#include <shlwapi.h>
#include <wchar.h>
#include <windows.h>

#define APP_REG_NAME_BASE L"Firefox-"

static bool IsWindowsLogonConnected() {
  WCHAR userName[UNLEN + 1];
  DWORD size = mozilla::ArrayLength(userName);
  if (!GetUserNameW(userName, &size)) {
    return false;
  }

  LPUSER_INFO_24 info;
  if (NetUserGetInfo(nullptr, userName, 24, (LPBYTE*)&info) != NERR_Success) {
    return false;
  }
  bool connected = info->usri24_internet_identity;
  NetApiBufferFree(info);

  return connected;
}

static bool SettingsAppBelievesConnected() {
  const wchar_t* keyPath = L"SOFTWARE\\Microsoft\\Windows\\Shell\\Associations";
  const wchar_t* valueName = L"IsConnectedAtLogon";

  uint32_t value = 0;
  DWORD size = sizeof(uint32_t);
  LSTATUS ls = RegGetValueW(HKEY_CURRENT_USER, keyPath, valueName, RRF_RT_ANY,
                            nullptr, &value, &size);
  if (ls != ERROR_SUCCESS) {
    return false;
  }

  return !!value;
}

// Generates the install directory without a trailing path separator.
bool GetInstallDirectory(mozilla::UniquePtr<wchar_t[]>& installPath) {
  installPath = mozilla::GetFullBinaryPath();
  // It's not safe to use PathRemoveFileSpecW with strings longer than MAX_PATH
  // (including null terminator).
  if (wcslen(installPath.get()) >= MAX_PATH) {
    return false;
  }
  PathRemoveFileSpecW(installPath.get());
  return true;
}

bool GetAppRegName(mozilla::UniquePtr<wchar_t[]>& aAppRegName) {
  mozilla::UniquePtr<wchar_t[]> appDirStr;
  bool success = GetInstallDirectory(appDirStr);
  if (!success) {
    return success;
  }

  uint64_t hash = CityHash64(reinterpret_cast<char*>(appDirStr.get()),
                             wcslen(appDirStr.get()) * sizeof(wchar_t));

  const wchar_t* format = L"%s%I64X";
  int bufferSize = _scwprintf(format, APP_REG_NAME_BASE, hash);
  ++bufferSize;  // Extra byte for terminating null
  aAppRegName = mozilla::MakeUnique<wchar_t[]>(bufferSize);

  _snwprintf_s(aAppRegName.get(), bufferSize, _TRUNCATE, format,
               APP_REG_NAME_BASE, hash);

  return success;
}

bool LaunchControlPanelDefaultPrograms() {
  // Build the path control.exe path safely
  WCHAR controlEXEPath[MAX_PATH + 1] = {'\0'};
  if (!GetSystemDirectoryW(controlEXEPath, MAX_PATH)) {
    return false;
  }
  LPCWSTR controlEXE = L"control.exe";
  if (wcslen(controlEXEPath) + wcslen(controlEXE) >= MAX_PATH) {
    return false;
  }
  if (!PathAppendW(controlEXEPath, controlEXE)) {
    return false;
  }

  const wchar_t* paramFormat =
      L"control.exe /name Microsoft.DefaultPrograms "
      L"/page pageDefaultProgram\\pageAdvancedSettings?pszAppName=%s";
  mozilla::UniquePtr<wchar_t[]> appRegName;
  GetAppRegName(appRegName);
  int bufferSize = _scwprintf(paramFormat, appRegName.get());
  ++bufferSize;  // Extra byte for terminating null
  mozilla::UniquePtr<wchar_t[]> params =
      mozilla::MakeUnique<wchar_t[]>(bufferSize);
  _snwprintf_s(params.get(), bufferSize, _TRUNCATE, paramFormat,
               appRegName.get());

  STARTUPINFOW si = {sizeof(si), 0};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_SHOWDEFAULT;
  PROCESS_INFORMATION pi = {0};
  if (!CreateProcessW(controlEXEPath, params.get(), nullptr, nullptr, FALSE, 0,
                      nullptr, nullptr, &si, &pi)) {
    return false;
  }
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return true;
}

bool LaunchModernSettingsDialogDefaultApps() {
  if (!mozilla::IsWindowsBuildOrLater(14965) && !IsWindowsLogonConnected() &&
      SettingsAppBelievesConnected()) {
    // Use the classic Control Panel to work around a bug of older
    // builds of Windows 10.
    return LaunchControlPanelDefaultPrograms();
  }

  IApplicationActivationManager* pActivator;
  HRESULT hr = CoCreateInstance(
      CLSID_ApplicationActivationManager, nullptr, CLSCTX_INPROC,
      IID_IApplicationActivationManager, (void**)&pActivator);

  if (SUCCEEDED(hr)) {
    DWORD pid;
    hr = pActivator->ActivateApplication(
        L"windows.immersivecontrolpanel_cw5n1h2txyewy"
        L"!microsoft.windows.immersivecontrolpanel",
        L"page=SettingsPageAppsDefaults", AO_NONE, &pid);
    if (SUCCEEDED(hr)) {
      // Do not check error because we could at least open
      // the "Default apps" setting.
      pActivator->ActivateApplication(
          L"windows.immersivecontrolpanel_cw5n1h2txyewy"
          L"!microsoft.windows.immersivecontrolpanel",
          L"page=SettingsPageAppsDefaults"
          L"&target=SystemSettings_DefaultApps_Browser",
          AO_NONE, &pid);
    }
    pActivator->Release();
    return SUCCEEDED(hr);
  }
  return true;
}
