/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#define MOZ_USE_LAUNCHER_ERROR

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/NativeNt.h"

const wchar_t kChildArg[] = L"--child";

using namespace mozilla;

template <typename T, int N>
void PrintLauncherError(const LauncherResult<T>& aResult,
                        const char (&aMsg)[N]) {
  const LauncherError& err = aResult.inspectErr();
  printf("TEST-FAILED | TestCrossProcessWin | %s - %lx at %s:%d\n", aMsg,
         err.mError.AsHResult(), err.mFile, err.mLine);
}

class ChildProcess final {
  nsAutoHandle mChildProcess;
  nsAutoHandle mChildMainThread;
  DWORD mProcessId;

 public:
  // The following variables are updated from the parent process via
  // WriteProcessMemory while the process is suspended as a part of
  // TestWithChildProcess().
  //
  // Having both a non-const and a const is important because a constant
  // is separately placed in the .rdata section which is read-only, so
  // the region's attribute needs to be changed before modifying data via
  // WriteProcessMemory.
  // The keyword "volatile" is needed for a constant, otherwise the compiler
  // evaluates a constant as a literal without fetching data from memory.
  static HMODULE sExecutableImageBase;
  static volatile const DWORD sReadOnlyProcessId;

  static int Main() {
    if (sExecutableImageBase != ::GetModuleHandle(nullptr)) {
      printf(
          "TEST-FAILED | TestCrossProcessWin | "
          "sExecutableImageBase is expected to be %p, but actually was %p.\n",
          ::GetModuleHandle(nullptr), sExecutableImageBase);
      return 1;
    }

    if (sReadOnlyProcessId != ::GetCurrentProcessId()) {
      printf(
          "TEST-FAILED | TestCrossProcessWin | "
          "sReadOnlyProcessId is expected to be %lx, but actually was %lx.\n",
          ::GetCurrentProcessId(), sReadOnlyProcessId);
      return 1;
    }

    return 0;
  }

  ChildProcess(const wchar_t* aExecutable, const wchar_t* aOption)
      : mProcessId(0) {
    const wchar_t* childArgv[] = {aExecutable, aOption};
    auto cmdLine(
        mozilla::MakeCommandLine(mozilla::ArrayLength(childArgv), childArgv));

    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    BOOL ok =
        ::CreateProcessW(aExecutable, cmdLine.get(), nullptr, nullptr, FALSE,
                         CREATE_SUSPENDED, nullptr, nullptr, &si, &pi);
    if (!ok) {
      printf(
          "TEST-FAILED | TestCrossProcessWin | "
          "CreateProcessW falied - %08lx.\n",
          GetLastError());
      return;
    }

    mProcessId = pi.dwProcessId;

    mChildProcess.own(pi.hProcess);
    mChildMainThread.own(pi.hThread);
  }

  ~ChildProcess() { ::TerminateProcess(mChildProcess, 0); }

  operator HANDLE() const { return mChildProcess; }
  DWORD GetProcessId() const { return mProcessId; }

  bool ResumeAndWaitUntilExit() {
    if (::ResumeThread(mChildMainThread) == 0xffffffff) {
      printf(
          "TEST-FAILED | TestCrossProcessWin | "
          "ResumeThread failed - %08lx\n",
          GetLastError());
      return false;
    }

    if (::WaitForSingleObject(mChildProcess, 60000) != WAIT_OBJECT_0) {
      printf(
          "TEST-FAILED | TestCrossProcessWin | "
          "Unexpected result from WaitForSingleObject\n");
      return false;
    }

    DWORD exitCode;
    if (!::GetExitCodeProcess(mChildProcess, &exitCode)) {
      printf(
          "TEST-FAILED | TestCrossProcessWin | "
          "GetExitCodeProcess failed - %08lx\n",
          GetLastError());
      return false;
    }

    return exitCode == 0;
  }
};

HMODULE ChildProcess::sExecutableImageBase = 0;
volatile const DWORD ChildProcess::sReadOnlyProcessId = 0;

int wmain(int argc, wchar_t* argv[]) {
  printf("Process: %-8lx Base: %p\n", ::GetCurrentProcessId(),
         ::GetModuleHandle(nullptr));

  if (argc == 2 && wcscmp(argv[1], kChildArg) == 0) {
    return ChildProcess::Main();
  }

  ChildProcess childProcess(argv[0], kChildArg);
  if (!childProcess) {
    return 1;
  }

  LauncherResult<HMODULE> remoteImageBase =
      nt::GetProcessExeModule(childProcess);
  if (remoteImageBase.isErr()) {
    PrintLauncherError(remoteImageBase, "nt::GetProcessExeModule failed");
    return 1;
  }

  nt::CrossExecTransferManager transferMgr(childProcess);
  if (!transferMgr) {
    printf(
        "TEST-FAILED | TestCrossProcessWin | "
        "CrossExecTransferManager instantiation failed.\n");
    return 1;
  }

  LauncherVoidResult writeResult =
      transferMgr.Transfer(&ChildProcess::sExecutableImageBase,
                           &remoteImageBase.inspect(), sizeof(HMODULE));
  if (writeResult.isErr()) {
    PrintLauncherError(writeResult,
                       "ChildProcess::WriteData(Imagebase) failed");
    return 1;
  }

  DWORD childPid = childProcess.GetProcessId();

  DWORD* readOnlyData = const_cast<DWORD*>(&ChildProcess::sReadOnlyProcessId);
  writeResult = transferMgr.Transfer(readOnlyData, &childPid, sizeof(DWORD));
  if (writeResult.isOk()) {
    printf(
        "TEST-UNEXPECTED | TestCrossProcessWin | "
        "A constant was located in a writable section.");
    return 1;
  }

  AutoVirtualProtect prot =
      transferMgr.Protect(readOnlyData, sizeof(uint32_t), PAGE_READWRITE);
  if (!prot) {
    printf(
        "TEST-FAILED | TestCrossProcessWin | "
        "VirtualProtect failed - %08lx\n",
        prot.GetError().AsHResult());
    return 1;
  }

  writeResult = transferMgr.Transfer(readOnlyData, &childPid, sizeof(DWORD));
  if (writeResult.isErr()) {
    PrintLauncherError(writeResult, "ChildProcess::WriteData(PID) failed");
    return 1;
  }

  if (!childProcess.ResumeAndWaitUntilExit()) {
    return 1;
  }

  return 0;
}
