/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"
#include "mozilla/XREAppData.h"
#include "application.ini.h"
#include "nsXPCOMGlue.h"
#if defined(XP_WIN)
#include <windows.h>
#include <stdlib.h>
#elif defined(XP_UNIX)
#include <sys/resource.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsStringGlue.h"

#ifdef XP_WIN
#ifdef MOZ_ASAN
// ASAN requires firefox.exe to be built with -MD, and it's OK if we don't
// support Windows XP SP2 in ASAN builds.
#define XRE_DONT_SUPPORT_XPSP2
#endif
#define XRE_WANT_ENVIRON
#define strcasecmp _stricmp
#ifdef MOZ_SANDBOX
#include "mozilla/sandboxing/SandboxInitialization.h"
#endif
#endif
#include "BinaryPath.h"

#include "nsXPCOMPrivate.h" // for MAXPATHLEN and XPCOM_DLL

#include "mozilla/Sprintf.h"
#include "mozilla/Telemetry.h"
#include "mozilla/WindowsDllBlocklist.h"

#ifdef MOZ_LINUX_32_SSE2_STARTUP_ERROR
#include <cpuid.h>
#include "mozilla/Unused.h"

static bool
IsSSE2Available()
{
  // The rest of the app has been compiled to assume that SSE2 is present
  // unconditionally, so we can't use the normal copy of SSE.cpp here.
  // Since SSE.cpp caches the results and we need them only transiently,
  // instead of #including SSE.cpp here, let's just inline the specific check
  // that's needed.
  unsigned int level = 1u;
  unsigned int eax, ebx, ecx, edx;
  unsigned int bits = (1u<<26);
  unsigned int max = __get_cpuid_max(0, nullptr);
  if (level > max) {
    return false;
  }
  __cpuid_count(level, 0, eax, ebx, ecx, edx);
  return (edx & bits) == bits;
}

static const char sSSE2Message[] =
    "This browser version requires a processor with the SSE2 instruction "
    "set extension.\nYou may be able to obtain a version that does not "
    "require SSE2 from your Linux distribution.\n";

__attribute__((constructor))
static void
SSE2Check()
{
  if (IsSSE2Available()) {
    return;
  }
  // Using write() in order to avoid jemalloc-based buffering. Ignoring return
  // values, since there isn't much we could do on failure and there is no
  // point in trying to recover from errors.
  MOZ_UNUSED(write(STDERR_FILENO,
                   sSSE2Message,
                   MOZ_ARRAY_LENGTH(sSSE2Message) - 1));
  // _exit() instead of exit() to avoid running the usual "at exit" code.
  _exit(255);
}
#endif

#if !defined(MOZ_WIDGET_COCOA) && !defined(MOZ_WIDGET_ANDROID) \
  && !(defined(XP_LINUX) && defined(MOZ_SANDBOX))
#define MOZ_BROWSER_CAN_BE_CONTENTPROC
#include "../../ipc/contentproc/plugin-container.cpp"
#endif

using namespace mozilla;

#ifdef XP_MACOSX
#define kOSXResourcesFolder "Resources"
#endif
#define kDesktopFolder "browser"

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
  // Linking user32 at load-time interferes with the DLL blocklist (bug 932100).
  // This is a rare codepath, so we can load user32 at run-time instead.
  HMODULE user32 = LoadLibraryW(L"user32.dll");
  if (user32) {
    decltype(MessageBoxW)* messageBoxW =
      (decltype(MessageBoxW)*) GetProcAddress(user32, "MessageBoxW");
    if (messageBoxW) {
      messageBoxW(nullptr, wide_msg, L"Firefox", MB_OK
                                               | MB_ICONERROR
                                               | MB_SETFOREGROUND);
    }
    FreeLibrary(user32);
  }
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

#if defined(XP_WIN)
  if (*arg == '/')
    return !strcasecmp(++arg, s);
#endif

  return false;
}

XRE_GetFileFromPathType XRE_GetFileFromPath;
XRE_ParseAppDataType XRE_ParseAppData;
XRE_TelemetryAccumulateType XRE_TelemetryAccumulate;
XRE_StartupTimelineRecordType XRE_StartupTimelineRecord;
XRE_mainType XRE_main;
XRE_StopLateWriteChecksType XRE_StopLateWriteChecks;
XRE_XPCShellMainType XRE_XPCShellMain;
XRE_GetProcessTypeType XRE_GetProcessType;
XRE_SetProcessTypeType XRE_SetProcessType;
XRE_InitChildProcessType XRE_InitChildProcess;
XRE_EnableSameExecutableForContentProcType XRE_EnableSameExecutableForContentProc;
#ifdef LIBFUZZER
XRE_LibFuzzerSetMainType XRE_LibFuzzerSetMain;
XRE_LibFuzzerGetFuncsType XRE_LibFuzzerGetFuncs;
#endif

static const nsDynamicFunctionLoad kXULFuncs[] = {
    { "XRE_GetFileFromPath", (NSFuncPtr*) &XRE_GetFileFromPath },
    { "XRE_ParseAppData", (NSFuncPtr*) &XRE_ParseAppData },
    { "XRE_TelemetryAccumulate", (NSFuncPtr*) &XRE_TelemetryAccumulate },
    { "XRE_StartupTimelineRecord", (NSFuncPtr*) &XRE_StartupTimelineRecord },
    { "XRE_main", (NSFuncPtr*) &XRE_main },
    { "XRE_StopLateWriteChecks", (NSFuncPtr*) &XRE_StopLateWriteChecks },
    { "XRE_XPCShellMain", (NSFuncPtr*) &XRE_XPCShellMain },
    { "XRE_GetProcessType", (NSFuncPtr*) &XRE_GetProcessType },
    { "XRE_SetProcessType", (NSFuncPtr*) &XRE_SetProcessType },
    { "XRE_InitChildProcess", (NSFuncPtr*) &XRE_InitChildProcess },
    { "XRE_EnableSameExecutableForContentProc", (NSFuncPtr*) &XRE_EnableSameExecutableForContentProc },
#ifdef LIBFUZZER
    { "XRE_LibFuzzerSetMain", (NSFuncPtr*) &XRE_LibFuzzerSetMain },
    { "XRE_LibFuzzerGetFuncs", (NSFuncPtr*) &XRE_LibFuzzerGetFuncs },
#endif
    { nullptr, nullptr }
};

#ifdef LIBFUZZER
int libfuzzer_main(int argc, char **argv);

/* This wrapper is used by the libFuzzer main to call into libxul */

void libFuzzerGetFuncs(const char* moduleName, LibFuzzerInitFunc* initFunc,
                       LibFuzzerTestingFunc* testingFunc) {
  return XRE_LibFuzzerGetFuncs(moduleName, initFunc, testingFunc);
}
#endif

static int do_main(int argc, char* argv[], char* envp[], nsIFile *xreDirectory)
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
    SprintfLiteral(appEnv, "XUL_APP_FILE=%s", argv[2]);
    if (putenv(strdup(appEnv))) {
      Output("Couldn't set %s.\n", appEnv);
      return 255;
    }
    argv[2] = argv[0];
    argv += 2;
    argc -= 2;
  } else if (argc > 1 && IsArg(argv[1], "xpcshell")) {
    for (int i = 1; i < argc; i++) {
      argv[i] = argv[i + 1];
    }

    XREShellData shellData;
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
    shellData.sandboxBrokerServices =
      sandboxing::GetInitializedBrokerServices();
#endif

    return XRE_XPCShellMain(--argc, argv, envp, &shellData);
  }

  if (appini) {
    XREAppData appData;
    rv = XRE_ParseAppData(appini, appData);
    if (NS_FAILED(rv)) {
      Output("Couldn't read application.ini");
      return 255;
    }
#if defined(HAS_DLL_BLOCKLIST)
    // The dll blocklist operates in the exe vs. xullib. Pass a flag to
    // xullib so automated tests can check the result once the browser
    // is up and running.
    appData->flags |=
      DllBlocklist_CheckStatus() ? NS_XRE_DLL_BLOCKLIST_ENABLED : 0;
#endif
    appData.xreDirectory = xreDirectory;
    appini->GetParent(getter_AddRefs(appData.directory));
    return XRE_main(argc, argv, appData, mainFlags);
  }

  XREAppData appData;
  appData = sAppData;
  nsCOMPtr<nsIFile> exeFile;
  rv = mozilla::BinaryPath::GetFile(argv[0], getter_AddRefs(exeFile));
  if (NS_FAILED(rv)) {
    Output("Couldn't find the application directory.\n");
    return 255;
  }

  nsCOMPtr<nsIFile> greDir;
  exeFile->GetParent(getter_AddRefs(greDir));
#ifdef XP_MACOSX
  greDir->SetNativeLeafName(NS_LITERAL_CSTRING(kOSXResourcesFolder));
#endif
  nsCOMPtr<nsIFile> appSubdir;
  greDir->Clone(getter_AddRefs(appSubdir));
  appSubdir->Append(NS_LITERAL_STRING(kDesktopFolder));

  appData.directory = appSubdir;
  appData.xreDirectory = xreDirectory;

#if defined(HAS_DLL_BLOCKLIST)
  appData.flags |=
    DllBlocklist_CheckStatus() ? NS_XRE_DLL_BLOCKLIST_ENABLED : 0;
#endif

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  sandbox::BrokerServices* brokerServices =
    sandboxing::GetInitializedBrokerServices();
#if defined(MOZ_CONTENT_SANDBOX)
  if (!brokerServices) {
    Output("Couldn't initialize the broker services.\n");
    return 255;
  }
#endif
  appData.sandboxBrokerServices = brokerServices;
#endif

#ifdef LIBFUZZER
  if (getenv("LIBFUZZER"))
    XRE_LibFuzzerSetMain(argc, argv, libfuzzer_main);
#endif

  return XRE_main(argc, argv, appData, mainFlags);
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
  if (!lastSlash ||
      (size_t(lastSlash - exePath) > MAXPATHLEN - sizeof(XPCOM_DLL) - 1))
    return NS_ERROR_FAILURE;

  strcpy(lastSlash + 1, XPCOM_DLL);

  if (!FileExists(exePath)) {
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

  // This will set this thread as the main thread.
  NS_LogInit();

  if (xreDirectory) {
    // chop XPCOM_DLL off exePath
    *lastSlash = '\0';
#ifdef XP_MACOSX
    lastSlash = strrchr(exePath, XPCOM_FILE_PATH_SEPARATOR[0]);
    strcpy(lastSlash + 1, kOSXResourcesFolder);
#endif
#ifdef XP_WIN
    rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(exePath), false,
                         xreDirectory);
#else
    rv = NS_NewNativeLocalFile(nsDependentCString(exePath), false,
                               xreDirectory);
#endif
  }

  return rv;
}

int main(int argc, char* argv[], char* envp[])
{
  mozilla::TimeStamp start = mozilla::TimeStamp::Now();

#ifdef HAS_DLL_BLOCKLIST
  DllBlocklist_Initialize();

#ifdef DEBUG
  // In order to be effective against AppInit DLLs, the blocklist must be
  // initialized before user32.dll is loaded into the process (bug 932100).
  if (GetModuleHandleA("user32.dll")) {
    fprintf(stderr, "DLL blocklist was unable to intercept AppInit DLLs.\n");
  }
#endif
#endif

#ifdef MOZ_BROWSER_CAN_BE_CONTENTPROC
  // We are launching as a content process, delegate to the appropriate
  // main
  if (argc > 1 && IsArg(argv[1], "contentproc")) {
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
    // We need to initialize the sandbox TargetServices before InitXPCOMGlue
    // because we might need the sandbox broker to give access to some files.
    if (IsSandboxedProcess() && !sandboxing::GetInitializedTargetServices()) {
      Output("Failed to initialize the sandbox target services.");
      return 255;
    }
#endif

    nsresult rv = InitXPCOMGlue(argv[0], nullptr);
    if (NS_FAILED(rv)) {
      return 255;
    }

    int result = content_process_main(argc, argv);

    // InitXPCOMGlue calls NS_LogInit, so we need to balance it here.
    NS_LogTerm();

    return result;
  }
#endif


  nsIFile *xreDirectory;

  nsresult rv = InitXPCOMGlue(argv[0], &xreDirectory);
  if (NS_FAILED(rv)) {
    return 255;
  }

  XRE_StartupTimelineRecord(mozilla::StartupTimeline::START, start);

#ifdef MOZ_BROWSER_CAN_BE_CONTENTPROC
  XRE_EnableSameExecutableForContentProc();
#endif

  int result = do_main(argc, argv, envp, xreDirectory);

  NS_LogTerm();

#ifdef XP_MACOSX
  // Allow writes again. While we would like to catch writes from static
  // destructors to allow early exits to use _exit, we know that there is
  // at least one such write that we don't control (see bug 826029). For
  // now we enable writes again and early exits will have to use exit instead
  // of _exit.
  XRE_StopLateWriteChecks();
#endif

  return result;
}
