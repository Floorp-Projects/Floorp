/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncParent.h"

#include "BackgroundParent.h"
#include "BackgroundParentImpl.h"
#include "gfxPlatform.h"
#include "mozilla/Unused.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "VsyncSource.h"

namespace mozilla {

using namespace ipc;

namespace layout {

/*static*/ already_AddRefed<VsyncParent>
VsyncParent::Create()
{
  AssertIsOnBackgroundThread();
  RefPtr<gfx::VsyncSource> vsyncSource = gfxPlatform::GetPlatform()->GetHardwareVsync();
  RefPtr<VsyncParent> vsyncParent = new VsyncParent();
  vsyncParent->mVsyncDispatcher = vsyncSource->GetRefreshTimerVsyncDispatcher();
  return vsyncParent.forget();
}

VsyncParent::VsyncParent()
  : mObservingVsync(false)
  , mDestroyed(false)
  , mBackgroundThread(NS_GetCurrentThread())
{
  MOZ_ASSERT(mBackgroundThread);
  AssertIsOnBackgroundThread();
}

VsyncParent::~VsyncParent()
{
  // Since we use NS_INLINE_DECL_THREADSAFE_REFCOUNTING, we can't make sure
  // VsyncParent is always released on the background thread.
}

bool
VsyncParent::NotifyVsync(TimeStamp aTimeStamp)
{
  // Called on hardware vsync thread. We should post to current ipc thread.
  MOZ_ASSERT(!IsOnBackgroundThread());
  nsCOMPtr<nsIRunnable> vsyncEvent =
    NewRunnableMethod<TimeStamp>("layout::VsyncParent::DispatchVsyncEvent",
                                 this,
                                 &VsyncParent::DispatchVsyncEvent,
                                 aTimeStamp);
  MOZ_ALWAYS_SUCCEEDS(mBackgroundThread->Dispatch(vsyncEvent, NS_DISPATCH_NORMAL));
  return true;
}

void
VsyncParent::DispatchVsyncEvent(TimeStamp aTimeStamp)
{
  AssertIsOnBackgroundThread();

  // If we call NotifyVsync() when we handle ActorDestroy() message, we might
  // still call DispatchVsyncEvent().
  // Similarly, we might also receive RecvUnobserveVsync() when call
  // NotifyVsync(). We use mObservingVsync and mDestroyed flags to skip this
  // notification.
  if (mObservingVsync && !mDestroyed) {
    Unused << SendNotify(aTimeStamp);
  }
}

mozilla::ipc::IPCResult
VsyncParent::RecvRequestVsyncRate()
{
  AssertIsOnBackgroundThread();
  TimeDuration vsyncRate = gfxPlatform::GetPlatform()->GetHardwareVsync()->GetGlobalDisplay().GetVsyncRate();
  Unused << SendVsyncRate(vsyncRate.ToMilliseconds());
  return IPC_OK();
}

mozilla::ipc::IPCResult
VsyncParent::RecvObserve()
{
  AssertIsOnBackgroundThread();
  if (!mObservingVsync) {
    mVsyncDispatcher->AddChildRefreshTimer(this);
    mObservingVsync = true;
    return IPC_OK();
  }
  return IPC_FAIL_NO_REASON(this);
}

mozilla::ipc::IPCResult
VsyncParent::RecvUnobserve()
{
  AssertIsOnBackgroundThread();
  if (mObservingVsync) {
    mVsyncDispatcher->RemoveChildRefreshTimer(this);
    mObservingVsync = false;
    return IPC_OK();
  }
  return IPC_FAIL_NO_REASON(this);
}

void
VsyncParent::ActorDestroy(ActorDestroyReason aReason)
{
  MOZ_ASSERT(!mDestroyed);
  AssertIsOnBackgroundThread();
  if (mObservingVsync) {
    mVsyncDispatcher->RemoveChildRefreshTimer(this);
  }
  mVsyncDispatcher = nullptr;
  mDestroyed = true;
}

} // namespace layout
} // namespace mozilla
