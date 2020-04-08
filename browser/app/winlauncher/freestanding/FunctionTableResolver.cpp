/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "FunctionTableResolver.h"

namespace mozilla {
namespace freestanding {

Kernel32ExportsSolver gK32;

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
    mOffsets.m##name = *rvaToFunction;                               \
  } while (0)

#define RESOLVE_FUNCTION(base, name) \
  m##name = reinterpret_cast<decltype(m##name)>(base + mOffsets.m##name)

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

void Kernel32ExportsSolver::Transfer(
    HANDLE aTargetProcess, Kernel32ExportsSolver* aTargetAddress) const {
  SIZE_T bytesWritten = 0;
  BOOL ok = ::WriteProcessMemory(aTargetProcess, &aTargetAddress->mOffsets,
                                 &mOffsets, sizeof(mOffsets), &bytesWritten);
  if (!ok) {
    return;
  }

  State stateInChild = State::Initialized;
  ::WriteProcessMemory(aTargetProcess, &aTargetAddress->mState, &stateInChild,
                       sizeof(stateInChild), &bytesWritten);
}

}  // namespace freestanding
}  // namespace mozilla
