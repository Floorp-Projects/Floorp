/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/XREAppData.h"
#include "XREShellData.h"
#include "application.ini.h"
#include "mozilla/Bootstrap.h"
#if defined(XP_WIN)
#  include <windows.h>
#  include <stdlib.h>
#  include "mozilla/PreXULSkeletonUI.h"
#elif defined(XP_UNIX)
#  include <sys/resource.h>
#  include <unistd.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "nsCOMPtr.h"

#ifdef XP_WIN
#  include "freestanding/SharedSection.h"
#  include "LauncherProcessWin.h"
#  include "mozilla/WindowsDllBlocklist.h"

#  define XRE_WANT_ENVIRON
#  define strcasecmp _stricmp
#  ifdef MOZ_SANDBOX
#    include "mozilla/sandboxing/SandboxInitialization.h"
#  endif
#endif
#include "BinaryPath.h"

#include "nsXPCOMPrivate.h"  // for MAXPATHLEN and XPCOM_DLL

#include "mozilla/Sprintf.h"
#include "mozilla/StartupTimeline.h"
#include "BaseProfiler.h"

#ifdef LIBFUZZER
#  include "FuzzerDefs.h"
#endif

#ifdef MOZ_LINUX_32_SSE2_STARTUP_ERROR
#  include <cpuid.h>
#  include "mozilla/Unused.h"

static bool IsSSE2Available() {
  // The rest of the app has been compiled to assume that SSE2 is present
  // unconditionally, so we can't use the normal copy of SSE.cpp here.
  // Since SSE.cpp caches the results and we need them only transiently,
  // instead of #including SSE.cpp here, let's just inline the specific check
  // that's needed.
  unsigned int level = 1u;
  unsigned int eax, ebx, ecx, edx;
  unsigned int bits = (1u << 26);
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

__attribute__((constructor)) static void SSE2Check() {
  if (IsSSE2Available()) {
    return;
  }
  // Using write() in order to avoid jemalloc-based buffering. Ignoring return
  // values, since there isn't much we could do on failure and there is no
  // point in trying to recover from errors.
  MOZ_UNUSED(
      write(STDERR_FILENO, sSSE2Message, MOZ_ARRAY_LENGTH(sSSE2Message) - 1));
  // _exit() instead of exit() to avoid running the usual "at exit" code.
  _exit(255);
}
#endif

#if !defined(MOZ_WIDGET_COCOA) && !defined(MOZ_WIDGET_ANDROID)
#  define MOZ_BROWSER_CAN_BE_CONTENTPROC
#  include "../../ipc/contentproc/plugin-container.cpp"
#endif

using namespace mozilla;

#ifdef XP_MACOSX
#  define kOSXResourcesFolder "Resources"
#endif
#define kDesktopFolder "browser"

static MOZ_FORMAT_PRINTF(1, 2) void Output(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

#ifndef XP_WIN
  vfprintf(stderr, fmt, ap);
#else
  char msg[2048];
  vsnprintf_s(msg, _countof(msg), _TRUNCATE, fmt, ap);

  wchar_t wide_msg[2048];
  MultiByteToWideChar(CP_UTF8, 0, msg, -1, wide_msg, _countof(wide_msg));
#  if MOZ_WINCONSOLE
  fwprintf_s(stderr, wide_msg);
#  else
  // Linking user32 at load-time interferes with the DLL blocklist (bug 932100).
  // This is a rare codepath, so we can load user32 at run-time instead.
  HMODULE user32 = LoadLibraryW(L"user32.dll");
  if (user32) {
    decltype(MessageBoxW)* messageBoxW =
        (decltype(MessageBoxW)*)GetProcAddress(user32, "MessageBoxW");
    if (messageBoxW) {
      messageBoxW(nullptr, wide_msg, L"Firefox",
                  MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    }
    FreeLibrary(user32);
  }
#  endif
#endif

  va_end(ap);
}

/**
 * Return true if |arg| matches the given argument name.
 */
static bool IsArg(const char* arg, const char* s) {
  if (*arg == '-') {
    if (*++arg == '-') ++arg;
    return !strcasecmp(arg, s);
  }

#if defined(XP_WIN)
  if (*arg == '/') return !strcasecmp(++arg, s);
#endif

  return false;
}

Bootstrap::UniquePtr gBootstrap;

static int do_main(int argc, char* argv[], char* envp[]) {
  // Allow firefox.exe to launch XULRunner apps via -app <application.ini>
  // Note that -app must be the *first* argument.
  const char* appDataFile = getenv("XUL_APP_FILE");
  if ((!appDataFile || !*appDataFile) && (argc > 1 && IsArg(argv[1], "app"))) {
    if (argc == 2) {
      Output("Incorrect number of arguments passed to -app");
      return 255;
    }
    appDataFile = argv[2];

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

#ifdef LIBFUZZER
    shellData.fuzzerDriver = fuzzer::FuzzerDriver;
#endif

    return gBootstrap->XRE_XPCShellMain(--argc, argv, envp, &shellData);
  }

  BootstrapConfig config;

  if (appDataFile && *appDataFile) {
    config.appData = nullptr;
    config.appDataPath = appDataFile;
  } else {
    // no -app flag so we use the compiled-in app data
    config.appData = &sAppData;
    config.appDataPath = kDesktopFolder;
  }

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  sandbox::BrokerServices* brokerServices =
      sandboxing::GetInitializedBrokerServices();
  sandboxing::PermissionsService* permissionsService =
      sandboxing::GetPermissionsService();
  if (!brokerServices) {
    Output("Couldn't initialize the broker services.\n");
    return 255;
  }
  config.sandboxBrokerServices = brokerServices;
  config.sandboxPermissionsService = permissionsService;
#endif

#ifdef LIBFUZZER
  if (getenv("FUZZER"))
    gBootstrap->XRE_LibFuzzerSetDriver(fuzzer::FuzzerDriver);
#endif

  // Note: keep in sync with LauncherProcessWin.
  const char* acceptableParams[] = {"url", nullptr};
  EnsureCommandlineSafe(argc, argv, acceptableParams);

  return gBootstrap->XRE_main(argc, argv, config);
}

static nsresult InitXPCOMGlue(LibLoadingStrategy aLibLoadingStrategy) {
  if (gBootstrap) {
    return NS_OK;
  }

  UniqueFreePtr<char> exePath = BinaryPath::Get();
  if (!exePath) {
    Output("Couldn't find the application directory.\n");
    return NS_ERROR_FAILURE;
  }

  auto bootstrapResult =
      mozilla::GetBootstrap(exePath.get(), aLibLoadingStrategy);
  if (bootstrapResult.isErr()) {
    Output("Couldn't load XPCOM.\n");
    return NS_ERROR_FAILURE;
  }

  gBootstrap = bootstrapResult.unwrap();

  // This will set this thread as the main thread.
  gBootstrap->NS_LogInit();

  return NS_OK;
}

#ifdef HAS_DLL_BLOCKLIST
// NB: This must be extern, as this value is checked elsewhere
uint32_t gBlocklistInitFlags = eDllBlocklistInitFlagDefault;
#endif

int main(int argc, char* argv[], char* envp[]) {
#if defined(MOZ_ENABLE_FORKSERVER)
  if (strcmp(argv[argc - 1], "forkserver") == 0) {
    nsresult rv = InitXPCOMGlue(LibLoadingStrategy::NoReadAhead);
    if (NS_FAILED(rv)) {
      return 255;
    }

    // Run a fork server in this process, single thread.  When it
    // returns, it means the fork server have been stopped or a new
    // content process is created.
    //
    // For the later case, XRE_ForkServer() will return false, running
    // in a content process just forked from the fork server process.
    // argc & argv will be updated with the values passing from the
    // chrome process.  With the new values, this function
    // continues the reset of the code acting as a content process.
    if (gBootstrap->XRE_ForkServer(&argc, &argv)) {
      // Return from the fork server in the fork server process.
      // Stop the fork server.
      gBootstrap->NS_LogTerm();
      return 0;
    }
    // In a content process forked from the fork server.
    // Start acting as a content process.
  }
#endif

  mozilla::TimeStamp start = mozilla::TimeStamp::Now();

  AUTO_BASE_PROFILER_INIT;
  AUTO_BASE_PROFILER_LABEL("nsBrowserApp main", OTHER);

#ifdef MOZ_BROWSER_CAN_BE_CONTENTPROC
  // We are launching as a content process, delegate to the appropriate
  // main
  if (argc > 1 && IsArg(argv[1], "contentproc")) {
#  ifdef HAS_DLL_BLOCKLIST
    DllBlocklist_Initialize(gBlocklistInitFlags |
                            eDllBlocklistInitFlagIsChildProcess);
#  endif
#  if defined(XP_WIN) && defined(MOZ_SANDBOX)
    // We need to initialize the sandbox TargetServices before InitXPCOMGlue
    // because we might need the sandbox broker to give access to some files.
    if (IsSandboxedProcess() && !sandboxing::GetInitializedTargetServices()) {
      Output("Failed to initialize the sandbox target services.");
      return 255;
    }
#  endif

    nsresult rv = InitXPCOMGlue(LibLoadingStrategy::NoReadAhead);
    if (NS_FAILED(rv)) {
      return 255;
    }

    int result = content_process_main(gBootstrap.get(), argc, argv);

#  if defined(DEBUG) && defined(HAS_DLL_BLOCKLIST)
    DllBlocklist_Shutdown();
#  endif

    // InitXPCOMGlue calls NS_LogInit, so we need to balance it here.
    gBootstrap->NS_LogTerm();

    return result;
  }
#endif

#ifdef HAS_DLL_BLOCKLIST
  DllBlocklist_Initialize(gBlocklistInitFlags);
#endif

#if defined(XP_WIN)
  // Once the browser process hits the main function, we no longer need
  // a writable section handle because all dependent modules have been
  // loaded.
  mozilla::freestanding::gSharedSection.ConvertToReadOnly();
  ::RtlRunOnceInitialize(&mozilla::freestanding::gK32ExportsResolveOnce);

  mozilla::CreateAndStorePreXULSkeletonUI(GetModuleHandle(nullptr), argc, argv);
#endif

  nsresult rv = InitXPCOMGlue(LibLoadingStrategy::ReadAhead);
  if (NS_FAILED(rv)) {
    return 255;
  }

  gBootstrap->XRE_StartupTimelineRecord(mozilla::StartupTimeline::START, start);

#ifdef MOZ_BROWSER_CAN_BE_CONTENTPROC
  gBootstrap->XRE_EnableSameExecutableForContentProc();
#endif

  int result = do_main(argc, argv, envp);

  gBootstrap->NS_LogTerm();

#if defined(DEBUG) && defined(HAS_DLL_BLOCKLIST)
  DllBlocklist_Shutdown();
#endif

#ifdef XP_MACOSX
  // Allow writes again. While we would like to catch writes from static
  // destructors to allow early exits to use _exit, we know that there is
  // at least one such write that we don't control (see bug 826029). For
  // now we enable writes again and early exits will have to use exit instead
  // of _exit.
  gBootstrap->XRE_StopLateWriteChecks();
#endif

  gBootstrap.reset();

  return result;
}
