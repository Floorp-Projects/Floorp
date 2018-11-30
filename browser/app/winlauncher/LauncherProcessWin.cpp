/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "LauncherProcessWin.h"

#include <io.h>  // For printf_stderr
#include <string.h>

#include "mozilla/Attributes.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "mozilla/Maybe.h"
#include "mozilla/SafeMode.h"
#include "mozilla/Sprintf.h"  // For printf_stderr
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsWindowsHelpers.h"

#include <windows.h>
#include <processthreadsapi.h>

#include "DllBlocklistWin.h"
#include "ErrorHandler.h"
#include "LauncherResult.h"
#include "LaunchUnelevated.h"
#include "ProcThreadAttributes.h"

/**
 * At this point the child process has been created in a suspended state. Any
 * additional startup work (eg, blocklist setup) should go here.
 *
 * @return true if browser startup should proceed, otherwise false.
 */
static mozilla::LauncherVoidResult PostCreationSetup(HANDLE aChildProcess,
                                                     HANDLE aChildMainThread,
                                                     const bool aIsSafeMode) {
  // The launcher process's DLL blocking code is incompatible with ASAN because
  // it is able to execute before ASAN itself has even initialized.
  // Also, the AArch64 build doesn't yet have a working interceptor.
#if defined(MOZ_ASAN) || defined(_M_ARM64)
  return mozilla::Ok();
#else
  return mozilla::InitializeDllBlocklistOOP(aChildProcess);
#endif  // defined(MOZ_ASAN) || defined(_M_ARM64)
}

#if !defined( \
    PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON)
#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON \
  (0x00000001ULL << 60)
#endif  // !defined(PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON)

#if (_WIN32_WINNT < 0x0602)
BOOL WINAPI
SetProcessMitigationPolicy(PROCESS_MITIGATION_POLICY aMitigationPolicy,
                           PVOID aBuffer, SIZE_T aBufferLen);
#endif  // (_WIN32_WINNT >= 0x0602)

/**
 * Any mitigation policies that should be set on the browser process should go
 * here.
 */
static void SetMitigationPolicies(mozilla::ProcThreadAttributes& aAttrs,
                                  const bool aIsSafeMode) {
  if (mozilla::IsWin10AnniversaryUpdateOrLater()) {
    aAttrs.AddMitigationPolicy(
        PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON);
  }
}

static mozilla::LauncherFlags ProcessCmdLine(int& aArgc, wchar_t* aArgv[]) {
  mozilla::LauncherFlags result = mozilla::LauncherFlags::eNone;

  if (mozilla::CheckArg(aArgc, aArgv, L"wait-for-browser",
                        static_cast<const wchar_t**>(nullptr),
                        mozilla::CheckArgFlag::RemoveArg) ==
          mozilla::ARG_FOUND ||
      mozilla::CheckArg(aArgc, aArgv, L"marionette",
                        static_cast<const wchar_t**>(nullptr),
                        mozilla::CheckArgFlag::None) == mozilla::ARG_FOUND ||
      mozilla::CheckArg(aArgc, aArgv, L"headless",
                        static_cast<const wchar_t**>(nullptr),
                        mozilla::CheckArgFlag::None) == mozilla::ARG_FOUND ||
      mozilla::EnvHasValue("MOZ_AUTOMATION") ||
      mozilla::EnvHasValue("MOZ_HEADLESS")) {
    result |= mozilla::LauncherFlags::eWaitForBrowser;
  }

  if (mozilla::CheckArg(
          aArgc, aArgv, L"no-deelevate", static_cast<const wchar_t**>(nullptr),
          mozilla::CheckArgFlag::CheckOSInt |
              mozilla::CheckArgFlag::RemoveArg) == mozilla::ARG_FOUND) {
    result |= mozilla::LauncherFlags::eNoDeelevate;
  }

  return result;
}

// Duplicated from xpcom glue. Ideally this should be shared.
static void printf_stderr(const char* fmt, ...) {
  if (IsDebuggerPresent()) {
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    VsprintfLiteral(buf, fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
  }

  FILE* fp = _fdopen(_dup(2), "a");
  if (!fp) return;

  va_list args;
  va_start(args, fmt);
  vfprintf(fp, fmt, args);
  va_end(args);

  fclose(fp);
}

static void MaybeBreakForBrowserDebugging() {
  if (mozilla::EnvHasValue("MOZ_DEBUG_BROWSER_PROCESS")) {
    ::DebugBreak();
    return;
  }

  const wchar_t* pauseLenS = _wgetenv(L"MOZ_DEBUG_BROWSER_PAUSE");
  if (!pauseLenS || !(*pauseLenS)) {
    return;
  }

  DWORD pauseLenMs = wcstoul(pauseLenS, nullptr, 10) * 1000;
  printf_stderr("\n\nBROWSERBROWSERBROWSERBROWSER\n  debug me @ %lu\n\n",
                ::GetCurrentProcessId());
  ::Sleep(pauseLenMs);
}

#if defined(MOZ_LAUNCHER_PROCESS)

static mozilla::LauncherResult<bool> IsSameBinaryAsParentProcess() {
  mozilla::LauncherResult<DWORD> parentPid = mozilla::nt::GetParentProcessId();
  if (parentPid.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(parentPid);
  }

  nsAutoHandle parentProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                           FALSE, parentPid.unwrap()));
  if (!parentProcess.get()) {
    DWORD err = ::GetLastError();
    if (err == ERROR_INVALID_PARAMETER) {
      // The process identified by parentPid has already exited. This is a
      // common case when the parent process is not Firefox, thus we should
      // return false instead of erroring out.
      return false;
    }

    return LAUNCHER_ERROR_FROM_WIN32(err);
  }

  WCHAR parentExe[MAX_PATH + 1] = {};
  DWORD parentExeLen = mozilla::ArrayLength(parentExe);
  if (!::QueryFullProcessImageNameW(parentProcess.get(), PROCESS_NAME_NATIVE,
                                    parentExe, &parentExeLen)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  WCHAR ourExe[MAX_PATH + 1] = {};
  DWORD ourExeOk =
      ::GetModuleFileNameW(nullptr, ourExe, mozilla::ArrayLength(ourExe));
  if (!ourExeOk || ourExeOk == mozilla::ArrayLength(ourExe)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  mozilla::WindowsErrorResult<bool> isSame =
      mozilla::DoPathsPointToIdenticalFile(parentExe, ourExe,
                                           mozilla::PathType::eNtPath);
  if (isSame.isErr()) {
    return LAUNCHER_ERROR_FROM_MOZ_WINDOWS_ERROR(isSame.unwrapErr());
  }

  return isSame.unwrap();
}

#endif  // defined(MOZ_LAUNCHER_PROCESS)

namespace mozilla {

bool RunAsLauncherProcess(int& argc, wchar_t** argv) {
  // NB: We run all tests in this function instead of returning early in order
  // to ensure that all side effects take place, such as clearing environment
  // variables.
  bool result = false;

#if defined(MOZ_LAUNCHER_PROCESS)
  LauncherResult<bool> isSame = IsSameBinaryAsParentProcess();
  if (isSame.isOk()) {
    result = !isSame.unwrap();
  } else {
    HandleLauncherError(isSame.unwrapErr());
  }
#endif  // defined(MOZ_LAUNCHER_PROCESS)

  if (mozilla::EnvHasValue("MOZ_LAUNCHER_PROCESS")) {
    mozilla::SaveToEnv("MOZ_LAUNCHER_PROCESS=");
    result = true;
  }

  result |=
      CheckArg(argc, argv, L"launcher", static_cast<const wchar_t**>(nullptr),
               CheckArgFlag::RemoveArg) == ARG_FOUND;

  if (!result) {
    // In this case, we will be proceeding to run as the browser.
    // We should check MOZ_DEBUG_BROWSER_* env vars.
    MaybeBreakForBrowserDebugging();
  }

  return result;
}

int LauncherMain(int argc, wchar_t* argv[]) {
  // Make sure that the launcher process itself has image load policies set
  if (IsWin10AnniversaryUpdateOrLater()) {
    const DynamicallyLinkedFunctionPtr<decltype(&SetProcessMitigationPolicy)>
        pSetProcessMitigationPolicy(L"kernel32.dll",
                                    "SetProcessMitigationPolicy");
    if (pSetProcessMitigationPolicy) {
      PROCESS_MITIGATION_IMAGE_LOAD_POLICY imgLoadPol = {};
      imgLoadPol.PreferSystem32Images = 1;

      DebugOnly<BOOL> setOk = pSetProcessMitigationPolicy(
          ProcessImageLoadPolicy, &imgLoadPol, sizeof(imgLoadPol));
      MOZ_ASSERT(setOk);
    }
  }

  if (!SetArgv0ToFullBinaryPath(argv)) {
    HandleLauncherError(LAUNCHER_ERROR_GENERIC());
    return 1;
  }

  LauncherFlags flags = ProcessCmdLine(argc, argv);

  nsAutoHandle mediumIlToken;
  LauncherResult<ElevationState> elevationState =
      GetElevationState(flags, mediumIlToken);
  if (elevationState.isErr()) {
    HandleLauncherError(elevationState);
    return 1;
  }

  // If we're elevated, we should relaunch ourselves as a normal user.
  // Note that we only call LaunchUnelevated when we don't need to wait for the
  // browser process.
  if (elevationState.unwrap() == ElevationState::eElevated &&
      !(flags &
        (LauncherFlags::eWaitForBrowser | LauncherFlags::eNoDeelevate)) &&
      !mediumIlToken.get()) {
    LauncherVoidResult launchedUnelevated = LaunchUnelevated(argc, argv);
    bool failed = launchedUnelevated.isErr();
    if (failed) {
      HandleLauncherError(launchedUnelevated);
    }

    return failed;
  }

  // Now proceed with setting up the parameters for process creation
  UniquePtr<wchar_t[]> cmdLine(MakeCommandLine(argc, argv));
  if (!cmdLine) {
    HandleLauncherError(LAUNCHER_ERROR_GENERIC());
    return 1;
  }

  const Maybe<bool> isSafeMode =
      IsSafeModeRequested(argc, argv, SafeModeFlag::NoKeyPressCheck);
  if (!isSafeMode) {
    HandleLauncherError(LAUNCHER_ERROR_FROM_WIN32(ERROR_INVALID_PARAMETER));
    return 1;
  }

  ProcThreadAttributes attrs;
  SetMitigationPolicies(attrs, isSafeMode.value());

  HANDLE stdHandles[] = {::GetStdHandle(STD_INPUT_HANDLE),
                         ::GetStdHandle(STD_OUTPUT_HANDLE),
                         ::GetStdHandle(STD_ERROR_HANDLE)};

  attrs.AddInheritableHandles(stdHandles);

  DWORD creationFlags = CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT;

  STARTUPINFOEXW siex;
  LauncherResult<bool> attrsOk = attrs.AssignTo(siex);
  if (attrsOk.isErr()) {
    HandleLauncherError(attrsOk);
    return 1;
  }

  BOOL inheritHandles = FALSE;

  if (attrsOk.unwrap()) {
    creationFlags |= EXTENDED_STARTUPINFO_PRESENT;

    if (attrs.HasInheritableHandles()) {
      siex.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
      siex.StartupInfo.hStdInput = stdHandles[0];
      siex.StartupInfo.hStdOutput = stdHandles[1];
      siex.StartupInfo.hStdError = stdHandles[2];

      // Since attrsOk == true, we have successfully set the handle inheritance
      // whitelist policy, so only the handles added to attrs will be inherited.
      inheritHandles = TRUE;
    }
  }

  PROCESS_INFORMATION pi = {};
  BOOL createOk;

  if (mediumIlToken.get()) {
    createOk =
        ::CreateProcessAsUserW(mediumIlToken.get(), argv[0], cmdLine.get(),
                               nullptr, nullptr, inheritHandles, creationFlags,
                               nullptr, nullptr, &siex.StartupInfo, &pi);
  } else {
    createOk = ::CreateProcessW(argv[0], cmdLine.get(), nullptr, nullptr,
                                inheritHandles, creationFlags, nullptr, nullptr,
                                &siex.StartupInfo, &pi);
  }

  if (!createOk) {
    HandleLauncherError(LAUNCHER_ERROR_FROM_LAST());
    return 1;
  }

  nsAutoHandle process(pi.hProcess);
  nsAutoHandle mainThread(pi.hThread);

  LauncherVoidResult setupResult =
      PostCreationSetup(process.get(), mainThread.get(), isSafeMode.value());
  if (setupResult.isErr()) {
    HandleLauncherError(setupResult);
    ::TerminateProcess(process.get(), 1);
    return 1;
  }

  if (::ResumeThread(mainThread.get()) == static_cast<DWORD>(-1)) {
    HandleLauncherError(LAUNCHER_ERROR_FROM_LAST());
    ::TerminateProcess(process.get(), 1);
    return 1;
  }

  if (flags & LauncherFlags::eWaitForBrowser) {
    DWORD exitCode;
    if (::WaitForSingleObject(process.get(), INFINITE) == WAIT_OBJECT_0 &&
        ::GetExitCodeProcess(process.get(), &exitCode)) {
      // Propagate the browser process's exit code as our exit code.
      return static_cast<int>(exitCode);
    }
  } else {
    const DWORD timeout =
        ::IsDebuggerPresent() ? INFINITE : kWaitForInputIdleTimeoutMS;

    // Keep the current process around until the callback process has created
    // its message queue, to avoid the launched process's windows being forced
    // into the background.
    mozilla::WaitForInputIdle(process.get(), timeout);
  }

  return 0;
}

}  // namespace mozilla
