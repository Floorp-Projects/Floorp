/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#undef WINVER
#undef _WIN32_WINNT
#define WINVER 0x602
#define _WIN32_WINNT 0x602

#include <windows.h>
#include <objbase.h>
#include <combaseapi.h>
#include <atlcore.h>
#include <atlstr.h>
#include <wininet.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <propkey.h>
#include <propvarutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include <io.h>
#include <shellapi.h>

static const WCHAR* kFirefoxExe = L"firefox.exe";
static const WCHAR* kDefaultMetroBrowserIDPathKey = L"FirefoxURL";

// Logging pipe handle
HANDLE gTestOutputPipe = INVALID_HANDLE_VALUE;
// Logging pipe read buffer
#define PIPE_BUFFER_SIZE 4096
char buffer[PIPE_BUFFER_SIZE + 1];

CString sAppParams;
CString sFirefoxPath;

// The tests file we write out for firefox.exe which contains test
// startup command line paramters.
#define kMetroTestFile "tests.ini"

// Process exit codes for buildbotcustom logic. These are currently ignored, but
// at some point releng expects to use these.
#define SUCCESS   0
#define WARNINGS  1
#define FAILURE   2
#define EXCEPTION 3
#define RETRY     4

static void Log(const wchar_t *fmt, ...)
{
  va_list a = nullptr;
  wchar_t szDebugString[1024];
  if(!lstrlenW(fmt))
    return;
  va_start(a,fmt);
  vswprintf(szDebugString, 1024, fmt, a);
  va_end(a);
  if(!lstrlenW(szDebugString))
    return;

  wprintf(L"INFO | metrotestharness.exe | %s\n", szDebugString);
  fflush(stdout);
}

static void Fail(bool aRequestRetry, const wchar_t *fmt, ...)
{
  va_list a = nullptr;
  wchar_t szDebugString[1024];
  if(!lstrlenW(fmt))
    return;
  va_start(a,fmt);
  vswprintf(szDebugString, 1024, fmt, a);
  va_end(a);
  if(!lstrlenW(szDebugString))
    return;
  if (aRequestRetry) {
    wprintf(L"FAIL-SHOULD-RETRY | metrotestharness.exe | %s\n", szDebugString);
  } else {
    wprintf(L"TEST-UNEXPECTED-FAIL | metrotestharness.exe | %s\n", szDebugString);
  }
  fflush(stdout);
}

/*
 * Retrieve our module dir path.
 *
 * @aPathBuffer Buffer to fill
 */
static bool GetModulePath(CStringW& aPathBuffer)
{
  WCHAR buffer[MAX_PATH];
  memset(buffer, 0, sizeof(buffer));

  if (!GetModuleFileName(nullptr, buffer, MAX_PATH)) {
    Fail(false, L"GetModuleFileName failed.");
    return false;
  }

  WCHAR* slash = wcsrchr(buffer, '\\');
  if (!slash)
    return false;
  *slash = '\0';

  aPathBuffer = buffer;
  return true;
}

/*
 * Retrieve 'module dir path\firefox.exe'
 *
 * @aPathBuffer Buffer to fill
 */
static bool GetDesktopBrowserPath(CStringW& aPathBuffer)
{
  if (!GetModulePath(aPathBuffer))
    return false;

  // ceh.exe sits in dist/bin root with the desktop browser. Since this
  // is a firefox only component, this hardcoded filename is ok.
  aPathBuffer.Append(L"\\");
  aPathBuffer.Append(kFirefoxExe);
  return true;
}

/*
 * Retrieve the app model id of the firefox metro browser.
 *
 * @aPathBuffer Buffer to fill
 * @aCharLength Length of buffer to fill in characters
 */
static bool GetDefaultBrowserAppModelID(WCHAR* aIDBuffer,
                                        long aCharLength)
{
  if (!aIDBuffer || aCharLength <= 0)
    return false;

  memset(aIDBuffer, 0, (sizeof(WCHAR)*aCharLength));

  HKEY key;
  if (RegOpenKeyExW(HKEY_CLASSES_ROOT, kDefaultMetroBrowserIDPathKey,
                    0, KEY_READ, &key) != ERROR_SUCCESS) {
    return false;
  }
  DWORD len = aCharLength * sizeof(WCHAR);
  memset(aIDBuffer, 0, len);
  if (RegQueryValueExW(key, L"AppUserModelID", nullptr, nullptr,
                       (LPBYTE)aIDBuffer, &len) != ERROR_SUCCESS || !len) {
    RegCloseKey(key);
    return false;
  }
  RegCloseKey(key);
  return true;
}

// Tests.ini file cleanup helper
class DeleteTestFileHelper
{
  CStringA mTestFile;
public:
  DeleteTestFileHelper(CStringA& aTestFile) :
    mTestFile(aTestFile) {}
  ~DeleteTestFileHelper() {
    if (mTestFile.GetLength()) {
      Log(L"Deleting %s", CStringW(mTestFile));
      DeleteFileA(mTestFile);
    }
  }
};

static bool SetupTestOutputPipe()
{
  SECURITY_ATTRIBUTES saAttr;
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = nullptr;

  gTestOutputPipe =
    CreateNamedPipeW(L"\\\\.\\pipe\\metrotestharness",
                     PIPE_ACCESS_INBOUND,
                     PIPE_TYPE_BYTE|PIPE_WAIT,
                     1,
                     PIPE_BUFFER_SIZE,
                     PIPE_BUFFER_SIZE, 0, nullptr);

  if (gTestOutputPipe == INVALID_HANDLE_VALUE) {
    Log(L"Failed to create named logging pipe.");
    return false;
  }
  return true;
}

static void ReadPipe()
{
  DWORD numBytesRead;
  while (ReadFile(gTestOutputPipe, buffer, PIPE_BUFFER_SIZE,
                  &numBytesRead, nullptr) &&
         numBytesRead) {
    buffer[numBytesRead] = '\0';
    printf("%s", buffer);
  }
}

static int Launch()
{
  Log(L"Launching browser...");

  DWORD processID;

  // The interface that allows us to activate the browser
  CComPtr<IApplicationActivationManager> activateMgr;
  if (FAILED(CoCreateInstance(CLSID_ApplicationActivationManager, nullptr,
                              CLSCTX_LOCAL_SERVER,
                              IID_IApplicationActivationManager,
                              (void**)&activateMgr))) {
    Fail(false, L"CoCreateInstance CLSID_ApplicationActivationManager failed.");
    return FAILURE;
  }
  
  HRESULT hr;
  WCHAR appModelID[256];
  // Activation is based on the browser's registered app model id
  if (!GetDefaultBrowserAppModelID(appModelID, (sizeof(appModelID)/sizeof(WCHAR)))) {
    Fail(false, L"GetDefaultBrowserAppModelID failed.");
    return FAILURE;
  }
  Log(L"App model id='%s'", appModelID);

  // Hand off focus rights if the terminal has focus to the out-of-process
  // activation server (explorer.exe). Without this the metro interface
  // won't launch.
  hr = CoAllowSetForegroundWindow(activateMgr, nullptr);
  if (FAILED(hr)) {
    // Log but don't fail. This has happened on vms with certain terminals run by
    // QA during mozmill testing.
    Log(L"Windows focus rights hand off failed (HRESULT=0x%X). Ignoring.", hr);
  }

  Log(L"Harness process id: %d", GetCurrentProcessId());

  // If provided, validate the firefox path passed in.
  int binLen = wcslen(kFirefoxExe);
  if (sFirefoxPath.GetLength() && sFirefoxPath.Right(binLen) != kFirefoxExe) {
    Log(L"firefoxpath is missing a valid bin name! Assuming '%s'.", kFirefoxExe);
    if (sFirefoxPath.Right(1) != L"\\") {
      sFirefoxPath += L"\\";
    }
    sFirefoxPath += kFirefoxExe;
  }

  // Because we can't pass command line args, we store params in a
  // tests.ini file in dist/bin which the browser picks up on launch.
  CStringA testFilePath;
  if (sFirefoxPath.GetLength()) {
    // Use the firefoxpath passed to us by the test harness
    int index = sFirefoxPath.ReverseFind('\\');
    if (index == -1) {
      Fail(false, L"Bad firefoxpath path");
      return FAILURE;
    }
    testFilePath = sFirefoxPath.Mid(0, index);
    testFilePath += "\\";
    testFilePath += kMetroTestFile;
  } else {
    // Use the module path
    char path[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, path, MAX_PATH)) {
      Fail(false, L"GetModuleFileNameA errorno=%d", GetLastError());
      return FAILURE;
    }
    char* slash = strrchr(path, '\\');
    if (!slash)
      return FAILURE;
    *slash = '\0'; // no trailing slash
    testFilePath = path;
    testFilePath += "\\";
    sFirefoxPath = testFilePath;
    sFirefoxPath += kFirefoxExe;
    testFilePath += kMetroTestFile;
  }

  // Make sure the firefox bin exists
  if (GetFileAttributesW(sFirefoxPath) == INVALID_FILE_ATTRIBUTES) {
    Fail(false, L"Invalid bin path: '%s'", sFirefoxPath);
    return FAILURE;
  }

  Log(L"Using bin path: '%s'", sFirefoxPath);

  Log(L"Writing out tests.ini to: '%s'", CStringW(testFilePath));
  HANDLE hTestFile = CreateFileA(testFilePath, GENERIC_WRITE,
                                 0, nullptr, CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL,
                                 nullptr);
  if (hTestFile == INVALID_HANDLE_VALUE) {
    Fail(false, L"CreateFileA errorno=%d", GetLastError());
    return FAILURE;
  }

  DeleteTestFileHelper dtf(testFilePath);

  // nsAppRunner expects the first param to be the bin path, just like a
  // normal startup. So prepend our bin path to our param string we write.
  CStringA asciiParams = sFirefoxPath;
  asciiParams += " ";
  asciiParams += sAppParams;
  asciiParams.Trim();
  Log(L"Browser command line args: '%s'", CString(asciiParams));
  if (!WriteFile(hTestFile, asciiParams, asciiParams.GetLength(),
                 nullptr, 0)) {
    CloseHandle(hTestFile);
    Fail(false, L"WriteFile errorno=%d", GetLastError());
    return FAILURE;
  }
  FlushFileBuffers(hTestFile);
  CloseHandle(hTestFile);

  // Create a named stdout pipe for the browser
  if (!SetupTestOutputPipe()) {
    Fail(false, L"SetupTestOutputPipe failed (errno=%d)", GetLastError());
    return FAILURE;
  }

  // Launch firefox
  hr = activateMgr->ActivateApplication(appModelID, L"", AO_NOERRORUI, &processID);
  if (FAILED(hr)) {
    Fail(true, L"ActivateApplication result %X", hr);
    return RETRY;
  }

  Log(L"Activation succeeded. processid=%d", processID);

  HANDLE child = OpenProcess(SYNCHRONIZE, FALSE, processID);
  if (!child) {
    Fail(false, L"Couldn't find child process. (%d)", GetLastError());
    return FAILURE;
  }

  Log(L"Waiting on child process...");

  MSG msg;
  DWORD waitResult = WAIT_TIMEOUT;
  HANDLE handles[2] = { child, gTestOutputPipe };
  while ((waitResult = MsgWaitForMultipleObjects(2, handles, FALSE, INFINITE, QS_ALLINPUT)) != WAIT_OBJECT_0) {
    if (waitResult == WAIT_FAILED) {
      Log(L"Wait failed (errno=%d)", GetLastError());
      break;
    } else if (waitResult == WAIT_OBJECT_0 + 1) {
      ReadPipe();
    } else if (waitResult == WAIT_OBJECT_0 + 2 &&
               PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  ReadPipe();
  CloseHandle(gTestOutputPipe);
  CloseHandle(child);

  Log(L"Exiting.");
  return SUCCESS;
}

int wmain(int argc, WCHAR* argv[])
{
  CoInitialize(nullptr);

  int idx;
  bool firefoxParam = false;
  for (idx = 1; idx < argc; idx++) {
    CString param = argv[idx];
    param.Trim();

    // Pickup the firefox path param and store it, we'll need this
    // when we create the tests.ini file.
    if (param == "-firefoxpath") {
      firefoxParam = true;
      continue;
    } else if (firefoxParam) {
      firefoxParam = false;
      sFirefoxPath = param;
      continue;
    }

    sAppParams.Append(argv[idx]);
    sAppParams.Append(L" ");
  }
  sAppParams.Trim();
  int res = Launch();
  CoUninitialize();
  return res;
}
