/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "SameBinary.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/Move.h"
#include "mozilla/NativeNt.h"
#include "mozilla/Unused.h"
#include "mozilla/Vector.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsWindowsHelpers.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * This test involves three processes:
 *   1. The "Monitor" process, which is executed by |MonitorMain|. This process
 *      is responsible for integrating with the test harness, so it spawns the
 *      "Parent" process (2), and then waits for the other two processes to
 *      finish.
 *   2. The "Parent" process, which is executed by |ParentMain|. This process
 *      creates the "Child" process (3) and then waits indefinitely.
 *   3. The "Child" process, which is executed by |ChildMain| and carries out
 *      the actual test. It terminates the Parent process during its execution,
 *      using the Child PID as the Parent process's exit code. This serves as a
 *      hacky yet effective way to signal to the Monitor process which PID it
 *      should wait on to ensure that the Child process has exited.
 */

static const char kMsgStart[] = "TEST-FAILED | SameBinary | ";

inline void PrintErrorMsg(const char* aMsg) {
  printf("%s%s\n", kMsgStart, aMsg);
}

inline void PrintWinError(const char* aMsg) {
  mozilla::WindowsError err(mozilla::WindowsError::FromLastError());
  printf("%s%s: %S\n", kMsgStart, aMsg, err.AsString().get());
}

template <typename T>
inline void PrintLauncherError(const mozilla::LauncherResult<T>& aResult,
                               const char* aMsg = nullptr) {
  const char* const kSep = aMsg ? ": " : "";
  const char* msg = aMsg ? aMsg : "";
  const mozilla::LauncherError& err = aResult.inspectErr();
  printf("%s%s%s%S (%s:%d)\n", kMsgStart, msg, kSep,
         err.mError.AsString().get(), err.mFile, err.mLine);
}

static int ChildMain(DWORD aExpectedParentPid) {
  mozilla::LauncherResult<DWORD> parentPid = mozilla::nt::GetParentProcessId();
  if (parentPid.isErr()) {
    PrintLauncherError(parentPid);
    return 1;
  }

  if (parentPid.inspect() != aExpectedParentPid) {
    PrintErrorMsg("Unexpected mismatch of parent PIDs");
    return 1;
  }

  const DWORD kAccess = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE;
  nsAutoHandle parentProcess(::OpenProcess(kAccess, FALSE, parentPid.inspect()));
  if (!parentProcess) {
    PrintWinError("Unexpectedly failed to call OpenProcess on parent");
    return 1;
  }

  mozilla::LauncherResult<bool> expectedSameBinary =
      mozilla::IsSameBinaryAsParentProcess();
  if (expectedSameBinary.isErr()) {
    PrintLauncherError(expectedSameBinary);
    return 1;
  }

  if (!expectedSameBinary.unwrap()) {
    PrintErrorMsg(
        "IsSameBinaryAsParentProcess returned incorrect result for identical "
        "binaries");
    return 1;
  }

  // Total hack, but who cares? We'll set the parent's exit code as our PID
  // so that the monitor process knows who to wait for!
  if (!::TerminateProcess(parentProcess.get(), ::GetCurrentProcessId())) {
    PrintWinError("Unexpected failure in TerminateProcess");
    return 1;
  }

  // Close our handle to the parent process so that no references are held.
  ::CloseHandle(parentProcess.disown());

  // Querying a pid on a terminated process may still succeed some time after
  // that process has been terminated. For the purposes of this test, we'll poll
  // the OS until we cannot succesfully open the parentPid anymore.
  const uint32_t kMaxAttempts = 100;
  uint32_t curAttempt = 0;
  while (HANDLE p = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                  parentPid.inspect())) {
    ::CloseHandle(p);
    ::Sleep(100);
    ++curAttempt;
    if (curAttempt >= kMaxAttempts) {
      PrintErrorMsg(
          "Exhausted retry attempts waiting for parent pid to become invalid");
      return 1;
    }
  }

  mozilla::LauncherResult<bool> expectedDifferentBinary =
      mozilla::IsSameBinaryAsParentProcess();
  if (expectedDifferentBinary.isErr()) {
    PrintLauncherError(expectedDifferentBinary,
                       "IsSameBinaryAsParentProcess returned error when we "
                       "were expecting success");
    return 1;
  }

  if (expectedDifferentBinary.unwrap()) {
    PrintErrorMsg(
        "IsSameBinaryAsParentProcess returned incorrect result for dead parent "
        "process");
    return 1;
  }

  return 0;
}

static nsReturnRef<HANDLE> CreateSelfProcess(int argc, wchar_t* argv[]) {
  nsAutoHandle empty;

  DWORD myPid = ::GetCurrentProcessId();

  wchar_t strPid[11] = {};
#if defined(__MINGW32__)
  _ultow(myPid, strPid, 16);
#else
  if (_ultow_s(myPid, strPid, 16)) {
    PrintErrorMsg("_ultow_s failed");
    return empty.out();
  }
#endif  // defined(__MINGW32__)

  wchar_t* extraArgs[] = {strPid};

  auto cmdLine = mozilla::MakeCommandLine(
      argc, argv, mozilla::ArrayLength(extraArgs), extraArgs);
  if (!cmdLine) {
    PrintErrorMsg("MakeCommandLine failed");
    return empty.out();
  }

  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi;
  BOOL ok =
      ::CreateProcessW(argv[0], cmdLine.get(), nullptr, nullptr, FALSE,
                       CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &pi);
  if (!ok) {
    PrintWinError("CreateProcess failed");
    return empty.out();
  }

  nsAutoHandle proc(pi.hProcess);
  nsAutoHandle thd(pi.hThread);

  return proc.out();
}

static int ParentMain(int argc, wchar_t* argv[]) {
  nsAutoHandle childProc(CreateSelfProcess(argc, argv));
  if (!childProc) {
    return 1;
  }

  if (::WaitForSingleObject(childProc.get(), INFINITE) != WAIT_OBJECT_0) {
    PrintWinError(
        "Unexpected result from WaitForSingleObject on child process");
    return 1;
  }

  MOZ_ASSERT_UNREACHABLE("This process should be terminated by now");
  return 0;
}

static int MonitorMain(int argc, wchar_t* argv[]) {
  // In this process, "parent" means the process that will be running
  // ParentMain, which is our child process (confusing, I know...)
  nsAutoHandle parentProc(CreateSelfProcess(argc, argv));
  if (!parentProc) {
    return 1;
  }

  if (::WaitForSingleObject(parentProc.get(), 60000) != WAIT_OBJECT_0) {
    PrintWinError("Unexpected result from WaitForSingleObject on parent");
    return 1;
  }

  DWORD childPid;
  if (!::GetExitCodeProcess(parentProc.get(), &childPid)) {
    PrintWinError("GetExitCodeProcess failed");
    return 1;
  }

  nsAutoHandle childProc(::OpenProcess(SYNCHRONIZE, FALSE, childPid));
  if (!childProc) {
    // Nothing to wait on anymore, which is OK.
    return 0;
  }

  // We want no more references to parentProc
  ::CloseHandle(parentProc.disown());

  if (::WaitForSingleObject(childProc.get(), 60000) != WAIT_OBJECT_0) {
    PrintWinError("Unexpected result from WaitForSingleObject on child");
    return 1;
  }

  return 0;
}

extern "C" int wmain(int argc, wchar_t* argv[]) {
  if (argc == 3) {
    return ChildMain(wcstoul(argv[2], nullptr, 16));
  }

  if (!mozilla::SetArgv0ToFullBinaryPath(argv)) {
    return 1;
  }

  if (argc == 1) {
    return MonitorMain(argc, argv);
  }

  if (argc == 2) {
    return ParentMain(argc, argv);
  }

  PrintErrorMsg("Unexpected argc");
  return 1;
}
