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

#include "CrashAnnotations.h"
#include "DllBlocklist.h"
#include "LoaderPrivateAPI.h"
#include "ModuleLoadFrame.h"
#include "SharedSection.h"

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

using WritableBuffer = mozilla::glue::detail::WritableBuffer<1024>;

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
  void Write(WritableBuffer& buffer);

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

void NativeNtBlockSet::Write(WritableBuffer& aBuffer) {
  // NB: If this function is called, it is long after kernel32 is initialized,
  // so it is safe to use Win32 calls here.
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
      aBuffer.Write(buf, convOk);

      if (entry->mVersion != DllBlockInfo::ALL_VERSIONS) {
        aBuffer.Write(",", 1);
        uint16_t parts[4];
        parts[0] = entry->mVersion >> 48;
        parts[1] = (entry->mVersion >> 32) & 0xFFFF;
        parts[2] = (entry->mVersion >> 16) & 0xFFFF;
        parts[3] = entry->mVersion & 0xFFFF;
        for (size_t p = 0; p < mozilla::ArrayLength(parts); ++p) {
          _ltoa_s(parts[p], buf, sizeof(buf), 10);
          aBuffer.Write(buf, strlen(buf));
          if (p != mozilla::ArrayLength(parts) - 1) {
            aBuffer.Write(".", 1);
          }
        }
      }
      aBuffer.Write(";", 1);
    }
  }
  MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {}

  ::ReleaseSRWLockExclusive(&mLock);
}

static NativeNtBlockSet gBlockSet;

extern "C" void MOZ_EXPORT
NativeNtBlockSet_Write(CrashReporter::AnnotationWriter& aWriter) {
  WritableBuffer buffer;
  gBlockSet.Write(buffer);
  aWriter.Write(CrashReporter::Annotation::BlockedDllList, buffer.Data(),
                buffer.Length());
}

enum class BlockAction {
  // Allow the DLL to be loaded.
  Allow,
  // Substitute in a different DLL to be loaded instead of this one?
  // This is intended to be used for Layered Service Providers, which
  // cannot be blocked in the normal way. Note that this doesn't seem
  // to be actually implemented right now, and no entries in the blocklist
  // use it.
  SubstituteLSP,
  // There was an error in determining whether we should block this DLL.
  // It will be blocked.
  Error,
  // Block the DLL from loading.
  Deny,
  // Effectively block the DLL from loading by redirecting its DllMain
  // to a stub version. This is needed for DLLs that add themselves to
  // the executable's Import Table, since failing to load would mean the
  // executable would fail to launch.
  NoOpEntryPoint,
};

static BlockAction CheckBlockInfo(const DllBlockInfo* aInfo,
                                  const mozilla::nt::PEHeaders& aHeaders,
                                  uint64_t& aVersion) {
  aVersion = DllBlockInfo::ALL_VERSIONS;

  if (aInfo->mFlags & (DllBlockInfo::BLOCK_WIN8_AND_OLDER |
                       DllBlockInfo::BLOCK_WIN7_AND_OLDER)) {
    RTL_OSVERSIONINFOW osv = {sizeof(osv)};
    NTSTATUS ntStatus = ::RtlGetVersion(&osv);
    if (!NT_SUCCESS(ntStatus)) {
      return BlockAction::Error;
    }

    if ((aInfo->mFlags & DllBlockInfo::BLOCK_WIN8_AND_OLDER) &&
        (osv.dwMajorVersion > 6 ||
         (osv.dwMajorVersion == 6 && osv.dwMinorVersion > 2))) {
      return BlockAction::Allow;
    }

    if ((aInfo->mFlags & DllBlockInfo::BLOCK_WIN7_AND_OLDER) &&
        (osv.dwMajorVersion > 6 ||
         (osv.dwMajorVersion == 6 && osv.dwMinorVersion > 1))) {
      return BlockAction::Allow;
    }
  }

  if ((aInfo->mFlags & DllBlockInfo::CHILD_PROCESSES_ONLY) &&
      !(gBlocklistInitFlags & eDllBlocklistInitFlagIsChildProcess)) {
    return BlockAction::Allow;
  }

  if ((aInfo->mFlags & DllBlockInfo::UTILITY_PROCESSES_ONLY) &&
      !(gBlocklistInitFlags & eDllBlocklistInitFlagIsUtilityProcess)) {
    return BlockAction::Allow;
  }

  if ((aInfo->mFlags & DllBlockInfo::SOCKET_PROCESSES_ONLY) &&
      !(gBlocklistInitFlags & eDllBlocklistInitFlagIsSocketProcess)) {
    return BlockAction::Allow;
  }

  if ((aInfo->mFlags & DllBlockInfo::GPU_PROCESSES_ONLY) &&
      !(gBlocklistInitFlags & eDllBlocklistInitFlagIsGPUProcess)) {
    return BlockAction::Allow;
  }

  if ((aInfo->mFlags & DllBlockInfo::BROWSER_PROCESS_ONLY) &&
      (gBlocklistInitFlags & eDllBlocklistInitFlagIsChildProcess)) {
    return BlockAction::Allow;
  }

  if ((aInfo->mFlags & DllBlockInfo::GMPLUGIN_PROCESSES_ONLY) &&
      !(gBlocklistInitFlags & eDllBlocklistInitFlagIsGMPluginProcess)) {
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

static BOOL WINAPI NoOp_DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }

// This helper function checks whether a given module is included
// in the executable's Import Table.  Because an injected module's
// DllMain may revert the Import Table to the original state, we parse
// the Import Table every time a module is loaded without creating a cache.
static bool IsInjectedDependentModule(
    const UNICODE_STRING& aModuleLeafName,
    mozilla::freestanding::Kernel32ExportsSolver& aK32Exports) {
  mozilla::nt::PEHeaders exeHeaders(aK32Exports.mGetModuleHandleW(nullptr));
  if (!exeHeaders || !exeHeaders.IsImportDirectoryTampered()) {
    // If no tampering is detected, no need to enumerate the Import Table.
    return false;
  }

  bool isDependent = false;
  exeHeaders.EnumImportChunks(
      [&isDependent, &aModuleLeafName, &exeHeaders](const char* aDepModule) {
        // If |aDepModule| is within the PE image, it's not an injected module
        // but a legitimate dependent module.
        if (isDependent || exeHeaders.IsWithinImage(aDepModule)) {
          return;
        }

        UNICODE_STRING depModuleLeafName;
        mozilla::nt::AllocatedUnicodeString depModuleName(aDepModule);
        mozilla::nt::GetLeafName(&depModuleLeafName, depModuleName);
        isDependent = (::RtlCompareUnicodeString(
                           &aModuleLeafName, &depModuleLeafName, TRUE) == 0);
      });
  return isDependent;
}

// Allowing a module to be loaded but detour the entrypoint to NoOp_DllMain
// so that the module has no chance to interact with our code.  We need this
// technique to safely block a module injected by IAT tampering because
// blocking such a module makes a process fail to launch.
static bool RedirectToNoOpEntryPoint(
    const mozilla::nt::PEHeaders& aModule,
    mozilla::freestanding::Kernel32ExportsSolver& aK32Exports) {
  mozilla::interceptor::WindowsDllEntryPointInterceptor interceptor(
      aK32Exports);
  if (!interceptor.Set(aModule, NoOp_DllMain)) {
    return false;
  }

  return true;
}

static BlockAction DetermineBlockAction(
    const UNICODE_STRING& aLeafName, void* aBaseAddress,
    mozilla::freestanding::Kernel32ExportsSolver* aK32Exports) {
  if (mozilla::nt::Contains12DigitHexString(aLeafName) ||
      mozilla::nt::IsFileNameAtLeast16HexDigits(aLeafName)) {
    return BlockAction::Deny;
  }

  DECLARE_POINTER_TO_FIRST_DLL_BLOCKLIST_ENTRY(info);
  DECLARE_DLL_BLOCKLIST_NUM_ENTRIES(infoNumEntries);

  mozilla::freestanding::DllBlockInfoComparator comp(aLeafName);

  size_t match = LowerBound(info, 0, infoNumEntries, comp);
  bool builtinListHasLowerBound = match != infoNumEntries;
  const DllBlockInfo* entry = nullptr;
  mozilla::nt::PEHeaders headers(aBaseAddress);
  uint64_t version;
  BlockAction checkResult = BlockAction::Allow;
  if (builtinListHasLowerBound) {
    // There may be multiple entries on the list. Since LowerBound() returns
    // the first entry that matches (if there are any matches),
    // search forward from there.
    while (match < infoNumEntries && (comp(info[match]) == 0)) {
      entry = &info[match];
      checkResult = CheckBlockInfo(entry, headers, version);
      if (checkResult != BlockAction::Allow) {
        break;
      }
      ++match;
    }
  }
  mozilla::DebugOnly<bool> blockedByDynamicBlocklist = false;
  // Make sure we handle a case that older versions are blocked by the static
  // list, but the dynamic list blocks all versions.
  if (checkResult == BlockAction::Allow) {
    if (!mozilla::freestanding::gSharedSection.IsDisabled()) {
      entry = mozilla::freestanding::gSharedSection.SearchBlocklist(aLeafName);
      if (entry) {
        checkResult = CheckBlockInfo(entry, headers, version);
        blockedByDynamicBlocklist = checkResult != BlockAction::Allow;
      }
    }
  }
  if (checkResult == BlockAction::Allow) {
    return BlockAction::Allow;
  }

  gBlockSet.Add(entry->mName, version);

  if ((entry->mFlags & DllBlockInfo::REDIRECT_TO_NOOP_ENTRYPOINT) &&
      aK32Exports && RedirectToNoOpEntryPoint(headers, *aK32Exports)) {
    MOZ_ASSERT(!blockedByDynamicBlocklist, "dynamic blocklist has redirect?");
    return BlockAction::NoOpEntryPoint;
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
constexpr DWORD kPageExecutable = PAGE_EXECUTE | PAGE_EXECUTE_READ |
                                  PAGE_EXECUTE_READWRITE |
                                  PAGE_EXECUTE_WRITECOPY;

// All the code for patched_NtMapViewOfSection that relies on stack buffers
// (e.g. mbi and sectionFileName) should be put in this helper function (see
// bug 1733532).
MOZ_NEVER_INLINE NTSTATUS AfterMapExecutableViewOfSection(
    HANDLE aProcess, PVOID* aBaseAddress, NTSTATUS aStubStatus) {
  // Do a query to see if the memory is MEM_IMAGE. If not, continue
  MEMORY_BASIC_INFORMATION mbi;
  NTSTATUS ntStatus =
      ::NtQueryVirtualMemory(aProcess, *aBaseAddress, MemoryBasicInformation,
                             &mbi, sizeof(mbi), nullptr);
  if (!NT_SUCCESS(ntStatus)) {
    ::NtUnmapViewOfSection(aProcess, *aBaseAddress);
    return STATUS_ACCESS_DENIED;
  }

  // We don't care about mappings that aren't MEM_IMAGE or executable.
  // We check for the AllocationProtect, not the Protect field because
  // the first section of a mapped image is always PAGE_READONLY even
  // when it's mapped as an executable.
  if (!(mbi.Type & MEM_IMAGE) || !(mbi.AllocationProtect & kPageExecutable)) {
    return aStubStatus;
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

  bool isInjectedDependent = false;
  const UNICODE_STRING k32Name = MOZ_LITERAL_UNICODE_STRING(L"kernel32.dll");
  Kernel32ExportsSolver* k32Exports = nullptr;
  BlockAction blockAction;
  // Trying to get the Kernel32Exports while loading kernel32.dll causes Firefox
  // to crash. (but only during a profile-guided optimization run, oddly) We
  // know we're never going to block kernel32.dll, so skip all this
  if (::RtlCompareUnicodeString(&k32Name, &leafOnStack, TRUE) == 0) {
    blockAction = BlockAction::Allow;
  } else {
    k32Exports = gSharedSection.GetKernel32Exports();
    // Small optimization: Since loading a dependent module does not involve
    // LdrLoadDll, we know isInjectedDependent is false if we hold a top frame.
    if (k32Exports && !ModuleLoadFrame::ExistsTopFrame()) {
      // Note that if a module is dependent but not injected, this means that
      // the executable built against it, and it should be signed by Mozilla
      // or Microsoft, so we don't need to worry about adding it to the list
      // for CIG. (and users won't be able to block it) So the only special
      // case here is a dependent module that has been injected.
      isInjectedDependent = IsInjectedDependentModule(leafOnStack, *k32Exports);
    }

    if (isInjectedDependent) {
      // Add an NT dv\path to the shared section so that a sandbox process can
      // use it to bypass CIG.  In a sandbox process, this addition fails
      // because we cannot map the section to a writable region, but it's
      // ignorable because the paths have been added by the browser process.
      Unused << SharedSection::AddDependentModule(sectionFileName);

      bool attemptToBlockViaRedirect;
#if defined(NIGHTLY_BUILD)
      // We enable automatic DLL blocking only in Nightly for now
      // because it caused a compat issue (bug 1682304 and 1704373).
      attemptToBlockViaRedirect = true;
      // We will set blockAction below in the if (attemptToBlockViaRedirect)
      // block, but I guess the compiler isn't smart enough to figure
      // that out and complains about an uninitialized variable :-(
      blockAction = BlockAction::NoOpEntryPoint;
#else
      // Check blocklist
      blockAction =
          DetermineBlockAction(leafOnStack, *aBaseAddress, k32Exports);
      // If we were going to block this dependent module, try redirection
      // instead of blocking it, since blocking it would cause the .exe not to
      // launch.
      // Note tht Deny and Error both end up blocking the module in a
      // straightforward way, so those are the cases in which we need
      // to redirect instead.
      attemptToBlockViaRedirect =
          blockAction == BlockAction::Deny || blockAction == BlockAction::Error;
#endif
      if (attemptToBlockViaRedirect) {
        // For a dependent module, try redirection instead of blocking it.
        // If we fail, we reluctantly allow the module for free.
        mozilla::nt::PEHeaders headers(*aBaseAddress);
        blockAction = RedirectToNoOpEntryPoint(headers, *k32Exports)
                          ? BlockAction::NoOpEntryPoint
                          : BlockAction::Allow;
      }
    } else {
      // Check blocklist
      blockAction =
          DetermineBlockAction(leafOnStack, *aBaseAddress, k32Exports);
    }
  }

  ModuleLoadInfo::Status loadStatus = ModuleLoadInfo::Status::Blocked;

  switch (blockAction) {
    case BlockAction::Allow:
      loadStatus = ModuleLoadInfo::Status::Loaded;
      break;

    case BlockAction::NoOpEntryPoint:
      loadStatus = ModuleLoadInfo::Status::Redirected;
      break;

    case BlockAction::SubstituteLSP:
      // The process heap needs to be available here because
      // NotifyLSPSubstitutionRequired below copies a given string into
      // the heap. We use a soft assert here, assuming LSP load always
      // occurs after the heap is initialized.
      MOZ_ASSERT(nt::RtlGetProcessHeap());

      // Notify patched_LdrLoadDll that it will be necessary to perform
      // a substitution before returning.
      ModuleLoadFrame::NotifyLSPSubstitutionRequired(&leafOnStack);
      break;

    default:
      break;
  }

  if (nt::RtlGetProcessHeap()) {
    ModuleLoadFrame::NotifySectionMap(
        nt::AllocatedUnicodeString(sectionFileName), *aBaseAddress, aStubStatus,
        loadStatus, isInjectedDependent);
  }

  if (loadStatus == ModuleLoadInfo::Status::Loaded ||
      loadStatus == ModuleLoadInfo::Status::Redirected) {
    return aStubStatus;
  }

  ::NtUnmapViewOfSection(aProcess, *aBaseAddress);
  return STATUS_ACCESS_DENIED;
}

// To preserve compatibility with third-parties, calling into this function
// must not use stack buffers when reached through Thread32Next (see bug
// 1733532). Therefore, all code relying on stack buffers should be put in the
// dedicated helper function AfterMapExecutableViewOfSection.
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

  if (aProcess != nt::kCurrentProcess) {
    // We're only interested in mapping for the current process.
    return stubStatus;
  }

  if (!(aProtectionFlags & kPageExecutable)) {
    // Bail out early if an executable mapping was not asked. In particular,
    // we will not use stack buffers during calls to Thread32Next, which can
    // result in crashes with third-party software (see bug 1733532).
    return stubStatus;
  }

  return AfterMapExecutableViewOfSection(aProcess, aBaseAddress, stubStatus);
}

}  // namespace freestanding
}  // namespace mozilla
