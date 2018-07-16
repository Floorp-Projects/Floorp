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
#include "mozilla/Maybe.h"
#include "mozilla/SafeMode.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsWindowsHelpers.h"

#include <windows.h>
#include <processthreadsapi.h>

#include "DllBlocklistWin.h"
#include "LaunchUnelevated.h"
#include "ProcThreadAttributes.h"

/**
 * At this point the child process has been created in a suspended state. Any
 * additional startup work (eg, blocklist setup) should go here.
 *
 * @return true if browser startup should proceed, otherwise false.
 */
static bool
PostCreationSetup(HANDLE aChildProcess, HANDLE aChildMainThread,
                  const bool aIsSafeMode)
{
  return mozilla::InitializeDllBlocklistOOP(aChildProcess);
}

#if !defined(PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON)
# define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON (0x00000001ui64 << 60)
#endif // !defined(PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON)

#if (_WIN32_WINNT < 0x0602)
BOOL WINAPI
SetProcessMitigationPolicy(PROCESS_MITIGATION_POLICY aMitigationPolicy,
                           PVOID aBuffer, SIZE_T aBufferLen);
#endif // (_WIN32_WINNT >= 0x0602)

/**
 * Any mitigation policies that should be set on the browser process should go
 * here.
 */
static void
SetMitigationPolicies(mozilla::ProcThreadAttributes& aAttrs, const bool aIsSafeMode)
{
  if (mozilla::IsWin10AnniversaryUpdateOrLater()) {
    aAttrs.AddMitigationPolicy(PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON);
  }
}

static void
ShowError(DWORD aError = ::GetLastError())
{
  if (aError == ERROR_SUCCESS) {
    return;
  }

  LPWSTR rawMsgBuf = nullptr;
  DWORD result = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                  FORMAT_MESSAGE_FROM_SYSTEM |
                                  FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                                  aError, 0, reinterpret_cast<LPWSTR>(&rawMsgBuf),
                                  0, nullptr);
  if (!result) {
    return;
  }

  ::MessageBoxW(nullptr, rawMsgBuf, L"Firefox", MB_OK | MB_ICONERROR);
  ::LocalFree(rawMsgBuf);
}

static mozilla::LauncherFlags
ProcessCmdLine(int& aArgc, wchar_t* aArgv[])
{
  mozilla::LauncherFlags result = mozilla::LauncherFlags::eNone;

  if (mozilla::CheckArg(aArgc, aArgv, L"wait-for-browser",
                        static_cast<const wchar_t**>(nullptr),
                        mozilla::CheckArgFlag::RemoveArg) == mozilla::ARG_FOUND ||
      mozilla::EnvHasValue("MOZ_AUTOMATION")) {
    result |= mozilla::LauncherFlags::eWaitForBrowser;
  }

  if (mozilla::CheckArg(aArgc, aArgv, L"no-deelevate",
                        static_cast<const wchar_t**>(nullptr),
                        mozilla::CheckArgFlag::CheckOSInt |
                        mozilla::CheckArgFlag::RemoveArg) == mozilla::ARG_FOUND) {
    result |= mozilla::LauncherFlags::eNoDeelevate;
  }

  return result;
}

#if defined(MOZ_LAUNCHER_PROCESS)

static mozilla::Maybe<bool>
IsSameBinaryAsParentProcess()
{
  mozilla::Maybe<DWORD> parentPid = mozilla::nt::GetParentProcessId();
  if (!parentPid) {
    return mozilla::Nothing();
  }

  nsAutoHandle parentProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                           FALSE, parentPid.value()));
  if (!parentProcess.get()) {
    return mozilla::Nothing();
  }

  WCHAR parentExe[MAX_PATH + 1] = {};
  DWORD parentExeLen = mozilla::ArrayLength(parentExe);
  if (!::QueryFullProcessImageNameW(parentProcess.get(), 0, parentExe,
                                    &parentExeLen)) {
    return mozilla::Nothing();
  }

  WCHAR ourExe[MAX_PATH + 1] = {};
  DWORD ourExeOk = ::GetModuleFileNameW(nullptr, ourExe,
                                        mozilla::ArrayLength(ourExe));
  if (!ourExeOk || ourExeOk == mozilla::ArrayLength(ourExe)) {
    return mozilla::Nothing();
  }

  bool isSame = parentExeLen == ourExeOk &&
                !_wcsnicmp(ourExe, parentExe, ourExeOk);
  return mozilla::Some(isSame);
}

#endif // defined(MOZ_LAUNCHER_PROCESS)

namespace mozilla {

bool
RunAsLauncherProcess(int& argc, wchar_t** argv)
{
#if defined(MOZ_LAUNCHER_PROCESS)
  Maybe<bool> isChildOfFirefox = IsSameBinaryAsParentProcess();
  if (!isChildOfFirefox) {
    return true;
  }

  return !isChildOfFirefox.value();
#else
  return CheckArg(argc, argv, L"launcher",
                  static_cast<const wchar_t**>(nullptr),
                  CheckArgFlag::CheckOSInt | CheckArgFlag::RemoveArg) == ARG_FOUND;
#endif // defined(MOZ_LAUNCHER_PROCESS)
}

int
LauncherMain(int argc, wchar_t* argv[])
{
  // Make sure that the launcher process itself has image load policies set
  if (IsWin10AnniversaryUpdateOrLater()) {
    const DynamicallyLinkedFunctionPtr<decltype(&SetProcessMitigationPolicy)>
      pSetProcessMitigationPolicy(L"kernel32.dll", "SetProcessMitigationPolicy");
    if (pSetProcessMitigationPolicy) {
      PROCESS_MITIGATION_IMAGE_LOAD_POLICY imgLoadPol = {};
      imgLoadPol.PreferSystem32Images = 1;

      DebugOnly<BOOL> setOk = pSetProcessMitigationPolicy(ProcessImageLoadPolicy,
                                                          &imgLoadPol,
                                                          sizeof(imgLoadPol));
      MOZ_ASSERT(setOk);
    }
  }

  if (!SetArgv0ToFullBinaryPath(argv)) {
    ShowError();
    return 1;
  }

  LauncherFlags flags = ProcessCmdLine(argc, argv);

  nsAutoHandle mediumIlToken;
  Maybe<ElevationState> elevationState = GetElevationState(flags, mediumIlToken);
  if (!elevationState) {
    return 1;
  }

  // If we're elevated, we should relaunch ourselves as a normal user.
  // Note that we only call LaunchUnelevated when we don't need to wait for the
  // browser process.
  if (elevationState.value() == ElevationState::eElevated &&
      !(flags & (LauncherFlags::eWaitForBrowser | LauncherFlags::eNoDeelevate)) &&
      !mediumIlToken.get()) {
    return !LaunchUnelevated(argc, argv);
  }

  // Now proceed with setting up the parameters for process creation
  UniquePtr<wchar_t[]> cmdLine(MakeCommandLine(argc, argv));
  if (!cmdLine) {
    return 1;
  }

  const Maybe<bool> isSafeMode = IsSafeModeRequested(argc, argv,
                                                     SafeModeFlag::None);
  if (!isSafeMode) {
    ShowError(ERROR_INVALID_PARAMETER);
    return 1;
  }

  ProcThreadAttributes attrs;
  SetMitigationPolicies(attrs, isSafeMode.value());

  HANDLE stdHandles[] = {
    ::GetStdHandle(STD_INPUT_HANDLE),
    ::GetStdHandle(STD_OUTPUT_HANDLE),
    ::GetStdHandle(STD_ERROR_HANDLE)
  };

  attrs.AddInheritableHandles(stdHandles);

  DWORD creationFlags = CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT;

  STARTUPINFOEXW siex;
  Maybe<bool> attrsOk = attrs.AssignTo(siex);
  if (!attrsOk) {
    ShowError();
    return 1;
  }

  BOOL inheritHandles = FALSE;

  if (attrsOk.value()) {
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
    createOk = ::CreateProcessAsUserW(mediumIlToken.get(), argv[0], cmdLine.get(),
                                      nullptr, nullptr, inheritHandles,
                                      creationFlags, nullptr, nullptr,
                                      &siex.StartupInfo, &pi);
  } else {
    createOk = ::CreateProcessW(argv[0], cmdLine.get(), nullptr, nullptr,
                                inheritHandles, creationFlags, nullptr, nullptr,
                                &siex.StartupInfo, &pi);
  }

  if (!createOk) {
    ShowError();
    return 1;
  }

  nsAutoHandle process(pi.hProcess);
  nsAutoHandle mainThread(pi.hThread);

  if (!PostCreationSetup(process.get(), mainThread.get(), isSafeMode.value()) ||
      ::ResumeThread(mainThread.get()) == static_cast<DWORD>(-1)) {
    ShowError();
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
    const DWORD timeout = ::IsDebuggerPresent() ? INFINITE :
                          kWaitForInputIdleTimeoutMS;

    // Keep the current process around until the callback process has created
    // its message queue, to avoid the launched process's windows being forced
    // into the background.
    mozilla::WaitForInputIdle(process.get(), timeout);
  }

  return 0;
}

} // namespace mozilla
