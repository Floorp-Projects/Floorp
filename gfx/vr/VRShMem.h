/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_VRSHMEM_H
#define GFX_VR_VRSHMEM_H

// This file declares VRShMem, which is used for cross-platform, cross-process
// communication between VR components.
// A shared memory handle is opened for the struct of type VRExternalShmem, and
// different processes write or read members of it to share data. Note that no
// process should be reading or writing to the same members, except for the
// versioning members used for synchronization.

#include "moz_external_vr.h"
#include "base/process.h"  // for base::ProcessHandle
#include <functional>

namespace mozilla {
namespace gfx {
class VRShMem final {
 public:
  VRShMem(volatile VRExternalShmem* aShmem, bool aVRProcessEnabled,
          bool aIsParentProcess);
  ~VRShMem() = default;

  void CreateShMem();
  void ClearShMem();
  void CloseShMem();

  bool JoinShMem();
  void LeaveShMem();

  void PushBrowserState(VRBrowserState& aBrowserState, bool aNotifyCond);
  void PullBrowserState(mozilla::gfx::VRBrowserState& aState);

  void PushSystemState(const mozilla::gfx::VRSystemState& aState);
  void PullSystemState(
      VRDisplayState& aDisplayState, VRHMDSensorState& aSensorState,
      VRControllerState (&aControllerState)[kVRControllerMaxCount],
      bool& aEnumerationCompleted,
      const std::function<bool()>& aWaitCondition = nullptr);

  void PushWindowState(VRWindowState& aState);
  void PullWindowState(VRWindowState& aState);

  bool HasExternalShmem() const { return mExternalShmem != nullptr; }
  volatile VRExternalShmem* GetExternalShmem() const;
  bool IsDisplayStateShutdown() const;

 private:
  volatile VRExternalShmem* mExternalShmem = nullptr;
  bool mVRProcessEnabled;

#if defined(XP_WIN)
  bool mIsParentProcess = false;
#endif

#if defined(XP_MACOSX)
  int mShmemFD;
#elif defined(XP_WIN)
  base::ProcessHandle mShmemFile;
  base::ProcessHandle mMutex;
#endif

#if !defined(MOZ_WIDGET_ANDROID)
  int64_t mBrowserGeneration = 0;
#endif
};
}  // namespace gfx
}  // namespace mozilla

#endif