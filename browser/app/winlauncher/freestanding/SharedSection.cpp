/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "SharedSection.h"

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

  if (sSectionHandle) {
    ::CloseHandle(sSectionHandle);
  }
  sSectionHandle = aNewSecionObject;
}

static void PackOffsetVector(const Vector<nt::MemorySectionNameOnHeap>& aSource,
                             SharedSection::Layout& aDestination,
                             size_t aStringBufferOffset) {
  aDestination.mModulePathArrayLength = aSource.length();

  uint32_t* curItem = aDestination.mModulePathArray;
  uint8_t* const arrayBase = reinterpret_cast<uint8_t*>(curItem);
  for (const auto& it : aSource) {
    // Fill the current offset value
    *(curItem++) = aStringBufferOffset;

    // Fill a string and a null character
    uint32_t lenInBytes = it.AsUnicodeString()->Length;
    memcpy(arrayBase + aStringBufferOffset, it.AsUnicodeString()->Buffer,
           lenInBytes);
    memset(arrayBase + aStringBufferOffset + lenInBytes, 0, sizeof(WCHAR));

    // Advance the offset
    aStringBufferOffset += (lenInBytes + sizeof(WCHAR));
  }

  // Sort the offset array so that we can binary-search it
  std::sort(aDestination.mModulePathArray,
            aDestination.mModulePathArray + aSource.length(),
            [arrayBase](uint32_t a, uint32_t b) {
              auto s1 = reinterpret_cast<const wchar_t*>(arrayBase + a);
              auto s2 = reinterpret_cast<const wchar_t*>(arrayBase + b);
              return wcsicmp(s1, s2) < 0;
            });
}

LauncherVoidResult SharedSection::Init(const nt::PEHeaders& aPEHeaders) {
  size_t stringBufferSize = 0;
  Vector<nt::MemorySectionNameOnHeap> modules;

  // We enable automatic DLL blocking only in Nightly for now because it caused
  // a compat issue (bug 1682304).
#if defined(NIGHTLY_BUILD)
  aPEHeaders.EnumImportChunks(
      [&stringBufferSize, &modules, &aPEHeaders](const char* aModule) {
#  if defined(DONT_SKIP_DEFAULT_DEPENDENT_MODULES)
        Unused << aPEHeaders;
#  else
        if (aPEHeaders.IsWithinImage(aModule)) {
          return;
        }
#  endif
        HMODULE module = ::GetModuleHandleA(aModule);
        nt::MemorySectionNameOnHeap ntPath =
            nt::MemorySectionNameOnHeap::GetBackingFilePath(nt::kCurrentProcess,
                                                            module);
        stringBufferSize += (ntPath.AsUnicodeString()->Length + sizeof(WCHAR));
        Unused << modules.emplaceBack(std::move(ntPath));
      });
#endif

  size_t arraySize = modules.length() * sizeof(Layout::mModulePathArray[0]);
  size_t totalSize =
      sizeof(Kernel32ExportsSolver) + arraySize + stringBufferSize;

  HANDLE section = ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
                                        PAGE_READWRITE, 0, totalSize, nullptr);
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
  PackOffsetVector(modules, *view, arraySize);

  return Ok();
}

LauncherResult<SharedSection::Layout*> SharedSection::GetView() {
  if (!sWriteCopyView) {
    nt::AutoMappedView view(sSectionHandle, PAGE_WRITECOPY);
    if (!view) {
      return LAUNCHER_ERROR_FROM_LAST();
    }
    sWriteCopyView = view.release();
  }
  return reinterpret_cast<SharedSection::Layout*>(sWriteCopyView);
}

LauncherVoidResult SharedSection::TransferHandle(
    nt::CrossExecTransferManager& aTransferMgr, HANDLE* aDestinationAddress) {
  HANDLE remoteHandle;
  if (!::DuplicateHandle(nt::kCurrentProcess, sSectionHandle,
                         aTransferMgr.RemoteProcess(), &remoteHandle,
                         GENERIC_READ, FALSE, 0)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  return aTransferMgr.Transfer(aDestinationAddress, &remoteHandle,
                               sizeof(remoteHandle));
}

extern "C" MOZ_EXPORT uint32_t GetDependentModulePaths(uint32_t** aOutArray) {
  if (aOutArray) {
    *aOutArray = nullptr;
  }

  // We enable pre-spawn CIG only in Nightly for now because it caused
  // a compat issue (bug 1682304).
#if defined(NIGHTLY_BUILD)
  LauncherResult<SharedSection::Layout*> resultView = gSharedSection.GetView();
  if (resultView.isErr()) {
    return 0;
  }

  if (aOutArray) {
    // Return a valid address even if the array length is zero
    // to distinguish it from an error case.
    *aOutArray = resultView.inspect()->mModulePathArray;
  }
  return resultView.inspect()->mModulePathArrayLength;
#else
  return 0;
#endif
}

}  // namespace freestanding
}  // namespace mozilla
