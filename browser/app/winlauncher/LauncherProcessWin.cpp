/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "LauncherProcessWin.h"

#include <string.h>

#include "mozilla/Attributes.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "mozilla/glue/Debug.h"
#include "mozilla/GeckoArgs.h"
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
#include "../BrowserDefines.h"

#if defined(MOZ_LAUNCHER_PROCESS)
#  include "mozilla/LauncherRegistryInfo.h"
#  include "SameBinary.h"
#endif  // defined(MOZ_LAUNCHER_PROCESS)

#if defined(MOZ_SANDBOX)
#  include "mozilla/sandboxing/SandboxInitialization.h"
#endif

namespace mozilla {
// "const" because nothing in this process modifies it.
// "volatile" because something in another process may.
const volatile DeelevationStatus gDeelevationStatus =
    DeelevationStatus::DefaultStaticValue;
}  // namespace mozilla

/**
 * At this point the child process has been created in a suspended state. Any
 * additional startup work (eg, blocklist setup) should go here.
 *
 * @return Ok if browser startup should proceed
 */
static mozilla::LauncherVoidResult PostCreationSetup(
    const wchar_t* aFullImagePath, HANDLE aChildProcess,
    HANDLE aChildMainThread, mozilla::DeelevationStatus aDStatus,
    const bool aIsSafeMode, const bool aDisableDynamicBlocklist,
    mozilla::Maybe<std::wstring> aBlocklistFileName) {
  /* scope for txManager */ {
    mozilla::nt::CrossExecTransferManager txManager(aChildProcess);
    if (!txManager) {
      return LAUNCHER_ERROR_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
    }

    using mozilla::gDeelevationStatus;

    void* targetAddress = (LPVOID)&gDeelevationStatus;

    auto const guard = txManager.Protect(
        targetAddress, sizeof(gDeelevationStatus), PAGE_READWRITE);

    mozilla::LauncherVoidResult result =
        txManager.Transfer(targetAddress, &aDStatus, sizeof(aDStatus));
    if (result.isErr()) {
      return result;
    }
  }

  return mozilla::InitializeDllBlocklistOOPFromLauncher(
      aFullImagePath, aChildProcess, aDisableDynamicBlocklist,
      aBlocklistFileName);
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

  // Set JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK to put only browser process
  // into a job without putting children of browser process into the job.
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {};
  jobInfo.BasicLimitInformation.LimitFlags =
      JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
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

  if (mozilla::CheckArg(aArgc, aArgv, "wait-for-browser", nullptr,
                        mozilla::CheckArgFlag::RemoveArg) ==
          mozilla::ARG_FOUND ||
      mozilla::CheckArg(aArgc, aArgv, "marionette", nullptr,
                        mozilla::CheckArgFlag::None) == mozilla::ARG_FOUND ||
      mozilla::CheckArg(aArgc, aArgv, "backgroundtask", nullptr,
                        mozilla::CheckArgFlag::None) == mozilla::ARG_FOUND ||
      mozilla::CheckArg(aArgc, aArgv, "headless", nullptr,
                        mozilla::CheckArgFlag::None) == mozilla::ARG_FOUND ||
      mozilla::CheckArg(aArgc, aArgv, "remote-debugging-port", nullptr,
                        mozilla::CheckArgFlag::None) == mozilla::ARG_FOUND ||
      mozilla::EnvHasValue("MOZ_AUTOMATION") ||
      mozilla::EnvHasValue("MOZ_HEADLESS")) {
    result |= mozilla::LauncherFlags::eWaitForBrowser;
  }

  if (mozilla::CheckArg(aArgc, aArgv, "no-deelevate") == mozilla::ARG_FOUND) {
    result |= mozilla::LauncherFlags::eNoDeelevate;
  }

  if (mozilla::CheckArg(aArgc, aArgv, ATTEMPTING_DEELEVATION_FLAG) ==
      mozilla::ARG_FOUND) {
    result |= mozilla::LauncherFlags::eDeelevating;
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

  result |=
      mozilla::CheckArg(argc, argv, "launcher", nullptr,
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
      mozilla::CheckArg(argc, argv, "force-launcher", nullptr,
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
  EnsureBrowserCommandlineSafe(argc, argv);

  SetLauncherErrorAppData(aAppData);

  if (CheckArg(argc, argv, "log-launcher-error", nullptr,
               mozilla::CheckArgFlag::RemoveArg) == ARG_FOUND) {
    SetLauncherErrorForceEventLog();
  }

  // return fast when we're a child process.
  // (The remainder of this function has some side effects that are
  // undesirable for content processes)
  if (mozilla::CheckArg(argc, argv, "contentproc", nullptr,
                        mozilla::CheckArgFlag::None) == mozilla::ARG_FOUND) {
    // A child process should not instantiate LauncherRegistryInfo.
    return Nothing();
  }

#if defined(MOZ_LAUNCHER_PROCESS)
  LauncherRegistryInfo regInfo;
  Maybe<bool> runAsLauncher = RunAsLauncherProcess(regInfo, argc, argv);
  LauncherResult<std::wstring> blocklistFileNameResult =
      regInfo.GetBlocklistFileName();
  Maybe<std::wstring> blocklistFileName =
      blocklistFileNameResult.isOk() ? Some(blocklistFileNameResult.unwrap())
                                     : Nothing();
#else
  Maybe<bool> runAsLauncher = RunAsLauncherProcess(argc, argv);
  Maybe<std::wstring> blocklistFileName = Nothing();
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

#if defined(MOZ_SANDBOX)
  // Ensure the relevant mitigations are enforced.
  mozilla::sandboxing::ApplyParentProcessMitigations();
#endif

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

  // Distill deelevation status, and/or attempt to perform launcher deelevation
  // via an indirect relaunch.
  DeelevationStatus deelevationStatus = DeelevationStatus::Unknown;
  if (mediumIlToken.get()) {
    // Rather than indirectly relaunch the launcher, we'll attempt to directly
    // launch the main process with a reduced-privilege security token.
    deelevationStatus = DeelevationStatus::PartiallyDeelevated;
  } else if (elevationState.unwrap() == ElevationState::eElevated) {
    if (flags & LauncherFlags::eWaitForBrowser) {
      // An indirect relaunch won't provide a process-handle to block on,
      // so we have to continue onwards with this process.
      deelevationStatus = DeelevationStatus::DeelevationProhibited;
    } else if (flags & LauncherFlags::eNoDeelevate) {
      // Our invoker (hopefully, the user) has explicitly requested that the
      // launcher not deelevate itself.
      deelevationStatus = DeelevationStatus::DeelevationProhibited;
    } else if (flags & LauncherFlags::eDeelevating) {
      // We've already tried to deelevate, to no effect. Continue onward.
      deelevationStatus = DeelevationStatus::UnsuccessfullyDeelevated;
    } else {
      // Otherwise, attempt to relaunch the launcher process itself via the
      // shell, which hopefully will not be elevated. (But see bug 1733821.)
      LauncherVoidResult launchedUnelevated = LaunchUnelevated(argc, argv);
      if (launchedUnelevated.isErr()) {
        // On failure, don't even try for a launcher process. Continue onwards
        // in this one. (TODO: why? This isn't technically fatal...)
        HandleLauncherError(launchedUnelevated);
        return Nothing();
      }
      // Otherwise, tell our caller to exit with a success code.
      return Some(0);
    }
  } else if (elevationState.unwrap() == ElevationState::eNormalUser) {
    if (flags & LauncherFlags::eDeelevating) {
      // Deelevation appears to have been successful!
      deelevationStatus = DeelevationStatus::SuccessfullyDeelevated;
    } else {
      // We haven't done anything and we don't need to.
      deelevationStatus = DeelevationStatus::StartedUnprivileged;
    }
  } else {
    // Some other elevation state with no medium-integrity token.
    // (This should probably not happen.)
    deelevationStatus = DeelevationStatus::Unknown;
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
  STARTUPINFOW currentStartupInfo = {.cb = sizeof(STARTUPINFOW)};
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

  bool disableDynamicBlocklist = IsDynamicBlocklistDisabled(
      isSafeMode.value(),
      mozilla::CheckArg(
          argc, argv, mozilla::geckoargs::sDisableDynamicDllBlocklist.sMatch,
          nullptr, mozilla::CheckArgFlag::None) == mozilla::ARG_FOUND);
  LauncherVoidResult setupResult = PostCreationSetup(
      argv[0], process.get(), mainThread.get(), deelevationStatus,
      isSafeMode.value(), disableDynamicBlocklist, blocklistFileName);
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
