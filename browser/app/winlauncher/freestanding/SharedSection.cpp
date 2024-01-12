/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "SharedSection.h"

#include <algorithm>
#include "CheckForCaller.h"
#include "mozilla/BinarySearch.h"

namespace {

bool AddString(mozilla::Span<wchar_t> aBuffer, const UNICODE_STRING& aStr) {
  size_t offsetElements = 0;
  while (offsetElements < aBuffer.Length()) {
    UNICODE_STRING uniStr;
    ::RtlInitUnicodeString(&uniStr, aBuffer.data() + offsetElements);

    if (uniStr.Length == 0) {
      // Reached to the array's last item.
      break;
    }

    if (::RtlCompareUnicodeString(&uniStr, &aStr, TRUE) == 0) {
      // Already included in the array.
      return true;
    }

    // Go to the next string.
    offsetElements += uniStr.MaximumLength / sizeof(wchar_t);
  }

  // Ensure enough space including the last empty string at the end.
  if (offsetElements * sizeof(wchar_t) + aStr.Length + sizeof(wchar_t) +
          sizeof(wchar_t) >
      aBuffer.LengthBytes()) {
    return false;
  }

  auto newStr = aBuffer.Subspan(offsetElements);
  memcpy(newStr.data(), aStr.Buffer, aStr.Length);
  memset(newStr.data() + aStr.Length / sizeof(wchar_t), 0, sizeof(wchar_t));
  return true;
}

}  // anonymous namespace

namespace mozilla {
namespace freestanding {

SharedSection gSharedSection;

// Why don't we use ::GetProcAddress?
// If the export table of kernel32.dll is tampered in the current process,
// we cannot transfer an RVA because the function pointed by the RVA may not
// exist in a target process.
// We can use ::GetProcAddress with additional check to detect tampering, but
// FindExportAddressTableEntry fits perfectly here because it returns nullptr
// if the target entry is outside the image, which means it's tampered or
// forwarded to another DLL.
#define INIT_FUNCTION(exports, name)                                 \
  do {                                                               \
    auto rvaToFunction = exports.FindExportAddressTableEntry(#name); \
    if (!rvaToFunction) {                                            \
      return;                                                        \
    }                                                                \
    m##name = reinterpret_cast<decltype(m##name)>(*rvaToFunction);   \
  } while (0)

#define RESOLVE_FUNCTION(base, name)             \
  m##name = reinterpret_cast<decltype(m##name)>( \
      base + reinterpret_cast<uintptr_t>(m##name))

void Kernel32ExportsSolver::Init() {
  interceptor::MMPolicyInProcess policy;
  auto k32Exports = nt::PEExportSection<interceptor::MMPolicyInProcess>::Get(
      ::GetModuleHandleW(L"kernel32.dll"), policy);
  if (!k32Exports) {
    return;
  }

  // Please make sure these functions are not forwarded to another DLL.
  INIT_FUNCTION(k32Exports, FlushInstructionCache);
  INIT_FUNCTION(k32Exports, GetModuleHandleW);
  INIT_FUNCTION(k32Exports, GetSystemInfo);
  INIT_FUNCTION(k32Exports, VirtualProtect);
}

bool Kernel32ExportsSolver::Resolve() {
  const UNICODE_STRING k32Name = MOZ_LITERAL_UNICODE_STRING(L"kernel32.dll");

  // We cannot use GetModuleHandleW because this code can be called
  // before IAT is resolved.
  auto k32Module = nt::GetModuleHandleFromLeafName(k32Name);
  if (k32Module.isErr()) {
    // Probably this is called before kernel32.dll is loaded.
    return false;
  }

  uintptr_t k32Base =
      nt::PEHeaders::HModuleToBaseAddr<uintptr_t>(k32Module.unwrap());

  RESOLVE_FUNCTION(k32Base, FlushInstructionCache);
  RESOLVE_FUNCTION(k32Base, GetModuleHandleW);
  RESOLVE_FUNCTION(k32Base, GetSystemInfo);
  RESOLVE_FUNCTION(k32Base, VirtualProtect);

  return true;
}

HANDLE SharedSection::sSectionHandle = nullptr;
SharedSection::Layout* SharedSection::sWriteCopyView = nullptr;
RTL_RUN_ONCE SharedSection::sEnsureOnce = RTL_RUN_ONCE_INIT;
nt::SRWLock SharedSection::sLock;

void SharedSection::Reset(HANDLE aNewSectionObject) {
  nt::AutoExclusiveLock{sLock};
  if (sWriteCopyView) {
    nt::AutoMappedView view(sWriteCopyView);
    sWriteCopyView = nullptr;
    ::RtlRunOnceInitialize(&sEnsureOnce);
  }

  if (sSectionHandle != aNewSectionObject) {
    if (sSectionHandle) {
      ::CloseHandle(sSectionHandle);
    }
    sSectionHandle = aNewSectionObject;
  }
}

void SharedSection::ConvertToReadOnly() {
  if (!sSectionHandle) {
    return;
  }

  HANDLE readonlyHandle;
  if (!::DuplicateHandle(nt::kCurrentProcess, sSectionHandle,
                         nt::kCurrentProcess, &readonlyHandle, GENERIC_READ,
                         FALSE, 0)) {
    return;
  }

  Reset(readonlyHandle);
}

LauncherVoidResult SharedSection::Init() {
  static_assert(
      kSharedViewSize >= sizeof(Layout),
      "kSharedViewSize is too small to represent SharedSection::Layout.");

  HANDLE section =
      ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                           kSharedViewSize, nullptr);
  if (!section) {
    return LAUNCHER_ERROR_FROM_LAST();
  }
  Reset(section);

  // The initial contents of the pages in a file mapping object backed by
  // the operating system paging file are 0 (zero).  No need to zero it out
  // ourselves.
  // https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-createfilemappingw
  nt::AutoMappedView writableView(sSectionHandle, PAGE_READWRITE);
  if (!writableView) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  Layout* view = writableView.as<Layout>();
  view->mK32Exports.Init();
  view->mState = Layout::State::kInitialized;
  // Leave view->mDependentModulePathArrayStart to be zero to indicate
  // we can add blocklist entries
  return Ok();
}

LauncherVoidResult SharedSection::AddDependentModule(PCUNICODE_STRING aNtPath) {
  nt::AutoMappedView writableView(sSectionHandle, PAGE_READWRITE);
  if (!writableView) {
    return LAUNCHER_ERROR_FROM_WIN32(::RtlGetLastWin32Error());
  }

  Layout* view = writableView.as<Layout>();
  if (!view->mDependentModulePathArrayStart) {
    // This is the first time AddDependentModule is called.  We set the initial
    // value to mDependentModulePathArrayStart, which *closes* the blocklist.
    // After this, AddBlocklist is no longer allowed.
    view->mDependentModulePathArrayStart =
        FIELD_OFFSET(Layout, mFirstBlockEntry) + sizeof(DllBlockInfo);
  }

  if (!AddString(view->GetDependentModules(), *aNtPath)) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
  }

  return Ok();
}

LauncherVoidResult SharedSection::SetBlocklist(
    const DynamicBlockList& aBlocklist, bool isDisabled) {
  if (!aBlocklist.GetPayloadSize()) {
    return Ok();
  }

  nt::AutoMappedView writableView(sSectionHandle, PAGE_READWRITE);
  if (!writableView) {
    return LAUNCHER_ERROR_FROM_WIN32(::RtlGetLastWin32Error());
  }

  Layout* view = writableView.as<Layout>();
  if (view->mDependentModulePathArrayStart > 0) {
    // If the dependent module array is already available, we must not update
    // the blocklist.
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_INVALID_STATE);
  }

  view->mBlocklistIsDisabled = isDisabled ? 1 : 0;
  uintptr_t bufferEnd = reinterpret_cast<uintptr_t>(view) + kSharedViewSize;
  size_t bytesCopied = aBlocklist.CopyTo(
      view->mFirstBlockEntry,
      bufferEnd - reinterpret_cast<uintptr_t>(view->mFirstBlockEntry));
  if (!bytesCopied) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
  }

  // Setting mDependentModulePathArrayStart to a non-zero value means
  // we no longer accept blocklist entries
  // Just to be safe, make sure we don't overwrite mFirstBlockEntry even
  // if there are no entries.
  view->mDependentModulePathArrayStart =
      FIELD_OFFSET(Layout, mFirstBlockEntry) +
      std::max(bytesCopied, sizeof(DllBlockInfo));
  return Ok();
}

/* static */
ULONG NTAPI SharedSection::EnsureWriteCopyViewOnce(PRTL_RUN_ONCE, PVOID,
                                                   PVOID*) {
  if (!sWriteCopyView) {
    nt::AutoMappedView view(sSectionHandle, PAGE_WRITECOPY);
    if (!view) {
      return TRUE;
    }
    sWriteCopyView = view.as<Layout>();
    view.release();
  }
  return sWriteCopyView->Resolve() ? TRUE : FALSE;
}

SharedSection::Layout* SharedSection::EnsureWriteCopyView(
    bool requireKernel32Exports /*= false */) {
  ::RtlRunOnceExecuteOnce(&sEnsureOnce, &EnsureWriteCopyViewOnce, nullptr,
                          nullptr);
  if (!sWriteCopyView) {
    return nullptr;
  }
  auto requiredState = requireKernel32Exports
                           ? Layout::State::kResolved
                           : Layout::State::kLoadedDynamicBlocklistEntries;
  return sWriteCopyView->mState >= requiredState ? sWriteCopyView : nullptr;
}

bool SharedSection::Layout::Resolve() {
  if (mState == State::kResolved) {
    return true;
  }
  if (mState == State::kUninitialized) {
    return false;
  }
  if (mState == State::kInitialized) {
    if (!mNumBlockEntries) {
      uintptr_t arrayBase = reinterpret_cast<uintptr_t>(mFirstBlockEntry);
      uint32_t numEntries = 0;
      for (DllBlockInfo* entry = mFirstBlockEntry;
           entry->mName.Length && numEntries < GetMaxNumBlockEntries();
           ++entry) {
        entry->mName.Buffer = reinterpret_cast<wchar_t*>(
            arrayBase + reinterpret_cast<uintptr_t>(entry->mName.Buffer));
        ++numEntries;
      }
      mNumBlockEntries = numEntries;
      // Sort by name so that we can binary-search
      std::sort(mFirstBlockEntry, mFirstBlockEntry + mNumBlockEntries,
                [](const DllBlockInfo& a, const DllBlockInfo& b) {
                  return ::RtlCompareUnicodeString(&a.mName, &b.mName, TRUE) <
                         0;
                });
    }
    mState = State::kLoadedDynamicBlocklistEntries;
  }

  if (!mK32Exports.Resolve()) {
    return false;
  }

  mState = State::kResolved;
  return true;
}

Span<wchar_t> SharedSection::Layout::GetDependentModules() {
  if (!mDependentModulePathArrayStart) {
    return nullptr;
  }
  return Span<wchar_t>(
      reinterpret_cast<wchar_t*>(reinterpret_cast<uintptr_t>(this) +
                                 mDependentModulePathArrayStart),
      (kSharedViewSize - mDependentModulePathArrayStart) / sizeof(wchar_t));
}

bool SharedSection::Layout::IsDisabled() const {
  return !!mBlocklistIsDisabled;
}

const DllBlockInfo* SharedSection::Layout::SearchBlocklist(
    const UNICODE_STRING& aLeafName) const {
  MOZ_ASSERT(mState >= State::kLoadedDynamicBlocklistEntries);
  DllBlockInfoComparator comp(aLeafName);
  size_t match;
  if (!BinarySearchIf(mFirstBlockEntry, 0, mNumBlockEntries, comp, &match)) {
    return nullptr;
  }
  return &mFirstBlockEntry[match];
}

Kernel32ExportsSolver* SharedSection::GetKernel32Exports() {
  Layout* writeCopyView = EnsureWriteCopyView(true);
  return writeCopyView ? &writeCopyView->mK32Exports : nullptr;
}

Maybe<Vector<const wchar_t*>> SharedSection::GetDependentModules() {
  Layout* writeCopyView = EnsureWriteCopyView();
  if (!writeCopyView) {
    return Nothing();
  }

  mozilla::Span<wchar_t> dependentModules =
      writeCopyView->GetDependentModules();
  // Convert a null-delimited string set to a string vector.
  Vector<const wchar_t*> paths;
  for (const wchar_t* p = dependentModules.data();
       (p - dependentModules.data() <
            static_cast<long long>(dependentModules.size()) &&
        *p);) {
    if (MOZ_UNLIKELY(!paths.append(p))) {
      return Nothing();
    }
    while (*p) {
      ++p;
    }
    ++p;
  }
  return Some(std::move(paths));
}

Span<const DllBlockInfo> SharedSection::GetDynamicBlocklist() {
  Layout* writeCopyView = EnsureWriteCopyView();
  return writeCopyView ? writeCopyView->GetModulePathArray() : nullptr;
}

const DllBlockInfo* SharedSection::SearchBlocklist(
    const UNICODE_STRING& aLeafName) {
  Layout* writeCopyView = EnsureWriteCopyView();
  return writeCopyView ? writeCopyView->SearchBlocklist(aLeafName) : nullptr;
}

bool SharedSection::IsDisabled() {
  Layout* writeCopyView = EnsureWriteCopyView();
  return writeCopyView ? writeCopyView->IsDisabled() : false;
}

LauncherVoidResult SharedSection::TransferHandle(
    nt::CrossExecTransferManager& aTransferMgr, DWORD aDesiredAccess,
    HANDLE* aDestinationAddress) {
  HANDLE remoteHandle;
  if (!::DuplicateHandle(nt::kCurrentProcess, sSectionHandle,
                         aTransferMgr.RemoteProcess(), &remoteHandle,
                         aDesiredAccess, FALSE, 0)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  return aTransferMgr.Transfer(aDestinationAddress, &remoteHandle,
                               sizeof(remoteHandle));
}

}  // namespace freestanding
}  // namespace mozilla
