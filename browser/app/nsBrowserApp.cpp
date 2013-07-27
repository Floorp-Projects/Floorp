/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"
#include "mozilla/AppData.h"
#include "application.ini.h"
#include "nsXPCOMGlue.h"
#if defined(XP_WIN)
#include <windows.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#elif defined(XP_UNIX)
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#endif

#ifdef XP_MACOSX
#include <mach/mach_time.h>
#include "MacQuirks.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsStringGlue.h"

// Easy access to a five second startup delay used to get
// a debugger attached in the metro environment. 
// #define DEBUG_delay_start_metro

#ifdef XP_WIN
// we want a wmain entry point
#include "nsWindowsWMain.cpp"
#define snprintf _snprintf
#define strcasecmp _stricmp
#endif
#include "BinaryPath.h"

#include "nsXPCOMPrivate.h" // for MAXPATHLEN and XPCOM_DLL

#include "mozilla/Telemetry.h"

using namespace mozilla;

#define kDesktopFolder "browser"
#define kMetroFolder "metro"
#define kMetroAppIniFilename "metroapp.ini"
#ifdef XP_WIN
#define kMetroTestFile "tests.ini"
const char* kMetroConsoleIdParam = "testconsoleid=";
#endif

static void Output(const char *fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

#ifndef XP_WIN
  vfprintf(stderr, fmt, ap);
#else
  char msg[2048];
  vsnprintf_s(msg, _countof(msg), _TRUNCATE, fmt, ap);

  wchar_t wide_msg[2048];
  MultiByteToWideChar(CP_UTF8,
                      0,
                      msg,
                      -1,
                      wide_msg,
                      _countof(wide_msg));
#if MOZ_WINCONSOLE
  fwprintf_s(stderr, wide_msg);
#else
  MessageBoxW(NULL, wide_msg, L"Firefox", MB_OK
                                        | MB_ICONERROR
                                        | MB_SETFOREGROUND);
#endif
#endif

  va_end(ap);
}

/**
 * Return true if |arg| matches the given argument name.
 */
static bool IsArg(const char* arg, const char* s)
{
  if (*arg == '-')
  {
    if (*++arg == '-')
      ++arg;
    return !strcasecmp(arg, s);
  }

#if defined(XP_WIN) || defined(XP_OS2)
  if (*arg == '/')
    return !strcasecmp(++arg, s);
#endif

  return false;
}

#ifdef XP_WIN
/*
 * AttachToTestHarness - Windows helper for when we are running
 * in the immersive environment. Firefox is launched by Windows in
 * response to a request by metrotestharness, which is launched by
 * runtests.py. As such stdout in fx doesn't point to the right
 * stream. This helper touches up stdout such that test output gets
 * routed to a named pipe metrotestharness creates and dumps to its
 * stdout.
 */
static void AttachToTestHarness()
{
  // attach to the metrotestharness named logging pipe
  HANDLE winOut = CreateFileA("\\\\.\\pipe\\metrotestharness",
                              GENERIC_WRITE,
                              FILE_SHARE_WRITE, 0,
                              OPEN_EXISTING, 0, 0);
  
  if (winOut == INVALID_HANDLE_VALUE) {
    OutputDebugStringW(L"Could not create named logging pipe.\n");
    return;
  }

  // Set the c runtime handle
  int stdOut = _open_osfhandle((intptr_t)winOut, _O_APPEND);
  if (stdOut == -1) {
    OutputDebugStringW(L"Could not open c-runtime handle.\n");
    return;
  }
  FILE *fp = _fdopen(stdOut, "a");
  *stdout = *fp;
}
#endif

XRE_GetFileFromPathType XRE_GetFileFromPath;
XRE_CreateAppDataType XRE_CreateAppData;
XRE_FreeAppDataType XRE_FreeAppData;
#ifdef XRE_HAS_DLL_BLOCKLIST
XRE_SetupDllBlocklistType XRE_SetupDllBlocklist;
#endif
XRE_TelemetryAccumulateType XRE_TelemetryAccumulate;
XRE_StartupTimelineRecordType XRE_StartupTimelineRecord;
XRE_mainType XRE_main;
XRE_DisableWritePoisoningType XRE_DisableWritePoisoning;

static const nsDynamicFunctionLoad kXULFuncs[] = {
    { "XRE_GetFileFromPath", (NSFuncPtr*) &XRE_GetFileFromPath },
    { "XRE_CreateAppData", (NSFuncPtr*) &XRE_CreateAppData },
    { "XRE_FreeAppData", (NSFuncPtr*) &XRE_FreeAppData },
#ifdef XRE_HAS_DLL_BLOCKLIST
    { "XRE_SetupDllBlocklist", (NSFuncPtr*) &XRE_SetupDllBlocklist },
#endif
    { "XRE_TelemetryAccumulate", (NSFuncPtr*) &XRE_TelemetryAccumulate },
    { "XRE_StartupTimelineRecord", (NSFuncPtr*) &XRE_StartupTimelineRecord },
    { "XRE_main", (NSFuncPtr*) &XRE_main },
    { "XRE_DisableWritePoisoning", (NSFuncPtr*) &XRE_DisableWritePoisoning },
    { nullptr, nullptr }
};

static int do_main(int argc, char* argv[], nsIFile *xreDirectory)
{
  nsCOMPtr<nsIFile> appini;
  nsresult rv;
  uint32_t mainFlags = 0;

  // Allow firefox.exe to launch XULRunner apps via -app <application.ini>
  // Note that -app must be the *first* argument.
  const char *appDataFile = getenv("XUL_APP_FILE");
  if (appDataFile && *appDataFile) {
    rv = XRE_GetFileFromPath(appDataFile, getter_AddRefs(appini));
    if (NS_FAILED(rv)) {
      Output("Invalid path found: '%s'", appDataFile);
      return 255;
    }
  }
  else if (argc > 1 && IsArg(argv[1], "app")) {
    if (argc == 2) {
      Output("Incorrect number of arguments passed to -app");
      return 255;
    }

    rv = XRE_GetFileFromPath(argv[2], getter_AddRefs(appini));
    if (NS_FAILED(rv)) {
      Output("application.ini path not recognized: '%s'", argv[2]);
      return 255;
    }

    char appEnv[MAXPATHLEN];
    snprintf(appEnv, MAXPATHLEN, "XUL_APP_FILE=%s", argv[2]);
    if (putenv(appEnv)) {
      Output("Couldn't set %s.\n", appEnv);
      return 255;
    }
    argv[2] = argv[0];
    argv += 2;
    argc -= 2;
  }

  if (appini) {
    nsXREAppData *appData;
    rv = XRE_CreateAppData(appini, &appData);
    if (NS_FAILED(rv)) {
      Output("Couldn't read application.ini");
      return 255;
    }
    // xreDirectory already has a refcount from NS_NewLocalFile
    appData->xreDirectory = xreDirectory;
    int result = XRE_main(argc, argv, appData, mainFlags);
    XRE_FreeAppData(appData);
    return result;
  }

  bool metroOnDesktop = false;

#ifdef MOZ_METRO
  if (argc > 1) {
    // This command-line flag is passed to our executable when it is to be
    // launched in metro mode (i.e. our EXE is registered as the default
    // browser and the user has tapped our EXE's tile)
    if (IsArg(argv[1], "ServerName:DefaultBrowserServer")) {
      mainFlags = XRE_MAIN_FLAG_USE_METRO;
      argv[1] = argv[0];
      argv++;
      argc--;
    } else if (IsArg(argv[1], "BackgroundSessionClosed")) {
      // This command line flag is used for indirect shutdowns, the OS
      // relaunches Metro Firefox with this command line arg.
      mainFlags = XRE_MAIN_FLAG_USE_METRO;
    } else {
      // This command-line flag is used to test the metro browser in a desktop
      // environment.
      for (int idx = 1; idx < argc; idx++) {
        if (IsArg(argv[idx], "metrodesktop")) {
          metroOnDesktop = true;
          // Disable crash reporting when running in metrodesktop mode.
          char crashSwitch[] = "MOZ_CRASHREPORTER_DISABLE=1";
          putenv(crashSwitch);
          break;
        } 
      }
    }
  }
#endif

  // Desktop browser launch
  if (mainFlags != XRE_MAIN_FLAG_USE_METRO && !metroOnDesktop) {
    ScopedAppData appData(&sAppData);
    nsCOMPtr<nsIFile> exeFile;
    rv = mozilla::BinaryPath::GetFile(argv[0], getter_AddRefs(exeFile));
    if (NS_FAILED(rv)) {
      Output("Couldn't find the application directory.\n");
      return 255;
    }

    nsCOMPtr<nsIFile> greDir;
    exeFile->GetParent(getter_AddRefs(greDir));

    nsCOMPtr<nsIFile> appSubdir;
    greDir->Clone(getter_AddRefs(appSubdir));
    appSubdir->Append(NS_LITERAL_STRING(kDesktopFolder));

    SetStrongPtr(appData.directory, static_cast<nsIFile*>(appSubdir.get()));
    // xreDirectory already has a refcount from NS_NewLocalFile
    appData.xreDirectory = xreDirectory;

    return XRE_main(argc, argv, &appData, mainFlags);
  }

  // Metro browser launch
#ifdef MOZ_METRO
  nsCOMPtr<nsIFile> iniFile, appSubdir;

  xreDirectory->Clone(getter_AddRefs(iniFile));
  xreDirectory->Clone(getter_AddRefs(appSubdir));

  iniFile->Append(NS_LITERAL_STRING(kMetroFolder));
  iniFile->Append(NS_LITERAL_STRING(kMetroAppIniFilename));

  appSubdir->Append(NS_LITERAL_STRING(kMetroFolder));

  nsAutoCString path;
  if (NS_FAILED(iniFile->GetNativePath(path))) {
    Output("Couldn't get ini file path.\n");
    return 255;
  }

  char appEnv[MAXPATHLEN];
  snprintf(appEnv, MAXPATHLEN, "XUL_APP_FILE=%s", path.get());
  if (putenv(appEnv)) {
    Output("Couldn't set %s.\n", appEnv);
    return 255;
  }

  nsXREAppData *appData;
  rv = XRE_CreateAppData(iniFile, &appData);
  if (NS_FAILED(rv) || !appData) {
    Output("Couldn't read application.ini");
    return 255;
  }

  SetStrongPtr(appData->directory, static_cast<nsIFile*>(appSubdir.get()));
  // xreDirectory already has a refcount from NS_NewLocalFile
  appData->xreDirectory = xreDirectory;

#ifdef XP_WIN
  if (!metroOnDesktop) {
    nsCOMPtr<nsIFile> testFile;

    xreDirectory->Clone(getter_AddRefs(testFile));
    testFile->Append(NS_LITERAL_STRING(kMetroTestFile));

    nsAutoCString path;
    if (NS_FAILED(testFile->GetNativePath(path))) {
      Output("Couldn't get test file path.\n");
      return 255;
    }

    // Check for a metro test harness command line args file
    HANDLE hTestFile = CreateFileA(path.get(),
                                   GENERIC_READ,
                                   0, NULL, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL,
                                   NULL);
    if (hTestFile != INVALID_HANDLE_VALUE) {
      // Typical test harness command line args string is around 100 bytes.
      char buffer[1024];
      memset(buffer, 0, sizeof(buffer));
      DWORD bytesRead = 0;
      if (!ReadFile(hTestFile, (VOID*)buffer, sizeof(buffer)-1,
                    &bytesRead, NULL) || !bytesRead) {
        CloseHandle(hTestFile);
        printf("failed to read test file '%s'", testFile);
        return -1;
      }
      CloseHandle(hTestFile);

      // Build new args array
      char* newArgv[20];
      int newArgc = 1;

      memset(newArgv, 0, sizeof(newArgv));

      char* ptr = buffer;
      newArgv[0] = ptr;
      while (*ptr != NULL &&
             (ptr - buffer) < sizeof(buffer) &&
             newArgc < ARRAYSIZE(newArgv)) {
        if (isspace(*ptr)) {
          *ptr = '\0';
          ptr++;
          newArgv[newArgc] = ptr;
          newArgc++;
          continue;
        }
        ptr++;
      }
      if (ptr == newArgv[newArgc-1])
        newArgc--;

      // attach browser stdout to metrotestharness stdout
      AttachToTestHarness();

      int result = XRE_main(newArgc, newArgv, appData, mainFlags);
      XRE_FreeAppData(appData);
      return result;
    }
  }
#endif

  int result = XRE_main(argc, argv, appData, mainFlags);
  XRE_FreeAppData(appData);
  return result;
#endif

  NS_NOTREACHED("browser do_main failed to pickup proper initialization");
  return 255;
}

#ifdef XP_WIN

/**
 * Used only when GetTickCount64 is not available on the platform.
 * Last result of GetTickCount call. Kept in [ms].
 */
static DWORD sLastGTCResult = 0;

/**
 *  Higher part of the 64-bit value of MozGetTickCount64,
 * incremented atomically.
 */
static DWORD sLastGTCRollover = 0;

/**
 * Function protecting GetTickCount result from rolling over. The original
 * code comes from the Windows implementation of the TimeStamp class minus the
 * locking harness which isn't needed here.
 *
 * @returns The current time in milliseconds
 */
static ULONGLONG WINAPI
MozGetTickCount64()
{
  DWORD GTC = ::GetTickCount();

  /* Pull the rollover counter forward only if new value of GTC goes way
   * down under the last saved result */
  if ((sLastGTCResult > GTC) && ((sLastGTCResult - GTC) > (1UL << 30)))
    ++sLastGTCRollover;

  sLastGTCResult = GTC;
  return (ULONGLONG)sLastGTCRollover << 32 | sLastGTCResult;
}

typedef ULONGLONG (WINAPI* GetTickCount64_t)();
static GetTickCount64_t sGetTickCount64 = nullptr;

#endif

/**
 * Local TimeStamp::Now()-compatible implementation used to record timestamps
 * which will be passed to XRE_StartupTimelineRecord().
 */
static uint64_t
TimeStamp_Now()
{
#ifdef XP_WIN
  LARGE_INTEGER freq;
  ::QueryPerformanceFrequency(&freq);

  HMODULE kernelDLL = GetModuleHandleW(L"kernel32.dll");
  sGetTickCount64 = reinterpret_cast<GetTickCount64_t>
    (GetProcAddress(kernelDLL, "GetTickCount64"));

  if (!sGetTickCount64) {
    /* If the platform does not support the GetTickCount64 (Windows XP doesn't),
     * then use our fallback implementation based on GetTickCount. */
    sGetTickCount64 = MozGetTickCount64;
  }

  return sGetTickCount64() * freq.QuadPart;
#elif defined(XP_MACOSX)
  return mach_absolute_time();
#elif defined(HAVE_CLOCK_MONOTONIC)
  struct timespec ts;
  int rv = clock_gettime(CLOCK_MONOTONIC, &ts);

  if (rv != 0) {
    return 0;
  }

  uint64_t baseNs = (uint64_t)ts.tv_sec * 1000000000;
  return baseNs + (uint64_t)ts.tv_nsec;
#endif
}

static bool
FileExists(const char *path)
{
#ifdef XP_WIN
  wchar_t wideDir[MAX_PATH];
  MultiByteToWideChar(CP_UTF8, 0, path, -1, wideDir, MAX_PATH);
  DWORD fileAttrs = GetFileAttributesW(wideDir);
  return fileAttrs != INVALID_FILE_ATTRIBUTES;
#else
  return access(path, R_OK) == 0;
#endif
}

#ifdef LIBXUL_SDK
#  define XPCOM_PATH "xulrunner" XPCOM_FILE_PATH_SEPARATOR XPCOM_DLL
#else
#  define XPCOM_PATH XPCOM_DLL
#endif
static nsresult
InitXPCOMGlue(const char *argv0, nsIFile **xreDirectory)
{
  char exePath[MAXPATHLEN];

  nsresult rv = mozilla::BinaryPath::Get(argv0, exePath);
  if (NS_FAILED(rv)) {
    Output("Couldn't find the application directory.\n");
    return rv;
  }

  char *lastSlash = strrchr(exePath, XPCOM_FILE_PATH_SEPARATOR[0]);
  if (!lastSlash || (size_t(lastSlash - exePath) > MAXPATHLEN - sizeof(XPCOM_PATH) - 1))
    return NS_ERROR_FAILURE;

  strcpy(lastSlash + 1, XPCOM_PATH);
  lastSlash += sizeof(XPCOM_PATH) - sizeof(XPCOM_DLL);

  if (!FileExists(exePath)) {
#if defined(LIBXUL_SDK) && defined(XP_MACOSX)
    // Check for <bundle>/Contents/Frameworks/XUL.framework/libxpcom.dylib
    bool greFound = false;
    CFBundleRef appBundle = CFBundleGetMainBundle();
    if (!appBundle)
      return NS_ERROR_FAILURE;
    CFURLRef fwurl = CFBundleCopyPrivateFrameworksURL(appBundle);
    CFURLRef absfwurl = nullptr;
    if (fwurl) {
      absfwurl = CFURLCopyAbsoluteURL(fwurl);
      CFRelease(fwurl);
    }
    if (absfwurl) {
      CFURLRef xulurl =
        CFURLCreateCopyAppendingPathComponent(NULL, absfwurl,
                                              CFSTR("XUL.framework"),
                                              true);

      if (xulurl) {
        CFURLRef xpcomurl =
          CFURLCreateCopyAppendingPathComponent(NULL, xulurl,
                                                CFSTR("libxpcom.dylib"),
                                                false);

        if (xpcomurl) {
          if (CFURLGetFileSystemRepresentation(xpcomurl, true,
                                               (UInt8*) exePath,
                                               sizeof(exePath)) &&
              access(tbuffer, R_OK | X_OK) == 0) {
            if (realpath(tbuffer, exePath)) {
              greFound = true;
            }
          }
          CFRelease(xpcomurl);
        }
        CFRelease(xulurl);
      }
      CFRelease(absfwurl);
    }
  }
  if (!greFound) {
#endif
    Output("Could not find the Mozilla runtime.\n");
    return NS_ERROR_FAILURE;
  }

  // We do this because of data in bug 771745
  XPCOMGlueEnablePreload();

  rv = XPCOMGlueStartup(exePath);
  if (NS_FAILED(rv)) {
    Output("Couldn't load XPCOM.\n");
    return rv;
  }

  rv = XPCOMGlueLoadXULFunctions(kXULFuncs);
  if (NS_FAILED(rv)) {
    Output("Couldn't load XRE functions.\n");
    return rv;
  }

  NS_LogInit();

  // chop XPCOM_DLL off exePath
  *lastSlash = '\0';
#ifdef XP_WIN
  rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(exePath), false,
                       xreDirectory);
#else
  rv = NS_NewNativeLocalFile(nsDependentCString(exePath), false,
                             xreDirectory);
#endif

  return rv;
}

int main(int argc, char* argv[])
{
#ifdef DEBUG_delay_start_metro
  Sleep(5000);
#endif
  uint64_t start = TimeStamp_Now();

#ifdef XP_MACOSX
  TriggerQuirks();
#endif

  int gotCounters;
#if defined(XP_UNIX)
  struct rusage initialRUsage;
  gotCounters = !getrusage(RUSAGE_SELF, &initialRUsage);
#elif defined(XP_WIN)
  IO_COUNTERS ioCounters;
  gotCounters = GetProcessIoCounters(GetCurrentProcess(), &ioCounters);
#endif

  nsIFile *xreDirectory;

  nsresult rv = InitXPCOMGlue(argv[0], &xreDirectory);
  if (NS_FAILED(rv)) {
    return 255;
  }

  XRE_StartupTimelineRecord(mozilla::StartupTimeline::START, start);

#ifdef XRE_HAS_DLL_BLOCKLIST
  XRE_SetupDllBlocklist();
#endif

  if (gotCounters) {
#if defined(XP_WIN)
    XRE_TelemetryAccumulate(mozilla::Telemetry::EARLY_GLUESTARTUP_READ_OPS,
                            int(ioCounters.ReadOperationCount));
    XRE_TelemetryAccumulate(mozilla::Telemetry::EARLY_GLUESTARTUP_READ_TRANSFER,
                            int(ioCounters.ReadTransferCount / 1024));
    IO_COUNTERS newIoCounters;
    if (GetProcessIoCounters(GetCurrentProcess(), &newIoCounters)) {
      XRE_TelemetryAccumulate(mozilla::Telemetry::GLUESTARTUP_READ_OPS,
                              int(newIoCounters.ReadOperationCount - ioCounters.ReadOperationCount));
      XRE_TelemetryAccumulate(mozilla::Telemetry::GLUESTARTUP_READ_TRANSFER,
                              int((newIoCounters.ReadTransferCount - ioCounters.ReadTransferCount) / 1024));
    }
#elif defined(XP_UNIX)
    XRE_TelemetryAccumulate(mozilla::Telemetry::EARLY_GLUESTARTUP_HARD_FAULTS,
                            int(initialRUsage.ru_majflt));
    struct rusage newRUsage;
    if (!getrusage(RUSAGE_SELF, &newRUsage)) {
      XRE_TelemetryAccumulate(mozilla::Telemetry::GLUESTARTUP_HARD_FAULTS,
                              int(newRUsage.ru_majflt - initialRUsage.ru_majflt));
    }
#endif
  }

  int result = do_main(argc, argv, xreDirectory);

  NS_LogTerm();

#ifdef XP_MACOSX
  // Allow writes again. While we would like to catch writes from static
  // destructors to allow early exits to use _exit, we know that there is
  // at least one such write that we don't control (see bug 826029). For
  // now we enable writes again and early exits will have to use exit instead
  // of _exit.
  XRE_DisableWritePoisoning();
#endif

  return result;
}
