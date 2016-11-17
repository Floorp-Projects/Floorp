/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncChild.h"

#include "mozilla/VsyncDispatcher.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layout {

VsyncChild::VsyncChild()
  : mObservingVsync(false)
  , mIsShutdown(false)
  , mVsyncRate(TimeDuration::Forever())
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncChild::~VsyncChild()
{
  MOZ_ASSERT(NS_IsMainThread());
}

bool
VsyncChild::SendObserve()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mObservingVsync && !mIsShutdown) {
    mObservingVsync = true;
    PVsyncChild::SendObserve();
  }
  return true;
}

bool
VsyncChild::SendUnobserve()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mObservingVsync && !mIsShutdown) {
    mObservingVsync = false;
    PVsyncChild::SendUnobserve();
  }
  return true;
}

void
VsyncChild::ActorDestroy(ActorDestroyReason aActorDestroyReason)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIsShutdown);
  mIsShutdown = true;
  mObserver = nullptr;
}

mozilla::ipc::IPCResult
VsyncChild::RecvNotify(const TimeStamp& aVsyncTimestamp)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIsShutdown);
  if (mObservingVsync && mObserver) {
    mObserver->NotifyVsync(aVsyncTimestamp);
  }
  return IPC_OK();
}

void
VsyncChild::SetVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(NS_IsMainThread());
  mObserver = aVsyncObserver;
}

TimeDuration
VsyncChild::GetVsyncRate()
{
  if (mVsyncRate == TimeDuration::Forever()) {
    PVsyncChild::SendRequestVsyncRate();
  }

  return mVsyncRate;
}

TimeDuration
VsyncChild::VsyncRate()
{
  return mVsyncRate;
}

mozilla::ipc::IPCResult
VsyncChild::RecvVsyncRate(const float& aVsyncRate)
{
  mVsyncRate = TimeDuration::FromMilliseconds(aVsyncRate);
  return IPC_OK();
}

} // namespace layout
} // namespace mozilla
