/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Animation.h"

#include "AnimationUtils.h"
#include "mozAutoDocUpdate.h"
#include "mozilla/dom/AnimationBinding.h"
#include "mozilla/dom/AnimationPlaybackEvent.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/DocumentTimeline.h"
#include "mozilla/dom/MutationObservers.h"
#include "mozilla/AnimationEventDispatcher.h"
#include "mozilla/AnimationTarget.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/Maybe.h"  // For Maybe
#include "mozilla/StaticPrefs_dom.h"
#include "nsAnimationManager.h"  // For CSSAnimation
#include "nsComputedDOMStyle.h"
#include "nsDOMMutationObserver.h"    // For nsAutoAnimationMutationBatch
#include "nsDOMCSSAttrDeclaration.h"  // For nsDOMCSSAttributeDeclaration
#include "nsThreadUtils.h"  // For nsRunnableMethod and nsRevocableEventPtr
#include "nsTransitionManager.h"      // For CSSTransition
#include "PendingAnimationTracker.h"  // For PendingAnimationTracker

namespace mozilla {
namespace dom {

// Static members
uint64_t Animation::sNextAnimationIndex = 0;

NS_IMPL_CYCLE_COLLECTION_INHERITED(Animation, DOMEventTargetHelper, mTimeline,
                                   mEffect, mReady, mFinished)

NS_IMPL_ADDREF_INHERITED(Animation, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Animation, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Animation)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

JSObject* Animation::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return dom::Animation_Binding::Wrap(aCx, this, aGivenProto);
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
  explicit AutoMutationBatchForAnimation(
      const Animation& aAnimation MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    NonOwningAnimationTarget target = aAnimation.GetTargetForAnimation();
    if (!target) {
      return;
    }

    // For mutation observers, we use the OwnerDoc.
    mAutoBatch.emplace(target.mElement->OwnerDoc());
  }

 private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  Maybe<nsAutoAnimationMutationBatch> mAutoBatch;
};
}  // namespace

// ---------------------------------------------------------------------------
//
// Animation interface:
//
// ---------------------------------------------------------------------------

NonOwningAnimationTarget Animation::GetTargetForAnimation() const {
  AnimationEffect* effect = GetEffect();
  NonOwningAnimationTarget target;
  if (!effect || !effect->AsKeyframeEffect()) {
    return target;
  }
  return effect->AsKeyframeEffect()->GetAnimationTarget();
}

/* static */
already_AddRefed<Animation> Animation::Constructor(
    const GlobalObject& aGlobal, AnimationEffect* aEffect,
    const Optional<AnimationTimeline*>& aTimeline, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<Animation> animation = new Animation(global);

  AnimationTimeline* timeline;
  if (aTimeline.WasPassed()) {
    timeline = aTimeline.Value();
  } else {
    Document* document =
        AnimationUtils::GetCurrentRealmDocument(aGlobal.Context());
    if (!document) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
    timeline = document->Timeline();
  }

  animation->SetTimelineNoUpdate(timeline);
  animation->SetEffectNoUpdate(aEffect);

  return animation.forget();
}

void Animation::SetId(const nsAString& aId) {
  if (mId == aId) {
    return;
  }
  mId = aId;
  MutationObservers::NotifyAnimationChanged(this);
}

void Animation::SetEffect(AnimationEffect* aEffect) {
  SetEffectNoUpdate(aEffect);
  PostUpdate();
}

// https://drafts.csswg.org/web-animations/#setting-the-target-effect
void Animation::SetEffectNoUpdate(AnimationEffect* aEffect) {
  RefPtr<Animation> kungFuDeathGrip(this);

  if (mEffect == aEffect) {
    return;
  }

  AutoMutationBatchForAnimation mb(*this);
  bool wasRelevant = mIsRelevant;

  if (mEffect) {
    // We need to notify observers now because once we set mEffect to null
    // we won't be able to find the target element to notify.
    if (mIsRelevant) {
      MutationObservers::NotifyAnimationRemoved(this);
    }

    // Break links with the old effect and then drop it.
    RefPtr<AnimationEffect> oldEffect = mEffect;
    mEffect = nullptr;
    oldEffect->SetAnimation(nullptr);

    // The following will not do any notification because mEffect is null.
    UpdateRelevance();
  }

  if (aEffect) {
    // Break links from the new effect to its previous animation, if any.
    RefPtr<AnimationEffect> newEffect = aEffect;
    Animation* prevAnim = aEffect->GetAnimation();
    if (prevAnim) {
      prevAnim->SetEffect(nullptr);
    }

    // Create links with the new effect. SetAnimation(this) will also update
    // mIsRelevant of this animation, and then notify mutation observer if
    // needed by calling Animation::UpdateRelevance(), so we don't need to
    // call it again.
    mEffect = newEffect;
    mEffect->SetAnimation(this);

    // Notify possible add or change.
    // If the target is different, the change notification will be ignored by
    // AutoMutationBatchForAnimation.
    if (wasRelevant && mIsRelevant) {
      MutationObservers::NotifyAnimationChanged(this);
    }

    ReschedulePendingTasks();
  }

  MaybeScheduleReplacementCheck();

  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Async);
}

void Animation::SetTimeline(AnimationTimeline* aTimeline) {
#ifndef NIGHTLY_BUILD
  MOZ_ASSERT_UNREACHABLE(
      "Animation.timeline setter is supported only on nightly");
#endif
  SetTimelineNoUpdate(aTimeline);
  PostUpdate();
}

// https://drafts.csswg.org/web-animations/#setting-the-timeline
void Animation::SetTimelineNoUpdate(AnimationTimeline* aTimeline) {
  if (mTimeline == aTimeline) {
    return;
  }

  StickyTimeDuration activeTime =
      mEffect ? mEffect->GetComputedTiming().mActiveTime : StickyTimeDuration();

  RefPtr<AnimationTimeline> oldTimeline = mTimeline;
  if (oldTimeline) {
    oldTimeline->RemoveAnimation(this);
  }

  mTimeline = aTimeline;
  if (!mStartTime.IsNull()) {
    mHoldTime.SetNull();
  }

  if (!aTimeline) {
    MaybeQueueCancelEvent(activeTime);
  }
  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Async);
}

// https://drafts.csswg.org/web-animations/#set-the-animation-start-time
void Animation::SetStartTime(const Nullable<TimeDuration>& aNewStartTime) {
  // Return early if the start time will not change. However, if we
  // are pending, then setting the start time to any value
  // including the current value has the effect of aborting
  // pending tasks so we should not return early in that case.
  if (!Pending() && aNewStartTime == mStartTime) {
    return;
  }

  AutoMutationBatchForAnimation mb(*this);

  Nullable<TimeDuration> timelineTime;
  if (mTimeline) {
    // The spec says to check if the timeline is active (has a resolved time)
    // before using it here, but we don't need to since it's harmless to set
    // the already null time to null.
    timelineTime = mTimeline->GetCurrentTimeAsDuration();
  }
  if (timelineTime.IsNull() && !aNewStartTime.IsNull()) {
    mHoldTime.SetNull();
  }

  Nullable<TimeDuration> previousCurrentTime = GetCurrentTimeAsDuration();

  ApplyPendingPlaybackRate();
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
    MutationObservers::NotifyAnimationChanged(this);
  }
  PostUpdate();
}

// https://drafts.csswg.org/web-animations/#current-time
Nullable<TimeDuration> Animation::GetCurrentTimeForHoldTime(
    const Nullable<TimeDuration>& aHoldTime) const {
  Nullable<TimeDuration> result;
  if (!aHoldTime.IsNull()) {
    result = aHoldTime;
    return result;
  }

  if (mTimeline && !mStartTime.IsNull()) {
    Nullable<TimeDuration> timelineTime = mTimeline->GetCurrentTimeAsDuration();
    if (!timelineTime.IsNull()) {
      result = CurrentTimeFromTimelineTime(timelineTime.Value(),
                                           mStartTime.Value(), mPlaybackRate);
    }
  }
  return result;
}

// https://drafts.csswg.org/web-animations/#set-the-current-time
void Animation::SetCurrentTime(const TimeDuration& aSeekTime) {
  // Return early if the current time has not changed. However, if we
  // are pause-pending, then setting the current time to any value
  // including the current value has the effect of aborting the
  // pause so we should not return early in that case.
  if (mPendingState != PendingState::PausePending &&
      Nullable<TimeDuration>(aSeekTime) == GetCurrentTimeAsDuration()) {
    return;
  }

  AutoMutationBatchForAnimation mb(*this);

  SilentlySetCurrentTime(aSeekTime);

  if (mPendingState == PendingState::PausePending) {
    // Finish the pause operation
    mHoldTime.SetValue(aSeekTime);

    ApplyPendingPlaybackRate();
    mStartTime.SetNull();

    if (mReady) {
      mReady->MaybeResolve(this);
    }
    CancelPendingTasks();
  }

  UpdateTiming(SeekFlag::DidSeek, SyncNotifyFlag::Async);
  if (IsRelevant()) {
    MutationObservers::NotifyAnimationChanged(this);
  }
  PostUpdate();
}

// https://drafts.csswg.org/web-animations/#set-the-playback-rate
void Animation::SetPlaybackRate(double aPlaybackRate) {
  mPendingPlaybackRate.reset();

  if (aPlaybackRate == mPlaybackRate) {
    return;
  }

  AutoMutationBatchForAnimation mb(*this);

  Nullable<TimeDuration> previousTime = GetCurrentTimeAsDuration();
  mPlaybackRate = aPlaybackRate;
  if (!previousTime.IsNull()) {
    SetCurrentTime(previousTime.Value());
  }

  // In the case where GetCurrentTimeAsDuration() returns the same result before
  // and after updating mPlaybackRate, SetCurrentTime will return early since,
  // as far as it can tell, nothing has changed.
  // As a result, we need to perform the following updates here:
  // - update timing (since, if the sign of the playback rate has changed, our
  //   finished state may have changed),
  // - dispatch a change notification for the changed playback rate, and
  // - update the playback rate on animations on layers.
  UpdateTiming(SeekFlag::DidSeek, SyncNotifyFlag::Async);
  if (IsRelevant()) {
    MutationObservers::NotifyAnimationChanged(this);
  }
  PostUpdate();
}

// https://drafts.csswg.org/web-animations/#seamlessly-update-the-playback-rate
void Animation::UpdatePlaybackRate(double aPlaybackRate) {
  if (mPendingPlaybackRate && mPendingPlaybackRate.value() == aPlaybackRate) {
    return;
  }

  // Calculate the play state using the existing playback rate since below we
  // want to know if the animation is _currently_ finished or not, not whether
  // it _will_ be finished.
  AnimationPlayState playState = PlayState();

  mPendingPlaybackRate = Some(aPlaybackRate);

  if (Pending()) {
    // If we already have a pending task, there is nothing more to do since the
    // playback rate will be applied then.
    //
    // However, as with the idle/paused case below, we still need to update the
    // relevance (and effect set to make sure it only contains relevant
    // animations) since the relevance is based on the Animation play state
    // which incorporates the _pending_ playback rate.
    UpdateEffect(PostRestyleMode::Never);
    return;
  }

  AutoMutationBatchForAnimation mb(*this);

  if (playState == AnimationPlayState::Idle ||
      playState == AnimationPlayState::Paused) {
    // We are either idle or paused. In either case we can apply the pending
    // playback rate immediately.
    ApplyPendingPlaybackRate();

    // We don't need to update timing or post an update here because:
    //
    // * the current time hasn't changed -- it's either unresolved or fixed
    //   with a hold time -- so the output won't have changed
    // * the finished state won't have changed even if the sign of the
    //   playback rate changed since we're not finished (we're paused or idle)
    // * the playback rate on layers doesn't need to be updated since we're not
    //   moving. Once we get a start time etc. we'll update the playback rate
    //   then.
    //
    // However we still need to update the relevance and effect set as well as
    // notifying observers.
    UpdateEffect(PostRestyleMode::Never);
    if (IsRelevant()) {
      MutationObservers::NotifyAnimationChanged(this);
    }
  } else if (playState == AnimationPlayState::Finished) {
    MOZ_ASSERT(mTimeline && !mTimeline->GetCurrentTimeAsDuration().IsNull(),
               "If we have no active timeline, we should be idle or paused");
    if (aPlaybackRate != 0) {
      // The unconstrained current time can only be unresolved if either we
      // don't have an active timeline (and we already asserted that is not
      // true) or we have an unresolved start time (in which case we should be
      // paused).
      MOZ_ASSERT(!GetUnconstrainedCurrentTime().IsNull(),
                 "Unconstrained current time should be resolved");
      TimeDuration unconstrainedCurrentTime =
          GetUnconstrainedCurrentTime().Value();
      TimeDuration timelineTime = mTimeline->GetCurrentTimeAsDuration().Value();
      mStartTime = StartTimeFromTimelineTime(
          timelineTime, unconstrainedCurrentTime, aPlaybackRate);
    } else {
      mStartTime = mTimeline->GetCurrentTimeAsDuration();
    }

    ApplyPendingPlaybackRate();

    // Even though we preserve the current time, we might now leave the finished
    // state (e.g. if the playback rate changes sign) so we need to update
    // timing.
    UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Async);
    if (IsRelevant()) {
      MutationObservers::NotifyAnimationChanged(this);
    }
    PostUpdate();
  } else {
    ErrorResult rv;
    Play(rv, LimitBehavior::Continue);
    MOZ_ASSERT(!rv.Failed(),
               "We should only fail to play when using auto-rewind behavior");
  }
}

// https://drafts.csswg.org/web-animations/#play-state
AnimationPlayState Animation::PlayState() const {
  Nullable<TimeDuration> currentTime = GetCurrentTimeAsDuration();
  if (currentTime.IsNull() && !Pending()) {
    return AnimationPlayState::Idle;
  }

  if (mPendingState == PendingState::PausePending ||
      (mStartTime.IsNull() && !Pending())) {
    return AnimationPlayState::Paused;
  }

  double playbackRate = CurrentOrPendingPlaybackRate();
  if (!currentTime.IsNull() &&
      ((playbackRate > 0.0 && currentTime.Value() >= EffectEnd()) ||
       (playbackRate < 0.0 && currentTime.Value() <= TimeDuration()))) {
    return AnimationPlayState::Finished;
  }

  return AnimationPlayState::Running;
}

Promise* Animation::GetReady(ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal();
  if (!mReady && global) {
    mReady = Promise::Create(global, aRv);  // Lazily create on demand
  }
  if (!mReady) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  if (!Pending()) {
    mReady->MaybeResolve(this);
  }
  return mReady;
}

Promise* Animation::GetFinished(ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal();
  if (!mFinished && global) {
    mFinished = Promise::Create(global, aRv);  // Lazily create on demand
  }
  if (!mFinished) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  if (mFinishedIsResolved) {
    MaybeResolveFinishedPromise();
  }
  return mFinished;
}

// https://drafts.csswg.org/web-animations/#cancel-an-animation
void Animation::Cancel(PostRestyleMode aPostRestyle) {
  bool newlyIdle = false;

  if (PlayState() != AnimationPlayState::Idle) {
    newlyIdle = true;

    ResetPendingTasks();

    if (mFinished) {
      mFinished->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      mFinished->SetSettledPromiseIsHandled();
    }
    ResetFinishedPromise();

    QueuePlaybackEvent(NS_LITERAL_STRING("cancel"),
                       GetTimelineCurrentTimeAsTimeStamp());
  }

  StickyTimeDuration activeTime =
      mEffect ? mEffect->GetComputedTiming().mActiveTime : StickyTimeDuration();

  mHoldTime.SetNull();
  mStartTime.SetNull();

  // Allow our effect to remove itself from the its target element's EffectSet.
  UpdateEffect(aPostRestyle);

  if (mTimeline) {
    mTimeline->RemoveAnimation(this);
  }
  MaybeQueueCancelEvent(activeTime);

  if (newlyIdle && aPostRestyle == PostRestyleMode::IfNeeded) {
    PostUpdate();
  }
}

// https://drafts.csswg.org/web-animations/#finish-an-animation
void Animation::Finish(ErrorResult& aRv) {
  double effectivePlaybackRate = CurrentOrPendingPlaybackRate();

  if (effectivePlaybackRate == 0) {
    return aRv.ThrowInvalidStateError(
        "Can't finish animation with zero playback rate");
  }
  if (effectivePlaybackRate > 0 && EffectEnd() == TimeDuration::Forever()) {
    return aRv.ThrowInvalidStateError("Can't finish infinite animation");
  }

  AutoMutationBatchForAnimation mb(*this);

  ApplyPendingPlaybackRate();

  // Seek to the end
  TimeDuration limit =
      mPlaybackRate > 0 ? TimeDuration(EffectEnd()) : TimeDuration(0);
  bool didChange = GetCurrentTimeAsDuration() != Nullable<TimeDuration>(limit);
  SilentlySetCurrentTime(limit);

  // If we are paused or play-pending we need to fill in the start time in
  // order to transition to the finished state.
  //
  // We only do this, however, if we have an active timeline. If we have an
  // inactive timeline we can't transition into the finished state just like
  // we can't transition to the running state (this finished state is really
  // a substate of the running state).
  if (mStartTime.IsNull() && mTimeline &&
      !mTimeline->GetCurrentTimeAsDuration().IsNull()) {
    mStartTime = StartTimeFromTimelineTime(
        mTimeline->GetCurrentTimeAsDuration().Value(), limit, mPlaybackRate);
    didChange = true;
  }

  // If we just resolved the start time for a pause or play-pending
  // animation, we need to clear the task. We don't do this as a branch of
  // the above however since we can have a play-pending animation with a
  // resolved start time if we aborted a pause operation.
  if (!mStartTime.IsNull() && (mPendingState == PendingState::PlayPending ||
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
    MutationObservers::NotifyAnimationChanged(this);
  }
  PostUpdate();
}

void Animation::Play(ErrorResult& aRv, LimitBehavior aLimitBehavior) {
  PlayNoUpdate(aRv, aLimitBehavior);
  PostUpdate();
}

// https://drafts.csswg.org/web-animations/#reverse-an-animation
void Animation::Reverse(ErrorResult& aRv) {
  if (!mTimeline) {
    return aRv.ThrowInvalidStateError(
        "Can't reverse an animation with no associated timeline");
  }
  if (mTimeline->GetCurrentTimeAsDuration().IsNull()) {
    return aRv.ThrowInvalidStateError(
        "Can't reverse an animation associated with an inactive timeline");
  }

  double effectivePlaybackRate = CurrentOrPendingPlaybackRate();

  if (effectivePlaybackRate == 0.0) {
    return;
  }

  Maybe<double> originalPendingPlaybackRate = mPendingPlaybackRate;

  mPendingPlaybackRate = Some(-effectivePlaybackRate);

  Play(aRv, LimitBehavior::AutoRewind);

  // If Play() threw, restore state and don't report anything to mutation
  // observers.
  if (aRv.Failed()) {
    mPendingPlaybackRate = originalPendingPlaybackRate;
  }

  // Play(), above, unconditionally calls PostUpdate so we don't need to do
  // it here.
}

void Animation::Persist() {
  if (mReplaceState == AnimationReplaceState::Persisted) {
    return;
  }

  bool wasRemoved = mReplaceState == AnimationReplaceState::Removed;

  mReplaceState = AnimationReplaceState::Persisted;

  // If the animation is not (yet) removed, there should be no side effects of
  // persisting it.
  if (wasRemoved) {
    UpdateEffect(PostRestyleMode::IfNeeded);
    PostUpdate();
  }
}

// https://drafts.csswg.org/web-animations/#dom-animation-commitstyles
void Animation::CommitStyles(ErrorResult& aRv) {
  if (!mEffect) {
    return;
  }

  // Take an owning reference to the keyframe effect. This will ensure that
  // this Animation and the target element remain alive after flushing style.
  RefPtr<KeyframeEffect> keyframeEffect = mEffect->AsKeyframeEffect();
  if (!keyframeEffect) {
    return;
  }

  NonOwningAnimationTarget target = keyframeEffect->GetAnimationTarget();
  if (!target) {
    return;
  }

  if (target.mPseudoType != PseudoStyleType::NotPseudo) {
    return aRv.ThrowNoModificationAllowedError(
        "Can't commit styles of a pseudo-element");
  }

  // Check it is an element with a style attribute
  nsCOMPtr<nsStyledElement> styledElement = do_QueryInterface(target.mElement);
  if (!styledElement) {
    return aRv.ThrowNoModificationAllowedError(
        "Target is not capable of having a style attribute");
  }

  // Flush frames before checking if the target element is rendered since the
  // result could depend on pending style changes, and IsRendered() looks at the
  // primary frame.
  if (Document* doc = target.mElement->GetComposedDoc()) {
    doc->FlushPendingNotifications(FlushType::Frames);
  }
  if (!target.mElement->IsRendered()) {
    return aRv.ThrowInvalidStateError("Target is not rendered");
  }
  nsPresContext* presContext =
      nsContentUtils::GetContextForContent(target.mElement);
  if (!presContext) {
    return aRv.ThrowInvalidStateError("Target is not rendered");
  }

  // Get the computed animation values
  UniquePtr<RawServoAnimationValueMap> animationValues =
      Servo_AnimationValueMap_Create().Consume();
  if (!presContext->EffectCompositor()->ComposeServoAnimationRuleForEffect(
          *keyframeEffect, CascadeLevel(), animationValues.get())) {
    NS_WARNING("Failed to compose animation style to commit");
    return;
  }

  // Calling SetCSSDeclaration will trigger attribute setting code.
  // Start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule.
  mozAutoDocUpdate autoUpdate(target.mElement->OwnerDoc(), true);

  // Get the inline style to append to
  RefPtr<DeclarationBlock> declarationBlock;
  if (auto* existing = target.mElement->GetInlineStyleDeclaration()) {
    declarationBlock = existing->EnsureMutable();
  } else {
    declarationBlock = new DeclarationBlock();
    declarationBlock->SetDirty();
  }

  // Prepare the callback
  MutationClosureData closureData;
  closureData.mClosure = nsDOMCSSAttributeDeclaration::MutationClosureFunction;
  closureData.mElement = target.mElement;
  DeclarationBlockMutationClosure beforeChangeClosure = {
      nsDOMCSSAttributeDeclaration::MutationClosureFunction,
      &closureData,
  };

  // Set the animated styles
  bool changed = false;
  nsCSSPropertyIDSet properties = keyframeEffect->GetPropertySet();
  for (nsCSSPropertyID property : properties) {
    RefPtr<RawServoAnimationValue> computedValue =
        Servo_AnimationValueMap_GetValue(animationValues.get(), property)
            .Consume();
    if (computedValue) {
      changed |= Servo_DeclarationBlock_SetPropertyToAnimationValue(
          declarationBlock->Raw(), computedValue, beforeChangeClosure);
    }
  }

  if (!changed) {
    return;
  }

  // Update inline style declaration
  target.mElement->SetInlineStyleDeclaration(*declarationBlock, closureData);
}

// ---------------------------------------------------------------------------
//
// JS wrappers for Animation interface:
//
// ---------------------------------------------------------------------------

Nullable<double> Animation::GetStartTimeAsDouble() const {
  return AnimationUtils::TimeDurationToDouble(mStartTime);
}

void Animation::SetStartTimeAsDouble(const Nullable<double>& aStartTime) {
  return SetStartTime(AnimationUtils::DoubleToTimeDuration(aStartTime));
}

Nullable<double> Animation::GetCurrentTimeAsDouble() const {
  return AnimationUtils::TimeDurationToDouble(GetCurrentTimeAsDuration());
}

void Animation::SetCurrentTimeAsDouble(const Nullable<double>& aCurrentTime,
                                       ErrorResult& aRv) {
  if (aCurrentTime.IsNull()) {
    if (!GetCurrentTimeAsDuration().IsNull()) {
      aRv.ThrowTypeError(
          "Current time is resolved but trying to set it to an unresolved "
          "time");
    }
    return;
  }

  return SetCurrentTime(TimeDuration::FromMilliseconds(aCurrentTime.Value()));
}

// ---------------------------------------------------------------------------

void Animation::Tick() {
  // Finish pending if we have a pending ready time, but only if we also
  // have an active timeline.
  if (mPendingState != PendingState::NotPending &&
      !mPendingReadyTime.IsNull() && mTimeline &&
      !mTimeline->GetCurrentTimeAsDuration().IsNull()) {
    // Even though mPendingReadyTime is initialized using TimeStamp::Now()
    // during the *previous* tick of the refresh driver, it can still be
    // ahead of the *current* timeline time when we are using the
    // vsync timer so we need to clamp it to the timeline time.
    TimeDuration currentTime = mTimeline->GetCurrentTimeAsDuration().Value();
    if (currentTime < mPendingReadyTime.Value()) {
      mPendingReadyTime.SetValue(currentTime);
    }
    FinishPendingAt(mPendingReadyTime.Value());
    mPendingReadyTime.SetNull();
  }

  if (IsPossiblyOrphanedPendingAnimation()) {
    MOZ_ASSERT(mTimeline && !mTimeline->GetCurrentTimeAsDuration().IsNull(),
               "Orphaned pending animations should have an active timeline");
    FinishPendingAt(mTimeline->GetCurrentTimeAsDuration().Value());
  }

  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Sync);

  // Check for changes to whether or not this animation is replaceable.
  bool isReplaceable = IsReplaceable();
  if (isReplaceable && !mWasReplaceableAtLastTick) {
    ScheduleReplacementCheck();
  }
  mWasReplaceableAtLastTick = isReplaceable;

  if (!mEffect) {
    return;
  }

  // Update layers if we are newly finished.
  KeyframeEffect* keyframeEffect = mEffect->AsKeyframeEffect();
  if (keyframeEffect && !keyframeEffect->Properties().IsEmpty() &&
      !mFinishedAtLastComposeStyle &&
      PlayState() == AnimationPlayState::Finished) {
    PostUpdate();
  }
}

void Animation::TriggerOnNextTick(const Nullable<TimeDuration>& aReadyTime) {
  // Normally we expect the play state to be pending but it's possible that,
  // due to the handling of possibly orphaned animations in Tick(), this
  // animation got started whilst still being in another document's pending
  // animation map.
  if (!Pending()) {
    return;
  }

  // If aReadyTime.IsNull() we'll detect this in Tick() where we check for
  // orphaned animations and trigger this animation anyway
  mPendingReadyTime = aReadyTime;
}

void Animation::TriggerNow() {
  // Normally we expect the play state to be pending but when an animation
  // is cancelled and its rendered document can't be reached, we can end up
  // with the animation still in a pending player tracker even after it is
  // no longer pending.
  if (!Pending()) {
    return;
  }

  // If we don't have an active timeline we can't trigger the animation.
  // However, this is a test-only method that we don't expect to be used in
  // conjunction with animations without an active timeline so generate
  // a warning if we do find ourselves in that situation.
  if (!mTimeline || mTimeline->GetCurrentTimeAsDuration().IsNull()) {
    NS_WARNING("Failed to trigger an animation with an active timeline");
    return;
  }

  FinishPendingAt(mTimeline->GetCurrentTimeAsDuration().Value());
}

Nullable<TimeDuration> Animation::GetCurrentOrPendingStartTime() const {
  Nullable<TimeDuration> result;

  // If we have a pending playback rate, work out what start time we will use
  // when we come to updating that playback rate.
  //
  // This logic roughly shadows that in ResumeAt but is just different enough
  // that it is difficult to extract out the common functionality (and
  // extracting that functionality out would make it harder to match ResumeAt up
  // against the spec).
  if (mPendingPlaybackRate && !mPendingReadyTime.IsNull() &&
      !mStartTime.IsNull()) {
    // If we have a hold time, use it as the current time to match.
    TimeDuration currentTimeToMatch =
        !mHoldTime.IsNull()
            ? mHoldTime.Value()
            : CurrentTimeFromTimelineTime(mPendingReadyTime.Value(),
                                          mStartTime.Value(), mPlaybackRate);

    result = StartTimeFromTimelineTime(
        mPendingReadyTime.Value(), currentTimeToMatch, *mPendingPlaybackRate);
    return result;
  }

  if (!mStartTime.IsNull()) {
    result = mStartTime;
    return result;
  }

  if (mPendingReadyTime.IsNull() || mHoldTime.IsNull()) {
    return result;
  }

  // Calculate the equivalent start time from the pending ready time.
  result = StartTimeFromTimelineTime(mPendingReadyTime.Value(),
                                     mHoldTime.Value(), mPlaybackRate);

  return result;
}

TimeStamp Animation::AnimationTimeToTimeStamp(
    const StickyTimeDuration& aTime) const {
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
  if (aTime == TimeDuration::Forever() || mPlaybackRate == 0.0 ||
      mStartTime.IsNull()) {
    return result;
  }

  // Invert the standard relation:
  //   current time = (timeline time - start time) * playback rate
  TimeDuration timelineTime =
      TimeDuration(aTime).MultDouble(1.0 / mPlaybackRate) + mStartTime.Value();

  result = mTimeline->ToTimeStamp(timelineTime);
  return result;
}

TimeStamp Animation::ElapsedTimeToTimeStamp(
    const StickyTimeDuration& aElapsedTime) const {
  TimeDuration delay =
      mEffect ? mEffect->SpecifiedTiming().Delay() : TimeDuration();
  return AnimationTimeToTimeStamp(aElapsedTime + delay);
}

// https://drafts.csswg.org/web-animations/#silently-set-the-current-time
void Animation::SilentlySetCurrentTime(const TimeDuration& aSeekTime) {
  if (!mHoldTime.IsNull() || mStartTime.IsNull() || !mTimeline ||
      mTimeline->GetCurrentTimeAsDuration().IsNull() || mPlaybackRate == 0.0) {
    mHoldTime.SetValue(aSeekTime);
    if (!mTimeline || mTimeline->GetCurrentTimeAsDuration().IsNull()) {
      mStartTime.SetNull();
    }
  } else {
    mStartTime =
        StartTimeFromTimelineTime(mTimeline->GetCurrentTimeAsDuration().Value(),
                                  aSeekTime, mPlaybackRate);
  }

  mPreviousCurrentTime.SetNull();
}

bool Animation::ShouldBeSynchronizedWithMainThread(
    const nsCSSPropertyIDSet& aPropertySet, const nsIFrame* aFrame,
    AnimationPerformanceWarning::Type& aPerformanceWarning) const {
  // Only synchronize playing animations
  if (!IsPlaying()) {
    return false;
  }

  // Currently only transform animations need to be synchronized
  if (!aPropertySet.Intersects(nsCSSPropertyIDSet::TransformLikeProperties())) {
    return false;
  }

  KeyframeEffect* keyframeEffect =
      mEffect ? mEffect->AsKeyframeEffect() : nullptr;
  if (!keyframeEffect) {
    return false;
  }

  // Are we starting at the same time as other geometric animations?
  // We check this before calling ShouldBlockAsyncTransformAnimations, partly
  // because it's cheaper, but also because it's often the most useful thing
  // to know when you're debugging performance.
  if (StaticPrefs::
          dom_animations_mainthread_synchronization_with_geometric_animations() &&
      mSyncWithGeometricAnimations &&
      keyframeEffect->HasAnimationOfPropertySet(
          nsCSSPropertyIDSet::TransformLikeProperties())) {
    aPerformanceWarning =
        AnimationPerformanceWarning::Type::TransformWithSyncGeometricAnimations;
    return true;
  }

  return keyframeEffect->ShouldBlockAsyncTransformAnimations(
      aFrame, aPropertySet, aPerformanceWarning);
}

void Animation::UpdateRelevance() {
  bool wasRelevant = mIsRelevant;
  mIsRelevant = mReplaceState != AnimationReplaceState::Removed &&
                (HasCurrentEffect() || IsInEffect());

  // Notify animation observers.
  if (wasRelevant && !mIsRelevant) {
    MutationObservers::NotifyAnimationRemoved(this);
  } else if (!wasRelevant && mIsRelevant) {
    MutationObservers::NotifyAnimationAdded(this);
  }
}

template <class T>
bool IsMarkupAnimation(T* aAnimation) {
  return aAnimation && aAnimation->IsTiedToMarkup();
}

// https://drafts.csswg.org/web-animations/#replaceable-animation
bool Animation::IsReplaceable() const {
  // We never replace CSS animations or CSS transitions since they are managed
  // by CSS.
  if (IsMarkupAnimation(AsCSSAnimation()) ||
      IsMarkupAnimation(AsCSSTransition())) {
    return false;
  }

  // Only finished animations can be replaced.
  if (PlayState() != AnimationPlayState::Finished) {
    return false;
  }

  // Already removed animations cannot be replaced.
  if (ReplaceState() == AnimationReplaceState::Removed) {
    return false;
  }

  // We can only replace an animation if we know that, uninterfered, it would
  // never start playing again. That excludes any animations on timelines that
  // aren't monotonically increasing.
  //
  // If we don't have any timeline at all, then we can't be in the finished
  // state (since we need both a resolved start time and current time for that)
  // and will have already returned false above.
  //
  // (However, if it ever does become possible to be finished without a timeline
  // then we will want to return false here since it probably suggests an
  // animation being driven directly by script, in which case we can't assume
  // anything about how they will behave.)
  if (!GetTimeline() || !GetTimeline()->TracksWallclockTime()) {
    return false;
  }

  // If the animation doesn't have an effect then we can't determine if it is
  // filling or not so just leave it alone.
  if (!GetEffect()) {
    return false;
  }

  // At the time of writing we only know about KeyframeEffects. If we introduce
  // other types of effects we will need to decide if they are replaceable or
  // not.
  MOZ_ASSERT(GetEffect()->AsKeyframeEffect(),
             "Effect should be a keyframe effect");

  // We only replace animations that are filling.
  if (GetEffect()->GetComputedTiming().mProgress.IsNull()) {
    return false;
  }

  // We should only replace animations with a target element (since otherwise
  // what other effects would we consider when determining if they are covered
  // or not?).
  if (!GetEffect()->AsKeyframeEffect()->GetAnimationTarget()) {
    return false;
  }

  return true;
}

bool Animation::IsRemovable() const {
  return ReplaceState() == AnimationReplaceState::Active && IsReplaceable();
}

void Animation::ScheduleReplacementCheck() {
  MOZ_ASSERT(
      IsReplaceable(),
      "Should only schedule a replacement check for a replaceable animation");

  // If IsReplaceable() is true, the following should also hold
  MOZ_ASSERT(GetEffect());
  MOZ_ASSERT(GetEffect()->AsKeyframeEffect());

  NonOwningAnimationTarget target =
      GetEffect()->AsKeyframeEffect()->GetAnimationTarget();

  MOZ_ASSERT(target);

  nsPresContext* presContext =
      nsContentUtils::GetContextForContent(target.mElement);
  if (presContext) {
    presContext->EffectCompositor()->NoteElementForReducing(target);
  }
}

void Animation::MaybeScheduleReplacementCheck() {
  if (!IsReplaceable()) {
    return;
  }

  ScheduleReplacementCheck();
}

void Animation::Remove() {
  MOZ_ASSERT(IsRemovable(),
             "Should not be trying to remove an effect that is not removable");

  mReplaceState = AnimationReplaceState::Removed;

  UpdateEffect(PostRestyleMode::IfNeeded);
  PostUpdate();

  QueuePlaybackEvent(NS_LITERAL_STRING("remove"),
                     GetTimelineCurrentTimeAsTimeStamp());
}

bool Animation::HasLowerCompositeOrderThan(const Animation& aOther) const {
  // 0. Object-equality case
  if (&aOther == this) {
    return false;
  }

  // 1. CSS Transitions sort lowest
  {
    auto asCSSTransitionForSorting =
        [](const Animation& anim) -> const CSSTransition* {
      const CSSTransition* transition = anim.AsCSSTransition();
      return transition && transition->IsTiedToMarkup() ? transition : nullptr;
    };
    auto thisTransition = asCSSTransitionForSorting(*this);
    auto otherTransition = asCSSTransitionForSorting(aOther);
    if (thisTransition && otherTransition) {
      return thisTransition->HasLowerCompositeOrderThan(*otherTransition);
    }
    if (thisTransition || otherTransition) {
      // Cancelled transitions no longer have an owning element. To be strictly
      // correct we should store a strong reference to the owning element
      // so that if we arrive here while sorting cancel events, we can sort
      // them in the correct order.
      //
      // However, given that cancel events are almost always queued
      // synchronously in some deterministic manner, we can be fairly sure
      // that cancel events will be dispatched in a deterministic order
      // (which is our only hard requirement until specs say otherwise).
      // Furthermore, we only reach here when we have events with equal
      // timestamps so this is an edge case we can probably ignore for now.
      return thisTransition;
    }
  }

  // 2. CSS Animations sort next
  {
    auto asCSSAnimationForSorting =
        [](const Animation& anim) -> const CSSAnimation* {
      const CSSAnimation* animation = anim.AsCSSAnimation();
      return animation && animation->IsTiedToMarkup() ? animation : nullptr;
    };
    auto thisAnimation = asCSSAnimationForSorting(*this);
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

void Animation::WillComposeStyle() {
  mFinishedAtLastComposeStyle = (PlayState() == AnimationPlayState::Finished);

  MOZ_ASSERT(mEffect);

  KeyframeEffect* keyframeEffect = mEffect->AsKeyframeEffect();
  if (keyframeEffect) {
    keyframeEffect->WillComposeStyle();
  }
}

void Animation::ComposeStyle(RawServoAnimationValueMap& aComposeResult,
                             const nsCSSPropertyIDSet& aPropertiesToSkip) {
  if (!mEffect) {
    return;
  }

  // In order to prevent flicker, there are a few cases where we want to use
  // a different time for rendering that would otherwise be returned by
  // GetCurrentTimeAsDuration. These are:
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
  bool pending = Pending();
  {
    AutoRestore<Nullable<TimeDuration>> restoreHoldTime(mHoldTime);

    if (pending && mHoldTime.IsNull() && !mStartTime.IsNull()) {
      Nullable<TimeDuration> timeToUse = mPendingReadyTime;
      if (timeToUse.IsNull() && mTimeline && mTimeline->TracksWallclockTime()) {
        timeToUse = mTimeline->ToTimelineTime(TimeStamp::Now());
      }
      if (!timeToUse.IsNull()) {
        mHoldTime = CurrentTimeFromTimelineTime(
            timeToUse.Value(), mStartTime.Value(), mPlaybackRate);
      }
    }

    KeyframeEffect* keyframeEffect = mEffect->AsKeyframeEffect();
    if (keyframeEffect) {
      keyframeEffect->ComposeStyle(aComposeResult, aPropertiesToSkip);
    }
  }

  MOZ_ASSERT(
      pending == Pending(),
      "Pending state should not change during the course of compositing");
}

void Animation::NotifyEffectTimingUpdated() {
  MOZ_ASSERT(mEffect,
             "We should only update effect timing when we have a target "
             "effect");
  UpdateTiming(Animation::SeekFlag::NoSeek, Animation::SyncNotifyFlag::Async);
}

void Animation::NotifyEffectPropertiesUpdated() {
  MOZ_ASSERT(mEffect,
             "We should only update effect properties when we have a target "
             "effect");

  MaybeScheduleReplacementCheck();
}

void Animation::NotifyEffectTargetUpdated() {
  MOZ_ASSERT(mEffect,
             "We should only update the effect target when we have a target "
             "effect");

  MaybeScheduleReplacementCheck();
}

void Animation::NotifyGeometricAnimationsStartingThisFrame() {
  if (!IsNewlyStarted() || !mEffect) {
    return;
  }

  mSyncWithGeometricAnimations = true;
}

// https://drafts.csswg.org/web-animations/#play-an-animation
void Animation::PlayNoUpdate(ErrorResult& aRv, LimitBehavior aLimitBehavior) {
  AutoMutationBatchForAnimation mb(*this);

  bool abortedPause = mPendingState == PendingState::PausePending;

  double effectivePlaybackRate = CurrentOrPendingPlaybackRate();

  Nullable<TimeDuration> currentTime = GetCurrentTimeAsDuration();
  if (effectivePlaybackRate > 0.0 &&
      (currentTime.IsNull() || (aLimitBehavior == LimitBehavior::AutoRewind &&
                                (currentTime.Value() < TimeDuration() ||
                                 currentTime.Value() >= EffectEnd())))) {
    mHoldTime.SetValue(TimeDuration(0));
  } else if (effectivePlaybackRate < 0.0 &&
             (currentTime.IsNull() ||
              (aLimitBehavior == LimitBehavior::AutoRewind &&
               (currentTime.Value() <= TimeDuration() ||
                currentTime.Value() > EffectEnd())))) {
    if (EffectEnd() == TimeDuration::Forever()) {
      return aRv.ThrowInvalidStateError(
          "Can't rewind animation with infinite effect end");
    }
    mHoldTime.SetValue(TimeDuration(EffectEnd()));
  } else if (effectivePlaybackRate == 0.0 && currentTime.IsNull()) {
    mHoldTime.SetValue(TimeDuration(0));
  }

  bool reuseReadyPromise = false;
  if (mPendingState != PendingState::NotPending) {
    CancelPendingTasks();
    reuseReadyPromise = true;
  }

  // If the hold time is null then we're already playing normally and,
  // typically, we can bail out here.
  //
  // However, there are two cases where we can't do that:
  //
  // (a) If we just aborted a pause. In this case, for consistency, we need to
  //     go through the motions of doing an asynchronous start.
  //
  // (b) If we have timing changes (specifically a change to the playbackRate)
  //     that should be applied asynchronously.
  //
  if (mHoldTime.IsNull() && !abortedPause && !mPendingPlaybackRate) {
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

  // Clear flag that causes us to sync transform animations with the main
  // thread for now. We'll set this when we go to set up compositor
  // animations if it applies.
  mSyncWithGeometricAnimations = false;

  if (Document* doc = GetRenderedDocument()) {
    PendingAnimationTracker* tracker =
        doc->GetOrCreatePendingAnimationTracker();
    tracker->AddPlayPending(*this);
  } else {
    TriggerOnNextTick(Nullable<TimeDuration>());
  }

  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Async);
  if (IsRelevant()) {
    MutationObservers::NotifyAnimationChanged(this);
  }
}

// https://drafts.csswg.org/web-animations/#pause-an-animation
void Animation::Pause(ErrorResult& aRv) {
  if (IsPausedOrPausing()) {
    return;
  }

  AutoMutationBatchForAnimation mb(*this);

  // If we are transitioning from idle, fill in the current time
  if (GetCurrentTimeAsDuration().IsNull()) {
    if (mPlaybackRate >= 0.0) {
      mHoldTime.SetValue(TimeDuration(0));
    } else {
      if (EffectEnd() == TimeDuration::Forever()) {
        return aRv.ThrowInvalidStateError("Can't seek to infinite effect end");
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

  if (Document* doc = GetRenderedDocument()) {
    PendingAnimationTracker* tracker =
        doc->GetOrCreatePendingAnimationTracker();
    tracker->AddPausePending(*this);
  } else {
    TriggerOnNextTick(Nullable<TimeDuration>());
  }

  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Async);
  if (IsRelevant()) {
    MutationObservers::NotifyAnimationChanged(this);
  }

  PostUpdate();
}

// https://drafts.csswg.org/web-animations/#play-an-animation
void Animation::ResumeAt(const TimeDuration& aReadyTime) {
  // This method is only expected to be called for an animation that is
  // waiting to play. We can easily adapt it to handle other states
  // but it's currently not necessary.
  MOZ_ASSERT(mPendingState == PendingState::PlayPending,
             "Expected to resume a play-pending animation");
  MOZ_ASSERT(!mHoldTime.IsNull() || !mStartTime.IsNull(),
             "An animation in the play-pending state should have either a"
             " resolved hold time or resolved start time");

  AutoMutationBatchForAnimation mb(*this);
  bool hadPendingPlaybackRate = mPendingPlaybackRate.isSome();

  if (!mHoldTime.IsNull()) {
    // The hold time is set, so we don't need any special handling to preserve
    // the current time.
    ApplyPendingPlaybackRate();
    mStartTime =
        StartTimeFromTimelineTime(aReadyTime, mHoldTime.Value(), mPlaybackRate);
    if (mPlaybackRate != 0) {
      mHoldTime.SetNull();
    }
  } else if (!mStartTime.IsNull() && mPendingPlaybackRate) {
    // Apply any pending playback rate, preserving the current time.
    TimeDuration currentTimeToMatch = CurrentTimeFromTimelineTime(
        aReadyTime, mStartTime.Value(), mPlaybackRate);
    ApplyPendingPlaybackRate();
    mStartTime = StartTimeFromTimelineTime(aReadyTime, currentTimeToMatch,
                                           mPlaybackRate);
    if (mPlaybackRate == 0) {
      mHoldTime.SetValue(currentTimeToMatch);
    }
  }

  mPendingState = PendingState::NotPending;

  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Sync);

  // If we had a pending playback rate, we will have now applied it so we need
  // to notify observers.
  if (hadPendingPlaybackRate && IsRelevant()) {
    MutationObservers::NotifyAnimationChanged(this);
  }

  if (mReady) {
    mReady->MaybeResolve(this);
  }
}

void Animation::PauseAt(const TimeDuration& aReadyTime) {
  MOZ_ASSERT(mPendingState == PendingState::PausePending,
             "Expected to pause a pause-pending animation");

  if (!mStartTime.IsNull() && mHoldTime.IsNull()) {
    mHoldTime = CurrentTimeFromTimelineTime(aReadyTime, mStartTime.Value(),
                                            mPlaybackRate);
  }
  ApplyPendingPlaybackRate();
  mStartTime.SetNull();
  mPendingState = PendingState::NotPending;

  UpdateTiming(SeekFlag::NoSeek, SyncNotifyFlag::Async);

  if (mReady) {
    mReady->MaybeResolve(this);
  }
}

void Animation::UpdateTiming(SeekFlag aSeekFlag,
                             SyncNotifyFlag aSyncNotifyFlag) {
  // We call UpdateFinishedState before UpdateEffect because the former
  // can change the current time, which is used by the latter.
  UpdateFinishedState(aSeekFlag, aSyncNotifyFlag);
  UpdateEffect(PostRestyleMode::IfNeeded);

  if (mTimeline) {
    mTimeline->NotifyAnimationUpdated(*this);
  }
}

// https://drafts.csswg.org/web-animations/#update-an-animations-finished-state
void Animation::UpdateFinishedState(SeekFlag aSeekFlag,
                                    SyncNotifyFlag aSyncNotifyFlag) {
  Nullable<TimeDuration> currentTime = GetCurrentTimeAsDuration();
  TimeDuration effectEnd = TimeDuration(EffectEnd());

  if (!mStartTime.IsNull() && mPendingState == PendingState::NotPending) {
    if (mPlaybackRate > 0.0 && !currentTime.IsNull() &&
        currentTime.Value() >= effectEnd) {
      if (aSeekFlag == SeekFlag::DidSeek) {
        mHoldTime = currentTime;
      } else if (!mPreviousCurrentTime.IsNull()) {
        mHoldTime.SetValue(std::max(mPreviousCurrentTime.Value(), effectEnd));
      } else {
        mHoldTime.SetValue(effectEnd);
      }
    } else if (mPlaybackRate < 0.0 && !currentTime.IsNull() &&
               currentTime.Value() <= TimeDuration()) {
      if (aSeekFlag == SeekFlag::DidSeek) {
        mHoldTime = currentTime;
      } else if (!mPreviousCurrentTime.IsNull()) {
        mHoldTime.SetValue(
            std::min(mPreviousCurrentTime.Value(), TimeDuration(0)));
      } else {
        mHoldTime.SetValue(0);
      }
    } else if (mPlaybackRate != 0.0 && !currentTime.IsNull() && mTimeline &&
               !mTimeline->GetCurrentTimeAsDuration().IsNull()) {
      if (aSeekFlag == SeekFlag::DidSeek && !mHoldTime.IsNull()) {
        mStartTime = StartTimeFromTimelineTime(
            mTimeline->GetCurrentTimeAsDuration().Value(), mHoldTime.Value(),
            mPlaybackRate);
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
  mPreviousCurrentTime = GetCurrentTimeAsDuration();
}

void Animation::UpdateEffect(PostRestyleMode aPostRestyle) {
  if (mEffect) {
    UpdateRelevance();

    KeyframeEffect* keyframeEffect = mEffect->AsKeyframeEffect();
    if (keyframeEffect) {
      keyframeEffect->NotifyAnimationTimingUpdated(aPostRestyle);
    }
  }
}

void Animation::FlushUnanimatedStyle() const {
  if (Document* doc = GetRenderedDocument()) {
    doc->FlushPendingNotifications(
        ChangesToFlush(FlushType::Style, false /* flush animations */));
  }
}

void Animation::PostUpdate() {
  if (!mEffect) {
    return;
  }

  KeyframeEffect* keyframeEffect = mEffect->AsKeyframeEffect();
  if (!keyframeEffect) {
    return;
  }
  keyframeEffect->RequestRestyle(EffectCompositor::RestyleType::Layer);
}

void Animation::CancelPendingTasks() {
  if (mPendingState == PendingState::NotPending) {
    return;
  }

  if (Document* doc = GetRenderedDocument()) {
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

// https://drafts.csswg.org/web-animations/#reset-an-animations-pending-tasks
void Animation::ResetPendingTasks() {
  if (mPendingState == PendingState::NotPending) {
    return;
  }

  CancelPendingTasks();
  ApplyPendingPlaybackRate();

  if (mReady) {
    mReady->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    mReady->SetSettledPromiseIsHandled();
    mReady = nullptr;
  }
}

void Animation::ReschedulePendingTasks() {
  if (mPendingState == PendingState::NotPending) {
    return;
  }

  mPendingReadyTime.SetNull();

  if (Document* doc = GetRenderedDocument()) {
    PendingAnimationTracker* tracker =
        doc->GetOrCreatePendingAnimationTracker();
    if (mPendingState == PendingState::PlayPending &&
        !tracker->IsWaitingToPlay(*this)) {
      tracker->AddPlayPending(*this);
    } else if (mPendingState == PendingState::PausePending &&
               !tracker->IsWaitingToPause(*this)) {
      tracker->AddPausePending(*this);
    }
  }
}

bool Animation::IsPossiblyOrphanedPendingAnimation() const {
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
  //   set mPendingState in PlayNoUpdate and relied on this method to catch us
  //   on the next tick.

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
  if (!mTimeline || mTimeline->GetCurrentTimeAsDuration().IsNull()) {
    return false;
  }

  // If we have no rendered document, or we're not in our rendered document's
  // PendingAnimationTracker then there's a good chance no one is tracking us.
  //
  // If we're wrong and another document is tracking us then, at worst, we'll
  // simply start/pause the animation one tick too soon. That's better than
  // never starting/pausing the animation and is unlikely.
  Document* doc = GetRenderedDocument();
  if (!doc) {
    return true;
  }

  PendingAnimationTracker* tracker = doc->GetPendingAnimationTracker();
  return !tracker || (!tracker->IsWaitingToPlay(*this) &&
                      !tracker->IsWaitingToPause(*this));
}

StickyTimeDuration Animation::EffectEnd() const {
  if (!mEffect) {
    return StickyTimeDuration(0);
  }

  return mEffect->SpecifiedTiming().EndTime();
}

Document* Animation::GetRenderedDocument() const {
  if (!mEffect || !mEffect->AsKeyframeEffect()) {
    return nullptr;
  }

  return mEffect->AsKeyframeEffect()->GetRenderedDocument();
}

Document* Animation::GetTimelineDocument() const {
  return mTimeline ? mTimeline->GetDocument() : nullptr;
}

class AsyncFinishNotification : public MicroTaskRunnable {
 public:
  explicit AsyncFinishNotification(Animation* aAnimation)
      : MicroTaskRunnable(), mAnimation(aAnimation) {}

  virtual void Run(AutoSlowOperation& aAso) override {
    mAnimation->DoFinishNotificationImmediately(this);
    mAnimation = nullptr;
  }

  virtual bool Suppressed() override {
    nsIGlobalObject* global = mAnimation->GetOwnerGlobal();
    return global && global->IsInSyncOperation();
  }

 private:
  RefPtr<Animation> mAnimation;
};

void Animation::DoFinishNotification(SyncNotifyFlag aSyncNotifyFlag) {
  CycleCollectedJSContext* context = CycleCollectedJSContext::Get();

  if (aSyncNotifyFlag == SyncNotifyFlag::Sync) {
    DoFinishNotificationImmediately();
  } else if (!mFinishNotificationTask) {
    RefPtr<MicroTaskRunnable> runnable = new AsyncFinishNotification(this);
    context->DispatchToMicroTask(do_AddRef(runnable));
    mFinishNotificationTask = std::move(runnable);
  }
}

void Animation::ResetFinishedPromise() {
  mFinishedIsResolved = false;
  mFinished = nullptr;
}

void Animation::MaybeResolveFinishedPromise() {
  if (mFinished) {
    mFinished->MaybeResolve(this);
  }
  mFinishedIsResolved = true;
}

void Animation::DoFinishNotificationImmediately(MicroTaskRunnable* aAsync) {
  if (aAsync && aAsync != mFinishNotificationTask) {
    return;
  }

  mFinishNotificationTask = nullptr;

  if (PlayState() != AnimationPlayState::Finished) {
    return;
  }

  MaybeResolveFinishedPromise();

  QueuePlaybackEvent(NS_LITERAL_STRING("finish"),
                     AnimationTimeToTimeStamp(EffectEnd()));
}

void Animation::QueuePlaybackEvent(const nsAString& aName,
                                   TimeStamp&& aScheduledEventTime) {
  // Use document for timing.
  // https://drafts.csswg.org/web-animations-1/#document-for-timing
  Document* doc = GetTimelineDocument();
  if (!doc) {
    return;
  }

  nsPresContext* presContext = doc->GetPresContext();
  if (!presContext) {
    return;
  }

  AnimationPlaybackEventInit init;

  if (aName.EqualsLiteral("finish") || aName.EqualsLiteral("remove")) {
    init.mCurrentTime = GetCurrentTimeAsDouble();
  }
  if (mTimeline) {
    init.mTimelineTime = mTimeline->GetCurrentTimeAsDouble();
  }

  RefPtr<AnimationPlaybackEvent> event =
      AnimationPlaybackEvent::Constructor(this, aName, init);
  event->SetTrusted(true);

  presContext->AnimationEventDispatcher()->QueueEvent(AnimationEventInfo(
      aName, std::move(event), std::move(aScheduledEventTime), this));
}

bool Animation::IsRunningOnCompositor() const {
  return mEffect && mEffect->AsKeyframeEffect() &&
         mEffect->AsKeyframeEffect()->IsRunningOnCompositor();
}

}  // namespace dom
}  // namespace mozilla
