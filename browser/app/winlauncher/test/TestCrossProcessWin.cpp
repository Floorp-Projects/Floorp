/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#define MOZ_USE_LAUNCHER_ERROR
#define DONT_SKIP_DEFAULT_DEPENDENT_MODULES

#include "freestanding/SharedSection.cpp"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/NativeNt.h"

const wchar_t kChildArg[] = L"--child";

#if !defined(__MINGW32__)
// MinGW includes an old winternl.h that defines FILE_BASIC_INFORMATION.
typedef struct FILE_BASIC_INFORMATION {
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  ULONG FileAttributes;
} FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;
#endif  // !defined(__MINGW32__)

extern "C" NTSTATUS NTAPI
NtQueryAttributesFile(POBJECT_ATTRIBUTES aObjectAttributes,
                      PFILE_BASIC_INFORMATION aFileInformation);

using namespace mozilla;
using namespace mozilla::freestanding;

template <typename T, int N>
void PrintLauncherError(const LauncherResult<T>& aResult,
                        const char (&aMsg)[N]) {
  const LauncherError& err = aResult.inspectErr();
  printf("TEST-FAILED | TestCrossProcessWin | %s - %lx at %s:%d\n", aMsg,
         err.mError.AsHResult(), err.mFile, err.mLine);
}

#define VERIFY_FUNCTION_RESOLVED(mod, exports, name)            \
  do {                                                          \
    if (reinterpret_cast<FARPROC>(exports.m##name) !=           \
        ::GetProcAddress(mod, #name)) {                         \
      printf(                                                   \
          "TEST-FAILED | TestCrossProcessWin | "                \
          "Kernel32ExportsSolver::" #name " did not match.\n"); \
      return false;                                             \
    }                                                           \
  } while (0)

static bool VerifySharedSection(SharedSection& aSharedSection) {
  LauncherResult<SharedSection::Layout*> resultView = aSharedSection.GetView();
  if (resultView.isErr()) {
    PrintLauncherError(resultView, "Failed to map a shared section");
    return false;
  }

  SharedSection::Layout* view = resultView.unwrap();

  // Use a local variable of RTL_RUN_ONCE to resolve Kernel32Exports every time
  RTL_RUN_ONCE sRunEveryTime = RTL_RUN_ONCE_INIT;
  view->mK32Exports.Resolve(sRunEveryTime);

  HMODULE k32mod = ::GetModuleHandleW(L"kernel32.dll");
  VERIFY_FUNCTION_RESOLVED(k32mod, view->mK32Exports, FlushInstructionCache);
  VERIFY_FUNCTION_RESOLVED(k32mod, view->mK32Exports, GetSystemInfo);
  VERIFY_FUNCTION_RESOLVED(k32mod, view->mK32Exports, VirtualProtect);

  const uint8_t* const arrayBase =
      reinterpret_cast<const uint8_t*>(view->mModulePathArray);
  for (uint32_t i = 0; i < view->mModulePathArrayLength; ++i) {
    uint32_t offset = view->mModulePathArray[i];
    // Use NtQueryAttributesFile to check the validity of an NT path.
    UNICODE_STRING ntpath;
    ::RtlInitUnicodeString(
        &ntpath, reinterpret_cast<const wchar_t*>(arrayBase + offset));
    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, &ntpath, OBJ_CASE_INSENSITIVE, nullptr,
                               nullptr);
    FILE_BASIC_INFORMATION info;
    NTSTATUS status = ::NtQueryAttributesFile(&oa, &info);
    if (!NT_SUCCESS(status)) {
      printf(
          "TEST-FAILED | TestCrossProcessWin | "
          "Invalid path %ls - %08lx.\n",
          ntpath.Buffer, status);
      return false;
    }

    printf("%p: %ls\n", &offset,
           reinterpret_cast<const wchar_t*>(arrayBase + offset));
  }

  return true;
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

    if (!VerifySharedSection(gSharedSection)) {
      return 1;
    }

    // Test a scenario to transfer a transferred section
    static HANDLE copiedHandle = nullptr;
    nt::CrossExecTransferManager tansferToSelf(::GetCurrentProcess());
    LauncherVoidResult result =
        gSharedSection.TransferHandle(tansferToSelf, &copiedHandle);
    if (result.isErr()) {
      PrintLauncherError(result, "SharedSection::TransferHandle(self) failed");
      return 1;
    }

    gSharedSection.Reset(copiedHandle);
    if (!VerifySharedSection(gSharedSection)) {
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

  LauncherVoidResult result =
      transferMgr.Transfer(&ChildProcess::sExecutableImageBase,
                           &remoteImageBase.inspect(), sizeof(HMODULE));
  if (result.isErr()) {
    PrintLauncherError(result, "ChildProcess::WriteData(Imagebase) failed");
    return 1;
  }

  DWORD childPid = childProcess.GetProcessId();

  DWORD* readOnlyData = const_cast<DWORD*>(&ChildProcess::sReadOnlyProcessId);
  result = transferMgr.Transfer(readOnlyData, &childPid, sizeof(DWORD));
  if (result.isOk()) {
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

  result = transferMgr.Transfer(readOnlyData, &childPid, sizeof(DWORD));
  if (result.isErr()) {
    PrintLauncherError(result, "ChildProcess::WriteData(PID) failed");
    return 1;
  }

  {
    // Define a scope for |sharedSection| to resume the child process after
    // the section is deleted in the parent process.
    result = gSharedSection.Init(transferMgr.LocalPEHeaders());
    if (result.isErr()) {
      PrintLauncherError(result, "SharedSection::Init failed");
      return 1;
    }

    result = gSharedSection.TransferHandle(transferMgr);
    if (result.isErr()) {
      PrintLauncherError(result, "SharedSection::TransferHandle failed");
      return 1;
    }
  }

  if (!childProcess.ResumeAndWaitUntilExit()) {
    return 1;
  }

  return 0;
}
