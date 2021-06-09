/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncParent.h"

#include "mozilla/ipc/BackgroundParent.h"
#include "gfxPlatform.h"
#include "mozilla/Unused.h"
#include "nsThreadUtils.h"
#include "VsyncSource.h"
#include "nsIThread.h"

namespace mozilla::dom {

VsyncParent::VsyncParent()
    : mObservingVsync(false),
      mDestroyed(false),
      mInitialThread(NS_GetCurrentThread()) {}

void VsyncParent::UpdateVsyncSource(
    const RefPtr<gfx::VsyncSource>& aVsyncSource) {
  mVsyncSource = aVsyncSource;
  if (!mVsyncSource) {
    mVsyncSource = gfxPlatform::GetPlatform()->GetHardwareVsync();
  }

  if (mObservingVsync && mVsyncDispatcher) {
    mVsyncDispatcher->RemoveChildRefreshTimer(this);
  }
  mVsyncDispatcher = mVsyncSource->GetRefreshTimerVsyncDispatcher();
  if (mObservingVsync) {
    mVsyncDispatcher->AddChildRefreshTimer(this);
  }
}

bool VsyncParent::NotifyVsync(const VsyncEvent& aVsync) {
  if (IsOnInitialThread()) {
    DispatchVsyncEvent(aVsync);
    return true;
  }

  // Called on hardware vsync thread. We should post to current ipc thread.
  nsCOMPtr<nsIRunnable> vsyncEvent = NewRunnableMethod<VsyncEvent>(
      "dom::VsyncParent::DispatchVsyncEvent", this,
      &VsyncParent::DispatchVsyncEvent, aVsync);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToThreadQueue(
      vsyncEvent.forget(), mInitialThread, EventQueuePriority::Vsync));
  return true;
}

void VsyncParent::DispatchVsyncEvent(const VsyncEvent& aVsync) {
  AssertIsOnInitialThread();

  // If we call NotifyVsync() when we handle ActorDestroy() message, we might
  // still call DispatchVsyncEvent().
  // Similarly, we might also receive RecvUnobserveVsync() when call
  // NotifyVsync(). We use mObservingVsync and mDestroyed flags to skip this
  // notification.
  if (mObservingVsync && !mDestroyed) {
    TimeDuration vsyncRate = mVsyncSource->GetGlobalDisplay().GetVsyncRate();
    Unused << SendNotify(aVsync, vsyncRate.ToMilliseconds());
  }
}

mozilla::ipc::IPCResult VsyncParent::RecvObserve() {
  AssertIsOnInitialThread();
  if (!mObservingVsync) {
    if (mVsyncDispatcher) {
      mVsyncDispatcher->AddChildRefreshTimer(this);
    }
    mObservingVsync = true;
    return IPC_OK();
  }
  return IPC_FAIL_NO_REASON(this);
}

mozilla::ipc::IPCResult VsyncParent::RecvUnobserve() {
  AssertIsOnInitialThread();
  if (mObservingVsync) {
    if (mVsyncDispatcher) {
      mVsyncDispatcher->RemoveChildRefreshTimer(this);
    }
    mObservingVsync = false;
    return IPC_OK();
  }
  return IPC_FAIL_NO_REASON(this);
}

void VsyncParent::ActorDestroy(ActorDestroyReason aActorDestroyReason) {
  MOZ_ASSERT(!mDestroyed);
  AssertIsOnInitialThread();
  if (mObservingVsync && mVsyncDispatcher) {
    mVsyncDispatcher->RemoveChildRefreshTimer(this);
  }
  mVsyncDispatcher = nullptr;
  mDestroyed = true;
}

bool VsyncParent::IsOnInitialThread() {
  return NS_GetCurrentThread() == mInitialThread;
}

void VsyncParent::AssertIsOnInitialThread() { MOZ_ASSERT(IsOnInitialThread()); }

}  // namespace mozilla::dom
