/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSSTransition.h"

#include "mozilla/AnimationEventDispatcher.h"
#include "mozilla/dom/CSSTransitionBinding.h"
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "mozilla/TimeStamp.h"
#include "nsPresContext.h"

namespace mozilla {
namespace dom {

JSObject* CSSTransition::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return dom::CSSTransition_Binding::Wrap(aCx, this, aGivenProto);
}

void CSSTransition::GetTransitionProperty(nsString& aRetVal) const {
  MOZ_ASSERT(eCSSProperty_UNKNOWN != mTransitionProperty,
             "Transition Property should be initialized");
  aRetVal =
      NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(mTransitionProperty));
}

AnimationPlayState CSSTransition::PlayStateFromJS() const {
  FlushUnanimatedStyle();
  return Animation::PlayStateFromJS();
}

bool CSSTransition::PendingFromJS() const {
  // Transitions don't become pending again after they start running but, if
  // while the transition is still pending, style is updated in such a way
  // that the transition will be canceled, we need to report false here.
  // Hence we need to flush, but only when we're pending.
  if (Pending()) {
    FlushUnanimatedStyle();
  }
  return Animation::PendingFromJS();
}

void CSSTransition::PlayFromJS(ErrorResult& aRv) {
  FlushUnanimatedStyle();
  Animation::PlayFromJS(aRv);
}

void CSSTransition::UpdateTiming(SeekFlag aSeekFlag,
                                 SyncNotifyFlag aSyncNotifyFlag) {
  if (mNeedsNewAnimationIndexWhenRun &&
      PlayState() != AnimationPlayState::Idle) {
    mAnimationIndex = sNextAnimationIndex++;
    mNeedsNewAnimationIndexWhenRun = false;
  }

  Animation::UpdateTiming(aSeekFlag, aSyncNotifyFlag);
}

void CSSTransition::QueueEvents(const StickyTimeDuration& aActiveTime) {
  if (!mOwningElement.IsSet()) {
    return;
  }

  nsPresContext* presContext = mOwningElement.GetPresContext();
  if (!presContext) {
    return;
  }

  static constexpr StickyTimeDuration zeroDuration = StickyTimeDuration();

  TransitionPhase currentPhase;
  StickyTimeDuration intervalStartTime;
  StickyTimeDuration intervalEndTime;

  if (!mEffect) {
    currentPhase = GetAnimationPhaseWithoutEffect<TransitionPhase>(*this);
  } else {
    ComputedTiming computedTiming = mEffect->GetComputedTiming();

    currentPhase = static_cast<TransitionPhase>(computedTiming.mPhase);
    intervalStartTime = IntervalStartTime(computedTiming.mActiveDuration);
    intervalEndTime = IntervalEndTime(computedTiming.mActiveDuration);
  }

  if (mPendingState != PendingState::NotPending &&
      (mPreviousTransitionPhase == TransitionPhase::Idle ||
       mPreviousTransitionPhase == TransitionPhase::Pending)) {
    currentPhase = TransitionPhase::Pending;
  }

  if (currentPhase == mPreviousTransitionPhase) {
    return;
  }

  // TimeStamps to use for ordering the events when they are dispatched. We
  // use a TimeStamp so we can compare events produced by different elements,
  // perhaps even with different timelines.
  // The zero timestamp is for transitionrun events where we ignore the delay
  // for the purpose of ordering events.
  TimeStamp zeroTimeStamp = AnimationTimeToTimeStamp(zeroDuration);
  TimeStamp startTimeStamp = ElapsedTimeToTimeStamp(intervalStartTime);
  TimeStamp endTimeStamp = ElapsedTimeToTimeStamp(intervalEndTime);

  AutoTArray<AnimationEventInfo, 3> events;

  auto appendTransitionEvent = [&](EventMessage aMessage,
                                   const StickyTimeDuration& aElapsedTime,
                                   const TimeStamp& aScheduledEventTimeStamp) {
    double elapsedTime = aElapsedTime.ToSeconds();
    if (aMessage == eTransitionCancel) {
      // 0 is an inappropriate value for this callsite. What we need to do is
      // use a single random value for all increasing times reportable.
      // That is to say, whenever elapsedTime goes negative (because an
      // animation restarts, something rewinds the animation, or otherwise)
      // a new random value for the mix-in must be generated.
      elapsedTime =
          nsRFPService::ReduceTimePrecisionAsSecsRFPOnly(elapsedTime, 0);
    }
    events.AppendElement(AnimationEventInfo(
        TransitionProperty(), mOwningElement.Target(), aMessage, elapsedTime,
        aScheduledEventTimeStamp, this));
  };

  // Handle cancel events first
  if ((mPreviousTransitionPhase != TransitionPhase::Idle &&
       mPreviousTransitionPhase != TransitionPhase::After) &&
      currentPhase == TransitionPhase::Idle) {
    appendTransitionEvent(eTransitionCancel, aActiveTime,
                          GetTimelineCurrentTimeAsTimeStamp());
  }

  // All other events
  switch (mPreviousTransitionPhase) {
    case TransitionPhase::Idle:
      if (currentPhase == TransitionPhase::Pending ||
          currentPhase == TransitionPhase::Before) {
        appendTransitionEvent(eTransitionRun, intervalStartTime, zeroTimeStamp);
      } else if (currentPhase == TransitionPhase::Active) {
        appendTransitionEvent(eTransitionRun, intervalStartTime, zeroTimeStamp);
        appendTransitionEvent(eTransitionStart, intervalStartTime,
                              startTimeStamp);
      } else if (currentPhase == TransitionPhase::After) {
        appendTransitionEvent(eTransitionRun, intervalStartTime, zeroTimeStamp);
        appendTransitionEvent(eTransitionStart, intervalStartTime,
                              startTimeStamp);
        appendTransitionEvent(eTransitionEnd, intervalEndTime, endTimeStamp);
      }
      break;

    case TransitionPhase::Pending:
    case TransitionPhase::Before:
      if (currentPhase == TransitionPhase::Active) {
        appendTransitionEvent(eTransitionStart, intervalStartTime,
                              startTimeStamp);
      } else if (currentPhase == TransitionPhase::After) {
        appendTransitionEvent(eTransitionStart, intervalStartTime,
                              startTimeStamp);
        appendTransitionEvent(eTransitionEnd, intervalEndTime, endTimeStamp);
      }
      break;

    case TransitionPhase::Active:
      if (currentPhase == TransitionPhase::After) {
        appendTransitionEvent(eTransitionEnd, intervalEndTime, endTimeStamp);
      } else if (currentPhase == TransitionPhase::Before) {
        appendTransitionEvent(eTransitionEnd, intervalStartTime,
                              startTimeStamp);
      }
      break;

    case TransitionPhase::After:
      if (currentPhase == TransitionPhase::Active) {
        appendTransitionEvent(eTransitionStart, intervalEndTime,
                              startTimeStamp);
      } else if (currentPhase == TransitionPhase::Before) {
        appendTransitionEvent(eTransitionStart, intervalEndTime,
                              startTimeStamp);
        appendTransitionEvent(eTransitionEnd, intervalStartTime, endTimeStamp);
      }
      break;
  }
  mPreviousTransitionPhase = currentPhase;

  if (!events.IsEmpty()) {
    presContext->AnimationEventDispatcher()->QueueEvents(std::move(events));
  }
}

void CSSTransition::Tick() {
  Animation::Tick();
  QueueEvents();
}

nsCSSPropertyID CSSTransition::TransitionProperty() const {
  MOZ_ASSERT(eCSSProperty_UNKNOWN != mTransitionProperty,
             "Transition property should be initialized");
  return mTransitionProperty;
}

AnimationValue CSSTransition::ToValue() const {
  MOZ_ASSERT(!mTransitionToValue.IsNull(),
             "Transition ToValue should be initialized");
  return mTransitionToValue;
}

bool CSSTransition::HasLowerCompositeOrderThan(
    const CSSTransition& aOther) const {
  MOZ_ASSERT(IsTiedToMarkup() && aOther.IsTiedToMarkup(),
             "Should only be called for CSS transitions that are sorted "
             "as CSS transitions (i.e. tied to CSS markup)");

  // 0. Object-equality case
  if (&aOther == this) {
    return false;
  }

  // 1. Sort by document order
  if (!mOwningElement.Equals(aOther.mOwningElement)) {
    return mOwningElement.LessThan(
        const_cast<CSSTransition*>(this)->CachedChildIndexRef(),
        aOther.mOwningElement,
        const_cast<CSSTransition*>(&aOther)->CachedChildIndexRef());
  }

  // 2. (Same element and pseudo): Sort by transition generation
  if (mAnimationIndex != aOther.mAnimationIndex) {
    return mAnimationIndex < aOther.mAnimationIndex;
  }

  // 3. (Same transition generation): Sort by transition property
  return nsCSSProps::GetStringValue(TransitionProperty()) <
         nsCSSProps::GetStringValue(aOther.TransitionProperty());
}

/* static */
Nullable<TimeDuration> CSSTransition::GetCurrentTimeAt(
    const AnimationTimeline& aTimeline, const TimeStamp& aBaseTime,
    const TimeDuration& aStartTime, double aPlaybackRate) {
  Nullable<TimeDuration> result;

  Nullable<TimeDuration> timelineTime = aTimeline.ToTimelineTime(aBaseTime);
  if (!timelineTime.IsNull()) {
    result.SetValue(
        (timelineTime.Value() - aStartTime).MultDouble(aPlaybackRate));
  }

  return result;
}

double CSSTransition::CurrentValuePortion() const {
  if (!GetEffect()) {
    return 0.0;
  }

  // Transitions use a fill mode of 'backwards' so GetComputedTiming will
  // never return a null time progress due to being *before* the animation
  // interval. However, it might be possible that we're behind on flushing
  // causing us to get called *after* the animation interval. So, just in
  // case, we override the fill mode to 'both' to ensure the progress
  // is never null.
  TimingParams timingToUse = GetEffect()->SpecifiedTiming();
  timingToUse.SetFill(dom::FillMode::Both);
  ComputedTiming computedTiming = GetEffect()->GetComputedTiming(&timingToUse);

  if (computedTiming.mProgress.IsNull()) {
    return 0.0;
  }

  // 'transition-timing-function' corresponds to the effect timing while
  // the transition keyframes have a linear timing function so we can ignore
  // them for the purposes of calculating the value portion.
  return computedTiming.mProgress.Value();
}

void CSSTransition::UpdateStartValueFromReplacedTransition() {
  MOZ_ASSERT(mEffect && mEffect->AsKeyframeEffect() &&
                 mEffect->AsKeyframeEffect()->HasAnimationOfPropertySet(
                     nsCSSPropertyIDSet::CompositorAnimatables()),
             "Should be called for compositor-runnable transitions");

  MOZ_ASSERT(mTimeline,
             "Should have a timeline if we are replacing transition start "
             "values");

  if (!mReplacedTransition) {
    return;
  }

  ComputedTiming computedTiming = AnimationEffect::GetComputedTimingAt(
      CSSTransition::GetCurrentTimeAt(*mTimeline, TimeStamp::Now(),
                                      mReplacedTransition->mStartTime,
                                      mReplacedTransition->mPlaybackRate),
      mReplacedTransition->mTiming, mReplacedTransition->mPlaybackRate);

  if (!computedTiming.mProgress.IsNull()) {
    double valuePosition = ComputedTimingFunction::GetPortion(
        mReplacedTransition->mTimingFunction, computedTiming.mProgress.Value(),
        computedTiming.mBeforeFlag);

    const AnimationValue& replacedFrom = mReplacedTransition->mFromValue;
    const AnimationValue& replacedTo = mReplacedTransition->mToValue;
    AnimationValue startValue;
    startValue.mServo =
        Servo_AnimationValues_Interpolate(replacedFrom.mServo,
                                          replacedTo.mServo, valuePosition)
            .Consume();

    mEffect->AsKeyframeEffect()->ReplaceTransitionStartValue(
        std::move(startValue));
  }

  mReplacedTransition.reset();
}

void CSSTransition::SetEffectFromStyle(dom::AnimationEffect* aEffect) {
  Animation::SetEffectNoUpdate(aEffect);

  // Initialize transition property and to value.
  //
  // Typically this should only be called with a KeyframeEffect representing
  // a simple transition, but just to be sure we check the effect has the
  // expected shape first.
  const KeyframeEffect* keyframeEffect = aEffect->AsKeyframeEffect();
  if (MOZ_LIKELY(keyframeEffect && keyframeEffect->Properties().Length() == 1 &&
                 keyframeEffect->Properties()[0].mSegments.Length() == 1)) {
    mTransitionProperty = keyframeEffect->Properties()[0].mProperty;
    mTransitionToValue = keyframeEffect->Properties()[0].mSegments[0].mToValue;
  } else {
    MOZ_ASSERT_UNREACHABLE("Transition effect has unexpected shape");
  }
}

}  // namespace dom
}  // namespace mozilla
