/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_SERVICE_VRSERVICE_H
#define GFX_VR_SERVICE_VRSERVICE_H

#include "mozilla/Atomics.h"

#include "moz_external_vr.h"

#include <thread>
namespace base {
class Thread;
} // namespace base
namespace mozilla {
namespace gfx {

class VRSession;

class VRService
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRService)
  static already_AddRefed<VRService> Create();

  void Start();
  void Stop();
  VRExternalShmem* GetAPIShmem();

private:
  VRService();
  ~VRService();
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

  std::unique_ptr<VRSession> mSession;
  base::Thread* mServiceThread;
  bool mShutdownRequested;

  VRExternalShmem mAPIShmem;

  bool IsInServiceThread();
  void RequestShutdown();

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

} // namespace gfx
} // namespace mozilla

#endif // GFX_VR_SERVICE_VRSERVICE_H
