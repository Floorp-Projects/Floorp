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
// mapped in the browser process and the sandboxed processes.  The section's
// layout is represented as SharedSection::Layout.
//
// (1) Kernel32's functions required for MMPolicyInProcessEarlyStage
//     Formatted as Kernel32ExportsSolver.
//
// (2) Array of NT paths of the executable's dependent modules
//     Formatted as a null-delimited wide-character string set ending with
//     an empty string.
//
// +--------------------------------------------------------------+
// | (1) | FlushInstructionCache                                  |
// |     | GetModuleHandleW                                       |
// |     | GetSystemInfo                                          |
// |     | VirtualProtect                                         |
// |     | State [Uninitialized|Initialized|Resolved]             |
// +--------------------------------------------------------------+
// | (2) | L"NT path 1"                                           |
// |     | L"NT path 2"                                           |
// |     | ...                                                    |
// |     | L""                                                    |
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

  static constexpr size_t kSharedViewSize = 0x1000;

 public:
  struct Layout final {
    Kernel32ExportsSolver mK32Exports;
    wchar_t mModulePathArray[1];

    Layout() = delete;  // disallow instantiation
  };

  // Replace |sSectionHandle| with a given handle.
  static void Reset(HANDLE aNewSecionObject = sSectionHandle);

  // Replace |sSectionHandle| with a new readonly handle.
  static void ConvertToReadOnly();

  // Create a new writable section and initialize the Kernel32ExportsSolver
  // part.
  static LauncherVoidResult Init(const nt::PEHeaders& aPEHeaders);

  // Append a new string to the |sSectionHandle|
  static LauncherVoidResult AddDepenentModule(PCUNICODE_STRING aNtPath);

  // Map |sSectionHandle| to a copy-on-write page and return its address.
  static LauncherResult<Layout*> GetView();

  // Transfer |sSectionHandle| to a process associated with |aTransferMgr|.
  static LauncherVoidResult TransferHandle(
      nt::CrossExecTransferManager& aTransferMgr, DWORD aDesiredAccess,
      HANDLE* aDestinationAddress = &sSectionHandle);
};

extern SharedSection gSharedSection;
extern RTL_RUN_ONCE gK32ExportsResolveOnce;

}  // namespace freestanding
}  // namespace mozilla

#endif  // mozilla_freestanding_SharedSection_h
