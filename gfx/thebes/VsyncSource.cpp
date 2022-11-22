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

VsyncSource::VsyncSource() : mState("VsyncSource::State") {
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncSource::~VsyncSource() { MOZ_ASSERT(NS_IsMainThread()); }

// Called on the vsync thread
void VsyncSource::NotifyVsync(const TimeStamp& aVsyncTimestamp,
                              const TimeStamp& aOutputTimestamp) {
  VsyncId vsyncId;
  nsTArray<DispatcherRefWithCount> dispatchers;

  {
    auto state = mState.Lock();
    vsyncId = state->mVsyncId.Next();
    dispatchers = state->mDispatchers.Clone();
    state->mVsyncId = vsyncId;
  }

  // Notify our listeners, outside of the lock.
  const VsyncEvent event(vsyncId, aVsyncTimestamp, aOutputTimestamp);
  for (const auto& dispatcher : dispatchers) {
    dispatcher.mDispatcher->NotifyVsync(event);
  }
}

void VsyncSource::AddVsyncDispatcher(VsyncDispatcher* aVsyncDispatcher) {
  MOZ_ASSERT(aVsyncDispatcher);
  {
    auto state = mState.Lock();

    // Find the dispatcher in mDispatchers. If it is already present, increment
    // the count. If not, add it with a count of 1.
    bool found = false;
    for (auto& dispatcherRefWithCount : state->mDispatchers) {
      if (dispatcherRefWithCount.mDispatcher == aVsyncDispatcher) {
        dispatcherRefWithCount.mCount++;
        found = true;
        break;
      }
    }
    if (!found) {
      state->mDispatchers.AppendElement(
          DispatcherRefWithCount{aVsyncDispatcher, 1});
    }
  }

  UpdateVsyncStatus();
}

void VsyncSource::RemoveVsyncDispatcher(VsyncDispatcher* aVsyncDispatcher) {
  MOZ_ASSERT(aVsyncDispatcher);
  {
    auto state = mState.Lock();

    // Find the dispatcher in mDispatchers. If found, decrement the count.
    // If the count becomes zero, remove it from mDispatchers.
    for (auto it = state->mDispatchers.begin(); it != state->mDispatchers.end();
         ++it) {
      if (it->mDispatcher == aVsyncDispatcher) {
        it->mCount--;
        if (it->mCount == 0) {
          state->mDispatchers.RemoveElementAt(it);
        }
        break;
      }
    }

    // In the future we should probably MOZ_RELEASE_ASSERT here that we don't
    // try to remove a dispatcher which isn't in mDispatchers.
  }

  UpdateVsyncStatus();
}

// This is the base class implementation. Subclasses override this method.
TimeDuration VsyncSource::GetVsyncRate() {
  // If hardware queries fail / are unsupported, we have to just guess.
  return TimeDuration::FromMilliseconds(1000.0 / 60.0);
}

void VsyncSource::UpdateVsyncStatus() {
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "VsyncSource::UpdateVsyncStatus",
        [self = RefPtr{this}] { self->UpdateVsyncStatus(); }));
    return;
  }

  MOZ_ASSERT(NS_IsMainThread());
  // WARNING: This function SHOULD NOT BE CALLED WHILE HOLDING LOCKS
  // NotifyVsync grabs a lock to dispatch vsync events
  // When disabling vsync, we wait for the underlying thread to stop on some
  // platforms We can deadlock if we wait for the underlying vsync thread to
  // stop while the vsync thread is in NotifyVsync.
  bool enableVsync = false;
  {  // scope lock
    auto state = mState.Lock();
    enableVsync = !state->mDispatchers.IsEmpty();
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

// static
Maybe<TimeDuration> VsyncSource::GetFastestVsyncRate() {
  Maybe<TimeDuration> retVal;
  if (!gfxPlatform::Initialized()) {
    return retVal;
  }

  RefPtr<VsyncDispatcher> vsyncDispatcher =
      gfxPlatform::GetPlatform()->GetGlobalVsyncDispatcher();
  RefPtr<VsyncSource> vsyncSource = vsyncDispatcher->GetCurrentVsyncSource();
  if (vsyncSource->IsVsyncEnabled()) {
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
