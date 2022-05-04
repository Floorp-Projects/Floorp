/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncSource.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/VsyncDispatcher.h"
#include "MainThreadUtils.h"
#include "gfxPlatform.h"

#ifdef MOZ_WAYLAND
#  include "WaylandVsyncSource.h"
#endif

namespace mozilla {
namespace gfx {

VsyncSource::VsyncSource()
    : mDispatcherLock("display dispatcher lock"),
      mVsyncDispatcherNeedsVsync(false),
      mHasGenericObservers(false) {
  MOZ_ASSERT(NS_IsMainThread());
  mVsyncDispatcher = new VsyncDispatcher(this);
}

VsyncSource::~VsyncSource() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mDispatcherLock);
  mVsyncDispatcher = nullptr;
}

void VsyncSource::NotifyVsync(const TimeStamp& aVsyncTimestamp,
                              const TimeStamp& aOutputTimestamp) {
  // Called on the vsync thread
  MutexAutoLock lock(mDispatcherLock);

  // mVsyncDispatcher might be null here if MoveListenersToNewSource
  // was called concurrently with this function and won the race to acquire
  // mDispatcherLock. In this case the new VsyncSource that is replacing this
  // one will handle notifications from now on, so we can abort.
  if (!mVsyncDispatcher) {
    return;
  }

  // If the task posted to the main thread from the last NotifyVsync call
  // hasn't been processed yet, then don't send another one. Otherwise we might
  // end up flooding the main thread.
  bool dispatchToMainThread =
      mHasGenericObservers &&
      (mLastVsyncIdSentToMainThread == mLastMainThreadProcessedVsyncId);

  mVsyncId = mVsyncId.Next();
  const VsyncEvent event(mVsyncId, aVsyncTimestamp, aOutputTimestamp);

  mVsyncDispatcher->NotifyVsync(event);

  if (dispatchToMainThread) {
    mLastVsyncIdSentToMainThread = mVsyncId;
    NS_DispatchToMainThread(NewRunnableMethod<VsyncEvent>(
        "VsyncSource::NotifyGenericObservers", this,
        &VsyncSource::NotifyGenericObservers, event));
  }
}

void VsyncSource::NotifyGenericObservers(VsyncEvent aEvent) {
  MOZ_ASSERT(NS_IsMainThread());
  for (size_t i = 0; i < mGenericObservers.Length(); i++) {
    mGenericObservers[i]->NotifyVsync(aEvent);
  }

  {  // Scope lock
    MutexAutoLock lock(mDispatcherLock);
    mLastMainThreadProcessedVsyncId = aEvent.mId;
  }
}

TimeDuration VsyncSource::GetVsyncRate() {
  // If hardware queries fail / are unsupported, we have to just guess.
  return TimeDuration::FromMilliseconds(1000.0 / 60.0);
}

void VsyncSource::AddGenericObserver(VsyncObserver* aObserver) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aObserver);
  mGenericObservers.AppendElement(aObserver);

  UpdateVsyncStatus();
}

void VsyncSource::RemoveGenericObserver(VsyncObserver* aObserver) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aObserver);
  mGenericObservers.RemoveElement(aObserver);

  UpdateVsyncStatus();
}

void VsyncSource::MoveListenersToNewSource(
    const RefPtr<VsyncSource>& aNewSource) {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mDispatcherLock);
  MutexAutoLock newLock(aNewSource->mDispatcherLock);
  aNewSource->mGenericObservers.AppendElements(std::move(mGenericObservers));

  aNewSource->mVsyncDispatcher = mVsyncDispatcher;
  mVsyncDispatcher->MoveToSource(aNewSource);
  mVsyncDispatcher = nullptr;
}

void VsyncSource::NotifyVsyncDispatcherVsyncStatus(bool aEnable) {
  MOZ_ASSERT(NS_IsMainThread());
  mVsyncDispatcherNeedsVsync = aEnable;
  UpdateVsyncStatus();
}

void VsyncSource::UpdateVsyncStatus() {
  MOZ_ASSERT(NS_IsMainThread());
  // WARNING: This function SHOULD NOT BE CALLED WHILE HOLDING LOCKS
  // NotifyVsync grabs a lock to dispatch vsync events
  // When disabling vsync, we wait for the underlying thread to stop on some
  // platforms We can deadlock if we wait for the underlying vsync thread to
  // stop while the vsync thread is in NotifyVsync.
  bool enableVsync = false;
  {  // scope lock
    MutexAutoLock lock(mDispatcherLock);
    enableVsync = mVsyncDispatcherNeedsVsync || !mGenericObservers.IsEmpty();
    mHasGenericObservers = !mGenericObservers.IsEmpty();
  }

  if (enableVsync) {
    EnableVsync();
  } else {
    DisableVsync();
  }

  if (IsVsyncEnabled() != enableVsync) {
    NS_WARNING("Vsync status did not change.");
  }
}

RefPtr<VsyncDispatcher> VsyncSource::GetVsyncDispatcher() {
  return mVsyncDispatcher;
}

// static
Maybe<TimeDuration> VsyncSource::GetFastestVsyncRate() {
  Maybe<TimeDuration> retVal;
  if (!gfxPlatform::Initialized()) {
    return retVal;
  }

  mozilla::gfx::VsyncSource* vsyncSource =
      gfxPlatform::GetPlatform()->GetHardwareVsync();
  if (vsyncSource && vsyncSource->IsVsyncEnabled()) {
    retVal.emplace(vsyncSource->GetVsyncRate());
  }

#ifdef MOZ_WAYLAND
  Maybe<TimeDuration> waylandRate = WaylandVsyncSource::GetFastestVsyncRate();
  if (waylandRate) {
    if (!retVal) {
      retVal.emplace(*waylandRate);
    } else if (*waylandRate < *retVal) {
      retVal = waylandRate;
    }
  }
#endif

  return retVal;
}

}  // namespace gfx
}  // namespace mozilla
