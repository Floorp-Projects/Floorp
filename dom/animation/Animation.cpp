/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Animation.h"
#include "AnimationUtils.h"
#include "mozilla/dom/AnimationBinding.h"
#include "mozilla/dom/AnimationPlaybackEvent.h"
#include "mozilla/AnimationTarget.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/AsyncEventDispatcher.h" // For AsyncEventDispatcher
#include "mozilla/Maybe.h" // For Maybe
#include "nsAnimationManager.h" // For CSSAnimation
#include "nsDOMMutationObserver.h" // For nsAutoAnimationMutationBatch
#include "nsIDocument.h" // For nsIDocument
#include "nsIPresShell.h" // For nsIPresShell
#include "nsLayoutUtils.h" // For PostRestyleEvent (remove after bug 1073336)
#include "nsThreadUtils.h" // For nsRunnableMethod and nsRevocableEventPtr
#include "nsTransitionManager.h" // For CSSTransition
#include "PendingAnimationTracker.h" // For PendingAnimationTracker

namespace mozilla {
namespace dom {

// Static members
uint64_t Animation::sNextAnimationIndex = 0;

NS_IMPL_CYCLE_COLLECTION_INHERITED(Animation, DOMEventTargetHelper,
                                   mTimeline,
                                   mEffect,
                                   mReady,
                                   mFinished)

NS_IMPL_ADDREF_INHERITED(Animation, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Animation, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Animation)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

JSObject*
Animation::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::AnimationBinding::Wrap(aCx, this, aGivenProto);
}

// ---------------------------------------------------------------------------
//
// Utility methods
//
// ---------------------------------------------------------------------------

namespace {
  // A wrapper around nsAutoAnimationMutationBatch that looks up the
  // appropriate document from the supplied animation.
  class MOZ_RAII AutoMutationBatchForAnimation {
  public:
    explicit AutoMutationBatchForAnimation(const Animation& aAnimation
                                           MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      Maybe<NonOwningAnimationTarget> target =
        nsNodeUtils::GetTargetForAnimation(&aAnimation);
      if (!target) {
        return;
      }

      // For mutation observers, we use the OwnerDoc.
      nsIDocument* doc = target->mElement->OwnerDoc();
      if (!doc) {
        return;
      }

      mAutoBatch.emplace(doc);
    }

  private:
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
    Maybe<nsAutoAnimationMutationBatch> mAutoBatch;
  };
}

// ---------------------------------------------------------------------------
//
// Animation interface:
//
// ---------------------------------------------------------------------------
/* static */ already_AddRefed<Animation>
Animation::Constructor(const GlobalObject& aGlobal,
                       KeyframeEffectReadOnly* aEffect,
                       AnimationTimeline* aTimeline,
                       ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<Animation> animation = new Animation(global);

  if (!aEffect) {
    // Bug 1049975: We do not support null effect yet.
    aRv.Throw(NS_ERROR_DOM_ANIM_NO_EFFECT_ERR);
    return nullptr;
  }
  if (!aTimeline) {
    // Bug 1096776: We do not support null timeline yet.
    aRv.Throw(NS_ERROR_DOM_ANIM_NO_TIMELINE_ERR);
    return nullptr;
  }

  animation->SetTimeline(aTimeline);
  animation->SetEffect(aEffect);

  return animation.forget();
}

void
Animation::SetId(const nsAString& aId)
{
  if (mId == aId) {
    return;
  }
  mId = aId;
  nsNodeUtils::AnimationChanged(this);
}

void
Animation::SetEffect(KeyframeEffectReadOnly* aEffect)
{
  RefPtr<Animation> kungFuDeathGrip(this);

  if (mEffect == aEffect) {
    return;
  }
  if (mEffect) {
    mEffect->SetAnimation(nullptr);
  }
  mEffect = aEffect;
  if (mEffect) {
    mEffect->SetAnimation(this);
  }

  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Async);
}

void
Animation::SetTimeline(AnimationTimeline* aTimeline)
{
  if (mTimeline == aTimeline) {
    return;
  }

  if (mTimeline) {
    mTimeline->NotifyAnimationUpdated(*this);
  }

  mTimeline = aTimeline;

  // FIXME(spec): Once we implement the seeking defined in the spec
  // surely this should be SeekFlag::DidSeek but the spec says otherwise.
  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Async);

  // FIXME: When we expose this method to script we'll need to call PostUpdate
  // (but *not* when this method gets called from style).
}

// https://w3c.github.io/web-animations/#set-the-animation-start-time
void
Animation::SetStartTime(const Nullable<TimeDuration>& aNewStartTime)
{
  if (aNewStartTime == mStartTime) {
    return;
  }

  AutoMutationBatchForAnimation mb(*this);

  Nullable<TimeDuration> timelineTime;
  if (mTimeline) {
    // The spec says to check if the timeline is active (has a resolved time)
    // before using it here, but we don't need to since it's harmless to set
    // the already null time to null.
    timelineTime = mTimeline->GetCurrentTime();
  }
  if (timelineTime.IsNull() && !aNewStartTime.IsNull()) {
    mHoldTime.SetNull();
  }

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

  UpdateTiming(SeekFlag::DidSeek, SyncNotifyFlag::Async);
  if (IsRelevant()) {
    nsNodeUtils::AnimationChanged(this);
  }
  PostUpdate();
}

// https://w3c.github.io/web-animations/#current-time
Nullable<TimeDuration>
Animation::GetCurrentTime() const
{
  Nullable<TimeDuration> result;
  if (!mHoldTime.IsNull()) {
    result = mHoldTime;
    return result;
  }

  if (mTimeline && !mStartTime.IsNull()) {
    Nullable<TimeDuration> timelineTime = mTimeline->GetCurrentTime();
    if (!timelineTime.IsNull()) {
      result.SetValue((timelineTime.Value() - mStartTime.Value())
                        .MultDouble(mPlaybackRate));
    }
  }
  return result;
}

// https://w3c.github.io/web-animations/#set-the-current-time
void
Animation::SetCurrentTime(const TimeDuration& aSeekTime)
{
  // Return early if the current time has not changed. However, if we
  // are pause-pending, then setting the current time to any value
  // including the current value has the effect of aborting the
  // pause so we should not return early in that case.
  if (mPendingState != PendingState::PausePending &&
      Nullable<TimeDuration>(aSeekTime) == GetCurrentTime()) {
    return;
  }

  AutoMutationBatchForAnimation mb(*this);

  SilentlySetCurrentTime(aSeekTime);

  if (mPendingState == PendingState::PausePending) {
    // Finish the pause operation
    mHoldTime.SetValue(aSeekTime);
    mStartTime.SetNull();

    if (mReady) {
      mReady->MaybeResolve(this);
    }
    CancelPendingTasks();
  }

  UpdateTiming(SeekFlag::DidSeek, SyncNotifyFlag::Async);
  if (IsRelevant()) {
    nsNodeUtils::AnimationChanged(this);
  }
  PostUpdate();
}

// https://w3c.github.io/web-animations/#set-the-animation-playback-rate
void
Animation::SetPlaybackRate(double aPlaybackRate)
{
  if (aPlaybackRate == mPlaybackRate) {
    return;
  }

  AutoMutationBatchForAnimation mb(*this);

  Nullable<TimeDuration> previousTime = GetCurrentTime();
  mPlaybackRate = aPlaybackRate;
  if (!previousTime.IsNull()) {
    SetCurrentTime(previousTime.Value());
  }

  // In the case where GetCurrentTime() returns the same result before and
  // after updating mPlaybackRate, SetCurrentTime will return early since,
  // as far as it can tell, nothing has changed.
  // As a result, we need to perform the following updates here:
  // - update timing (since, if the sign of the playback rate has changed, our
  //   finished state may have changed),
  // - dispatch a change notification for the changed playback rate, and
  // - update the playback rate on animations on layers.
  UpdateTiming(SeekFlag::DidSeek, SyncNotifyFlag::Async);
  if (IsRelevant()) {
    nsNodeUtils::AnimationChanged(this);
  }
  PostUpdate();
}

// https://w3c.github.io/web-animations/#play-state
AnimationPlayState
Animation::PlayState() const
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

  if ((mPlaybackRate > 0.0 && currentTime.Value() >= EffectEnd()) ||
      (mPlaybackRate < 0.0 && currentTime.Value().ToMilliseconds() <= 0.0)) {
    return AnimationPlayState::Finished;
  }

  return AnimationPlayState::Running;
}

Promise*
Animation::GetReady(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal();
  if (!mReady && global) {
    mReady = Promise::Create(global, aRv); // Lazily create on demand
  }
  if (!mReady) {
    aRv.Throw(NS_ERROR_FAILURE);
  } else if (PlayState() != AnimationPlayState::Pending) {
    mReady->MaybeResolve(this);
  }
  return mReady;
}

Promise*
Animation::GetFinished(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal();
  if (!mFinished && global) {
    mFinished = Promise::Create(global, aRv); // Lazily create on demand
  }
  if (!mFinished) {
    aRv.Throw(NS_ERROR_FAILURE);
  } else if (mFinishedIsResolved) {
    MaybeResolveFinishedPromise();
  }
  return mFinished;
}

void
Animation::Cancel()
{
  DoCancel();
  PostUpdate();
}

// https://w3c.github.io/web-animations/#finish-an-animation
void
Animation::Finish(ErrorResult& aRv)
{
  if (mPlaybackRate == 0 ||
      (mPlaybackRate > 0 && EffectEnd() == TimeDuration::Forever())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  AutoMutationBatchForAnimation mb(*this);

  // Seek to the end
  TimeDuration limit =
    mPlaybackRate > 0 ? TimeDuration(EffectEnd()) : TimeDuration(0);
  bool didChange = GetCurrentTime() != Nullable<TimeDuration>(limit);
  SilentlySetCurrentTime(limit);

  // If we are paused or play-pending we need to fill in the start time in
  // order to transition to the finished state.
  //
  // We only do this, however, if we have an active timeline. If we have an
  // inactive timeline we can't transition into the finished state just like
  // we can't transition to the running state (this finished state is really
  // a substate of the running state).
  if (mStartTime.IsNull() &&
      mTimeline &&
      !mTimeline->GetCurrentTime().IsNull()) {
    mStartTime.SetValue(mTimeline->GetCurrentTime().Value() -
                        limit.MultDouble(1.0 / mPlaybackRate));
    didChange = true;
  }

  // If we just resolved the start time for a pause or play-pending
  // animation, we need to clear the task. We don't do this as a branch of
  // the above however since we can have a play-pending animation with a
  // resolved start time if we aborted a pause operation.
  if (!mStartTime.IsNull() &&
      (mPendingState == PendingState::PlayPending ||
       mPendingState == PendingState::PausePending)) {
    if (mPendingState == PendingState::PausePending) {
      mHoldTime.SetNull();
    }
    CancelPendingTasks();
    didChange = true;
    if (mReady) {
      mReady->MaybeResolve(this);
    }
  }
  UpdateTiming(SeekFlag::DidSeek, SyncNotifyFlag::Sync);
  if (didChange && IsRelevant()) {
    nsNodeUtils::AnimationChanged(this);
  }
  PostUpdate();
}

void
Animation::Play(ErrorResult& aRv, LimitBehavior aLimitBehavior)
{
  DoPlay(aRv, aLimitBehavior);
  PostUpdate();
}

void
Animation::Pause(ErrorResult& aRv)
{
  DoPause(aRv);
  PostUpdate();
}

// https://w3c.github.io/web-animations/#reverse-an-animation
void
Animation::Reverse(ErrorResult& aRv)
{
  if (!mTimeline || mTimeline->GetCurrentTime().IsNull()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mPlaybackRate == 0.0) {
    return;
  }

  AutoMutationBatchForAnimation mb(*this);

  SilentlySetPlaybackRate(-mPlaybackRate);
  Play(aRv, LimitBehavior::AutoRewind);

  if (IsRelevant()) {
    nsNodeUtils::AnimationChanged(this);
  }
}

// ---------------------------------------------------------------------------
//
// JS wrappers for Animation interface:
//
// ---------------------------------------------------------------------------

Nullable<double>
Animation::GetStartTimeAsDouble() const
{
  return AnimationUtils::TimeDurationToDouble(mStartTime);
}

void
Animation::SetStartTimeAsDouble(const Nullable<double>& aStartTime)
{
  return SetStartTime(AnimationUtils::DoubleToTimeDuration(aStartTime));
}

Nullable<double>
Animation::GetCurrentTimeAsDouble() const
{
  return AnimationUtils::TimeDurationToDouble(GetCurrentTime());
}

void
Animation::SetCurrentTimeAsDouble(const Nullable<double>& aCurrentTime,
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

// ---------------------------------------------------------------------------

void
Animation::Tick()
{
  // Finish pending if we have a pending ready time, but only if we also
  // have an active timeline.
  if (mPendingState != PendingState::NotPending &&
      !mPendingReadyTime.IsNull() &&
      mTimeline &&
      !mTimeline->GetCurrentTime().IsNull()) {
    // Even though mPendingReadyTime is initialized using TimeStamp::Now()
    // during the *previous* tick of the refresh driver, it can still be
    // ahead of the *current* timeline time when we are using the
    // vsync timer so we need to clamp it to the timeline time.
    mPendingReadyTime.SetValue(std::min(mTimeline->GetCurrentTime().Value(),
                                        mPendingReadyTime.Value()));
    FinishPendingAt(mPendingReadyTime.Value());
    mPendingReadyTime.SetNull();
  }

  if (IsPossiblyOrphanedPendingAnimation()) {
    MOZ_ASSERT(mTimeline && !mTimeline->GetCurrentTime().IsNull(),
               "Orphaned pending animtaions should have an active timeline");
    FinishPendingAt(mTimeline->GetCurrentTime().Value());
  }

  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Async);

  // Update layers if we are newly finished.
  if (mEffect &&
      !mEffect->Properties().IsEmpty() &&
      !mFinishedAtLastComposeStyle &&
      PlayState() == AnimationPlayState::Finished) {
    PostUpdate();
  }
}

void
Animation::TriggerOnNextTick(const Nullable<TimeDuration>& aReadyTime)
{
  // Normally we expect the play state to be pending but it's possible that,
  // due to the handling of possibly orphaned animations in Tick(), this
  // animation got started whilst still being in another document's pending
  // animation map.
  if (PlayState() != AnimationPlayState::Pending) {
    return;
  }

  // If aReadyTime.IsNull() we'll detect this in Tick() where we check for
  // orphaned animations and trigger this animation anyway
  mPendingReadyTime = aReadyTime;
}

void
Animation::TriggerNow()
{
  // Normally we expect the play state to be pending but when an animation
  // is cancelled and its rendered document can't be reached, we can end up
  // with the animation still in a pending player tracker even after it is
  // no longer pending.
  if (PlayState() != AnimationPlayState::Pending) {
    return;
  }

  // If we don't have an active timeline we can't trigger the animation.
  // However, this is a test-only method that we don't expect to be used in
  // conjunction with animations without an active timeline so generate
  // a warning if we do find ourselves in that situation.
  if (!mTimeline || mTimeline->GetCurrentTime().IsNull()) {
    NS_WARNING("Failed to trigger an animation with an active timeline");
    return;
  }

  FinishPendingAt(mTimeline->GetCurrentTime().Value());
}

Nullable<TimeDuration>
Animation::GetCurrentOrPendingStartTime() const
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

TimeStamp
Animation::AnimationTimeToTimeStamp(const StickyTimeDuration& aTime) const
{
  // Initializes to null. Return the same object every time to benefit from
  // return-value-optimization.
  TimeStamp result;

  // We *don't* check for mTimeline->TracksWallclockTime() here because that
  // method only tells us if the timeline times can be converted to
  // TimeStamps that can be compared to TimeStamp::Now() or not, *not*
  // whether the timelines can be converted to TimeStamp values at all.
  //
  // Furthermore, we want to be able to use this method when the refresh driver
  // is under test control (in which case TracksWallclockTime() will return
  // false).
  //
  // Once we introduce timelines that are not time-based we will need to
  // differentiate between them here and determine how to sort their events.
  if (!mTimeline) {
    return result;
  }

  // Check the time is convertible to a timestamp
  if (aTime == TimeDuration::Forever() ||
      mPlaybackRate == 0.0 ||
      mStartTime.IsNull()) {
    return result;
  }

  // Invert the standard relation:
  //   animation time = (timeline time - start time) * playback rate
  TimeDuration timelineTime =
    TimeDuration(aTime).MultDouble(1.0 / mPlaybackRate) + mStartTime.Value();

  result = mTimeline->ToTimeStamp(timelineTime);
  return result;
}

// https://w3c.github.io/web-animations/#silently-set-the-current-time
void
Animation::SilentlySetCurrentTime(const TimeDuration& aSeekTime)
{
  if (!mHoldTime.IsNull() ||
      mStartTime.IsNull() ||
      !mTimeline ||
      mTimeline->GetCurrentTime().IsNull() ||
      mPlaybackRate == 0.0) {
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

void
Animation::SilentlySetPlaybackRate(double aPlaybackRate)
{
  Nullable<TimeDuration> previousTime = GetCurrentTime();
  mPlaybackRate = aPlaybackRate;
  if (!previousTime.IsNull()) {
    SilentlySetCurrentTime(previousTime.Value());
  }
}

// https://w3c.github.io/web-animations/#cancel-an-animation
void
Animation::DoCancel()
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
  ResetFinishedPromise();

  DispatchPlaybackEvent(NS_LITERAL_STRING("cancel"));

  mHoldTime.SetNull();
  mStartTime.SetNull();

  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Async);

  if (mTimeline) {
    mTimeline->RemoveAnimation(this);
  }
}

void
Animation::UpdateRelevance()
{
  bool wasRelevant = mIsRelevant;
  mIsRelevant = HasCurrentEffect() || IsInEffect();

  // Notify animation observers.
  if (wasRelevant && !mIsRelevant) {
    nsNodeUtils::AnimationRemoved(this);
  } else if (!wasRelevant && mIsRelevant) {
    nsNodeUtils::AnimationAdded(this);
  }
}

bool
Animation::HasLowerCompositeOrderThan(const Animation& aOther) const
{
  // 0. Object-equality case
  if (&aOther == this) {
    return false;
  }

  // 1. CSS Transitions sort lowest
  {
    auto asCSSTransitionForSorting =
      [] (const Animation& anim) -> const CSSTransition*
      {
        const CSSTransition* transition = anim.AsCSSTransition();
        return transition && transition->IsTiedToMarkup() ?
               transition :
               nullptr;
      };
    auto thisTransition  = asCSSTransitionForSorting(*this);
    auto otherTransition = asCSSTransitionForSorting(aOther);
    if (thisTransition && otherTransition) {
      return thisTransition->HasLowerCompositeOrderThan(*otherTransition);
    }
    if (thisTransition || otherTransition) {
      return thisTransition;
    }
  }

  // 2. CSS Animations sort next
  {
    auto asCSSAnimationForSorting =
      [] (const Animation& anim) -> const CSSAnimation*
      {
        const CSSAnimation* animation = anim.AsCSSAnimation();
        return animation && animation->IsTiedToMarkup() ? animation : nullptr;
      };
    auto thisAnimation  = asCSSAnimationForSorting(*this);
    auto otherAnimation = asCSSAnimationForSorting(aOther);
    if (thisAnimation && otherAnimation) {
      return thisAnimation->HasLowerCompositeOrderThan(*otherAnimation);
    }
    if (thisAnimation || otherAnimation) {
      return thisAnimation;
    }
  }

  // Subclasses of Animation repurpose mAnimationIndex to implement their
  // own brand of composite ordering. However, by this point we should have
  // handled any such custom composite ordering so we should now have unique
  // animation indices.
  MOZ_ASSERT(mAnimationIndex != aOther.mAnimationIndex,
             "Animation indices should be unique");

  // 3. Finally, generic animations sort by their position in the global
  // animation array.
  return mAnimationIndex < aOther.mAnimationIndex;
}

void
Animation::ComposeStyle(RefPtr<AnimValuesStyleRule>& aStyleRule,
                        nsCSSPropertySet& aSetProperties)
{
  if (!mEffect) {
    return;
  }

  if (!IsInEffect()) {
    return;
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
  AnimationPlayState playState = PlayState();
  {
    AutoRestore<Nullable<TimeDuration>> restoreHoldTime(mHoldTime);

    if (playState == AnimationPlayState::Pending &&
        mHoldTime.IsNull() &&
        !mStartTime.IsNull()) {
      Nullable<TimeDuration> timeToUse = mPendingReadyTime;
      if (timeToUse.IsNull() &&
          mTimeline &&
          mTimeline->TracksWallclockTime()) {
        timeToUse = mTimeline->ToTimelineTime(TimeStamp::Now());
      }
      if (!timeToUse.IsNull()) {
        mHoldTime.SetValue((timeToUse.Value() - mStartTime.Value())
                            .MultDouble(mPlaybackRate));
      }
    }

    mEffect->ComposeStyle(aStyleRule, aSetProperties);
  }

  MOZ_ASSERT(playState == PlayState(),
             "Play state should not change during the course of compositing");
  mFinishedAtLastComposeStyle = (playState == AnimationPlayState::Finished);
}

void
Animation::NotifyEffectTimingUpdated()
{
  MOZ_ASSERT(mEffect,
             "We should only update timing effect when we have a target "
             "effect");
  UpdateTiming(Animation::SeekFlag::NoSeek,
               Animation::SyncNotifyFlag::Async);
}

// https://w3c.github.io/web-animations/#play-an-animation
void
Animation::DoPlay(ErrorResult& aRv, LimitBehavior aLimitBehavior)
{
  AutoMutationBatchForAnimation mb(*this);

  bool abortedPause = mPendingState == PendingState::PausePending;

  Nullable<TimeDuration> currentTime = GetCurrentTime();
  if (mPlaybackRate > 0.0 &&
      (currentTime.IsNull() ||
       (aLimitBehavior == LimitBehavior::AutoRewind &&
        (currentTime.Value().ToMilliseconds() < 0.0 ||
         currentTime.Value() >= EffectEnd())))) {
    mHoldTime.SetValue(TimeDuration(0));
  } else if (mPlaybackRate < 0.0 &&
             (currentTime.IsNull() ||
              (aLimitBehavior == LimitBehavior::AutoRewind &&
               (currentTime.Value().ToMilliseconds() <= 0.0 ||
                currentTime.Value() > EffectEnd())))) {
    if (EffectEnd() == TimeDuration::Forever()) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }
    mHoldTime.SetValue(TimeDuration(EffectEnd()));
  } else if (mPlaybackRate == 0.0 && currentTime.IsNull()) {
    mHoldTime.SetValue(TimeDuration(0));
  }

  bool reuseReadyPromise = false;
  if (mPendingState != PendingState::NotPending) {
    CancelPendingTasks();
    reuseReadyPromise = true;
  }

  // If the hold time is null then we're either already playing normally (and
  // we can ignore this call) or we aborted a pending pause operation (in which
  // case, for consistency, we need to go through the motions of doing an
  // asynchronous start even though we already have a resolved start time).
  if (mHoldTime.IsNull() && !abortedPause) {
    return;
  }

  // Clear the start time until we resolve a new one. We do this except
  // for the case where we are aborting a pause and don't have a hold time.
  //
  // If we're aborting a pause and *do* have a hold time (e.g. because
  // the animation is finished or we just applied the auto-rewind behavior
  // above) we should respect it by clearing the start time. If we *don't*
  // have a hold time we should keep the current start time so that the
  // the animation continues moving uninterrupted by the aborted pause.
  //
  // (If we're not aborting a pause, mHoldTime must be resolved by now
  //  or else we would have returned above.)
  if (!mHoldTime.IsNull()) {
    mStartTime.SetNull();
  }

  if (!reuseReadyPromise) {
    // Clear ready promise. We'll create a new one lazily.
    mReady = nullptr;
  }

  mPendingState = PendingState::PlayPending;

  nsIDocument* doc = GetRenderedDocument();
  if (doc) {
    PendingAnimationTracker* tracker =
      doc->GetOrCreatePendingAnimationTracker();
    tracker->AddPlayPending(*this);
  } else {
    TriggerOnNextTick(Nullable<TimeDuration>());
  }

  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Async);
  if (IsRelevant()) {
    nsNodeUtils::AnimationChanged(this);
  }
}

// https://w3c.github.io/web-animations/#pause-an-animation
void
Animation::DoPause(ErrorResult& aRv)
{
  if (IsPausedOrPausing()) {
    return;
  }

  AutoMutationBatchForAnimation mb(*this);

  // If we are transitioning from idle, fill in the current time
  if (GetCurrentTime().IsNull()) {
    if (mPlaybackRate >= 0.0) {
      mHoldTime.SetValue(TimeDuration(0));
    } else {
      if (EffectEnd() == TimeDuration::Forever()) {
        aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
        return;
      }
      mHoldTime.SetValue(TimeDuration(EffectEnd()));
    }
  }

  bool reuseReadyPromise = false;
  if (mPendingState == PendingState::PlayPending) {
    CancelPendingTasks();
    reuseReadyPromise = true;
  }

  if (!reuseReadyPromise) {
    // Clear ready promise. We'll create a new one lazily.
    mReady = nullptr;
  }

  mPendingState = PendingState::PausePending;

  nsIDocument* doc = GetRenderedDocument();
  if (doc) {
    PendingAnimationTracker* tracker =
      doc->GetOrCreatePendingAnimationTracker();
    tracker->AddPausePending(*this);
  } else {
    TriggerOnNextTick(Nullable<TimeDuration>());
  }

  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Async);
  if (IsRelevant()) {
    nsNodeUtils::AnimationChanged(this);
  }
}

void
Animation::ResumeAt(const TimeDuration& aReadyTime)
{
  // This method is only expected to be called for an animation that is
  // waiting to play. We can easily adapt it to handle other states
  // but it's currently not necessary.
  MOZ_ASSERT(mPendingState == PendingState::PlayPending,
             "Expected to resume a play-pending animation");
  MOZ_ASSERT(mHoldTime.IsNull() != mStartTime.IsNull(),
             "An animation in the play-pending state should have either a"
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

  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Async);

  if (mReady) {
    mReady->MaybeResolve(this);
  }
}

void
Animation::PauseAt(const TimeDuration& aReadyTime)
{
  MOZ_ASSERT(mPendingState == PendingState::PausePending,
             "Expected to pause a pause-pending animation");

  if (!mStartTime.IsNull() && mHoldTime.IsNull()) {
    mHoldTime.SetValue((aReadyTime - mStartTime.Value())
                        .MultDouble(mPlaybackRate));
  }
  mStartTime.SetNull();
  mPendingState = PendingState::NotPending;

  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Async);

  if (mReady) {
    mReady->MaybeResolve(this);
  }
}

void
Animation::UpdateTiming(SeekFlag aSeekFlag, SyncNotifyFlag aSyncNotifyFlag)
{
  // We call UpdateFinishedState before UpdateEffect because the former
  // can change the current time, which is used by the latter.
  UpdateFinishedState(aSeekFlag, aSyncNotifyFlag);
  UpdateEffect();

  if (mTimeline) {
    mTimeline->NotifyAnimationUpdated(*this);
  }
}

// https://w3c.github.io/web-animations/#update-an-animations-finished-state
void
Animation::UpdateFinishedState(SeekFlag aSeekFlag,
                               SyncNotifyFlag aSyncNotifyFlag)
{
  Nullable<TimeDuration> currentTime = GetCurrentTime();
  TimeDuration effectEnd = TimeDuration(EffectEnd());

  if (!mStartTime.IsNull() &&
      mPendingState == PendingState::NotPending) {
    if (mPlaybackRate > 0.0 &&
        !currentTime.IsNull() &&
        currentTime.Value() >= effectEnd) {
      if (aSeekFlag == SeekFlag::DidSeek) {
        mHoldTime = currentTime;
      } else if (!mPreviousCurrentTime.IsNull()) {
        mHoldTime.SetValue(std::max(mPreviousCurrentTime.Value(), effectEnd));
      } else {
        mHoldTime.SetValue(effectEnd);
      }
    } else if (mPlaybackRate < 0.0 &&
               !currentTime.IsNull() &&
               currentTime.Value().ToMilliseconds() <= 0.0) {
      if (aSeekFlag == SeekFlag::DidSeek) {
        mHoldTime = currentTime;
      } else {
        mHoldTime.SetValue(0);
      }
    } else if (mPlaybackRate != 0.0 &&
               !currentTime.IsNull() &&
               mTimeline &&
               !mTimeline->GetCurrentTime().IsNull()) {
      if (aSeekFlag == SeekFlag::DidSeek && !mHoldTime.IsNull()) {
        mStartTime.SetValue(mTimeline->GetCurrentTime().Value() -
                             (mHoldTime.Value().MultDouble(1 / mPlaybackRate)));
      }
      mHoldTime.SetNull();
    }
  }

  bool currentFinishedState = PlayState() == AnimationPlayState::Finished;
  if (currentFinishedState && !mFinishedIsResolved) {
    DoFinishNotification(aSyncNotifyFlag);
  } else if (!currentFinishedState && mFinishedIsResolved) {
    ResetFinishedPromise();
  }
  // We must recalculate the current time to take account of any mHoldTime
  // changes the code above made.
  mPreviousCurrentTime = GetCurrentTime();
}

void
Animation::UpdateEffect()
{
  if (mEffect) {
    UpdateRelevance();
    mEffect->NotifyAnimationTimingUpdated();
  }
}

void
Animation::FlushStyle() const
{
  nsIDocument* doc = GetRenderedDocument();
  if (doc) {
    doc->FlushPendingNotifications(Flush_Style);
  }
}

void
Animation::PostUpdate()
{
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return;
  }

  Maybe<NonOwningAnimationTarget> target = mEffect->GetTarget();
  if (!target) {
    return;
  }

  presContext->EffectCompositor()
             ->RequestRestyle(target->mElement,
                              target->mPseudoType,
                              EffectCompositor::RestyleType::Layer,
                              CascadeLevel());
}

void
Animation::CancelPendingTasks()
{
  if (mPendingState == PendingState::NotPending) {
    return;
  }

  nsIDocument* doc = GetRenderedDocument();
  if (doc) {
    PendingAnimationTracker* tracker = doc->GetPendingAnimationTracker();
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
Animation::IsPossiblyOrphanedPendingAnimation() const
{
  // Check if we are pending but might never start because we are not being
  // tracked.
  //
  // This covers the following cases:
  //
  // * We started playing but our effect's target element was orphaned
  //   or bound to a different document.
  //   (note that for the case of our effect changing we should handle
  //   that in SetEffect)
  // * We started playing but our timeline became inactive.
  //   In this case the pending animation tracker will drop us from its hashmap
  //   when we have been painted.
  // * When we started playing we couldn't find a PendingAnimationTracker to
  //   register with (perhaps the effect had no document) so we simply
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
  // PendingAnimationTracker then there's a good chance no one is tracking us.
  //
  // If we're wrong and another document is tracking us then, at worst, we'll
  // simply start/pause the animation one tick too soon. That's better than
  // never starting/pausing the animation and is unlikely.
  nsIDocument* doc = GetRenderedDocument();
  if (!doc) {
    return true;
  }

  PendingAnimationTracker* tracker = doc->GetPendingAnimationTracker();
  return !tracker ||
         (!tracker->IsWaitingToPlay(*this) &&
          !tracker->IsWaitingToPause(*this));
}

StickyTimeDuration
Animation::EffectEnd() const
{
  if (!mEffect) {
    return StickyTimeDuration(0);
  }

  return mEffect->SpecifiedTiming().EndTime();
}

nsIDocument*
Animation::GetRenderedDocument() const
{
  if (!mEffect) {
    return nullptr;
  }

  return mEffect->GetRenderedDocument();
}

nsPresContext*
Animation::GetPresContext() const
{
  if (!mEffect) {
    return nullptr;
  }

  return mEffect->GetPresContext();
}

void
Animation::DoFinishNotification(SyncNotifyFlag aSyncNotifyFlag)
{
  CycleCollectedJSRuntime* runtime = CycleCollectedJSRuntime::Get();

  if (aSyncNotifyFlag == SyncNotifyFlag::Sync) {
    DoFinishNotificationImmediately();
  } else if (!mFinishNotificationTask.IsPending()) {
    RefPtr<nsRunnableMethod<Animation>> runnable =
      NewRunnableMethod(this, &Animation::DoFinishNotificationImmediately);
    runtime->DispatchToMicroTask(runnable);
    mFinishNotificationTask = runnable;
  }
}

void
Animation::ResetFinishedPromise()
{
  mFinishedIsResolved = false;
  mFinished = nullptr;
}

void
Animation::MaybeResolveFinishedPromise()
{
  if (mFinished) {
    mFinished->MaybeResolve(this);
  }
  mFinishedIsResolved = true;
}

void
Animation::DoFinishNotificationImmediately()
{
  mFinishNotificationTask.Revoke();

  if (PlayState() != AnimationPlayState::Finished) {
    return;
  }

  MaybeResolveFinishedPromise();

  DispatchPlaybackEvent(NS_LITERAL_STRING("finish"));
}

void
Animation::DispatchPlaybackEvent(const nsAString& aName)
{
  AnimationPlaybackEventInit init;

  if (aName.EqualsLiteral("finish")) {
    init.mCurrentTime = GetCurrentTimeAsDouble();
  }
  if (mTimeline) {
    init.mTimelineTime = mTimeline->GetCurrentTimeAsDouble();
  }

  RefPtr<AnimationPlaybackEvent> event =
    AnimationPlaybackEvent::Constructor(this, aName, init);
  event->SetTrusted(true);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, event);
  asyncDispatcher->PostDOMEvent();
}

bool
Animation::IsRunningOnCompositor() const
{
  return mEffect && mEffect->IsRunningOnCompositor();
}

} // namespace dom
} // namespace mozilla
