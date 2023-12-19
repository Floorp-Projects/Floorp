/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PendingAnimationTracker.h"

#include "mozilla/PresShell.h"
#include "mozilla/dom/AnimationEffect.h"
#include "mozilla/dom/AnimationTimeline.h"
#include "mozilla/dom/Nullable.h"
#include "nsIFrame.h"
#include "nsTransitionManager.h"  // For CSSTransition

using mozilla::dom::Nullable;

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION(PendingAnimationTracker, mPlayPendingSet,
                         mPausePendingSet, mDocument)

PendingAnimationTracker::PendingAnimationTracker(dom::Document* aDocument)
    : mDocument(aDocument) {}

void PendingAnimationTracker::AddPending(dom::Animation& aAnimation,
                                         AnimationSet& aSet) {
  aSet.Insert(&aAnimation);

  // Schedule a paint. Otherwise animations that don't trigger a paint by
  // themselves (e.g. CSS animations with an empty keyframes rule) won't
  // start until something else paints.
  EnsurePaintIsScheduled();
}

void PendingAnimationTracker::RemovePending(dom::Animation& aAnimation,
                                            AnimationSet& aSet) {
  aSet.Remove(&aAnimation);
}

bool PendingAnimationTracker::IsWaiting(const dom::Animation& aAnimation,
                                        const AnimationSet& aSet) const {
  return aSet.Contains(const_cast<dom::Animation*>(&aAnimation));
}

void PendingAnimationTracker::TriggerPendingAnimationsOnNextTick(
    const TimeStamp& aReadyTime) {
  auto triggerAnimationsAtReadyTime = [aReadyTime](
                                          AnimationSet& aAnimationSet) {
    for (auto iter = aAnimationSet.begin(), end = aAnimationSet.end();
         iter != end; ++iter) {
      dom::Animation* animation = *iter;
      dom::AnimationTimeline* timeline = animation->GetTimeline();

      // If the animation does not have a timeline, just drop it from the map.
      // The animation will detect that it is not being tracked and will trigger
      // itself on the next tick where it has a timeline.
      if (!timeline) {
        aAnimationSet.Remove(iter);
        continue;
      }

      MOZ_ASSERT(timeline->IsMonotonicallyIncreasing(),
                 "The non-monotonicially-increasing timeline should be in "
                 "ScrollTimelineAnimationTracker");

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

      aAnimationSet.Remove(iter);
    }
  };

  triggerAnimationsAtReadyTime(mPlayPendingSet);
  triggerAnimationsAtReadyTime(mPausePendingSet);

  mHasPlayPendingGeometricAnimations =
      mPlayPendingSet.Count() ? CheckState::Indeterminate : CheckState::Absent;
}

void PendingAnimationTracker::TriggerPendingAnimationsNow() {
  auto triggerAndClearAnimations = [](AnimationSet& aAnimationSet) {
    for (const auto& animation : aAnimationSet) {
      animation->TriggerNow();
    }
    aAnimationSet.Clear();
  };

  triggerAndClearAnimations(mPlayPendingSet);
  triggerAndClearAnimations(mPausePendingSet);

  mHasPlayPendingGeometricAnimations = CheckState::Absent;
}

static bool IsTransition(const dom::Animation& aAnimation) {
  const dom::CSSTransition* transition = aAnimation.AsCSSTransition();
  return transition && transition->IsTiedToMarkup();
}

void PendingAnimationTracker::MarkAnimationsThatMightNeedSynchronization() {
  // We only set mHasPlayPendingGeometricAnimations to "present" in this method
  // and nowhere else. After setting the state to "present", if there is any
  // change to the set of play-pending animations we will reset
  // mHasPlayPendingGeometricAnimations to either "indeterminate" or "absent".
  //
  // As a result, if mHasPlayPendingGeometricAnimations is "present", we can
  // assume that this method has already been called for the current set of
  // play-pending animations and it is not necessary to run this method again.
  //
  // If mHasPlayPendingGeometricAnimations is "absent", then we can also skip
  // the body of this method since there are no notifications to be sent.
  //
  // Therefore, the only case we need to be concerned about is the
  // "indeterminate" case. For all other cases we can return early.
  //
  // Note that *without* this optimization, starting animations would become
  // O(n^2) in the case where each animation is on a different element and
  // contains a compositor-animatable property since we would end up iterating
  // over all animations in the play-pending set for each target element.
  if (mHasPlayPendingGeometricAnimations != CheckState::Indeterminate) {
    return;
  }

  // We only synchronize CSS transitions with other CSS transitions (and we only
  // synchronize non-transition animations with non-transition animations)
  // since typically the author will not trigger both CSS animations and
  // CSS transitions simultaneously and expect them to be synchronized.
  //
  // If we try to synchronize CSS transitions with non-transitions then for some
  // content we will end up degrading performance by forcing animations to run
  // on the main thread that really don't need to.

  mHasPlayPendingGeometricAnimations = CheckState::Absent;
  for (const auto& animation : mPlayPendingSet) {
    if (animation->GetEffect() && animation->GetEffect()->AffectsGeometry()) {
      mHasPlayPendingGeometricAnimations &= ~CheckState::Absent;
      mHasPlayPendingGeometricAnimations |= IsTransition(*animation)
                                                ? CheckState::TransitionsPresent
                                                : CheckState::AnimationsPresent;

      // If we have both transitions and animations we don't need to look any
      // further.
      if (mHasPlayPendingGeometricAnimations ==
          (CheckState::TransitionsPresent | CheckState::AnimationsPresent)) {
        break;
      }
    }
  }

  if (mHasPlayPendingGeometricAnimations == CheckState::Absent) {
    return;
  }

  for (const auto& animation : mPlayPendingSet) {
    bool isTransition = IsTransition(*animation);
    if ((isTransition &&
         mHasPlayPendingGeometricAnimations & CheckState::TransitionsPresent) ||
        (!isTransition &&
         mHasPlayPendingGeometricAnimations & CheckState::AnimationsPresent)) {
      animation->NotifyGeometricAnimationsStartingThisFrame();
    }
  }
}

void PendingAnimationTracker::EnsurePaintIsScheduled() {
  if (!mDocument) {
    return;
  }

  PresShell* presShell = mDocument->GetPresShell();
  if (!presShell) {
    return;
  }

  nsIFrame* rootFrame = presShell->GetRootFrame();
  if (!rootFrame) {
    return;
  }

  rootFrame->SchedulePaintWithoutInvalidatingObservers();
}

}  // namespace mozilla
