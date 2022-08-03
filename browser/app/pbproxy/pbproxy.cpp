/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <processthreadsapi.h>
#include <winbase.h>
#include <winuser.h>
#include <shlwapi.h>
#include <string>

// Max command line length, per CreateProcessW docs
#define MAX_CMD_LENGTH 32767
#define EXTRA_ERR_MSG_LENGTH 39
#define ERR_GET_OUR_PATH L"844fa30e-0860-11ed-898b-373276936058"
#define ERR_GET_APP_DIR L"811237de-0904-11ed-8745-c7c269742323"
#define ERR_GET_APP_EXE L"8964fd30-0860-11ed-8374-576505ba4488"
#define ERR_LAUNCHING_APP L"89d2ca2c-0860-11ed-883c-bf345b8391bc"

void raiseError(DWORD err, std::wstring uuid) {
  LPWSTR winerr;
  if (err && ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                  FORMAT_MESSAGE_FROM_SYSTEM |
                                  FORMAT_MESSAGE_IGNORE_INSERTS,
                              nullptr, err, 0, (LPWSTR)&winerr, 0, nullptr)) {
    std::wstring errmsg(winerr);
    errmsg += L"\n\n" + uuid;
    ::MessageBoxW(nullptr, errmsg.c_str(), MOZ_APP_NAME " private_browsing.exe",
                  MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
  } else {
    ::MessageBoxW(nullptr, uuid.c_str(), MOZ_APP_NAME " private_browsing.exe",
                  MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
  }
}

/*
 * A very simple wrapper that always launches Firefox in Private Browsing
 * mode. Any arguments given to this program will be forwarded to Firefox,
 * as well the information provided by GetStartupInfoW() (the latter is mainly
 * done to ensure that Firefox's `launch_method` Telemetry works, which
 * depends on shortcut information).
 *
 * Any errors that happen during this process will pop up a MessageBox
 * with a Windows error (if present) and a unique UUID for debugability --
 * but these are very unlikely to be seen in practice.
 */
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR aCmdLine, int) {
  wchar_t app[MAX_PATH];
  DWORD ret = GetModuleFileNameW(nullptr, app, MAX_PATH);
  if (!ret ||
      (ret == MAX_PATH && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
    ret = ::GetLastError();
    raiseError(ret, ERR_GET_OUR_PATH);
    return ret;
  }
  if (!PathRemoveFileSpecW(app)) {
    raiseError(0, ERR_GET_APP_DIR);
    return 1;
  }
  if (!PathAppendW(app, MOZ_APP_NAME L".exe")) {
    raiseError(0, ERR_GET_APP_EXE);
    return 1;
  }

  std::wstring cmdLine(L"\"");
  cmdLine += app;
  cmdLine += L"\" -private-window";
  if (wcslen(aCmdLine) > 0) {
    cmdLine += L" ";
    cmdLine += aCmdLine;
  }
  DWORD creationFlags = CREATE_UNICODE_ENVIRONMENT;
  // Mainly used to pass along shortcut information to ensure
  // launch_method Telemetry will be accurate.
  STARTUPINFOW startupInfo = {0};
  startupInfo.cb = sizeof(STARTUPINFOW);
  GetStartupInfoW(&startupInfo);
  PROCESS_INFORMATION pi;

  bool rv =
      ::CreateProcessW(app, cmdLine.data(), nullptr, nullptr, FALSE,
                       creationFlags, nullptr, nullptr, &startupInfo, &pi);

  if (!rv) {
    ret = ::GetLastError();
    raiseError(ret, ERR_LAUNCHING_APP);
    return ret;
  }

  ::CloseHandle(pi.hProcess);
  ::CloseHandle(pi.hThread);

  return 0;
}
