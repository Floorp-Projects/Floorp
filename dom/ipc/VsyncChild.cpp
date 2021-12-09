/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncChild.h"

#include "mozilla/SchedulerGroup.h"
#include "mozilla/VsyncDispatcher.h"
#include "nsThreadUtils.h"

namespace mozilla::dom {

VsyncChild::VsyncChild()
    : mIsShutdown(false), mVsyncRate(TimeDuration::Forever()) {
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncChild::~VsyncChild() { MOZ_ASSERT(NS_IsMainThread()); }

void VsyncChild::AddChildRefreshTimer(VsyncObserver* aVsyncObserver) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mObservers.Contains(aVsyncObserver));

  if (mIsShutdown) {
    return;
  }

  if (mObservers.IsEmpty()) {
    Unused << PVsyncChild::SendObserve();
  }
  mObservers.AppendElement(std::move(aVsyncObserver));
}

void VsyncChild::RemoveChildRefreshTimer(VsyncObserver* aVsyncObserver) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mIsShutdown) {
    return;
  }

  if (mObservers.RemoveElement(aVsyncObserver) && mObservers.IsEmpty()) {
    Unused << PVsyncChild::SendUnobserve();
  }
}

void VsyncChild::ActorDestroy(ActorDestroyReason aActorDestroyReason) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIsShutdown);
  mIsShutdown = true;

  if (!mObservers.IsEmpty()) {
    Unused << PVsyncChild::SendUnobserve();
  }
  mObservers.Clear();
}

mozilla::ipc::IPCResult VsyncChild::RecvNotify(const VsyncEvent& aVsync,
                                               const float& aVsyncRate) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIsShutdown);

  SchedulerGroup::MarkVsyncRan();

  mVsyncRate = TimeDuration::FromMilliseconds(aVsyncRate);

  for (VsyncObserver* observer : mObservers.ForwardRange()) {
    observer->NotifyVsync(aVsync);
  }
  return IPC_OK();
}

TimeDuration VsyncChild::GetVsyncRate() { return mVsyncRate; }

}  // namespace mozilla::dom
