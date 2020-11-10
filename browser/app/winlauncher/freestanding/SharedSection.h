/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_freestanding_SharedSection_h
#define mozilla_freestanding_SharedSection_h

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
};

// This class manages a section which is created in the launcher process and
// mapped in the browser process and the sandboxed processes as a copy-on-write
// region.  The section's layout is represented as SharedSection::Layout.
//
// (1) Kernel32's functions required for MMPolicyInProcessEarlyStage
//     Formatted as Kernel32ExportsSolver.
//
// (2) Array of NT paths of the executable's dependent modules
//     Array item is an offset to a string buffer of a module's
//     NT path, relative to the beginning of the array.
//     Array is case-insensitive sorted.
//
// +--------------------------------------------------------------+
// | (1) | FlushInstructionCache                                  |
// |     | GetSystemInfo                                          |
// |     | VirtualProtect                                         |
// |     | State [Uninitialized|Initialized|Resolved]             |
// +--------------------------------------------------------------+
// | (2) | The length of the offset array                         |
// |     | Offset1 to String1                                     |
// |     |   * Offset is relative to the beginning of the array   |
// |     |   * String is an NT path in wchar_t                    |
// |     | Offset2 to String2                                     |
// |     | ...                                                    |
// |     | OffsetN to StringN                                     |
// |     | String1, 2, ..., N (null delimited strings)            |
// +--------------------------------------------------------------+
class MOZ_TRIVIAL_CTOR_DTOR SharedSection final {
  // As we define a global variable of this class and use it in our blocklist
  // which is excuted in a process's early stage.  If we have a complex dtor,
  // the static initializer tries to register that dtor with onexit() of
  // ucrtbase.dll which is not loaded yet, resulting in crash.  Thus, we have
  // a raw handle and a pointer as a static variable and manually release them
  // by calling Reset() where possible.
  static HANDLE sSectionHandle;
  static void* sWriteCopyView;

 public:
  struct Layout final {
    Kernel32ExportsSolver mK32Exports;
    uint32_t mModulePathArrayLength;
    uint32_t mModulePathArray[1];

    Layout() = delete;  // disallow instantiation
  };

  static void Reset(HANDLE aNewSecionObject);
  static LauncherVoidResult Init(const nt::PEHeaders& aPEHeaders);

  static LauncherResult<Layout*> GetView();
  static LauncherVoidResult TransferHandle(
      nt::CrossExecTransferManager& aTransferMgr,
      HANDLE* aDestinationAddress = &sSectionHandle);
};

extern SharedSection gSharedSection;

}  // namespace freestanding
}  // namespace mozilla

#endif  // mozilla_freestanding_SharedSection_h
