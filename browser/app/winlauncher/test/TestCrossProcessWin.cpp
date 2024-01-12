/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <thread>
#include <winternl.h>

#define MOZ_USE_LAUNCHER_ERROR

#include <atomic>
#include <thread>
#include "freestanding/SharedSection.cpp"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/DynamicBlocklist.h"
#include "mozilla/NativeNt.h"
#include "mozilla/Vector.h"

#define DLL_BLOCKLIST_ENTRY(name, ...) \
  {MOZ_LITERAL_UNICODE_STRING(L##name), __VA_ARGS__},
#define DLL_BLOCKLIST_STRING_TYPE UNICODE_STRING

#include "mozilla/WindowsDllBlocklistLauncherDefs.h"

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
const wchar_t* kExpectedDependentModules[] = {
    L"A B C",
    L"a b c.dll",
    L"A B C.exe",
    L"X Y Z.dll",
};

const UNICODE_STRING kStringNotInBlocklist =
    MOZ_LITERAL_UNICODE_STRING(L"Test_NotInBlocklist.dll");
const UNICODE_STRING kTestDependentModuleString =
    MOZ_LITERAL_UNICODE_STRING(L"Test_DependentModule.dll");

using namespace mozilla;
using namespace mozilla::freestanding;

// clang-format off
const DllBlockInfo kDllBlocklistShort[] = {
  // The entries do not have to be sorted.
  DLL_BLOCKLIST_ENTRY("X Y Z_Test", MAKE_VERSION(1, 2, 65535, 65535))
  DLL_BLOCKLIST_ENTRY("\u30E9\u30FC\u30E1\u30F3_Test")
  DLL_BLOCKLIST_ENTRY("Avmvirtualsource_Test.ax", MAKE_VERSION(1, 0, 0, 3),
                      DllBlockInfoFlags::BROWSER_PROCESS_ONLY)
  DLL_BLOCKLIST_ENTRY("1ccelerator_Test.dll", MAKE_VERSION(3, 2, 1, 6))
  DLL_BLOCKLIST_ENTRY("atkdx11disp_Test.dll", DllBlockInfo::ALL_VERSIONS)
  {},
};
// clang-format on

namespace mozilla::freestanding {
class SharedSectionTestHelper {
 public:
  static constexpr size_t GetModulePathArraySize() {
    return SharedSection::kSharedViewSize -
           (offsetof(SharedSection::Layout, mFirstBlockEntry) +
            sizeof(DllBlockInfo));
  }
};
}  // namespace mozilla::freestanding

class TempFile final {
  wchar_t mFullPath[MAX_PATH + 1];

 public:
  TempFile() : mFullPath{0} {
    wchar_t tempDir[MAX_PATH + 1];
    DWORD len = ::GetTempPathW(ArrayLength(tempDir), tempDir);
    if (!len) {
      return;
    }

    len = ::GetTempFileNameW(tempDir, L"blocklist", 0, mFullPath);
    if (!len) {
      return;
    }
  }

  operator const wchar_t*() const { return mFullPath[0] ? mFullPath : nullptr; }
};

template <typename T, int N>
void PrintLauncherError(const LauncherResult<T>& aResult,
                        const char (&aMsg)[N]) {
  const LauncherError& err = aResult.inspectErr();
  printf("TEST-FAILED | TestCrossProcessWin | %s - %lx at %s:%d\n", aMsg,
         err.mError.AsHResult(), err.mFile, err.mLine);
}

#define VERIFY_FUNCTION_RESOLVED(mod, exports, name)            \
  do {                                                          \
    if (reinterpret_cast<FARPROC>(exports->m##name) !=          \
        ::GetProcAddress(mod, #name)) {                         \
      printf(                                                   \
          "TEST-FAILED | TestCrossProcessWin | "                \
          "Kernel32ExportsSolver::" #name " did not match.\n"); \
      return false;                                             \
    }                                                           \
  } while (0)

static bool VerifySharedSection(SharedSection& aSharedSection) {
  Kernel32ExportsSolver* k32Exports = aSharedSection.GetKernel32Exports();
  if (!k32Exports) {
    printf(
        "TEST-FAILED | TestCrossProcessWin | Failed to map a shared section\n");
    return false;
  }

  HMODULE k32mod = ::GetModuleHandleW(L"kernel32.dll");
  VERIFY_FUNCTION_RESOLVED(k32mod, k32Exports, FlushInstructionCache);
  VERIFY_FUNCTION_RESOLVED(k32mod, k32Exports, GetModuleHandleW);
  VERIFY_FUNCTION_RESOLVED(k32mod, k32Exports, GetSystemInfo);
  VERIFY_FUNCTION_RESOLVED(k32mod, k32Exports, VirtualProtect);

  Maybe<Vector<const wchar_t*>> modulesArray =
      aSharedSection.GetDependentModules();
  if (modulesArray.isNothing()) {
    printf(
        "TEST-FAILED | TestCrossProcessWin | GetDependentModules returned "
        "Nothing");
    return false;
  }
  bool matched =
      modulesArray->length() ==
      sizeof(kExpectedDependentModules) / sizeof(kExpectedDependentModules[0]);
  if (matched) {
    for (size_t i = 0; i < modulesArray->length(); ++i) {
      if (wcscmp((*modulesArray)[i], kExpectedDependentModules[i])) {
        matched = false;
        break;
      }
    }
  }
  if (!matched) {
    // Print actual strings on error
    for (const wchar_t* p : *modulesArray) {
      printf("%p: %S\n", p, p);
    }
    return false;
  }

  for (const DllBlockInfo* info = kDllBlocklistShort; info->mName.Buffer;
       ++info) {
    const DllBlockInfo* matched = aSharedSection.SearchBlocklist(info->mName);
    if (!matched) {
      printf(
          "TEST-FAILED | TestCrossProcessWin | No blocklist entry match for "
          "entry in blocklist.\n");
      return false;
    }
  }

  if (aSharedSection.SearchBlocklist(kStringNotInBlocklist)) {
    printf(
        "TEST-FAILED | TestCrossProcessWin | Found blocklist entry match for "
        "something not in the blocklist.\n");
  }

  if (aSharedSection.IsDisabled()) {
    printf("TEST-FAILED | TestCrossProcessWin | Wrong disabled value.\n");
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

// Convert |aBlockEntries|, which is an array ending with an empty instance
// of DllBlockInfo, to DynamicBlockList by storing it to a temp file, loading
// as DynamicBlockList, and deleting the temp file.
static DynamicBlockList ConvertStaticBlocklistToDynamic(
    const DllBlockInfo aBlockEntries[]) {
  size_t originalLength = 0;
  CheckedUint32 totalStringLen = 0;
  for (const DllBlockInfo* entry = aBlockEntries; entry->mName.Length;
       ++entry) {
    totalStringLen += entry->mName.Length;
    MOZ_RELEASE_ASSERT(totalStringLen.isValid());
    ++originalLength;
  }

  // Pack all strings in this buffer without null characters
  UniquePtr<uint8_t[]> stringBuffer =
      MakeUnique<uint8_t[]>(totalStringLen.value());

  // The string buffer is placed immediately after the array of DllBlockInfo
  const size_t stringBufferOffset = (originalLength + 1) * sizeof(DllBlockInfo);

  // Entries in the dynamic blocklist do have to be sorted,
  // unlike in the static blocklist.
  UniquePtr<DllBlockInfo[]> sortedBlockEntries =
      MakeUnique<DllBlockInfo[]>(originalLength);
  memcpy(sortedBlockEntries.get(), aBlockEntries,
         sizeof(DllBlockInfo) * originalLength);
  std::sort(sortedBlockEntries.get(), sortedBlockEntries.get() + originalLength,
            [](const DllBlockInfo& a, const DllBlockInfo& b) {
              return ::RtlCompareUnicodeString(&a.mName, &b.mName, TRUE) < 0;
            });

  Vector<DllBlockInfo> copied;
  Unused << copied.resize(originalLength + 1);  // aBlockEntries + sentinel

  size_t currentStringOffset = 0;
  for (size_t i = 0; i < originalLength; ++i) {
    copied[i].mMaxVersion = sortedBlockEntries[i].mMaxVersion;
    copied[i].mFlags = sortedBlockEntries[i].mFlags;

    // Copy the module's name to the string buffer and store its offset
    // in mName.Buffer
    memcpy(stringBuffer.get() + currentStringOffset,
           sortedBlockEntries[i].mName.Buffer,
           sortedBlockEntries[i].mName.Length);
    copied[i].mName.Buffer =
        reinterpret_cast<wchar_t*>(stringBufferOffset + currentStringOffset);
    // Only keep mName.Length and leave mName.MaximumLength to be zero
    copied[i].mName.Length = sortedBlockEntries[i].mName.Length;

    currentStringOffset += sortedBlockEntries[i].mName.Length;
  }

  TempFile blocklistFile;
  nsAutoHandle file(::CreateFileW(blocklistFile, GENERIC_WRITE, FILE_SHARE_READ,
                                  nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                  nullptr));
  MOZ_RELEASE_ASSERT(file);

  DynamicBlockListBase::FileHeader header;
  header.mSignature = DynamicBlockListBase::kSignature;
  header.mFileVersion = DynamicBlockListBase::kCurrentVersion;
  header.mPayloadSize =
      sizeof(DllBlockInfo) * copied.length() + totalStringLen.value();

  DWORD written = 0;
  MOZ_RELEASE_ASSERT(
      ::WriteFile(file.get(), &header, sizeof(header), &written, nullptr));
  MOZ_RELEASE_ASSERT(::WriteFile(file.get(), copied.begin(),
                                 sizeof(DllBlockInfo) * copied.length(),
                                 &written, nullptr));
  MOZ_RELEASE_ASSERT(::WriteFile(file.get(), stringBuffer.get(),
                                 totalStringLen.value(), &written, nullptr));

  DynamicBlockList blockList(blocklistFile);
  ::DeleteFileW(blocklistFile);
  return blockList;
}

const DynamicBlockList gFullList =
    ConvertStaticBlocklistToDynamic(gWindowsDllBlocklist);
const DynamicBlockList gShortList =
    ConvertStaticBlocklistToDynamic(kDllBlocklistShort);

static bool TestDependentModules() {
  LauncherVoidResult result = gSharedSection.Init();
  if (result.isErr()) {
    PrintLauncherError(result, "SharedSection::Init failed");
    return false;
  }

  constexpr size_t sizeInBytes =
      SharedSectionTestHelper::GetModulePathArraySize();
  UniquePtr<uint8_t[]> bufferData = MakeUnique<uint8_t[]>(sizeInBytes);
  Span<uint8_t> buffer(bufferData, sizeInBytes);
  memset(buffer.data(), 0x88, buffer.size());

  // Try to add a long string that does not fit in the section,
  // since there's no room for the NULL character to indicate the final string.
  UNICODE_STRING ustr;
  ustr.Buffer = reinterpret_cast<wchar_t*>(buffer.data());
  ustr.Length = ustr.MaximumLength = buffer.size();

  result = gSharedSection.AddDependentModule(&ustr);
  if (result.isOk() || result.inspectErr() != WindowsError::FromWin32Error(
                                                  ERROR_INSUFFICIENT_BUFFER)) {
    printf(
        "TEST-FAILED | TestCrossProcessWin | "
        "Adding a too long string should fail.\n");
    return false;
  }

  result = gSharedSection.Init();
  if (result.isErr()) {
    PrintLauncherError(result, "SharedSection::Init failed");
    return false;
  }

  // Keep adding a single-char string until it fails and
  // make sure no crash.
  // We want to make sure no strings match any earlier strings so
  // we can get the expected count. This is a little tricky since
  // it includes case-insensitivity, so start at the "CJK Unified Ideographs
  // Extension A" block of Unicode, which has no two characters that compare
  // equal under a case insensitive comparison.
  *(reinterpret_cast<wchar_t*>(buffer.data())) = 0x3400;
  ustr.Length = ustr.MaximumLength = sizeof(wchar_t);
  wchar_t numberOfStringsAdded = 0;
  while (gSharedSection.AddDependentModule(&ustr).isOk()) {
    ++numberOfStringsAdded;
    // Make sure the string doesn't match any earlier strings
    wchar_t oldValue = *(reinterpret_cast<wchar_t*>(buffer.data()));
    *(reinterpret_cast<wchar_t*>(buffer.data())) = oldValue + 1;
  }

  int numberOfCharactersInBuffer =
      SharedSectionTestHelper::GetModulePathArraySize() / sizeof(wchar_t);
  // Each string is two characters long (one "real" character and a null), but
  // the whole buffer needs an additional null at the end.
  int expectedNumberOfStringsAdded = (numberOfCharactersInBuffer - 1) / 2;
  if (numberOfStringsAdded != expectedNumberOfStringsAdded) {
    printf(
        "TEST-FAILED | TestCrossProcessWin | "
        "Added %d dependent strings before failing (expected %d).\n",
        static_cast<int>(numberOfStringsAdded), expectedNumberOfStringsAdded);
    return false;
  }

  // SetBlocklist is not allowed after AddDependentModule
  result = gSharedSection.SetBlocklist(gShortList, false);
  if (result.isOk() || result.inspectErr() !=
                           WindowsError::FromWin32Error(ERROR_INVALID_STATE)) {
    printf(
        "TEST-FAILED | TestCrossProcessWin | "
        "SetBlocklist is not allowed after AddDependentModule\n");
    return false;
  }

  gSharedSection.Reset();
  return true;
}

static bool TestDynamicBlocklist() {
  if (!gFullList.GetPayloadSize() || !gShortList.GetPayloadSize()) {
    printf(
        "TEST-FAILED | TestCrossProcessWin | DynamicBlockList::LoadFile "
        "failed\n");
    return false;
  }

  LauncherVoidResult result = gSharedSection.Init();
  if (result.isErr()) {
    PrintLauncherError(result, "SharedSection::Init failed");
    return false;
  }

  // Set gShortList, and gShortList
  // 1. Setting gShortList succeeds
  // 2. Next try to set gShortList fails
  result = gSharedSection.SetBlocklist(gShortList, false);
  if (result.isErr()) {
    PrintLauncherError(result, "SetBlocklist(gShortList) failed");
    return false;
  }
  result = gSharedSection.SetBlocklist(gShortList, false);
  if (result.isOk() || result.inspectErr() !=
                           WindowsError::FromWin32Error(ERROR_INVALID_STATE)) {
    printf(
        "TEST-FAILED | TestCrossProcessWin | "
        "SetBlocklist is allowed only once\n");
    return false;
  }

  result = gSharedSection.Init();
  if (result.isErr()) {
    PrintLauncherError(result, "SharedSection::Init failed");
    return false;
  }

  // Add gFullList and gShortList
  // 1. Adding gFullList always fails because it doesn't fit the section
  // 2. Adding gShortList succeeds because no entry is added yet
  MOZ_RELEASE_ASSERT(
      gFullList.GetPayloadSize() >
          SharedSectionTestHelper::GetModulePathArraySize(),
      "Test assumes gFullList is too big to fit in shared section");
  result = gSharedSection.SetBlocklist(gFullList, false);
  if (result.isOk() || result.inspectErr() != WindowsError::FromWin32Error(
                                                  ERROR_INSUFFICIENT_BUFFER)) {
    printf(
        "TEST-FAILED | TestCrossProcessWin | "
        "SetBlocklist(gFullList) should fail\n");
    return false;
  }
  result = gSharedSection.SetBlocklist(gShortList, false);
  if (result.isErr()) {
    PrintLauncherError(result, "SetBlocklist(gShortList) failed");
    return false;
  }

  // AddDependentModule is allowed after SetBlocklist
  result = gSharedSection.AddDependentModule(&kTestDependentModuleString);
  if (result.isErr()) {
    PrintLauncherError(result, "SharedSection::AddDependentModule failed");
    return false;
  }

  gSharedSection.Reset();
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
    SRWLOCK lock = SRWLOCK_INIT;
    ::AcquireSRWLockExclusive(&lock);

    Vector<std::thread> threads;
    std::atomic<bool> success = true;
    for (int i = 0; i < 10; ++i) {
      Unused << threads.emplaceBack(
          [&success](SRWLOCK* aLock) {
            // All threads call GetKernel32Exports(), but only the first thread
            // maps a write-copy section and populates it.
            ::AcquireSRWLockShared(aLock);
            if (gSharedSection.GetKernel32Exports() == nullptr) {
              success = false;
            }
            ::ReleaseSRWLockShared(aLock);
          },
          &lock);
    }

    // Wait a msec for all threads to be ready and release the lock
    ::Sleep(1);
    ::ReleaseSRWLockExclusive(&lock);

    for (auto& thread : threads) {
      thread.join();
    }

    if (!success) {
      printf(
          "TEST-FAILED | TestCrossProcessWin | "
          "GetKernel32Exports() returned null.\n");
      return 1;
    }

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

    // Test a scenario to transfer a transferred section as a readonly handle
    gSharedSection.ConvertToReadOnly();

    // AddDependentModule fails as the handle is readonly.
    LauncherVoidResult result =
        gSharedSection.AddDependentModule(&kTestDependentModuleString);
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

  if (!TestDependentModules()) {
    return 1;
  }

  if (!TestDynamicBlocklist()) {
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

  result = gSharedSection.SetBlocklist(gShortList, false);
  if (result.isErr()) {
    PrintLauncherError(result, "SetBlocklist(gShortList) failed");
    return false;
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
