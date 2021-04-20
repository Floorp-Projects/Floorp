/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "SharedSection.h"

#include "CheckForCaller.h"

namespace {

bool AddString(void* aBuffer, size_t aBufferSize, const UNICODE_STRING& aStr) {
  size_t offset = 0;
  while (offset < aBufferSize) {
    UNICODE_STRING uniStr;
    ::RtlInitUnicodeString(&uniStr,
                           reinterpret_cast<wchar_t*>(
                               reinterpret_cast<uintptr_t>(aBuffer) + offset));

    if (uniStr.Length == 0) {
      // Reached to the array's last item.
      break;
    }

    if (::RtlCompareUnicodeString(&uniStr, &aStr, TRUE) == 0) {
      // Already included in the array.
      return true;
    }

    // Go to the next string.
    offset += uniStr.MaximumLength;
  }

  // Ensure enough space including the last empty string at the end.
  if (offset + aStr.MaximumLength + sizeof(wchar_t) > aBufferSize) {
    return false;
  }

  auto newStr = reinterpret_cast<uint8_t*>(aBuffer) + offset;
  memcpy(newStr, aStr.Buffer, aStr.Length);
  memset(newStr + aStr.Length, 0, sizeof(wchar_t));
  return true;
}

}  // anonymous namespace

namespace mozilla {
namespace freestanding {

SharedSection gSharedSection;

bool Kernel32ExportsSolver::IsInitialized() const {
  return mState == State::Initialized || IsResolved();
}

bool Kernel32ExportsSolver::IsResolved() const {
  return mState == State::Resolved;
}

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
  if (mState == State::Initialized || mState == State::Resolved) {
    return;
  }

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

  mState = State::Initialized;
}

void Kernel32ExportsSolver::ResolveInternal() {
  if (mState == State::Resolved) {
    return;
  }

  MOZ_RELEASE_ASSERT(mState == State::Initialized);

  UNICODE_STRING k32Name;
  ::RtlInitUnicodeString(&k32Name, L"kernel32.dll");

  // We cannot use GetModuleHandleW because this code can be called
  // before IAT is resolved.
  auto k32Module = nt::GetModuleHandleFromLeafName(k32Name);
  MOZ_RELEASE_ASSERT(k32Module.isOk());

  uintptr_t k32Base =
      nt::PEHeaders::HModuleToBaseAddr<uintptr_t>(k32Module.unwrap());

  RESOLVE_FUNCTION(k32Base, FlushInstructionCache);
  RESOLVE_FUNCTION(k32Base, GetModuleHandleW);
  RESOLVE_FUNCTION(k32Base, GetSystemInfo);
  RESOLVE_FUNCTION(k32Base, VirtualProtect);

  mState = State::Resolved;
}

/* static */
ULONG NTAPI Kernel32ExportsSolver::ResolveOnce(PRTL_RUN_ONCE aRunOnce,
                                               PVOID aParameter, PVOID*) {
  reinterpret_cast<Kernel32ExportsSolver*>(aParameter)->ResolveInternal();
  return TRUE;
}

void Kernel32ExportsSolver::Resolve(RTL_RUN_ONCE& aRunOnce) {
  ::RtlRunOnceExecuteOnce(&aRunOnce, &ResolveOnce, this, nullptr);
}

HANDLE SharedSection::sSectionHandle = nullptr;
void* SharedSection::sWriteCopyView = nullptr;

void SharedSection::Reset(HANDLE aNewSecionObject) {
  if (sWriteCopyView) {
    nt::AutoMappedView view(sWriteCopyView);
    sWriteCopyView = nullptr;
  }

  if (sSectionHandle != aNewSecionObject) {
    if (sSectionHandle) {
      ::CloseHandle(sSectionHandle);
    }
    sSectionHandle = aNewSecionObject;
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

LauncherVoidResult SharedSection::Init(const nt::PEHeaders& aPEHeaders) {
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

  SharedSection::Layout* view = writableView.as<SharedSection::Layout>();
  view->mK32Exports.Init();

  return Ok();
}

LauncherVoidResult SharedSection::AddDepenentModule(PCUNICODE_STRING aNtPath) {
  nt::AutoMappedView writableView(sSectionHandle, PAGE_READWRITE);
  if (!writableView) {
    return LAUNCHER_ERROR_FROM_WIN32(::RtlGetLastWin32Error());
  }

  SharedSection::Layout* view = writableView.as<SharedSection::Layout>();
  if (!AddString(view->mModulePathArray,
                 kSharedViewSize - sizeof(Kernel32ExportsSolver), *aNtPath)) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
  }

  return Ok();
}

LauncherResult<SharedSection::Layout*> SharedSection::GetView() {
  if (!sWriteCopyView) {
    nt::AutoMappedView view(sSectionHandle, PAGE_WRITECOPY);
    if (!view) {
      return LAUNCHER_ERROR_FROM_WIN32(::RtlGetLastWin32Error());
    }
    sWriteCopyView = view.release();
  }
  return reinterpret_cast<SharedSection::Layout*>(sWriteCopyView);
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

// This exported function is invoked by SandboxBroker of xul.dll
// in order to add dependent modules to the CIG exception list.
extern "C" MOZ_EXPORT const wchar_t* GetDependentModulePaths() {
  // We enable pre-spawn CIG only in early Beta or earlier for now
  // because it caused a compat issue (bug 1682304 and 1704373).
#if defined(EARLY_BETA_OR_EARLIER)
  const bool isCallerXul = CheckForAddress(RETURN_ADDRESS(), L"xul.dll");
  MOZ_ASSERT(isCallerXul);
  if (!isCallerXul) {
    return nullptr;
  }

  // Remap a write-copy section to take the latest update in mModulePathArray.
  gSharedSection.Reset();

  LauncherResult<SharedSection::Layout*> resultView = gSharedSection.GetView();
  if (resultView.isErr()) {
    return nullptr;
  }

  return resultView.inspect()->mModulePathArray;
#else
  return nullptr;
#endif
}

}  // namespace freestanding
}  // namespace mozilla
