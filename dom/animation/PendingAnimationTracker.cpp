/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PendingAnimationTracker.h"

#include "mozilla/dom/AnimationTimeline.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"

using namespace mozilla;

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION(PendingAnimationTracker,
                         mPlayPendingSet,
                         mPausePendingSet,
                         mDocument)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(PendingAnimationTracker, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(PendingAnimationTracker, Release)

void
PendingAnimationTracker::AddPending(dom::Animation& aAnimation,
                                    AnimationSet& aSet)
{
  aSet.PutEntry(&aAnimation);

  // Schedule a paint. Otherwise animations that don't trigger a paint by
  // themselves (e.g. CSS animations with an empty keyframes rule) won't
  // start until something else paints.
  EnsurePaintIsScheduled();
}

void
PendingAnimationTracker::RemovePending(dom::Animation& aAnimation,
                                       AnimationSet& aSet)
{
  aSet.RemoveEntry(&aAnimation);
}

bool
PendingAnimationTracker::IsWaiting(const dom::Animation& aAnimation,
                                   const AnimationSet& aSet) const
{
  return aSet.Contains(const_cast<dom::Animation*>(&aAnimation));
}

void
PendingAnimationTracker::TriggerPendingAnimationsOnNextTick(const TimeStamp&
                                                        aReadyTime)
{
  auto triggerAnimationsAtReadyTime = [aReadyTime](AnimationSet& aAnimationSet)
  {
    for (auto iter = aAnimationSet.Iter(); !iter.Done(); iter.Next()) {
      dom::Animation* animation = iter.Get()->GetKey();
      dom::AnimationTimeline* timeline = animation->GetTimeline();

      // If the animation does not have a timeline, just drop it from the map.
      // The animation will detect that it is not being tracked and will trigger
      // itself on the next tick where it has a timeline.
      if (!timeline) {
        iter.Remove();
        continue;
      }

      // When the timeline's refresh driver is under test control, its values
      // have no correspondance to wallclock times so we shouldn't try to
      // convert aReadyTime (which is a wallclock time) to a timeline value.
      // Instead, the animation will be started/paused when the refresh driver
      // is next advanced since this will trigger a call to
      // TriggerPendingAnimationsNow.
      if (!timeline->TracksWallclockTime()) {
        continue;
      }

      Nullable<TimeDuration> readyTime = timeline->ToTimelineTime(aReadyTime);
      animation->TriggerOnNextTick(readyTime);

      iter.Remove();
    }
  };

  triggerAnimationsAtReadyTime(mPlayPendingSet);
  triggerAnimationsAtReadyTime(mPausePendingSet);
}

void
PendingAnimationTracker::TriggerPendingAnimationsNow()
{
  auto triggerAndClearAnimations = [](AnimationSet& aAnimationSet) {
    for (auto iter = aAnimationSet.Iter(); !iter.Done(); iter.Next()) {
      iter.Get()->GetKey()->TriggerNow();
    }
    aAnimationSet.Clear();
  };

  triggerAndClearAnimations(mPlayPendingSet);
  triggerAndClearAnimations(mPausePendingSet);
}

void
PendingAnimationTracker::EnsurePaintIsScheduled()
{
  if (!mDocument) {
    return;
  }

  nsIPresShell* presShell = mDocument->GetShell();
  if (!presShell) {
    return;
  }

  nsIFrame* rootFrame = presShell->GetRootFrame();
  if (!rootFrame) {
    return;
  }

  rootFrame->SchedulePaint();
}

} // namespace mozilla
