/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationPlayer.h"
#include "AnimationUtils.h"
#include "mozilla/dom/AnimationPlayerBinding.h"
#include "AnimationCommon.h" // For AnimationPlayerCollection,
                             // CommonAnimationManager
#include "nsIDocument.h" // For nsIDocument
#include "nsIPresShell.h" // For nsIPresShell
#include "nsLayoutUtils.h" // For PostRestyleEvent (remove after bug 1073336)
#include "PendingPlayerTracker.h" // For PendingPlayerTracker

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AnimationPlayer, mTimeline,
                                      mSource, mReady)
NS_IMPL_CYCLE_COLLECTING_ADDREF(AnimationPlayer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AnimationPlayer)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AnimationPlayer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
AnimationPlayer::WrapObject(JSContext* aCx)
{
  return dom::AnimationPlayerBinding::Wrap(aCx, this);
}

Nullable<TimeDuration>
AnimationPlayer::GetCurrentTime() const
{
  Nullable<TimeDuration> result;
  if (!mHoldTime.IsNull()) {
    result = mHoldTime;
    return result;
  }

  if (!mStartTime.IsNull()) {
    Nullable<TimeDuration> timelineTime = mTimeline->GetCurrentTime();
    if (!timelineTime.IsNull()) {
      result.SetValue(timelineTime.Value() - mStartTime.Value());
    }
  }
  return result;
}

AnimationPlayState
AnimationPlayer::PlayState() const
{
  if (mIsPending) {
    return AnimationPlayState::Pending;
  }

  Nullable<TimeDuration> currentTime = GetCurrentTime();
  if (currentTime.IsNull()) {
    return AnimationPlayState::Idle;
  }

  if (mStartTime.IsNull()) {
    return AnimationPlayState::Paused;
  }

  if (currentTime.Value() >= SourceContentEnd()) {
    return AnimationPlayState::Finished;
  }

  return AnimationPlayState::Running;
}

Promise*
AnimationPlayer::GetReady(ErrorResult& aRv)
{
  // Lazily create the ready promise if it doesn't exist
  if (!mReady) {
    nsIGlobalObject* global = mTimeline->GetParentObject();
    if (global) {
      mReady = Promise::Create(global, aRv);
      if (mReady && PlayState() != AnimationPlayState::Pending) {
        mReady->MaybeResolve(this);
      }
    }
  }
  if (!mReady) {
    aRv.Throw(NS_ERROR_FAILURE);
  }

  return mReady;
}

void
AnimationPlayer::Play()
{
  DoPlay();
  PostUpdate();
}

void
AnimationPlayer::Pause()
{
  DoPause();
  PostUpdate();
}

Nullable<double>
AnimationPlayer::GetStartTimeAsDouble() const
{
  return AnimationUtils::TimeDurationToDouble(mStartTime);
}

Nullable<double>
AnimationPlayer::GetCurrentTimeAsDouble() const
{
  return AnimationUtils::TimeDurationToDouble(GetCurrentTime());
}

void
AnimationPlayer::SetSource(Animation* aSource)
{
  if (mSource) {
    mSource->SetParentTime(Nullable<TimeDuration>());
  }
  mSource = aSource;
  if (mSource) {
    mSource->SetParentTime(GetCurrentTime());
  }
}

void
AnimationPlayer::Tick()
{
  // FIXME (bug 1112969): Check if we are pending but have lost access to the
  // pending player tracker. If that's the case we should probably trigger the
  // animation now.

  UpdateSourceContent();
}

void
AnimationPlayer::StartNow()
{
  // This method is only expected to be called for an animation that is
  // waiting to play. We can easily adapt it to handle other states
  // but it's currently not necessary.
  MOZ_ASSERT(PlayState() == AnimationPlayState::Pending,
             "Expected to start a pending player");
  MOZ_ASSERT(!mHoldTime.IsNull(),
             "A player in the pending state should have a resolved hold time");

  Nullable<TimeDuration> readyTime = mTimeline->GetCurrentTime();

  // FIXME (bug 1096776): If readyTime.IsNull(), we should return early here.
  // This will leave mIsPending = true but the caller will remove us from the
  // PendingPlayerTracker if we were added there.
  // Then, in Tick(), if we have:
  // - a resolved timeline, and
  // - mIsPending = true, and
  // - *no* document or we are *not* in the PendingPlayerTracker
  // then we should call StartNow.
  //
  // For now, however, we don't support inactive/missing timelines so
  // |readyTime| should be resolved.
  MOZ_ASSERT(!readyTime.IsNull(), "Missing or inactive timeline");

  mStartTime.SetValue(readyTime.Value() - mHoldTime.Value());
  mHoldTime.SetNull();
  mIsPending = false;

  UpdateSourceContent();
  PostUpdate();

  if (mReady) {
    mReady->MaybeResolve(this);
  }
}

void
AnimationPlayer::Cancel()
{
  if (mIsPending) {
    CancelPendingPlay();
    if (mReady) {
      mReady->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    }
  }

  mHoldTime.SetNull();
  mStartTime.SetNull();
}

bool
AnimationPlayer::IsRunning() const
{
  if (IsPaused() || !GetSource() || GetSource()->IsFinishedTransition()) {
    return false;
  }

  ComputedTiming computedTiming = GetSource()->GetComputedTiming();
  return computedTiming.mPhase == ComputedTiming::AnimationPhase_Active;
}

bool
AnimationPlayer::CanThrottle() const
{
  if (!mSource ||
      mSource->IsFinishedTransition() ||
      mSource->Properties().IsEmpty()) {
    return true;
  }

  if (!mIsRunningOnCompositor) {
    return false;
  }

  if (PlayState() != AnimationPlayState::Finished) {
    // Unfinished animations can be throttled.
    return true;
  }

  // The animation has finished but, if this is the first sample since
  // finishing, we need an unthrottled sample so we can apply the correct
  // end-of-animation behavior on the main thread (either removing the
  // animation style or applying the fill mode).
  return mIsPreviousStateFinished;
}

void
AnimationPlayer::ComposeStyle(nsRefPtr<css::AnimValuesStyleRule>& aStyleRule,
                              nsCSSPropertySet& aSetProperties,
                              bool& aNeedsRefreshes)
{
  if (!mSource || mSource->IsFinishedTransition()) {
    return;
  }

  AnimationPlayState playState = PlayState();
  if (playState == AnimationPlayState::Running ||
      playState == AnimationPlayState::Pending) {
    aNeedsRefreshes = true;
  }

  mSource->ComposeStyle(aStyleRule, aSetProperties);

  mIsPreviousStateFinished = (playState == AnimationPlayState::Finished);
}

void
AnimationPlayer::DoPlay()
{
  // FIXME: When we implement finishing behavior (bug 1074630) we will
  // need to pass a flag so that when we start playing due to a change in
  // animation-play-state we *don't* trigger finishing behavior.

  Nullable<TimeDuration> currentTime = GetCurrentTime();
  if (currentTime.IsNull()) {
    mHoldTime.SetValue(TimeDuration(0));
  } else if (mHoldTime.IsNull()) {
    // If the hold time is null, we are already playing normally
    return;
  }

  // Clear ready promise. We'll create a new one lazily.
  mReady = nullptr;

  mIsPending = true;

  nsIDocument* doc = GetRenderedDocument();
  if (!doc) {
    // If we have no rendered document (e.g. because the source content's
    // target element is orphaned), then treat the animation as ready and
    // start it immediately. It is probably preferable to make playing
    // *always* asynchronous (e.g. by setting some additional state that
    // marks this player as pending and queueing a runnable to resolve the
    // start time). That situation, however, is currently rare enough that
    // we don't bother for now.
    StartNow();
    return;
  }

  PendingPlayerTracker* tracker = doc->GetOrCreatePendingPlayerTracker();
  tracker->AddPlayPending(*this);

  // We may have updated the current time when we set the hold time above
  // so notify source content.
  UpdateSourceContent();
}

void
AnimationPlayer::DoPause()
{
  if (mIsPending) {
    CancelPendingPlay();
    // Resolve the ready promise since we currently only use it for
    // players that are waiting to play. Later (in bug 1109390), we will
    // use this for players waiting to pause as well and then we won't
    // want to resolve it just yet.
    if (mReady) {
      mReady->MaybeResolve(this);
    }
  }

  // Mark this as no longer running on the compositor so that next time
  // we update animations we won't throttle them and will have a chance
  // to remove the animation from any layer it might be on.
  mIsRunningOnCompositor = false;

  // Bug 1109390 - check for null result here and go to pending state
  mHoldTime = GetCurrentTime();
  mStartTime.SetNull();
}

void
AnimationPlayer::UpdateSourceContent()
{
  if (mSource) {
    mSource->SetParentTime(GetCurrentTime());
  }
}

void
AnimationPlayer::FlushStyle() const
{
  nsIDocument* doc = GetRenderedDocument();
  if (doc) {
    doc->FlushPendingNotifications(Flush_Style);
  }
}

void
AnimationPlayer::PostUpdate()
{
  AnimationPlayerCollection* collection = GetCollection();
  if (collection) {
    collection->NotifyPlayerUpdated();
  }
}

void
AnimationPlayer::CancelPendingPlay()
{
  if (!mIsPending) {
    return;
  }

  nsIDocument* doc = GetRenderedDocument();
  if (doc) {
    PendingPlayerTracker* tracker = doc->GetPendingPlayerTracker();
    if (tracker) {
      tracker->RemovePlayPending(*this);
    }
  }

  mIsPending = false;
}

StickyTimeDuration
AnimationPlayer::SourceContentEnd() const
{
  if (!mSource) {
    return StickyTimeDuration(0);
  }

  return mSource->Timing().mDelay
         + mSource->GetComputedTiming().mActiveDuration;
}

nsIDocument*
AnimationPlayer::GetRenderedDocument() const
{
  if (!mSource) {
    return nullptr;
  }

  Element* targetElement;
  nsCSSPseudoElements::Type pseudoType;
  mSource->GetTarget(targetElement, pseudoType);
  if (!targetElement) {
    return nullptr;
  }

  return targetElement->GetComposedDoc();
}

nsPresContext*
AnimationPlayer::GetPresContext() const
{
  nsIDocument* doc = GetRenderedDocument();
  if (!doc) {
    return nullptr;
  }
  nsIPresShell* shell = doc->GetShell();
  if (!shell) {
    return nullptr;
  }
  return shell->GetPresContext();
}

AnimationPlayerCollection*
AnimationPlayer::GetCollection() const
{
  css::CommonAnimationManager* manager = GetAnimationManager();
  if (!manager) {
    return nullptr;
  }
  MOZ_ASSERT(mSource, "A player with an animation manager must have a source");

  Element* targetElement;
  nsCSSPseudoElements::Type targetPseudoType;
  mSource->GetTarget(targetElement, targetPseudoType);
  MOZ_ASSERT(targetElement,
             "A player with an animation manager must have a target");

  return manager->GetAnimationPlayers(targetElement, targetPseudoType, false);
}

} // namespace dom
} // namespace mozilla
