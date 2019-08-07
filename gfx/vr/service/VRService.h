/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_SERVICE_VRSERVICE_H
#define GFX_VR_SERVICE_VRSERVICE_H

#include "mozilla/Atomics.h"
#include "moz_external_vr.h"
#include "base/process.h"  // for base::ProcessHandle

namespace base {
class Thread;
}  // namespace base
namespace mozilla {
namespace gfx {

class VRSession;
class VRShMem;

static const int kVRFrameTimingHistoryDepth = 100;

class VRService {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRService)
  static already_AddRefed<VRService> Create(
      volatile VRExternalShmem* aShmem = nullptr);

  void Refresh();
  void Start();
  void Stop();

 private:
  explicit VRService(volatile VRExternalShmem* aShmem);
  ~VRService();

  bool InitShmem();
  void PushState(const mozilla::gfx::VRSystemState& aState);
  void PullState(mozilla::gfx::VRBrowserState& aState);

  /**
   * VRSystemState contains the most recent state of the VR
   * system, to be shared with the browser by Shmem.
   * mSystemState is the VR Service copy of this data, which
   * is memcpy'ed atomically to the Shmem.
   * VRSystemState is written by the VR Service, but read-only
   * by the browser.
   */
  VRSystemState mSystemState;
  /**
   * VRBrowserState contains the most recent state of the browser.
   * mBrowserState is memcpy'ed from the Shmem atomically
   */
  VRBrowserState mBrowserState;

  UniquePtr<VRSession> mSession;
  base::Thread* mServiceThread;
  bool mShutdownRequested;

  // Note: mShmem doesn't support RefPtr; thus, do not share this private
  // pointer so that its lifetime can still be controlled by VRService
  VRShMem* mShmem;
  VRHapticState mLastHapticState[kVRHapticsMaxCount];
  TimeStamp mFrameStartTime[kVRFrameTimingHistoryDepth];

  bool IsInServiceThread();
  void UpdateHaptics();

  /**
   * The VR Service thread is a state machine that always has one
   * task queued depending on the state.
   *
   * VR Service thread state task functions:
   */
  void ServiceInitialize();
  void ServiceShutdown();
  void ServiceWaitForImmersive();
  void ServiceImmersiveMode();
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_SERVICE_VRSERVICE_H
