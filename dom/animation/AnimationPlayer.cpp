/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationPlayer.h"
#include "AnimationUtils.h"
#include "mozilla/dom/AnimationPlayerBinding.h"
#include "mozilla/AutoRestore.h"
#include "AnimationCommon.h" // For AnimationPlayerCollection,
                             // CommonAnimationManager
#include "nsIDocument.h" // For nsIDocument
#include "nsIPresShell.h" // For nsIPresShell
#include "nsLayoutUtils.h" // For PostRestyleEvent (remove after bug 1073336)
#include "PendingPlayerTracker.h" // For PendingPlayerTracker

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AnimationPlayer, mTimeline,
                                      mSource, mReady, mFinished)
NS_IMPL_CYCLE_COLLECTING_ADDREF(AnimationPlayer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AnimationPlayer)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AnimationPlayer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
AnimationPlayer::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::AnimationPlayerBinding::Wrap(aCx, this, aGivenProto);
}

void
AnimationPlayer::SetStartTime(const Nullable<TimeDuration>& aNewStartTime)
{
#if 1
  // Bug 1096776: once we support inactive/missing timelines we'll want to take
  // the disabled branch.
  MOZ_ASSERT(mTimeline && !mTimeline->GetCurrentTime().IsNull(),
             "We don't support inactive/missing timelines yet");
#else
  Nullable<TimeDuration> timelineTime = mTimeline->GetCurrentTime();
  if (mTimeline) {
    // The spec says to check if the timeline is active (has a resolved time)
    // before using it here, but we don't need to since it's harmless to set
    // the already null time to null.
    timelineTime = mTimeline->GetCurrentTime();
  }
  if (timelineTime.IsNull() && !aNewStartTime.IsNull()) {
    mHoldTime.SetNull();
  }
#endif
  Nullable<TimeDuration> previousCurrentTime = GetCurrentTime();
  mStartTime = aNewStartTime;
  if (!aNewStartTime.IsNull()) {
    if (mPlaybackRate != 0.0) {
      mHoldTime.SetNull();
    }
  } else {
    mHoldTime = previousCurrentTime;
  }

  CancelPendingTasks();
  if (mReady) {
    // We may have already resolved mReady, but in that case calling
    // MaybeResolve is a no-op, so that's okay.
    mReady->MaybeResolve(this);
  }

  UpdateTiming();
  PostUpdate();
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
      result.SetValue((timelineTime.Value() - mStartTime.Value())
                        .MultDouble(mPlaybackRate));
    }
  }
  return result;
}

// Implements http://w3c.github.io/web-animations/#silently-set-the-current-time
void
AnimationPlayer::SilentlySetCurrentTime(const TimeDuration& aSeekTime)
{
  if (!mHoldTime.IsNull() ||
      !mTimeline ||
      mTimeline->GetCurrentTime().IsNull() ||
      mPlaybackRate == 0.0
      /*or, once supported, if we have a pending pause task*/) {
    mHoldTime.SetValue(aSeekTime);
    if (!mTimeline || mTimeline->GetCurrentTime().IsNull()) {
      mStartTime.SetNull();
    }
  } else {
    mStartTime.SetValue(mTimeline->GetCurrentTime().Value() -
                          (aSeekTime.MultDouble(1 / mPlaybackRate)));
  }

  mPreviousCurrentTime.SetNull();
}

// Implements http://w3c.github.io/web-animations/#set-the-current-time
void
AnimationPlayer::SetCurrentTime(const TimeDuration& aSeekTime)
{
  SilentlySetCurrentTime(aSeekTime);

  if (mPendingState == PendingState::PausePending) {
    CancelPendingTasks();
    if (mReady) {
      mReady->MaybeResolve(this);
    }
  }

  UpdateFinishedState(true);
  UpdateSourceContent();
  PostUpdate();
}

void
AnimationPlayer::SetPlaybackRate(double aPlaybackRate)
{
  Nullable<TimeDuration> previousTime = GetCurrentTime();
  mPlaybackRate = aPlaybackRate;
  if (!previousTime.IsNull()) {
    ErrorResult rv;
    SetCurrentTime(previousTime.Value());
    MOZ_ASSERT(!rv.Failed(), "Should not assert for non-null time");
  }
}

void
AnimationPlayer::SilentlySetPlaybackRate(double aPlaybackRate)
{
  Nullable<TimeDuration> previousTime = GetCurrentTime();
  mPlaybackRate = aPlaybackRate;
  if (!previousTime.IsNull()) {
    ErrorResult rv;
    SilentlySetCurrentTime(previousTime.Value());
    MOZ_ASSERT(!rv.Failed(), "Should not assert for non-null time");
  }
}

AnimationPlayState
AnimationPlayer::PlayState() const
{
  if (mPendingState != PendingState::NotPending) {
    return AnimationPlayState::Pending;
  }

  Nullable<TimeDuration> currentTime = GetCurrentTime();
  if (currentTime.IsNull()) {
    return AnimationPlayState::Idle;
  }

  if (mStartTime.IsNull()) {
    return AnimationPlayState::Paused;
  }

  if ((mPlaybackRate > 0.0 && currentTime.Value() >= SourceContentEnd()) ||
      (mPlaybackRate < 0.0 && currentTime.Value().ToMilliseconds() <= 0.0)) {
    return AnimationPlayState::Finished;
  }

  return AnimationPlayState::Running;
}

static inline already_AddRefed<Promise>
CreatePromise(DocumentTimeline* aTimeline, ErrorResult& aRv)
{
  nsIGlobalObject* global = aTimeline->GetParentObject();
  if (global) {
    return Promise::Create(global, aRv);
  }
  return nullptr;
}

Promise*
AnimationPlayer::GetReady(ErrorResult& aRv)
{
  if (!mReady) {
    mReady = CreatePromise(mTimeline, aRv); // Lazily create on demand
  }
  if (!mReady) {
    aRv.Throw(NS_ERROR_FAILURE);
  } else if (PlayState() != AnimationPlayState::Pending) {
    mReady->MaybeResolve(this);
  }
  return mReady;
}

Promise*
AnimationPlayer::GetFinished(ErrorResult& aRv)
{
  if (!mFinished) {
    mFinished = CreatePromise(mTimeline, aRv); // Lazily create on demand
  }
  if (!mFinished) {
    aRv.Throw(NS_ERROR_FAILURE);
  } else if (IsFinished()) {
    mFinished->MaybeResolve(this);
  }
  return mFinished;
}

void
AnimationPlayer::Play(LimitBehavior aLimitBehavior)
{
  DoPlay(aLimitBehavior);
  PostUpdate();
}

void
AnimationPlayer::Pause()
{
  // TODO: The DoPause() call should not be synchronous (bug 1109390). See
  // http://w3c.github.io/web-animations/#pausing-an-animation-section
  DoPause();
  PostUpdate();
}

Nullable<double>
AnimationPlayer::GetStartTimeAsDouble() const
{
  return AnimationUtils::TimeDurationToDouble(mStartTime);
}

void
AnimationPlayer::SetStartTimeAsDouble(const Nullable<double>& aStartTime)
{
  return SetStartTime(AnimationUtils::DoubleToTimeDuration(aStartTime));
}
  
Nullable<double>
AnimationPlayer::GetCurrentTimeAsDouble() const
{
  return AnimationUtils::TimeDurationToDouble(GetCurrentTime());
}

void
AnimationPlayer::SetCurrentTimeAsDouble(const Nullable<double>& aCurrentTime,
                                        ErrorResult& aRv)
{
  if (aCurrentTime.IsNull()) {
    if (!GetCurrentTime().IsNull()) {
      aRv.Throw(NS_ERROR_DOM_TYPE_ERR);
    }
    return;
  }

  return SetCurrentTime(TimeDuration::FromMilliseconds(aCurrentTime.Value()));
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
  UpdateRelevance();
}

void
AnimationPlayer::Tick()
{
  // Since we are not guaranteed to get only one call per refresh driver tick,
  // it's possible that mPendingReadyTime is set to a time in the future.
  // In that case, we should wait until the next refresh driver tick before
  // resuming.
  if (mPendingState != PendingState::NotPending &&
      !mPendingReadyTime.IsNull() &&
      mPendingReadyTime.Value() <= mTimeline->GetCurrentTime().Value()) {
    FinishPendingAt(mPendingReadyTime.Value());
    mPendingReadyTime.SetNull();
  }

  if (IsPossiblyOrphanedPendingPlayer()) {
    MOZ_ASSERT(mTimeline && !mTimeline->GetCurrentTime().IsNull(),
               "Orphaned pending players should have an active timeline");
    FinishPendingAt(mTimeline->GetCurrentTime().Value());
  }

  UpdateTiming();
}

void
AnimationPlayer::TriggerOnNextTick(const Nullable<TimeDuration>& aReadyTime)
{
  // Normally we expect the play state to be pending but it's possible that,
  // due to the handling of possibly orphaned players in Tick(), this player got
  // started whilst still being in another document's pending player map.
  if (PlayState() != AnimationPlayState::Pending) {
    return;
  }

  // If aReadyTime.IsNull() we'll detect this in Tick() where we check for
  // orphaned players and trigger this animation anyway
  mPendingReadyTime = aReadyTime;
}

void
AnimationPlayer::TriggerNow()
{
  MOZ_ASSERT(PlayState() == AnimationPlayState::Pending,
             "Expected to start a pending player");
  MOZ_ASSERT(mTimeline && !mTimeline->GetCurrentTime().IsNull(),
             "Expected an active timeline");

  FinishPendingAt(mTimeline->GetCurrentTime().Value());
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
  if (mPendingState != PendingState::NotPending) {
    CancelPendingTasks();
    if (mReady) {
      mReady->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    }
  }

  if (mFinished) {
    mFinished->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }
  // Clear finished promise. We'll create a new one lazily.
  mFinished = nullptr;

  mHoldTime.SetNull();
  mStartTime.SetNull();

  UpdateSourceContent();
}

void
AnimationPlayer::UpdateRelevance()
{
  bool wasRelevant = mIsRelevant;
  mIsRelevant = HasCurrentSource() || HasInEffectSource();

  // Notify animation observers.
  if (wasRelevant && !mIsRelevant) {
    nsNodeUtils::AnimationRemoved(this);
  } else if (!wasRelevant && mIsRelevant) {
    nsNodeUtils::AnimationAdded(this);
  }
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
  return mFinishedAtLastComposeStyle;
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

  // In order to prevent flicker, there are a few cases where we want to use
  // a different time for rendering that would otherwise be returned by
  // GetCurrentTime. These are:
  //
  // (a) For animations that are pausing but which are still running on the
  //     compositor. In this case we send a layer transaction that removes the
  //     animation but which also contains the animation values calculated on
  //     the main thread. To prevent flicker when this occurs we want to ensure
  //     the timeline time used to calculate the main thread animation values
  //     does not lag far behind the time used on the compositor. Ideally we
  //     would like to use the "animation ready time" calculated at the end of
  //     the layer transaction as the timeline time but it will be too late to
  //     update the style rule at that point so instead we just use the current
  //     wallclock time.
  //
  // (b) For animations that are pausing that we have already taken off the
  //     compositor. In this case we record a pending ready time but we don't
  //     apply it until the next tick. However, while waiting for the next tick,
  //     we should still use the pending ready time as the timeline time. If we
  //     use the regular timeline time the animation may appear jump backwards
  //     if the main thread's timeline time lags behind the compositor.
  //
  // (c) For animations that are play-pending due to an aborted pause operation
  //     (i.e. a pause operation that was interrupted before we entered the
  //     paused state). When we cancel a pending pause we might momentarily take
  //     the animation off the compositor, only to re-add it moments later. In
  //     that case the compositor might have been ahead of the main thread so we
  //     should use the current wallclock time to ensure the animation doesn't
  //     temporarily jump backwards.
  //
  // To address each of these cases we temporarily tweak the hold time
  // immediately before updating the style rule and then restore it immediately
  // afterwards. This is purely to prevent visual flicker. Other behavior
  // such as dispatching events continues to rely on the regular timeline time.
  {
    AutoRestore<Nullable<TimeDuration>> restoreHoldTime(mHoldTime);
    bool updatedHoldTime = false;

    AnimationPlayState playState = PlayState();

    if (playState == AnimationPlayState::Pending &&
        mHoldTime.IsNull() &&
        !mStartTime.IsNull()) {
      Nullable<TimeDuration> timeToUse = mPendingReadyTime;
      if (timeToUse.IsNull() &&
          mTimeline &&
          !mTimeline->IsUnderTestControl()) {
        timeToUse = mTimeline->ToTimelineTime(TimeStamp::Now());
      }
      if (!timeToUse.IsNull()) {
        mHoldTime.SetValue((timeToUse.Value() - mStartTime.Value())
                            .MultDouble(mPlaybackRate));
        // Push the change down to the source content
        UpdateSourceContent();
        updatedHoldTime = true;
      }
    }

    mSource->ComposeStyle(aStyleRule, aSetProperties);

    if (updatedHoldTime) {
      UpdateTiming();
    }

    mFinishedAtLastComposeStyle = (playState == AnimationPlayState::Finished);
  }
}

void
AnimationPlayer::DoPlay(LimitBehavior aLimitBehavior)
{
  bool abortedPause = mPendingState == PendingState::PausePending;

  bool reuseReadyPromise = false;
  if (mPendingState != PendingState::NotPending) {
    CancelPendingTasks();
    reuseReadyPromise = true;
  }

  Nullable<TimeDuration> currentTime = GetCurrentTime();
  if (mPlaybackRate > 0.0 &&
      (currentTime.IsNull() ||
       (aLimitBehavior == LimitBehavior::AutoRewind &&
        (currentTime.Value().ToMilliseconds() < 0.0 ||
         currentTime.Value() >= SourceContentEnd())))) {
    mHoldTime.SetValue(TimeDuration(0));
  } else if (mPlaybackRate < 0.0 &&
             (currentTime.IsNull() ||
              (aLimitBehavior == LimitBehavior::AutoRewind &&
               (currentTime.Value().ToMilliseconds() <= 0.0 ||
                currentTime.Value() > SourceContentEnd())))) {
    mHoldTime.SetValue(TimeDuration(SourceContentEnd()));
  } else if (mPlaybackRate == 0.0 && currentTime.IsNull()) {
    mHoldTime.SetValue(TimeDuration(0));
  }

  // If the hold time is null then we're either already playing normally (and
  // we can ignore this call) or we aborted a pending pause operation (in which
  // case, for consistency, we need to go through the motions of doing an
  // asynchronous start even though we already have a resolved start time).
  if (mHoldTime.IsNull() && !abortedPause) {
    return;
  }

  // Clear the start time until we resolve a new one (unless we are aborting
  // a pending pause operation, in which case we keep the old start time so
  // that the animation continues moving uninterrupted by the aborted pause).
  if (!abortedPause) {
    mStartTime.SetNull();
  }

  if (!reuseReadyPromise) {
    // Clear ready promise. We'll create a new one lazily.
    mReady = nullptr;
  }

  mPendingState = PendingState::PlayPending;

  nsIDocument* doc = GetRenderedDocument();
  if (!doc) {
    TriggerOnNextTick(Nullable<TimeDuration>());
    return;
  }

  PendingPlayerTracker* tracker = doc->GetOrCreatePendingPlayerTracker();
  tracker->AddPlayPending(*this);

  // We may have updated the current time when we set the hold time above.
  UpdateTiming();
}

void
AnimationPlayer::DoPause()
{
  if (IsPausedOrPausing()) {
    return;
  }

  bool reuseReadyPromise = false;
  if (mPendingState == PendingState::PlayPending) {
    CancelPendingTasks();
    reuseReadyPromise = true;
  }

  // Mark this as no longer running on the compositor so that next time
  // we update animations we won't throttle them and will have a chance
  // to remove the animation from any layer it might be on.
  mIsRunningOnCompositor = false;

  if (!reuseReadyPromise) {
    // Clear ready promise. We'll create a new one lazily.
    mReady = nullptr;
  }

  mPendingState = PendingState::PausePending;

  nsIDocument* doc = GetRenderedDocument();
  if (!doc) {
    TriggerOnNextTick(Nullable<TimeDuration>());
    return;
  }

  PendingPlayerTracker* tracker = doc->GetOrCreatePendingPlayerTracker();
  tracker->AddPausePending(*this);

  UpdateFinishedState();
}

void
AnimationPlayer::ResumeAt(const TimeDuration& aReadyTime)
{
  // This method is only expected to be called for a player that is
  // waiting to play. We can easily adapt it to handle other states
  // but it's currently not necessary.
  MOZ_ASSERT(mPendingState == PendingState::PlayPending,
             "Expected to resume a play-pending player");
  MOZ_ASSERT(mHoldTime.IsNull() != mStartTime.IsNull(),
             "A player in the play-pending state should have either a"
             " resolved hold time or resolved start time (but not both)");

  // If we aborted a pending pause operation we will already have a start time
  // we should use. In all other cases, we resolve it from the ready time.
  if (mStartTime.IsNull()) {
    if (mPlaybackRate != 0) {
      mStartTime.SetValue(aReadyTime -
                          (mHoldTime.Value().MultDouble(1 / mPlaybackRate)));
      mHoldTime.SetNull();
    } else {
      mStartTime.SetValue(aReadyTime);
    }
  }
  mPendingState = PendingState::NotPending;

  UpdateTiming();

  if (mReady) {
    mReady->MaybeResolve(this);
  }
}

void
AnimationPlayer::PauseAt(const TimeDuration& aReadyTime)
{
  MOZ_ASSERT(mPendingState == PendingState::PausePending,
             "Expected to pause a pause-pending player");

  if (!mStartTime.IsNull()) {
    mHoldTime.SetValue((aReadyTime - mStartTime.Value())
                        .MultDouble(mPlaybackRate));
  }
  mStartTime.SetNull();
  mPendingState = PendingState::NotPending;

  UpdateTiming();

  if (mReady) {
    mReady->MaybeResolve(this);
  }
}

void
AnimationPlayer::UpdateTiming()
{
  // We call UpdateFinishedState before UpdateSourceContent because the former
  // can change the current time, which is used by the latter.
  UpdateFinishedState();
  UpdateSourceContent();
}

void
AnimationPlayer::UpdateFinishedState(bool aSeekFlag)
{
  Nullable<TimeDuration> currentTime = GetCurrentTime();
  TimeDuration targetEffectEnd = TimeDuration(SourceContentEnd());

  if (!mStartTime.IsNull() &&
      mPendingState == PendingState::NotPending) {
    if (mPlaybackRate > 0.0 &&
        !currentTime.IsNull() &&
        currentTime.Value() >= targetEffectEnd) {
      if (aSeekFlag) {
        mHoldTime = currentTime;
      } else if (!mPreviousCurrentTime.IsNull()) {
        mHoldTime.SetValue(std::max(mPreviousCurrentTime.Value(),
                                    targetEffectEnd));
      } else {
        mHoldTime.SetValue(targetEffectEnd);
      }
    } else if (mPlaybackRate < 0.0 &&
               !currentTime.IsNull() &&
               currentTime.Value().ToMilliseconds() <= 0.0) {
      if (aSeekFlag) {
        mHoldTime = currentTime;
      } else {
        mHoldTime.SetValue(0);
      }
    } else if (mPlaybackRate != 0.0 &&
               !currentTime.IsNull()) {
      if (aSeekFlag && !mHoldTime.IsNull()) {
        mStartTime.SetValue(mTimeline->GetCurrentTime().Value() -
                              (mHoldTime.Value().MultDouble(1 / mPlaybackRate)));
      }
      mHoldTime.SetNull();
    }
  }

  bool currentFinishedState = IsFinished();
  if (currentFinishedState && !mIsPreviousStateFinished) {
    if (mFinished) {
      mFinished->MaybeResolve(this);
    }
  } else if (!currentFinishedState && mIsPreviousStateFinished) {
    // Clear finished promise. We'll create a new one lazily.
    mFinished = nullptr;
  }
  mIsPreviousStateFinished = currentFinishedState;
  // We must recalculate the current time to take account of any mHoldTime
  // changes the code above made.
  mPreviousCurrentTime = GetCurrentTime();
}

void
AnimationPlayer::UpdateSourceContent()
{
  if (mSource) {
    mSource->SetParentTime(GetCurrentTime());
    UpdateRelevance();
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
AnimationPlayer::CancelPendingTasks()
{
  if (mPendingState == PendingState::NotPending) {
    return;
  }

  nsIDocument* doc = GetRenderedDocument();
  if (doc) {
    PendingPlayerTracker* tracker = doc->GetPendingPlayerTracker();
    if (tracker) {
      if (mPendingState == PendingState::PlayPending) {
        tracker->RemovePlayPending(*this);
      } else {
        tracker->RemovePausePending(*this);
      }
    }
  }

  mPendingState = PendingState::NotPending;
  mPendingReadyTime.SetNull();
}

bool
AnimationPlayer::IsFinished() const
{
  // Unfortunately there's some weirdness in the spec at the moment where if
  // you're finished and paused, the playState is paused. This prevents us
  // from just checking |PlayState() == AnimationPlayState::Finished| here,
  // and we need this much more messy check to see if we're finished.
  Nullable<TimeDuration> currentTime = GetCurrentTime();
  return !currentTime.IsNull() &&
      ((mPlaybackRate > 0.0 && currentTime.Value() >= SourceContentEnd()) ||
       (mPlaybackRate < 0.0 && currentTime.Value().ToMilliseconds() <= 0.0));
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
  //   set mPendingState in DoPlay and relied on this method to catch us on the
  //   next tick.

  // If we're not pending we're ok.
  if (mPendingState == PendingState::NotPending) {
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
  // simply start/pause the animation one tick too soon. That's better than
  // never starting/pausing the animation and is unlikely.
  nsIDocument* doc = GetRenderedDocument();
  if (!doc) {
    return false;
  }

  PendingPlayerTracker* tracker = doc->GetPendingPlayerTracker();
  return !tracker ||
         (!tracker->IsWaitingToPlay(*this) &&
          !tracker->IsWaitingToPause(*this));
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

  return manager->GetAnimations(targetElement, targetPseudoType, false);
}

} // namespace dom
} // namespace mozilla
