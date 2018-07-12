/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "NativeNt.h"
#include "nsWindowsDllInterceptor.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/Types.h"
#include "mozilla/WindowsDllBlocklist.h"

#define MOZ_LITERAL_UNICODE_STRING(s) \
  { \
    /* Length of the string in bytes, less the null terminator */ \
    sizeof(s) - sizeof(wchar_t), \
    /* Length of the string in bytes, including the null terminator */ \
    sizeof(s), \
    /* Pointer to the buffer */ \
    const_cast<wchar_t*>(s) \
  }

#define DLL_BLOCKLIST_ENTRY(name, ...) \
  { MOZ_LITERAL_UNICODE_STRING(L##name), __VA_ARGS__ },
#define DLL_BLOCKLIST_STRING_TYPE UNICODE_STRING

// Restrict the blocklist definitions to Nightly-only for now
#if defined(NIGHTLY_BUILD)
#include "mozilla/WindowsDllBlocklistDefs.h"
#else
#include "mozilla/WindowsDllBlocklistCommon.h"
DLL_BLOCKLIST_DEFINITIONS_BEGIN
DLL_BLOCKLIST_DEFINITIONS_END
#endif

extern uint32_t gBlocklistInitFlags;

static const HANDLE kCurrentProcess = reinterpret_cast<HANDLE>(-1);

class MOZ_STATIC_CLASS MOZ_TRIVIAL_CTOR_DTOR NativeNtBlockSet final
{
  struct NativeNtBlockSetEntry
  {
    NativeNtBlockSetEntry() = default;
    ~NativeNtBlockSetEntry() = default;
    NativeNtBlockSetEntry(const UNICODE_STRING& aName, uint64_t aVersion,
                          NativeNtBlockSetEntry* aNext)
      : mName(aName)
      , mVersion(aVersion)
      , mNext(aNext)
    {}
    UNICODE_STRING          mName;
    uint64_t                mVersion;
    NativeNtBlockSetEntry*  mNext;
  };

public:
  // Constructor and destructor MUST be trivial
  NativeNtBlockSet() = default;
  ~NativeNtBlockSet() = default;

  void Add(const UNICODE_STRING& aName, uint64_t aVersion);
  void Write(HANDLE aFile);

private:
  static NativeNtBlockSetEntry* NewEntry(const UNICODE_STRING& aName,
                                         uint64_t aVersion,
                                         NativeNtBlockSetEntry* aNextEntry);

private:
  NativeNtBlockSetEntry* mFirstEntry;
  // SRWLOCK_INIT == 0, so this is okay to use without any additional work as
  // long as NativeNtBlockSet is instantiated statically
  SRWLOCK                mLock;
};

NativeNtBlockSet::NativeNtBlockSetEntry*
NativeNtBlockSet::NewEntry(const UNICODE_STRING& aName, uint64_t aVersion,
                           NativeNtBlockSet::NativeNtBlockSetEntry* aNextEntry)
{
  HANDLE processHeap = mozilla::nt::RtlGetProcessHeap();
  if (!processHeap) {
    return nullptr;
  }

  PVOID memory = ::RtlAllocateHeap(processHeap, 0, sizeof(NativeNtBlockSetEntry));
  if (!memory) {
    return nullptr;
  }

  return new (memory) NativeNtBlockSetEntry(aName, aVersion, aNextEntry);
}

void
NativeNtBlockSet::Add(const UNICODE_STRING& aName, uint64_t aVersion)
{
  ::RtlAcquireSRWLockExclusive(&mLock);

  for (NativeNtBlockSetEntry* entry = mFirstEntry; entry; entry = entry->mNext) {
    if (::RtlEqualUnicodeString(&entry->mName, &aName, TRUE) &&
        aVersion == entry->mVersion) {
      ::RtlReleaseSRWLockExclusive(&mLock);
      return;
    }
  }

  // Not present, add it
  NativeNtBlockSetEntry* newEntry = NewEntry(aName, aVersion, mFirstEntry);
  mFirstEntry = newEntry;

  ::RtlReleaseSRWLockExclusive(&mLock);
}

void
NativeNtBlockSet::Write(HANDLE aFile)
{
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

      if (entry->mVersion != ALL_VERSIONS) {
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
  MOZ_SEH_EXCEPT (EXCEPTION_EXECUTE_HANDLER) {
  }

  ::ReleaseSRWLockExclusive(&mLock);
}

static NativeNtBlockSet gBlockSet;

extern "C" void MOZ_EXPORT
NativeNtBlockSet_Write(HANDLE aHandle)
{
  gBlockSet.Write(aHandle);
}

static bool
CheckBlockInfo(const DllBlockInfo* aInfo, void* aBaseAddress, uint64_t& aVersion)
{
  aVersion = ALL_VERSIONS;

  if (aInfo->flags & (DllBlockInfo::BLOCK_WIN8PLUS_ONLY | DllBlockInfo::BLOCK_WIN8_ONLY)) {
    RTL_OSVERSIONINFOW osv;
    NTSTATUS ntStatus = ::RtlGetVersion(&osv);
    if (!NT_SUCCESS(ntStatus)) {
      // huh?
      return false;
    }

    if (osv.dwMajorVersion < 8) {
      return true;
    }

    if ((aInfo->flags & DllBlockInfo::BLOCK_WIN8_ONLY) && (osv.dwMajorVersion > 8 ||
          (osv.dwMajorVersion == 8 && osv.dwMinorVersion > 0))) {
      return true;
    }
  }

  // We're not bootstrapping child processes at this time, so this case is
  // always true.
  if (aInfo->flags & DllBlockInfo::CHILD_PROCESSES_ONLY) {
    return true;
  }

  if (aInfo->maxVersion == ALL_VERSIONS) {
    return false;
  }

  mozilla::nt::PEHeaders headers(aBaseAddress);
  if (!headers) {
    return false;
  }

  if (aInfo->flags & DllBlockInfo::USE_TIMESTAMP) {
    DWORD timestamp;
    if (!headers.GetTimeStamp(timestamp)) {
      return false;
    }

    return timestamp > aInfo->maxVersion;
  }

  // Else we try to get the file version information. Note that we don't have
  // access to GetFileVersionInfo* APIs.
  if (!headers.GetVersionInfo(aVersion)) {
    return false;
  }

  return aVersion > aInfo->maxVersion;
}

static bool
IsDllAllowed(const UNICODE_STRING& aLeafName, void* aBaseAddress)
{
  if (mozilla::nt::Contains12DigitHexString(aLeafName) ||
      mozilla::nt::IsFileNameAtLeast16HexDigits(aLeafName)) {
    return false;
  }

  DECLARE_POINTER_TO_FIRST_DLL_BLOCKLIST_ENTRY(info);
  DECLARE_POINTER_TO_LAST_DLL_BLOCKLIST_ENTRY(end);

  while (info < end) {
    if (::RtlEqualUnicodeString(&aLeafName, &info->name, TRUE)) {
      break;
    }

    ++info;
  }

  uint64_t version;
  if (info->name.Length && !CheckBlockInfo(info, aBaseAddress, version)) {
    gBlockSet.Add(info->name, version);
    return false;
  }

  return true;
}

typedef decltype(&NtMapViewOfSection) NtMapViewOfSection_func;
static mozilla::CrossProcessDllInterceptor::FuncHookType<NtMapViewOfSection_func>
  stub_NtMapViewOfSection;

static NTSTATUS NTAPI
patched_NtMapViewOfSection(HANDLE aSection, HANDLE aProcess, PVOID* aBaseAddress,
                           ULONG_PTR aZeroBits, SIZE_T aCommitSize,
                           PLARGE_INTEGER aSectionOffset, PSIZE_T aViewSize,
                           SECTION_INHERIT aInheritDisposition,
                           ULONG aAllocationType, ULONG aProtectionFlags)
{
  // We always map first, then we check for additional info after.
  NTSTATUS stubStatus = stub_NtMapViewOfSection(aSection, aProcess, aBaseAddress,
                                                aZeroBits, aCommitSize,
                                                aSectionOffset, aViewSize,
                                                aInheritDisposition,
                                                aAllocationType, aProtectionFlags);
  if (!NT_SUCCESS(stubStatus)) {
    return stubStatus;
  }

  if (aProcess != kCurrentProcess) {
    // We're only interested in mapping for the current process.
    return stubStatus;
  }

  // Do a query to see if the memory is MEM_IMAGE. If not, continue
  MEMORY_BASIC_INFORMATION mbi;
  NTSTATUS ntStatus = ::NtQueryVirtualMemory(aProcess, *aBaseAddress,
                                             MemoryBasicInformation, &mbi,
                                             sizeof(mbi), nullptr);
  if (!NT_SUCCESS(ntStatus)) {
    ::NtUnmapViewOfSection(aProcess, *aBaseAddress);
    return STATUS_ACCESS_DENIED;
  }

  // We don't care about mappings that aren't MEM_IMAGE
  if (!(mbi.Type & MEM_IMAGE)) {
    return stubStatus;
  }

  // Get the section name
  mozilla::nt::MemorySectionNameBuf buf;

  ntStatus = ::NtQueryVirtualMemory(aProcess, *aBaseAddress, MemorySectionName,
                                    &buf, sizeof(buf), nullptr);
  if (!NT_SUCCESS(ntStatus)) {
    ::NtUnmapViewOfSection(aProcess, *aBaseAddress);
    return STATUS_ACCESS_DENIED;
  }

  // Find the leaf name
  UNICODE_STRING leaf;
  mozilla::nt::GetLeafName(&leaf, &buf.mSectionFileName);

  // Check blocklist
  if (IsDllAllowed(leaf, *aBaseAddress)) {
    return stubStatus;
  }

  ::NtUnmapViewOfSection(aProcess, *aBaseAddress);
  return STATUS_ACCESS_DENIED;
}

namespace mozilla {

class MOZ_RAII AutoVirtualProtect final
{
public:
  AutoVirtualProtect(void* aAddress, size_t aLength, DWORD aProtFlags,
                     HANDLE aTargetProcess = nullptr)
    : mAddress(aAddress)
    , mLength(aLength)
    , mTargetProcess(aTargetProcess)
    , mPrevProt(0)
  {
    ::VirtualProtectEx(aTargetProcess, aAddress, aLength, aProtFlags,
                       &mPrevProt);
  }

  ~AutoVirtualProtect()
  {
    if (!mPrevProt) {
      return;
    }

    ::VirtualProtectEx(mTargetProcess, mAddress, mLength, mPrevProt,
                       &mPrevProt);
  }

  explicit operator bool() const
  {
    return !!mPrevProt;
  }

  AutoVirtualProtect(const AutoVirtualProtect&) = delete;
  AutoVirtualProtect(AutoVirtualProtect&&) = delete;
  AutoVirtualProtect& operator=(const AutoVirtualProtect&) = delete;
  AutoVirtualProtect& operator=(AutoVirtualProtect&&) = delete;

private:
  void*   mAddress;
  size_t  mLength;
  HANDLE  mTargetProcess;
  DWORD   mPrevProt;
};

bool
InitializeDllBlocklistOOP(HANDLE aChildProcess)
{
  mozilla::CrossProcessDllInterceptor intcpt(aChildProcess);
  intcpt.Init(L"ntdll.dll");
  bool ok = stub_NtMapViewOfSection.SetDetour(aChildProcess, intcpt,
                                              "NtMapViewOfSection",
                                              &patched_NtMapViewOfSection);
  if (!ok) {
    return false;
  }

  // Because aChildProcess has just been created in a suspended state, its
  // dynamic linker has not yet been initialized, thus its executable has
  // not yet been linked with ntdll.dll. If the blocklist hook intercepts a
  // library load prior to the link, the hook will be unable to invoke any
  // ntdll.dll functions.
  //
  // We know that the executable for our *current* process's binary is already
  // linked into ntdll, so we obtain the IAT from our own executable and graft
  // it onto the child process's IAT, thus enabling the child process's hook to
  // safely make its ntdll calls.
  mozilla::nt::PEHeaders ourExeImage(::GetModuleHandleW(nullptr));
  if (!ourExeImage) {
    return false;
  }

  PIMAGE_IMPORT_DESCRIPTOR impDesc = ourExeImage.GetIATForModule("ntdll.dll");
  if (!impDesc) {
    return false;
  }

  // This is the pointer we need to write
  auto firstIatThunk = ourExeImage.template
    RVAToPtr<PIMAGE_THUNK_DATA>(impDesc->FirstThunk);
  if (!firstIatThunk) {
    return false;
  }

  // Find the length by iterating through the table until we find a null entry
  PIMAGE_THUNK_DATA curIatThunk = firstIatThunk;
  while (mozilla::nt::PEHeaders::IsValid(curIatThunk)) {
    ++curIatThunk;
  }

  ptrdiff_t iatLength = (curIatThunk - firstIatThunk) * sizeof(IMAGE_THUNK_DATA);

  SIZE_T bytesWritten;

  { // Scope for prot
    AutoVirtualProtect prot(firstIatThunk, iatLength, PAGE_READWRITE,
                            aChildProcess);
    if (!prot) {
      return false;
    }

    ok = !!::WriteProcessMemory(aChildProcess, firstIatThunk, firstIatThunk,
                                iatLength, &bytesWritten);
    if (!ok) {
      return false;
    }
  }

  // Tell the mozglue blocklist that we have bootstrapped
  uint32_t newFlags = eDllBlocklistInitFlagWasBootstrapped;
  ok = !!::WriteProcessMemory(aChildProcess, &gBlocklistInitFlags, &newFlags,
                              sizeof(newFlags), &bytesWritten);
  return ok;
}

} // namespace mozilla
