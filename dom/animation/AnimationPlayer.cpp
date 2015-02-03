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
  // Since we are not guaranteed to get only one call per refresh driver tick,
  // it's possible that mPendingReadyTime is set to a time in the future.
  // In that case, we should wait until the next refresh driver tick before
  // resuming.
  if (mIsPending &&
      !mPendingReadyTime.IsNull() &&
      mPendingReadyTime.Value() <= mTimeline->GetCurrentTime().Value()) {
    ResumeAt(mPendingReadyTime.Value());
    mPendingReadyTime.SetNull();
  }

  if (IsPossiblyOrphanedPendingPlayer()) {
    MOZ_ASSERT(mTimeline && !mTimeline->GetCurrentTime().IsNull(),
               "Orphaned pending players should have an active timeline");
    ResumeAt(mTimeline->GetCurrentTime().Value());
  }

  UpdateSourceContent();
}

void
AnimationPlayer::StartOnNextTick(const Nullable<TimeDuration>& aReadyTime)
{
  // Normally we expect the play state to be pending but it's possible that,
  // due to the handling of possibly orphaned players in Tick() [coming
  // in a later patch in this series], this player got started whilst still
  // being in another document's pending player map.
  if (PlayState() != AnimationPlayState::Pending) {
    return;
  }

  // If aReadyTime.IsNull() we'll detect this in Tick() where we check for
  // orphaned players and trigger this animation anyway
  mPendingReadyTime = aReadyTime;
}

void
AnimationPlayer::StartNow()
{
  MOZ_ASSERT(PlayState() == AnimationPlayState::Pending,
             "Expected to start a pending player");
  MOZ_ASSERT(mTimeline && !mTimeline->GetCurrentTime().IsNull(),
             "Expected an active timeline");

  ResumeAt(mTimeline->GetCurrentTime().Value());
}

Nullable<TimeDuration>
AnimationPlayer::GetCurrentOrPendingStartTime() const
{
  Nullable<TimeDuration> result;

  if (!mStartTime.IsNull()) {
    result = mStartTime;
    return result;
  }

  if (mPendingReadyTime.IsNull() || mHoldTime.IsNull()) {
    return result;
  }

  // Calculate the equivalent start time from the pending ready time.
  // This is the same as the calculation performed in ResumeAt and will
  // need to incorporate the playbackRate when implemented (bug 1127380).
  result.SetValue(mPendingReadyTime.Value() - mHoldTime.Value());
  return result;
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
    StartOnNextTick(Nullable<TimeDuration>());
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
AnimationPlayer::ResumeAt(const TimeDuration& aResumeTime)
{
  // This method is only expected to be called for a player that is
  // waiting to play. We can easily adapt it to handle other states
  // but it's currently not necessary.
  MOZ_ASSERT(PlayState() == AnimationPlayState::Pending,
             "Expected to resume a pending player");
  MOZ_ASSERT(!mHoldTime.IsNull(),
             "A player in the pending state should have a resolved hold time");

  mStartTime.SetValue(aResumeTime - mHoldTime.Value());
  mHoldTime.SetNull();
  mIsPending = false;

  UpdateSourceContent();

  if (mReady) {
    mReady->MaybeResolve(this);
  }
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
  mPendingReadyTime.SetNull();
}

bool
AnimationPlayer::IsPossiblyOrphanedPendingPlayer() const
{
  // Check if we are pending but might never start because we are not being
  // tracked.
  //
  // This covers the following cases:
  //
  // * We started playing but our source content's target element was orphaned
  //   or bound to a different document.
  //   (note that for the case of our source content changing we should handle
  //   that in SetSource)
  // * We started playing but our timeline became inactive.
  //   In this case the pending player tracker will drop us from its hashmap
  //   when we have been painted.
  // * When we started playing we couldn't find a PendingPlayerTracker to
  //   register with (perhaps the source content had no document) so we simply
  //   set mIsPending in DoPlay and relied on this method to catch us on the
  //   next tick.

  // If we're not pending we're ok.
  if (!mIsPending) {
    return false;
  }

  // If we have a pending ready time then we will be started on the next
  // tick.
  if (!mPendingReadyTime.IsNull()) {
    return false;
  }

  // If we don't have an active timeline then we shouldn't start until
  // we do.
  if (!mTimeline || mTimeline->GetCurrentTime().IsNull()) {
    return false;
  }

  // If we have no rendered document, or we're not in our rendered document's
  // PendingPlayerTracker then there's a good chance no one is tracking us.
  //
  // If we're wrong and another document is tracking us then, at worst, we'll
  // simply start the animation one tick too soon. That's better than never
  // starting the animation and is unlikely.
  nsIDocument* doc = GetRenderedDocument();
  return !doc ||
         !doc->GetPendingPlayerTracker() ||
         !doc->GetPendingPlayerTracker()->IsWaitingToPlay(*this);
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
