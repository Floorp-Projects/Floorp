/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#define MOZ_USE_LAUNCHER_ERROR

#include "LauncherProcessWin.h"

#include <string.h>

#include "mozilla/Attributes.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "mozilla/glue/Debug.h"
#include "mozilla/Maybe.h"
#include "mozilla/SafeMode.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsConsole.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsWindowsHelpers.h"

#include <windows.h>
#include <processthreadsapi.h>

#include "DllBlocklistInit.h"
#include "ErrorHandler.h"
#include "LaunchUnelevated.h"
#include "ProcThreadAttributes.h"

#if defined(MOZ_LAUNCHER_PROCESS)
#  include "mozilla/LauncherRegistryInfo.h"
#  include "SameBinary.h"
#endif  // defined(MOZ_LAUNCHER_PROCESS)

/**
 * At this point the child process has been created in a suspended state. Any
 * additional startup work (eg, blocklist setup) should go here.
 *
 * @return Ok if browser startup should proceed
 */
static mozilla::LauncherVoidResult PostCreationSetup(
    const wchar_t* aFullImagePath, HANDLE aChildProcess,
    HANDLE aChildMainThread, const bool aIsSafeMode) {
  return mozilla::InitializeDllBlocklistOOPFromLauncher(aFullImagePath,
                                                        aChildProcess);
}

/**
 * Create a new Job object and assign |aProcess| to it.  If something fails
 * in this function, we return nullptr but continue without recording
 * a launcher failure because it's not a critical problem to launch
 * the browser process.
 */
static nsReturnRef<HANDLE> CreateJobAndAssignProcess(HANDLE aProcess) {
  nsAutoHandle empty;
  nsAutoHandle job(::CreateJobObjectW(nullptr, nullptr));

  // Set JOB_OBJECT_LIMIT_BREAKAWAY_OK to allow the browser process
  // to put child processes into a job on Win7, which does not support
  // nested jobs.  See CanUseJob() in sandboxBroker.cpp.
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {};
  jobInfo.BasicLimitInformation.LimitFlags =
      JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
  if (!::SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                 &jobInfo, sizeof(jobInfo))) {
    return empty.out();
  }

  if (!::AssignProcessToJobObject(job.get(), aProcess)) {
    return empty.out();
  }

  return job.out();
}

#if !defined( \
    PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON)
#  define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON \
    (0x00000001ULL << 60)
#endif  // !defined(PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON)

#if !defined(PROCESS_CREATION_MITIGATION_POLICY_CONTROL_FLOW_GUARD_ALWAYS_OFF)
#  define PROCESS_CREATION_MITIGATION_POLICY_CONTROL_FLOW_GUARD_ALWAYS_OFF \
    (0x00000002ULL << 40)
#endif  // !defined(PROCESS_CREATION_MITIGATION_POLICY_CONTROL_FLOW_GUARD_ALWAYS_OFF)

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

#if defined(_M_ARM64)
  // Disable CFG on older versions of ARM64 Windows to avoid a crash in COM.
  if (!mozilla::IsWin10Sep2018UpdateOrLater()) {
    aAttrs.AddMitigationPolicy(
        PROCESS_CREATION_MITIGATION_POLICY_CONTROL_FLOW_GUARD_ALWAYS_OFF);
  }
#endif  // defined(_M_ARM64)
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
      mozilla::CheckArg(aArgc, aArgv, L"backgroundtask",
                        static_cast<const wchar_t**>(nullptr),
                        mozilla::CheckArgFlag::None) == mozilla::ARG_FOUND ||
      mozilla::CheckArg(aArgc, aArgv, L"headless",
                        static_cast<const wchar_t**>(nullptr),
                        mozilla::CheckArgFlag::None) == mozilla::ARG_FOUND ||
      mozilla::CheckArg(aArgc, aArgv, L"remote-debugging-port",
                        static_cast<const wchar_t**>(nullptr),
                        mozilla::CheckArgFlag::None) == mozilla::ARG_FOUND ||
      mozilla::EnvHasValue("MOZ_AUTOMATION") ||
      mozilla::EnvHasValue("MOZ_HEADLESS")) {
    result |= mozilla::LauncherFlags::eWaitForBrowser;
  }

  if (mozilla::CheckArg(aArgc, aArgv, L"no-deelevate") == mozilla::ARG_FOUND) {
    result |= mozilla::LauncherFlags::eNoDeelevate;
  }

  return result;
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

static bool DoLauncherProcessChecks(int& argc, wchar_t** argv) {
  // NB: We run all tests in this function instead of returning early in order
  // to ensure that all side effects take place, such as clearing environment
  // variables.
  bool result = false;

#if defined(MOZ_LAUNCHER_PROCESS)
  // We still prefer to compare file ids.  Comparing NT paths i.e. passing
  // CompareNtPathsOnly to IsSameBinaryAsParentProcess is much faster, but
  // we're not 100% sure that NT path comparison perfectly prevents the
  // launching loop of the launcher process.
  mozilla::LauncherResult<bool> isSame = mozilla::IsSameBinaryAsParentProcess();
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

  result |= mozilla::CheckArg(
                argc, argv, L"launcher", static_cast<const wchar_t**>(nullptr),
                mozilla::CheckArgFlag::RemoveArg) == mozilla::ARG_FOUND;

  return result;
}

#if defined(MOZ_LAUNCHER_PROCESS)
static mozilla::Maybe<bool> RunAsLauncherProcess(
    mozilla::LauncherRegistryInfo& aRegInfo, int& argc, wchar_t** argv) {
#else
static mozilla::Maybe<bool> RunAsLauncherProcess(int& argc, wchar_t** argv) {
#endif  // defined(MOZ_LAUNCHER_PROCESS)
  bool runAsLauncher = DoLauncherProcessChecks(argc, argv);

#if defined(MOZ_LAUNCHER_PROCESS)
  bool forceLauncher =
      runAsLauncher &&
      mozilla::CheckArg(argc, argv, L"force-launcher",
                        static_cast<const wchar_t**>(nullptr),
                        mozilla::CheckArgFlag::RemoveArg) == mozilla::ARG_FOUND;

  mozilla::LauncherRegistryInfo::ProcessType desiredType =
      runAsLauncher ? mozilla::LauncherRegistryInfo::ProcessType::Launcher
                    : mozilla::LauncherRegistryInfo::ProcessType::Browser;

  mozilla::LauncherRegistryInfo::CheckOption checkOption =
      forceLauncher ? mozilla::LauncherRegistryInfo::CheckOption::Force
                    : mozilla::LauncherRegistryInfo::CheckOption::Default;

  mozilla::LauncherResult<mozilla::LauncherRegistryInfo::ProcessType>
      runAsType = aRegInfo.Check(desiredType, checkOption);

  if (runAsType.isErr()) {
    mozilla::HandleLauncherError(runAsType);
    return mozilla::Nothing();
  }

  runAsLauncher = runAsType.unwrap() ==
                  mozilla::LauncherRegistryInfo::ProcessType::Launcher;
#endif  // defined(MOZ_LAUNCHER_PROCESS)

  if (!runAsLauncher) {
    // In this case, we will be proceeding to run as the browser.
    // We should check MOZ_DEBUG_BROWSER_* env vars.
    MaybeBreakForBrowserDebugging();
  }

  return mozilla::Some(runAsLauncher);
}

namespace mozilla {

Maybe<int> LauncherMain(int& argc, wchar_t* argv[],
                        const StaticXREAppData& aAppData) {
  // Note: keep in sync with nsBrowserApp.
  const wchar_t* acceptableParams[] = {L"url", L"private-window", nullptr};
  EnsureCommandlineSafe(argc, argv, acceptableParams);

  SetLauncherErrorAppData(aAppData);

  if (CheckArg(argc, argv, L"log-launcher-error",
               static_cast<const wchar_t**>(nullptr),
               mozilla::CheckArgFlag::RemoveArg) == ARG_FOUND) {
    SetLauncherErrorForceEventLog();
  }

  // return fast when we're a child process.
  // (The remainder of this function has some side effects that are
  // undesirable for content processes)
  if (mozilla::CheckArg(argc, argv, L"contentproc",
                        static_cast<const wchar_t**>(nullptr),
                        mozilla::CheckArgFlag::None) == mozilla::ARG_FOUND) {
    // A child process should not instantiate LauncherRegistryInfo.
    return Nothing();
  }

#if defined(MOZ_LAUNCHER_PROCESS)
  LauncherRegistryInfo regInfo;
  Maybe<bool> runAsLauncher = RunAsLauncherProcess(regInfo, argc, argv);
#else
  Maybe<bool> runAsLauncher = RunAsLauncherProcess(argc, argv);
#endif  // defined(MOZ_LAUNCHER_PROCESS)
  if (!runAsLauncher || !runAsLauncher.value()) {
#if defined(MOZ_LAUNCHER_PROCESS)
    // Update the registry as Browser
    LauncherVoidResult commitResult = regInfo.Commit();
    if (commitResult.isErr()) {
      mozilla::HandleLauncherError(commitResult);
    }
#endif  // defined(MOZ_LAUNCHER_PROCESS)
    return Nothing();
  }

  // Make sure that the launcher process itself has image load policies set
  if (IsWin10AnniversaryUpdateOrLater()) {
    static const StaticDynamicallyLinkedFunctionPtr<
        decltype(&SetProcessMitigationPolicy)>
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

  mozilla::UseParentConsole();

  if (!SetArgv0ToFullBinaryPath(argv)) {
    HandleLauncherError(LAUNCHER_ERROR_GENERIC());
    return Nothing();
  }

  LauncherFlags flags = ProcessCmdLine(argc, argv);

  nsAutoHandle mediumIlToken;
  LauncherResult<ElevationState> elevationState =
      GetElevationState(argv[0], flags, mediumIlToken);
  if (elevationState.isErr()) {
    HandleLauncherError(elevationState);
    return Nothing();
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
      return Nothing();
    }

    return Some(0);
  }

#if defined(MOZ_LAUNCHER_PROCESS)
  // Update the registry as Launcher
  LauncherVoidResult commitResult = regInfo.Commit();
  if (commitResult.isErr()) {
    mozilla::HandleLauncherError(commitResult);
    return Nothing();
  }
#endif  // defined(MOZ_LAUNCHER_PROCESS)

  // Now proceed with setting up the parameters for process creation
  UniquePtr<wchar_t[]> cmdLine(MakeCommandLine(argc, argv));
  if (!cmdLine) {
    HandleLauncherError(LAUNCHER_ERROR_GENERIC());
    return Nothing();
  }

  const Maybe<bool> isSafeMode =
      IsSafeModeRequested(argc, argv, SafeModeFlag::NoKeyPressCheck);
  if (!isSafeMode) {
    HandleLauncherError(LAUNCHER_ERROR_FROM_WIN32(ERROR_INVALID_PARAMETER));
    return Nothing();
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
    return Nothing();
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

  // Pass on the path of the shortcut used to launch this process, if any.
  STARTUPINFOW currentStartupInfo;
  GetStartupInfoW(&currentStartupInfo);
  if ((currentStartupInfo.dwFlags & STARTF_TITLEISLINKNAME) &&
      currentStartupInfo.lpTitle) {
    siex.StartupInfo.dwFlags |= STARTF_TITLEISLINKNAME;
    siex.StartupInfo.lpTitle = currentStartupInfo.lpTitle;
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
    return Nothing();
  }

  nsAutoHandle process(pi.hProcess);
  nsAutoHandle mainThread(pi.hThread);

  nsAutoHandle job;
  if (flags & LauncherFlags::eWaitForBrowser) {
    job = CreateJobAndAssignProcess(process.get());
  }

  LauncherVoidResult setupResult = PostCreationSetup(
      argv[0], process.get(), mainThread.get(), isSafeMode.value());
  if (setupResult.isErr()) {
    HandleLauncherError(setupResult);
    ::TerminateProcess(process.get(), 1);
    return Nothing();
  }

  if (::ResumeThread(mainThread.get()) == static_cast<DWORD>(-1)) {
    HandleLauncherError(LAUNCHER_ERROR_FROM_LAST());
    ::TerminateProcess(process.get(), 1);
    return Nothing();
  }

  if (flags & LauncherFlags::eWaitForBrowser) {
    DWORD exitCode;
    if (::WaitForSingleObject(process.get(), INFINITE) == WAIT_OBJECT_0 &&
        ::GetExitCodeProcess(process.get(), &exitCode)) {
      // Propagate the browser process's exit code as our exit code.
      return Some(static_cast<int>(exitCode));
    }
  } else {
    const DWORD timeout =
        ::IsDebuggerPresent() ? INFINITE : kWaitForInputIdleTimeoutMS;

    // Keep the current process around until the callback process has created
    // its message queue, to avoid the launched process's windows being forced
    // into the background.
    mozilla::WaitForInputIdle(process.get(), timeout);
  }

  return Some(0);
}

}  // namespace mozilla
