/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/NativeNt.h"
#include "mozilla/Types.h"
#include "mozilla/WindowsDllBlocklist.h"

#include "DllBlocklist.h"
#include "LoaderPrivateAPI.h"
#include "ModuleLoadFrame.h"

// clang-format off
#define MOZ_LITERAL_UNICODE_STRING(s)                                        \
  {                                                                          \
    /* Length of the string in bytes, less the null terminator */            \
    sizeof(s) - sizeof(wchar_t),                                             \
    /* Length of the string in bytes, including the null terminator */       \
    sizeof(s),                                                               \
    /* Pointer to the buffer */                                              \
    const_cast<wchar_t*>(s)                                                  \
  }
// clang-format on

#define DLL_BLOCKLIST_ENTRY(name, ...) \
  {MOZ_LITERAL_UNICODE_STRING(L##name), __VA_ARGS__},
#define DLL_BLOCKLIST_STRING_TYPE UNICODE_STRING

#if defined(MOZ_LAUNCHER_PROCESS) || defined(NIGHTLY_BUILD)
#  include "mozilla/WindowsDllBlocklistLauncherDefs.h"
#else
#  include "mozilla/WindowsDllBlocklistCommon.h"
DLL_BLOCKLIST_DEFINITIONS_BEGIN
DLL_BLOCKLIST_DEFINITIONS_END
#endif

static const HANDLE kCurrentProcess = reinterpret_cast<HANDLE>(-1);

class MOZ_STATIC_CLASS MOZ_TRIVIAL_CTOR_DTOR NativeNtBlockSet final {
  struct NativeNtBlockSetEntry {
    NativeNtBlockSetEntry() = default;
    ~NativeNtBlockSetEntry() = default;
    NativeNtBlockSetEntry(const UNICODE_STRING& aName, uint64_t aVersion,
                          NativeNtBlockSetEntry* aNext)
        : mName(aName), mVersion(aVersion), mNext(aNext) {}
    UNICODE_STRING mName;
    uint64_t mVersion;
    NativeNtBlockSetEntry* mNext;
  };

 public:
  // Constructor and destructor MUST be trivial
  constexpr NativeNtBlockSet() : mFirstEntry(nullptr) {}
  ~NativeNtBlockSet() = default;

  void Add(const UNICODE_STRING& aName, uint64_t aVersion);
  void Write(HANDLE aFile);

 private:
  static NativeNtBlockSetEntry* NewEntry(const UNICODE_STRING& aName,
                                         uint64_t aVersion,
                                         NativeNtBlockSetEntry* aNextEntry);

 private:
  NativeNtBlockSetEntry* mFirstEntry;
  mozilla::nt::SRWLock mLock;
};

NativeNtBlockSet::NativeNtBlockSetEntry* NativeNtBlockSet::NewEntry(
    const UNICODE_STRING& aName, uint64_t aVersion,
    NativeNtBlockSet::NativeNtBlockSetEntry* aNextEntry) {
  return mozilla::freestanding::RtlNew<NativeNtBlockSetEntry>(aName, aVersion,
                                                              aNextEntry);
}

void NativeNtBlockSet::Add(const UNICODE_STRING& aName, uint64_t aVersion) {
  mozilla::nt::AutoExclusiveLock lock(mLock);

  for (NativeNtBlockSetEntry* entry = mFirstEntry; entry;
       entry = entry->mNext) {
    if (::RtlEqualUnicodeString(&entry->mName, &aName, TRUE) &&
        aVersion == entry->mVersion) {
      return;
    }
  }

  // Not present, add it
  NativeNtBlockSetEntry* newEntry = NewEntry(aName, aVersion, mFirstEntry);
  if (newEntry) {
    mFirstEntry = newEntry;
  }
}

void NativeNtBlockSet::Write(HANDLE aFile) {
  // NB: If this function is called, it is long after kernel32 is initialized,
  // so it is safe to use Win32 calls here.
  DWORD nBytes;
  char buf[MAX_PATH];

  // It would be nicer to use RAII here. However, its destructor
  // might not run if an exception occurs, in which case we would never release
  // the lock (MSVC warns about this possibility). So we acquire and release
  // manually.
  ::AcquireSRWLockExclusive(&mLock);

  MOZ_SEH_TRY {
    for (auto entry = mFirstEntry; entry; entry = entry->mNext) {
      int convOk = ::WideCharToMultiByte(CP_UTF8, 0, entry->mName.Buffer,
                                         entry->mName.Length / sizeof(wchar_t),
                                         buf, sizeof(buf), nullptr, nullptr);
      if (!convOk) {
        continue;
      }

      // write name[,v.v.v.v];
      if (!WriteFile(aFile, buf, convOk, &nBytes, nullptr)) {
        continue;
      }

      if (entry->mVersion != DllBlockInfo::ALL_VERSIONS) {
        WriteFile(aFile, ",", 1, &nBytes, nullptr);
        uint16_t parts[4];
        parts[0] = entry->mVersion >> 48;
        parts[1] = (entry->mVersion >> 32) & 0xFFFF;
        parts[2] = (entry->mVersion >> 16) & 0xFFFF;
        parts[3] = entry->mVersion & 0xFFFF;
        for (size_t p = 0; p < mozilla::ArrayLength(parts); ++p) {
          ltoa(parts[p], buf, 10);
          WriteFile(aFile, buf, strlen(buf), &nBytes, nullptr);
          if (p != mozilla::ArrayLength(parts) - 1) {
            WriteFile(aFile, ".", 1, &nBytes, nullptr);
          }
        }
      }
      WriteFile(aFile, ";", 1, &nBytes, nullptr);
    }
  }
  MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {}

  ::ReleaseSRWLockExclusive(&mLock);
}

static NativeNtBlockSet gBlockSet;

extern "C" void MOZ_EXPORT NativeNtBlockSet_Write(HANDLE aHandle) {
  gBlockSet.Write(aHandle);
}

enum class BlockAction {
  Allow,
  SubstituteLSP,
  Error,
  Deny,
};

static BlockAction CheckBlockInfo(const DllBlockInfo* aInfo,
                                  const mozilla::nt::PEHeaders& aHeaders,
                                  uint64_t& aVersion) {
  aVersion = DllBlockInfo::ALL_VERSIONS;

  if (aInfo->mFlags &
      (DllBlockInfo::BLOCK_WIN8PLUS_ONLY | DllBlockInfo::BLOCK_WIN8_ONLY)) {
    RTL_OSVERSIONINFOW osv = {sizeof(osv)};
    NTSTATUS ntStatus = ::RtlGetVersion(&osv);
    if (!NT_SUCCESS(ntStatus)) {
      return BlockAction::Error;
    }

    if (osv.dwMajorVersion < 8) {
      return BlockAction::Allow;
    }

    if ((aInfo->mFlags & DllBlockInfo::BLOCK_WIN8_ONLY) &&
        (osv.dwMajorVersion > 8 ||
         (osv.dwMajorVersion == 8 && osv.dwMinorVersion > 0))) {
      return BlockAction::Allow;
    }
  }

  if ((aInfo->mFlags & DllBlockInfo::CHILD_PROCESSES_ONLY) &&
      !(gBlocklistInitFlags & eDllBlocklistInitFlagIsChildProcess)) {
    return BlockAction::Allow;
  }

  if ((aInfo->mFlags & DllBlockInfo::BROWSER_PROCESS_ONLY) &&
      (gBlocklistInitFlags & eDllBlocklistInitFlagIsChildProcess)) {
    return BlockAction::Allow;
  }

  if (aInfo->mMaxVersion == DllBlockInfo::ALL_VERSIONS) {
    return BlockAction::Deny;
  }

  if (!aHeaders) {
    return BlockAction::Error;
  }

  if (aInfo->mFlags & DllBlockInfo::USE_TIMESTAMP) {
    DWORD timestamp;
    if (!aHeaders.GetTimeStamp(timestamp)) {
      return BlockAction::Error;
    }

    if (timestamp > aInfo->mMaxVersion) {
      return BlockAction::Allow;
    }

    return BlockAction::Deny;
  }

  // Else we try to get the file version information. Note that we don't have
  // access to GetFileVersionInfo* APIs.
  if (!aHeaders.GetVersionInfo(aVersion)) {
    return BlockAction::Error;
  }

  if (aInfo->IsVersionBlocked(aVersion)) {
    return BlockAction::Deny;
  }

  return BlockAction::Allow;
}

struct DllBlockInfoComparator {
  explicit DllBlockInfoComparator(const UNICODE_STRING& aTarget)
      : mTarget(&aTarget) {}

  int operator()(const DllBlockInfo& aVal) const {
    return static_cast<int>(
        ::RtlCompareUnicodeString(mTarget, &aVal.mName, TRUE));
  }

  PCUNICODE_STRING mTarget;
};

static BlockAction IsDllAllowed(const UNICODE_STRING& aLeafName,
                                void* aBaseAddress) {
  if (mozilla::nt::Contains12DigitHexString(aLeafName) ||
      mozilla::nt::IsFileNameAtLeast16HexDigits(aLeafName)) {
    return BlockAction::Deny;
  }

  DECLARE_POINTER_TO_FIRST_DLL_BLOCKLIST_ENTRY(info);
  DECLARE_DLL_BLOCKLIST_NUM_ENTRIES(infoNumEntries);

  DllBlockInfoComparator comp(aLeafName);

  size_t match;
  if (!BinarySearchIf(info, 0, infoNumEntries, comp, &match)) {
    return BlockAction::Allow;
  }

  const DllBlockInfo& entry = info[match];

  mozilla::nt::PEHeaders headers(aBaseAddress);
  uint64_t version;
  BlockAction checkResult = CheckBlockInfo(&entry, headers, version);
  if (checkResult != BlockAction::Allow) {
    gBlockSet.Add(entry.mName, version);
  }

  return checkResult;
}

namespace mozilla {
namespace freestanding {

CrossProcessDllInterceptor::FuncHookType<LdrLoadDllPtr> stub_LdrLoadDll;

NTSTATUS NTAPI patched_LdrLoadDll(PWCHAR aDllPath, PULONG aFlags,
                                  PUNICODE_STRING aDllName,
                                  PHANDLE aOutHandle) {
  ModuleLoadFrame frame(aDllName);

  NTSTATUS ntStatus = stub_LdrLoadDll(aDllPath, aFlags, aDllName, aOutHandle);

  return frame.SetLoadStatus(ntStatus, aOutHandle);
}

CrossProcessDllInterceptor::FuncHookType<NtMapViewOfSectionPtr>
    stub_NtMapViewOfSection;

NTSTATUS NTAPI patched_NtMapViewOfSection(
    HANDLE aSection, HANDLE aProcess, PVOID* aBaseAddress, ULONG_PTR aZeroBits,
    SIZE_T aCommitSize, PLARGE_INTEGER aSectionOffset, PSIZE_T aViewSize,
    SECTION_INHERIT aInheritDisposition, ULONG aAllocationType,
    ULONG aProtectionFlags) {
  // We always map first, then we check for additional info after.
  NTSTATUS stubStatus = stub_NtMapViewOfSection(
      aSection, aProcess, aBaseAddress, aZeroBits, aCommitSize, aSectionOffset,
      aViewSize, aInheritDisposition, aAllocationType, aProtectionFlags);
  if (!NT_SUCCESS(stubStatus)) {
    return stubStatus;
  }

  if (aProcess != kCurrentProcess) {
    // We're only interested in mapping for the current process.
    return stubStatus;
  }

  // Do a query to see if the memory is MEM_IMAGE. If not, continue
  MEMORY_BASIC_INFORMATION mbi;
  NTSTATUS ntStatus =
      ::NtQueryVirtualMemory(aProcess, *aBaseAddress, MemoryBasicInformation,
                             &mbi, sizeof(mbi), nullptr);
  if (!NT_SUCCESS(ntStatus)) {
    ::NtUnmapViewOfSection(aProcess, *aBaseAddress);
    return STATUS_ACCESS_DENIED;
  }

  // We don't care about mappings that aren't MEM_IMAGE
  if (!(mbi.Type & MEM_IMAGE)) {
    return stubStatus;
  }

  // Get the section name
  nt::MemorySectionNameBuf sectionFileName(
      gLoaderPrivateAPI.GetSectionNameBuffer(*aBaseAddress));
  if (sectionFileName.IsEmpty()) {
    ::NtUnmapViewOfSection(aProcess, *aBaseAddress);
    return STATUS_ACCESS_DENIED;
  }

  // Find the leaf name
  UNICODE_STRING leafOnStack;
  nt::GetLeafName(&leafOnStack, sectionFileName);

  // Check blocklist
  BlockAction blockAction = IsDllAllowed(leafOnStack, *aBaseAddress);

  if (blockAction == BlockAction::Allow) {
    if (nt::RtlGetProcessHeap()) {
      ModuleLoadFrame::NotifySectionMap(
          nt::AllocatedUnicodeString(sectionFileName), *aBaseAddress,
          stubStatus);
    }
    return stubStatus;
  }

  if (blockAction == BlockAction::SubstituteLSP) {
    // The process heap needs to be available here because
    // NotifyLSPSubstitutionRequired below copies a given string into the heap.
    // We use a soft assert here, assuming LSP load always occurs after the heap
    // is initialized.
    MOZ_ASSERT(nt::RtlGetProcessHeap());

    // Notify patched_LdrLoadDll that it will be necessary to perform a
    // substitution before returning.
    ModuleLoadFrame::NotifyLSPSubstitutionRequired(&leafOnStack);
  }

  ::NtUnmapViewOfSection(aProcess, *aBaseAddress);
  return STATUS_ACCESS_DENIED;
}

}  // namespace freestanding
}  // namespace mozilla
