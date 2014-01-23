/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"
#include "application.ini.h"
#include "nsXPCOMGlue.h"
#if defined(XP_WIN)
#include <windows.h>
#include <stdlib.h>
#elif defined(XP_UNIX)
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdarg.h>

#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsStringGlue.h"

#ifdef XP_WIN
// we want a wmain entry point
#include "nsWindowsWMain.cpp"
#define snprintf _snprintf
#define strcasecmp _stricmp
#endif

#ifdef MOZ_WIDGET_GONK
#include "GonkDisplay.h"
#endif

#include "BinaryPath.h"

#include "nsXPCOMPrivate.h" // for MAXPATHLEN and XPCOM_DLL

#ifdef MOZ_WIDGET_GONK
# include <binder/ProcessState.h>
#endif

#include "mozilla/Telemetry.h"
#include "mozilla/WindowsDllBlocklist.h"

static void Output(const char *fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

#if defined(XP_WIN) && !MOZ_WINCONSOLE
  PRUnichar msg[2048];
  _vsnwprintf(msg, sizeof(msg)/sizeof(msg[0]), NS_ConvertUTF8toUTF16(fmt).get(), ap);
  MessageBoxW(nullptr, msg, L"XULRunner", MB_OK | MB_ICONERROR);
#else
  vfprintf(stderr, fmt, ap);
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

/**
 * A helper class which calls NS_LogInit/NS_LogTerm in its scope.
 */
class ScopedLogging
{
public:
  ScopedLogging() { NS_LogInit(); }
  ~ScopedLogging() { NS_LogTerm(); }
};

XRE_GetFileFromPathType XRE_GetFileFromPath;
XRE_CreateAppDataType XRE_CreateAppData;
XRE_FreeAppDataType XRE_FreeAppData;
XRE_TelemetryAccumulateType XRE_TelemetryAccumulate;
XRE_mainType XRE_main;

static const nsDynamicFunctionLoad kXULFuncs[] = {
    { "XRE_GetFileFromPath", (NSFuncPtr*) &XRE_GetFileFromPath },
    { "XRE_CreateAppData", (NSFuncPtr*) &XRE_CreateAppData },
    { "XRE_FreeAppData", (NSFuncPtr*) &XRE_FreeAppData },
    { "XRE_TelemetryAccumulate", (NSFuncPtr*) &XRE_TelemetryAccumulate },
    { "XRE_main", (NSFuncPtr*) &XRE_main },
    { nullptr, nullptr }
};

static int do_main(int argc, char* argv[])
{
  nsCOMPtr<nsIFile> appini;
  nsresult rv;

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

#ifdef MOZ_WIDGET_GONK
  /* Called to start the boot animation */
  (void) mozilla::GetGonkDisplay();
#endif

  if (appini) {
    nsXREAppData *appData;
    rv = XRE_CreateAppData(appini, &appData);
    if (NS_FAILED(rv)) {
      Output("Couldn't read application.ini");
      return 255;
    }
    int result = XRE_main(argc, argv, appData, 0);
    XRE_FreeAppData(appData);
    return result;
  }

  return XRE_main(argc, argv, &sAppData, 0);
}

int main(int argc, char* argv[])
{
  char exePath[MAXPATHLEN];

#ifdef MOZ_WIDGET_GONK
  // This creates a ThreadPool for binder ipc. A ThreadPool is necessary to
  // receive binder calls, though not necessary to send binder calls.
  // ProcessState::Self() also needs to be called once on the main thread to
  // register the main thread with the binder driver.
  android::ProcessState::self()->startThreadPool();
#endif

  nsresult rv = mozilla::BinaryPath::Get(argv[0], exePath);
  if (NS_FAILED(rv)) {
    Output("Couldn't calculate the application directory.\n");
    return 255;
  }

  char *lastSlash = strrchr(exePath, XPCOM_FILE_PATH_SEPARATOR[0]);
  if (!lastSlash || ((lastSlash - exePath) + sizeof(XPCOM_DLL) + 1 > MAXPATHLEN))
    return 255;

  strcpy(++lastSlash, XPCOM_DLL);

#if defined(XP_UNIX)
  // If the b2g app is launched from adb shell, then the shell will wind
  // up being the process group controller. This means that we can't send
  // signals to the process group (useful for profiling).
  // We ignore the return value since setsid() fails if we're already the
  // process group controller (the normal situation).
  (void)setsid();
#endif

  int gotCounters;
#if defined(XP_UNIX)
  struct rusage initialRUsage;
  gotCounters = !getrusage(RUSAGE_SELF, &initialRUsage);
#elif defined(XP_WIN)
  IO_COUNTERS ioCounters;
  gotCounters = GetProcessIoCounters(GetCurrentProcess(), &ioCounters);
#endif

  // We do this because of data in bug 771745
  XPCOMGlueEnablePreload();

  rv = XPCOMGlueStartup(exePath);
  if (NS_FAILED(rv)) {
    Output("Couldn't load XPCOM.\n");
    return 255;
  }
  // Reset exePath so that it is the directory name and not the xpcom dll name
  *lastSlash = 0;

  rv = XPCOMGlueLoadXULFunctions(kXULFuncs);
  if (NS_FAILED(rv)) {
    Output("Couldn't load XRE functions.\n");
    return 255;
  }

#ifdef HAS_DLL_BLOCKLIST
  DllBlocklist_Initialize();
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

  int result;
  {
    ScopedLogging log;
    result = do_main(argc, argv);
  }

  return result;
}
