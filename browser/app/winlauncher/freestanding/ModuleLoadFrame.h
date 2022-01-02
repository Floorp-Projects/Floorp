/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_freestanding_ModuleLoadFrame_h
#define mozilla_freestanding_ModuleLoadFrame_h

#include "mozilla/LoaderAPIInterfaces.h"
#include "mozilla/NativeNt.h"
#include "mozilla/ThreadLocal.h"

#include "SafeThreadLocal.h"

namespace mozilla {
namespace freestanding {

/**
 * This class holds information about a DLL load at a particular frame in the
 * current thread's stack. Each instance adds itself to a thread-local linked
 * list of ModuleLoadFrames, enabling us to query information about the
 * previous module load on the stack.
 */
class MOZ_RAII ModuleLoadFrame final {
 public:
  /**
   * This constructor is for use by the LdrLoadDll hook.
   */
  explicit ModuleLoadFrame(PCUNICODE_STRING aRequestedDllName);
  ~ModuleLoadFrame();

  static void NotifyLSPSubstitutionRequired(PCUNICODE_STRING aLeafName);

  /**
   * This static method is called by the NtMapViewOfSection hook.
   */
  static void NotifySectionMap(nt::AllocatedUnicodeString&& aSectionName,
                               const void* aMapBaseAddr, NTSTATUS aMapNtStatus,
                               ModuleLoadInfo::Status aLoadStatus,
                               bool aIsDependent);
  static bool ExistsTopFrame();

  /**
   * Called by the LdrLoadDll hook to indicate the status of the load and for
   * us to provide a substitute output handle if necessary.
   */
  NTSTATUS SetLoadStatus(NTSTATUS aNtStatus, PHANDLE aOutHandle);

  ModuleLoadFrame(const ModuleLoadFrame&) = delete;
  ModuleLoadFrame(ModuleLoadFrame&&) = delete;
  ModuleLoadFrame& operator=(const ModuleLoadFrame&) = delete;
  ModuleLoadFrame& operator=(ModuleLoadFrame&&) = delete;

 private:
  /**
   * Called by OnBareSectionMap to construct a frame for a bare load.
   */
  ModuleLoadFrame(nt::AllocatedUnicodeString&& aSectionName,
                  const void* aMapBaseAddr, NTSTATUS aNtStatus,
                  ModuleLoadInfo::Status aLoadStatus, bool aIsDependent);

  void SetLSPSubstitutionRequired(PCUNICODE_STRING aLeafName);
  void OnSectionMap(nt::AllocatedUnicodeString&& aSectionName,
                    const void* aMapBaseAddr, NTSTATUS aMapNtStatus,
                    ModuleLoadInfo::Status aLoadStatus, bool aIsDependent);

  /**
   * A "bare" section mapping is one that was mapped without the code passing
   * through a call to ntdll!LdrLoadDll. This method is invoked when we detect
   * that condition.
   */
  static void OnBareSectionMap(nt::AllocatedUnicodeString&& aSectionName,
                               const void* aMapBaseAddr, NTSTATUS aMapNtStatus,
                               ModuleLoadInfo::Status aLoadStatus,
                               bool aIsDependent);

 private:
  // Link to the previous frame
  ModuleLoadFrame* mPrev;
  // Pointer to context managed by the nt::LoaderObserver implementation
  void* mContext;
  // Set to |true| when we need to block a WinSock LSP
  bool mLSPSubstitutionRequired;
  // NTSTATUS code from the |LdrLoadDll| call
  NTSTATUS mLoadNtStatus;
  // Telemetry information that will be forwarded to the nt::LoaderObserver
  ModuleLoadInfo mLoadInfo;

  // Head of the linked list
  static SafeThreadLocal<ModuleLoadFrame*> sTopFrame;
};

}  // namespace freestanding
}  // namespace mozilla

#endif  // mozilla_freestanding_ModuleLoadFrame_h
