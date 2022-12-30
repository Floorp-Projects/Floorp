/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#define MOZ_USE_LAUNCHER_ERROR

#include "freestanding/SharedSection.cpp"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/NativeNt.h"

const wchar_t kChildArg[] = L"--child";
const char* kTestDependentModulePaths[] = {
    "\\Device\\HarddiskVolume4\\Windows\\system32\\A B C",
    "\\Device\\HarddiskVolume4\\Windows\\system32\\a b c.dll",
    "\\Device\\HarddiskVolume4\\Windows\\system32\\A B C.exe",
    "\\Device\\HarddiskVolume4\\Windows\\system32\\X Y Z.dll",
    "\\Device\\HarddiskVolume1\\a b C",
    "\\Device\\HarddiskVolume2\\A b c.DLL",
    "\\Device\\HarddiskVolume3\\A B c.exe",
    "\\Device\\HarddiskVolume4\\X y Z.dll",
};
const wchar_t kExpectedDependentModules[] =
    L"A B C\0"
    L"a b c.dll\0"
    L"A B C.exe\0"
    L"X Y Z.dll\0";

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
  VERIFY_FUNCTION_RESOLVED(k32mod, view->mK32Exports, GetModuleHandleW);
  VERIFY_FUNCTION_RESOLVED(k32mod, view->mK32Exports, GetSystemInfo);
  VERIFY_FUNCTION_RESOLVED(k32mod, view->mK32Exports, VirtualProtect);

  const wchar_t* modulesArray = view->mModulePathArray;
  bool matched = memcmp(modulesArray, kExpectedDependentModules,
                        sizeof(kExpectedDependentModules)) == 0;
  if (!matched) {
    // Print actual strings on error
    for (const wchar_t* p = modulesArray; *p;) {
      printf("%p: %S\n", p, p);
      while (*p) {
        ++p;
      }
      ++p;
    }
    return false;
  }

  return true;
}

static bool TestAddString() {
  wchar_t testBuffer[3] = {0};
  UNICODE_STRING ustr;

  // This makes |testBuffer| full.
  ::RtlInitUnicodeString(&ustr, L"a");
  if (!AddString(testBuffer, ustr)) {
    printf(
        "TEST-FAILED | TestCrossProcessWin | "
        "AddString failed.\n");
    return false;
  }

  // Adding a string to a full buffer should fail.
  ::RtlInitUnicodeString(&ustr, L"b");
  if (AddString(testBuffer, ustr)) {
    printf(
        "TEST-FAILED | TestCrossProcessWin | "
        "AddString caused OOB memory access.\n");
    return false;
  }

  bool matched = memcmp(testBuffer, L"a\0", sizeof(testBuffer)) == 0;
  if (!matched) {
    printf(
        "TEST-FAILED | TestCrossProcessWin | "
        "AddString wrote wrong values.\n");
    return false;
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

    auto getDependentModulePaths =
        reinterpret_cast<const wchar_t* (*)()>(::GetProcAddress(
            ::GetModuleHandleW(nullptr), "GetDependentModulePaths"));
    if (!getDependentModulePaths) {
      printf(
          "TEST-FAILED | TestCrossProcessWin | "
          "Failed to get a pointer to GetDependentModulePaths - %08lx.\n",
          ::GetLastError());
      return 1;
    }

#if !defined(DEBUG)
    // GetDependentModulePaths does not allow a caller other than xul.dll.
    // Skip on Debug build because it hits MOZ_ASSERT.
    if (getDependentModulePaths()) {
      printf(
          "TEST-FAILED | TestCrossProcessWin | "
          "GetDependentModulePaths should return zero if the caller is "
          "not xul.dll.\n");
      return 1;
    }
#endif  // !defined(DEBUG)

    if (!VerifySharedSection(gSharedSection)) {
      return 1;
    }

    // Test a scenario to transfer a transferred section as a readonly handle
    gSharedSection.ConvertToReadOnly();

    UNICODE_STRING ustr;
    ::RtlInitUnicodeString(&ustr, L"test");
    LauncherVoidResult result = gSharedSection.AddDependentModule(&ustr);

    // AddDependentModule fails as the handle is readonly.
    if (result.inspectErr() !=
        WindowsError::FromWin32Error(ERROR_ACCESS_DENIED)) {
      PrintLauncherError(result, "The readonly section was writable");
      return 1;
    }

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

  if (!TestAddString()) {
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

  result = gSharedSection.Init();
  if (result.isErr()) {
    PrintLauncherError(result, "SharedSection::Init failed");
    return 1;
  }

  for (const char* testString : kTestDependentModulePaths) {
    // Test AllocatedUnicodeString(const char*) that is used
    // in IsDependentModule()
    nt::AllocatedUnicodeString depModule(testString);
    UNICODE_STRING depModuleLeafName;
    nt::GetLeafName(&depModuleLeafName, depModule);
    result = gSharedSection.AddDependentModule(&depModuleLeafName);
    if (result.isErr()) {
      PrintLauncherError(result, "SharedSection::AddDependentModule failed");
      return 1;
    }
  }

  result =
      gSharedSection.TransferHandle(transferMgr, GENERIC_READ | GENERIC_WRITE);
  if (result.isErr()) {
    PrintLauncherError(result, "SharedSection::TransferHandle failed");
    return 1;
  }

  // Close the section in the parent process before resuming the child process
  gSharedSection.Reset(nullptr);

  if (!childProcess.ResumeAndWaitUntilExit()) {
    return 1;
  }

  printf("TEST-PASS | TestCrossProcessWin | All checks passed\n");
  return 0;
}
