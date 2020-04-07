/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_freestanding_FunctionTableResolver_h
#define mozilla_freestanding_FunctionTableResolver_h

#include "mozilla/NativeNt.h"
#include "mozilla/interceptor/MMPolicies.h"

namespace mozilla {
namespace freestanding {

// This class calculates RVAs of kernel32's functions and transfers them
// to a target process, where the transferred RVAs are resolved into
// function addresses so that the target process can use them after
// kernel32.dll is loaded and before IAT is resolved.
class MOZ_TRIVIAL_CTOR_DTOR Kernel32ExportsSolver final
    : public interceptor::MMPolicyInProcessEarlyStage::Kernel32Exports {
  enum class State {
    Uninitialized,
    Initialized,
    Resolved,
  } mState;

  struct FunctionOffsets {
    uint32_t mFlushInstructionCache;
    uint32_t mGetSystemInfo;
    uint32_t mVirtualProtect;
  } mOffsets;

  static ULONG NTAPI ResolveOnce(PRTL_RUN_ONCE aRunOnce, PVOID aParameter,
                                 PVOID*);
  void ResolveInternal();

 public:
  Kernel32ExportsSolver() = default;

  Kernel32ExportsSolver(const Kernel32ExportsSolver&) = delete;
  Kernel32ExportsSolver(Kernel32ExportsSolver&&) = delete;
  Kernel32ExportsSolver& operator=(const Kernel32ExportsSolver&) = delete;
  Kernel32ExportsSolver& operator=(Kernel32ExportsSolver&&) = delete;

  bool IsInitialized() const;
  bool IsResolved() const;

  void Init();
  void Resolve(RTL_RUN_ONCE& aRunOnce);
  void Transfer(HANDLE aTargetProcess,
                Kernel32ExportsSolver* aTargetAddress) const;
};

extern Kernel32ExportsSolver gK32;

}  // namespace freestanding
}  // namespace mozilla

#endif  // mozilla_freestanding_FunctionTableResolver_h
