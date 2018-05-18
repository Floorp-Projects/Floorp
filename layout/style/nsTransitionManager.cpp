/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Code to start and animate CSS transitions. */

#include "nsTransitionManager.h"
#include "nsAnimationManager.h"
#include "mozilla/dom/CSSTransitionBinding.h"

#include "nsIContent.h"
#include "nsContentUtils.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"
#include "nsRefreshDriver.h"
#include "nsCSSPropertyIDSet.h"
#include "mozilla/AnimationEventDispatcher.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EffectSet.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/DocumentTimeline.h"
#include "mozilla/dom/Element.h"
#include "nsIFrame.h"
#include "Layers.h"
#include "FrameLayerBuilder.h"
#include "nsCSSProps.h"
#include "nsCSSPseudoElements.h"
#include "nsDisplayList.h"
#include "nsRFPService.h"
#include "nsStyleChangeList.h"
#include "mozilla/RestyleManager.h"
#include "nsDOMMutationObserver.h"

using mozilla::TimeStamp;
using mozilla::TimeDuration;
using mozilla::dom::Animation;
using mozilla::dom::AnimationPlayState;
using mozilla::dom::CSSTransition;

using namespace mozilla;
using namespace mozilla::css;

double
ElementPropertyTransition::CurrentValuePortion() const
{
  MOZ_ASSERT(!GetLocalTime().IsNull(),
             "Getting the value portion of an animation that's not being "
             "sampled");

  // Transitions use a fill mode of 'backwards' so GetComputedTiming will
  // never return a null time progress due to being *before* the animation
  // interval. However, it might be possible that we're behind on flushing
  // causing us to get called *after* the animation interval. So, just in
  // case, we override the fill mode to 'both' to ensure the progress
  // is never null.
  TimingParams timingToUse = SpecifiedTiming();
  timingToUse.SetFill(dom::FillMode::Both);
  ComputedTiming computedTiming = GetComputedTiming(&timingToUse);

  MOZ_ASSERT(!computedTiming.mProgress.IsNull(),
             "Got a null progress for a fill mode of 'both'");
  MOZ_ASSERT(mKeyframes.Length() == 2,
             "Should have two animation keyframes for a transition");
  return ComputedTimingFunction::GetPortion(mKeyframes[0].mTimingFunction,
                                            computedTiming.mProgress.Value(),
                                            computedTiming.mBeforeFlag);
}

void
ElementPropertyTransition::UpdateStartValueFromReplacedTransition()
{
  if (!mReplacedTransition) {
    return;
  }
  MOZ_ASSERT(nsCSSProps::PropHasFlags(TransitionProperty(),
                                      CSSPropFlags::CanAnimateOnCompositor),
             "The transition property should be able to be run on the "
             "compositor");
  MOZ_ASSERT(mTarget && mTarget->mElement->OwnerDoc(),
             "We should have a valid document at this moment");

  dom::DocumentTimeline* timeline = mTarget->mElement->OwnerDoc()->Timeline();
  ComputedTiming computedTiming = GetComputedTimingAt(
    dom::CSSTransition::GetCurrentTimeAt(*timeline,
                                         TimeStamp::Now(),
                                         mReplacedTransition->mStartTime,
                                         mReplacedTransition->mPlaybackRate),
    mReplacedTransition->mTiming,
    mReplacedTransition->mPlaybackRate);

  if (!computedTiming.mProgress.IsNull()) {
    double valuePosition =
      ComputedTimingFunction::GetPortion(mReplacedTransition->mTimingFunction,
                                         computedTiming.mProgress.Value(),
                                         computedTiming.mBeforeFlag);

    MOZ_ASSERT(mProperties.Length() == 1 &&
               mProperties[0].mSegments.Length() == 1,
               "The transition should have one property and one segment");
    MOZ_ASSERT(mKeyframes.Length() == 2,
               "Transitions should have exactly two animation keyframes");
    MOZ_ASSERT(mKeyframes[0].mPropertyValues.Length() == 1,
               "Transitions should have exactly one property in their first "
               "frame");

    const AnimationValue& replacedFrom = mReplacedTransition->mFromValue;
    const AnimationValue& replacedTo = mReplacedTransition->mToValue;
    AnimationValue startValue;
    startValue.mServo =
      Servo_AnimationValues_Interpolate(replacedFrom.mServo,
                                        replacedTo.mServo,
                                        valuePosition).Consume();
    if (startValue.mServo) {
      mKeyframes[0].mPropertyValues[0].mServoDeclarationBlock =
        Servo_AnimationValue_Uncompute(startValue.mServo).Consume();
      mProperties[0].mSegments[0].mFromValue = Move(startValue);
    }
  }

  mReplacedTransition.reset();
}

////////////////////////// CSSTransition ////////////////////////////

JSObject*
CSSTransition::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::CSSTransitionBinding::Wrap(aCx, this, aGivenProto);
}

void
CSSTransition::GetTransitionProperty(nsString& aRetVal) const
{
  MOZ_ASSERT(eCSSProperty_UNKNOWN != mTransitionProperty,
             "Transition Property should be initialized");
  aRetVal =
    NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(mTransitionProperty));
}

AnimationPlayState
CSSTransition::PlayStateFromJS() const
{
  FlushUnanimatedStyle();
  return Animation::PlayStateFromJS();
}

bool
CSSTransition::PendingFromJS() const
{
  // Transitions don't become pending again after they start running but, if
  // while the transition is still pending, style is updated in such a way
  // that the transition will be canceled, we need to report false here.
  // Hence we need to flush, but only when we're pending.
  if (Pending()) {
    FlushUnanimatedStyle();
  }
  return Animation::PendingFromJS();
}

void
CSSTransition::PlayFromJS(ErrorResult& aRv)
{
  FlushUnanimatedStyle();
  Animation::PlayFromJS(aRv);
}

void
CSSTransition::UpdateTiming(SeekFlag aSeekFlag, SyncNotifyFlag aSyncNotifyFlag)
{
  if (mNeedsNewAnimationIndexWhenRun &&
      PlayState() != AnimationPlayState::Idle) {
    mAnimationIndex = sNextAnimationIndex++;
    mNeedsNewAnimationIndexWhenRun = false;
  }

  Animation::UpdateTiming(aSeekFlag, aSyncNotifyFlag);
}

void
CSSTransition::QueueEvents(const StickyTimeDuration& aActiveTime)
{
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
    intervalStartTime =
      std::max(std::min(StickyTimeDuration(-mEffect->SpecifiedTiming().Delay()),
                        computedTiming.mActiveDuration), zeroDuration);
    intervalEndTime =
      std::max(std::min((EffectEnd() - mEffect->SpecifiedTiming().Delay()),
                        computedTiming.mActiveDuration), zeroDuration);
  }

  // TimeStamps to use for ordering the events when they are dispatched. We
  // use a TimeStamp so we can compare events produced by different elements,
  // perhaps even with different timelines.
  // The zero timestamp is for transitionrun events where we ignore the delay
  // for the purpose of ordering events.
  TimeStamp zeroTimeStamp  = AnimationTimeToTimeStamp(zeroDuration);
  TimeStamp startTimeStamp = ElapsedTimeToTimeStamp(intervalStartTime);
  TimeStamp endTimeStamp   = ElapsedTimeToTimeStamp(intervalEndTime);

  if (mPendingState != PendingState::NotPending &&
      (mPreviousTransitionPhase == TransitionPhase::Idle ||
       mPreviousTransitionPhase == TransitionPhase::Pending))
  {
    currentPhase = TransitionPhase::Pending;
  }

  AutoTArray<AnimationEventInfo, 3> events;

  auto appendTransitionEvent = [&](EventMessage aMessage,
                                   const StickyTimeDuration& aElapsedTime,
                                   const TimeStamp& aTimeStamp) {
    double elapsedTime = aElapsedTime.ToSeconds();
    if (aMessage == eTransitionCancel) {
      // 0 is an inappropriate value for this callsite. What we need to do is
      // use a single random value for all increasing times reportable.
      // That is to say, whenever elapsedTime goes negative (because an
      // animation restarts, something rewinds the animation, or otherwise)
      // a new random value for the mix-in must be generated.
      elapsedTime = nsRFPService::ReduceTimePrecisionAsSecs(elapsedTime, 0, TimerPrecisionType::RFPOnly);
    }
    events.AppendElement(AnimationEventInfo(TransitionProperty(),
                                            mOwningElement.Target(),
                                            aMessage,
                                            elapsedTime,
                                            aTimeStamp,
                                            this));
  };

  // Handle cancel events first
  if ((mPreviousTransitionPhase != TransitionPhase::Idle &&
       mPreviousTransitionPhase != TransitionPhase::After) &&
      currentPhase == TransitionPhase::Idle) {
    TimeStamp activeTimeStamp = ElapsedTimeToTimeStamp(aActiveTime);
    appendTransitionEvent(eTransitionCancel, aActiveTime, activeTimeStamp);
  }

  // All other events
  switch (mPreviousTransitionPhase) {
    case TransitionPhase::Idle:
      if (currentPhase == TransitionPhase::Pending ||
          currentPhase == TransitionPhase::Before) {
        appendTransitionEvent(eTransitionRun, intervalStartTime, zeroTimeStamp);
      } else if (currentPhase == TransitionPhase::Active) {
        appendTransitionEvent(eTransitionRun, intervalStartTime, zeroTimeStamp);
        appendTransitionEvent(eTransitionStart,
                              intervalStartTime,
                              startTimeStamp);
      } else if (currentPhase == TransitionPhase::After) {
        appendTransitionEvent(eTransitionRun, intervalStartTime, zeroTimeStamp);
        appendTransitionEvent(eTransitionStart,
                              intervalStartTime,
                              startTimeStamp);
        appendTransitionEvent(eTransitionEnd, intervalEndTime, endTimeStamp);
      }
      break;

    case TransitionPhase::Pending:
    case TransitionPhase::Before:
      if (currentPhase == TransitionPhase::Active) {
        appendTransitionEvent(eTransitionStart,
                              intervalStartTime,
                              startTimeStamp);
      } else if (currentPhase == TransitionPhase::After) {
        appendTransitionEvent(eTransitionStart,
                              intervalStartTime,
                              startTimeStamp);
        appendTransitionEvent(eTransitionEnd, intervalEndTime, endTimeStamp);
      }
      break;

    case TransitionPhase::Active:
      if (currentPhase == TransitionPhase::After) {
        appendTransitionEvent(eTransitionEnd, intervalEndTime, endTimeStamp);
      } else if (currentPhase == TransitionPhase::Before) {
        appendTransitionEvent(eTransitionEnd,
                              intervalStartTime,
                              startTimeStamp);
      }
      break;

    case TransitionPhase::After:
      if (currentPhase == TransitionPhase::Active) {
        appendTransitionEvent(eTransitionStart,
                              intervalEndTime,
                              startTimeStamp);
      } else if (currentPhase == TransitionPhase::Before) {
        appendTransitionEvent(eTransitionStart,
                              intervalEndTime,
                              startTimeStamp);
        appendTransitionEvent(eTransitionEnd,
                              intervalStartTime,
                              endTimeStamp);
      }
      break;
  }
  mPreviousTransitionPhase = currentPhase;

  if (!events.IsEmpty()) {
    presContext->AnimationEventDispatcher()->QueueEvents(Move(events));
  }
}

void
CSSTransition::Tick()
{
  Animation::Tick();
  QueueEvents();
}

nsCSSPropertyID
CSSTransition::TransitionProperty() const
{
  MOZ_ASSERT(eCSSProperty_UNKNOWN != mTransitionProperty,
             "Transition property should be initialized");
  return mTransitionProperty;
}

AnimationValue
CSSTransition::ToValue() const
{
  MOZ_ASSERT(!mTransitionToValue.IsNull(),
             "Transition ToValue should be initialized");
  return mTransitionToValue;
}

bool
CSSTransition::HasLowerCompositeOrderThan(const CSSTransition& aOther) const
{
  MOZ_ASSERT(IsTiedToMarkup() && aOther.IsTiedToMarkup(),
             "Should only be called for CSS transitions that are sorted "
             "as CSS transitions (i.e. tied to CSS markup)");

  // 0. Object-equality case
  if (&aOther == this) {
    return false;
  }

  // 1. Sort by document order
  if (!mOwningElement.Equals(aOther.mOwningElement)) {
    return mOwningElement.LessThan(aOther.mOwningElement);
  }

  // 2. (Same element and pseudo): Sort by transition generation
  if (mAnimationIndex != aOther.mAnimationIndex) {
    return mAnimationIndex < aOther.mAnimationIndex;
  }

  // 3. (Same transition generation): Sort by transition property
  return nsCSSProps::GetStringValue(TransitionProperty()) <
         nsCSSProps::GetStringValue(aOther.TransitionProperty());
}

/* static */ Nullable<TimeDuration>
CSSTransition::GetCurrentTimeAt(const dom::DocumentTimeline& aTimeline,
                                const TimeStamp& aBaseTime,
                                const TimeDuration& aStartTime,
                                double aPlaybackRate)
{
  Nullable<TimeDuration> result;

  Nullable<TimeDuration> timelineTime = aTimeline.ToTimelineTime(aBaseTime);
  if (!timelineTime.IsNull()) {
    result.SetValue((timelineTime.Value() - aStartTime)
                      .MultDouble(aPlaybackRate));
  }

  return result;
}

void
CSSTransition::SetEffectFromStyle(dom::AnimationEffect* aEffect)
{
  Animation::SetEffectNoUpdate(aEffect);

  // Initialize transition property.
  ElementPropertyTransition* pt = aEffect ? aEffect->AsTransition() : nullptr;
  if (eCSSProperty_UNKNOWN == mTransitionProperty && pt) {
    mTransitionProperty = pt->TransitionProperty();
    mTransitionToValue = pt->ToValue();
  }
}

////////////////////////// nsTransitionManager ////////////////////////////


static inline bool
ExtractNonDiscreteComputedValue(nsCSSPropertyID aProperty,
                                const ComputedStyle& aComputedStyle,
                                AnimationValue& aAnimationValue)
{
  if (Servo_Property_IsDiscreteAnimatable(aProperty) &&
      aProperty != eCSSProperty_visibility) {
    return false;
  }

  aAnimationValue.mServo =
    Servo_ComputedValues_ExtractAnimationValue(&aComputedStyle,
                                               aProperty).Consume();
  return !!aAnimationValue.mServo;
}


bool
nsTransitionManager::UpdateTransitions(
  dom::Element *aElement,
  CSSPseudoElementType aPseudoType,
  const ComputedStyle& aOldStyle,
  const ComputedStyle& aNewStyle)
{
  if (!mPresContext->IsDynamic()) {
    // For print or print preview, ignore transitions.
    return false;
  }

  CSSTransitionCollection* collection =
    CSSTransitionCollection::GetAnimationCollection(aElement, aPseudoType);
  const nsStyleDisplay* disp =
      aNewStyle.ComputedData()->GetStyleDisplay();
  return DoUpdateTransitions(*disp,
                             aElement, aPseudoType,
                             collection,
                             aOldStyle, aNewStyle);
}

bool
nsTransitionManager::DoUpdateTransitions(
  const nsStyleDisplay& aDisp,
  dom::Element* aElement,
  CSSPseudoElementType aPseudoType,
  CSSTransitionCollection*& aElementTransitions,
  const ComputedStyle& aOldStyle,
  const ComputedStyle& aNewStyle)
{
  MOZ_ASSERT(!aElementTransitions ||
             aElementTransitions->mElement == aElement, "Element mismatch");

  // Per http://lists.w3.org/Archives/Public/www-style/2009Aug/0109.html
  // I'll consider only the transitions from the number of items in
  // 'transition-property' on down, and later ones will override earlier
  // ones (tracked using |propertiesChecked|).
  bool startedAny = false;
  nsCSSPropertyIDSet propertiesChecked;
  for (uint32_t i = aDisp.mTransitionPropertyCount; i--; ) {
    // We're not going to look at any further transitions, so we can just avoid
    // looking at this if we know it will not start any transitions.
    if (i == 0 && aDisp.GetTransitionCombinedDuration(i) <= 0.0f) {
      continue;
    }

    nsCSSPropertyID property = aDisp.GetTransitionProperty(i);
    if (property == eCSSPropertyExtra_no_properties ||
        property == eCSSPropertyExtra_variable ||
        property == eCSSProperty_UNKNOWN) {
      // Nothing to do.
      continue;
    }
    // We might have something to transition.  See if any of the
    // properties in question changed and are animatable.
    // FIXME: Would be good to find a way to share code between this
    // interpretation of transition-property and the one below.
    // FIXME(emilio): This should probably just use the "all" shorthand id, and
    // we should probably remove eCSSPropertyExtra_all_properties.
    if (property == eCSSPropertyExtra_all_properties) {
      for (nsCSSPropertyID p = nsCSSPropertyID(0);
           p < eCSSProperty_COUNT_no_shorthands;
           p = nsCSSPropertyID(p + 1)) {
        startedAny |=
          ConsiderInitiatingTransition(p, aDisp, i, aElement, aPseudoType,
                                       aElementTransitions,
                                       aOldStyle, aNewStyle,
                                       propertiesChecked);
      }
    } else if (nsCSSProps::IsShorthand(property)) {
      CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subprop, property,
                                           CSSEnabledState::eForAllContent)
      {
        startedAny |=
          ConsiderInitiatingTransition(*subprop, aDisp, i, aElement, aPseudoType,
                                       aElementTransitions,
                                       aOldStyle, aNewStyle,
                                       propertiesChecked);
      }
    } else {
      startedAny |=
        ConsiderInitiatingTransition(property, aDisp, i, aElement, aPseudoType,
                                     aElementTransitions,
                                     aOldStyle, aNewStyle,
                                     propertiesChecked);
    }
  }

  // Stop any transitions for properties that are no longer in
  // 'transition-property', including finished transitions.
  // Also stop any transitions (and remove any finished transitions)
  // for properties that just changed (and are still in the set of
  // properties to transition), but for which we didn't just start the
  // transition.  This can happen delay and duration are both zero, or
  // because the new value is not interpolable.
  // Note that we also do the latter set of work in
  // nsTransitionManager::PruneCompletedTransitions.
  if (aElementTransitions) {
    bool checkProperties =
      aDisp.GetTransitionProperty(0) != eCSSPropertyExtra_all_properties;
    nsCSSPropertyIDSet allTransitionProperties;
    if (checkProperties) {
      for (uint32_t i = aDisp.mTransitionPropertyCount; i-- != 0; ) {
        // FIXME: Would be good to find a way to share code between this
        // interpretation of transition-property and the one above.
        nsCSSPropertyID property = aDisp.GetTransitionProperty(i);
        if (property == eCSSPropertyExtra_no_properties ||
            property == eCSSPropertyExtra_variable ||
            property == eCSSProperty_UNKNOWN) {
          // Nothing to do, but need to exclude this from cases below.
        } else if (property == eCSSPropertyExtra_all_properties) {
          for (nsCSSPropertyID p = nsCSSPropertyID(0);
               p < eCSSProperty_COUNT_no_shorthands;
               p = nsCSSPropertyID(p + 1)) {
            allTransitionProperties.AddProperty(p);
          }
        } else if (nsCSSProps::IsShorthand(property)) {
          CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(
              subprop, property, CSSEnabledState::eForAllContent) {
            allTransitionProperties.AddProperty(*subprop);
          }
        } else {
          allTransitionProperties.AddProperty(property);
        }
      }
    }

    OwningCSSTransitionPtrArray& animations = aElementTransitions->mAnimations;
    size_t i = animations.Length();
    MOZ_ASSERT(i != 0, "empty transitions list?");
    AnimationValue currentValue;
    do {
      --i;
      CSSTransition* anim = animations[i];
          // properties no longer in 'transition-property'
      if ((checkProperties &&
           !allTransitionProperties.HasProperty(anim->TransitionProperty())) ||
          // properties whose computed values changed but for which we
          // did not start a new transition (because delay and
          // duration are both zero, or because the new value is not
          // interpolable); a new transition would have anim->ToValue()
          // matching currentValue
          !ExtractNonDiscreteComputedValue(anim->TransitionProperty(),
                                           aNewStyle, currentValue) ||
          currentValue != anim->ToValue()) {
        // stop the transition
        if (anim->HasCurrentEffect()) {
          EffectSet* effectSet =
            EffectSet::GetEffectSet(aElement, aPseudoType);
          if (effectSet) {
            effectSet->UpdateAnimationGeneration(mPresContext);
          }
        }
        anim->CancelFromStyle();
        animations.RemoveElementAt(i);
      }
    } while (i != 0);

    if (animations.IsEmpty()) {
      aElementTransitions->Destroy();
      aElementTransitions = nullptr;
    }
  }

  return startedAny;
}

static Keyframe&
AppendKeyframe(double aOffset,
               nsCSSPropertyID aProperty,
               AnimationValue&& aValue,
               nsTArray<Keyframe>& aKeyframes)
{
  Keyframe& frame = *aKeyframes.AppendElement();
  frame.mOffset.emplace(aOffset);

  if (aValue.mServo) {
    RefPtr<RawServoDeclarationBlock> decl =
      Servo_AnimationValue_Uncompute(aValue.mServo).Consume();
    frame.mPropertyValues.AppendElement(
      Move(PropertyValuePair(aProperty, Move(decl))));
  } else {
    MOZ_CRASH("old style system disabled");
  }
  return frame;
}

static nsTArray<Keyframe>
GetTransitionKeyframes(nsCSSPropertyID aProperty,
                       AnimationValue&& aStartValue,
                       AnimationValue&& aEndValue,
                       const nsTimingFunction& aTimingFunction)
{
  nsTArray<Keyframe> keyframes(2);

  Keyframe& fromFrame = AppendKeyframe(0.0, aProperty, Move(aStartValue),
                                       keyframes);
  if (aTimingFunction.mType != nsTimingFunction::Type::Linear) {
    fromFrame.mTimingFunction.emplace();
    fromFrame.mTimingFunction->Init(aTimingFunction);
  }

  AppendKeyframe(1.0, aProperty, Move(aEndValue), keyframes);

  return keyframes;
}

static bool
IsTransitionable(nsCSSPropertyID aProperty)
{
  return Servo_Property_IsTransitionable(aProperty);
}

bool
nsTransitionManager::ConsiderInitiatingTransition(
  nsCSSPropertyID aProperty,
  const nsStyleDisplay& aStyleDisplay,
  uint32_t transitionIdx,
  dom::Element* aElement,
  CSSPseudoElementType aPseudoType,
  CSSTransitionCollection*& aElementTransitions,
  const ComputedStyle& aOldStyle,
  const ComputedStyle& aNewStyle,
  nsCSSPropertyIDSet& aPropertiesChecked)
{
  // IsShorthand itself will assert if aProperty is not a property.
  MOZ_ASSERT(!nsCSSProps::IsShorthand(aProperty),
             "property out of range");
  NS_ASSERTION(!aElementTransitions ||
               aElementTransitions->mElement == aElement, "Element mismatch");

  // A later item in transition-property already specified a transition for this
  // property, so we ignore this one.
  // See http://lists.w3.org/Archives/Public/www-style/2009Aug/0109.html .
  if (aPropertiesChecked.HasProperty(aProperty)) {
    return false;
  }

  aPropertiesChecked.AddProperty(aProperty);

  // Ignore disabled properties. We can arrive here if the transition-property
  // is 'all' and the disabled property has a default value which derives value
  // from another property, e.g. color.
  if (!nsCSSProps::IsEnabled(aProperty, CSSEnabledState::eForAllContent)) {
    return false;
  }

  if (!IsTransitionable(aProperty)) {
    return false;
  }

  float delay = aStyleDisplay.GetTransitionDelay(transitionIdx);

  // The spec says a negative duration is treated as zero.
  float duration =
    std::max(aStyleDisplay.GetTransitionDuration(transitionIdx), 0.0f);

  // If the combined duration of this transition is 0 or less don't start a
  // transition.
  if (delay + duration <= 0.0f) {
    return false;
  }

  dom::DocumentTimeline* timeline = aElement->OwnerDoc()->Timeline();

  AnimationValue startValue, endValue;
  bool haveValues =
    ExtractNonDiscreteComputedValue(aProperty, aOldStyle, startValue) &&
    ExtractNonDiscreteComputedValue(aProperty, aNewStyle, endValue);

  bool haveChange = startValue != endValue;

  bool shouldAnimate =
    haveValues &&
    haveChange &&
    startValue.IsInterpolableWith(aProperty, endValue);

  bool haveCurrentTransition = false;
  size_t currentIndex = nsTArray<ElementPropertyTransition>::NoIndex;
  const ElementPropertyTransition *oldPT = nullptr;
  if (aElementTransitions) {
    OwningCSSTransitionPtrArray& animations = aElementTransitions->mAnimations;
    for (size_t i = 0, i_end = animations.Length(); i < i_end; ++i) {
      if (animations[i]->TransitionProperty() == aProperty) {
        haveCurrentTransition = true;
        currentIndex = i;
        oldPT = animations[i]->GetEffect()
                ? animations[i]->GetEffect()->AsTransition()
                : nullptr;
        break;
      }
    }
  }

  // If we got a style change that changed the value to the endpoint
  // of the currently running transition, we don't want to interrupt
  // its timing function.
  // This needs to be before the !shouldAnimate && haveCurrentTransition
  // case below because we might be close enough to the end of the
  // transition that the current value rounds to the final value.  In
  // this case, we'll end up with shouldAnimate as false (because
  // there's no value change), but we need to return early here rather
  // than cancel the running transition because shouldAnimate is false!
  //
  // Likewise, if we got a style change that changed the value to the
  // endpoint of our finished transition, we also don't want to start
  // a new transition for the reasons described in
  // https://lists.w3.org/Archives/Public/www-style/2015Jan/0444.html .
  if (haveCurrentTransition && haveValues &&
      aElementTransitions->mAnimations[currentIndex]->ToValue() == endValue) {
    // GetAnimationRule already called RestyleForAnimation.
    return false;
  }

  if (!shouldAnimate) {
    if (haveCurrentTransition) {
      // We're in the middle of a transition, and just got a non-transition
      // style change to something that we can't animate.  This might happen
      // because we got a non-transition style change changing to the current
      // in-progress value (which is particularly easy to cause when we're
      // currently in the 'transition-delay').  It also might happen because we
      // just got a style change to a value that can't be interpolated.
      OwningCSSTransitionPtrArray& animations =
        aElementTransitions->mAnimations;
      animations[currentIndex]->CancelFromStyle();
      oldPT = nullptr; // Clear pointer so it doesn't dangle
      animations.RemoveElementAt(currentIndex);
      EffectSet* effectSet = EffectSet::GetEffectSet(aElement, aPseudoType);
      if (effectSet) {
        effectSet->UpdateAnimationGeneration(mPresContext);
      }

      if (animations.IsEmpty()) {
        aElementTransitions->Destroy();
        // |aElementTransitions| is now a dangling pointer!
        aElementTransitions = nullptr;
      }
      // GetAnimationRule already called RestyleForAnimation.
    }
    return false;
  }

  const nsTimingFunction &tf =
    aStyleDisplay.GetTransitionTimingFunction(transitionIdx);

  AnimationValue startForReversingTest = startValue;
  double reversePortion = 1.0;

  // If the new transition reverses an existing one, we'll need to
  // handle the timing differently.
  // FIXME: Move mStartForReversingTest, mReversePortion to CSSTransition,
  //        and set the timing function on transitions as an effect-level
  //        easing (rather than keyframe-level easing). (Bug 1292001)
  if (haveCurrentTransition &&
      aElementTransitions->mAnimations[currentIndex]->HasCurrentEffect() &&
      oldPT &&
      oldPT->mStartForReversingTest == endValue) {
    // Compute the appropriate negative transition-delay such that right
    // now we'd end up at the current position.
    double valuePortion =
      oldPT->CurrentValuePortion() * oldPT->mReversePortion +
      (1.0 - oldPT->mReversePortion);
    // A timing function with negative y1 (or y2!) might make
    // valuePortion negative.  In this case, we still want to apply our
    // reversing logic based on relative distances, not make duration
    // negative.
    if (valuePortion < 0.0) {
      valuePortion = -valuePortion;
    }
    // A timing function with y2 (or y1!) greater than one might
    // advance past its terminal value.  It's probably a good idea to
    // clamp valuePortion to be at most one to preserve the invariant
    // that a transition will complete within at most its specified
    // time.
    if (valuePortion > 1.0) {
      valuePortion = 1.0;
    }

    // Negative delays are essentially part of the transition
    // function, so reduce them along with the duration, but don't
    // reduce positive delays.
    if (delay < 0.0f) {
      delay *= valuePortion;
    }

    duration *= valuePortion;

    startForReversingTest = oldPT->ToValue();
    reversePortion = valuePortion;
  }

  TimingParams timing =
    TimingParamsFromCSSParams(duration, delay,
                              1.0 /* iteration count */,
                              dom::PlaybackDirection::Normal,
                              dom::FillMode::Backwards);

  // aElement is non-null here, so we emplace it directly.
  Maybe<OwningAnimationTarget> target;
  target.emplace(aElement, aPseudoType);
  KeyframeEffectParams effectOptions;
  RefPtr<ElementPropertyTransition> pt =
    new ElementPropertyTransition(aElement->OwnerDoc(), target, timing,
                                  startForReversingTest, reversePortion,
                                  effectOptions);

  pt->SetKeyframes(GetTransitionKeyframes(aProperty,
                                          Move(startValue), Move(endValue), tf),
                   &aNewStyle);

  RefPtr<CSSTransition> animation =
    new CSSTransition(mPresContext->Document()->GetScopeObject());
  animation->SetOwningElement(OwningElementRef(*aElement, aPseudoType));
  animation->SetTimelineNoUpdate(timeline);
  animation->SetCreationSequence(
    mPresContext->RestyleManager()->GetAnimationGeneration());
  animation->SetEffectFromStyle(pt);
  animation->PlayFromStyle();

  if (!aElementTransitions) {
    bool createdCollection = false;
    aElementTransitions =
      CSSTransitionCollection::GetOrCreateAnimationCollection(
        aElement, aPseudoType, &createdCollection);
    if (!aElementTransitions) {
      MOZ_ASSERT(!createdCollection, "outparam should agree with return value");
      NS_WARNING("allocating collection failed");
      return false;
    }

    if (createdCollection) {
      AddElementCollection(aElementTransitions);
    }
  }

  OwningCSSTransitionPtrArray& animations = aElementTransitions->mAnimations;
#ifdef DEBUG
  for (size_t i = 0, i_end = animations.Length(); i < i_end; ++i) {
    MOZ_ASSERT(
      i == currentIndex || animations[i]->TransitionProperty() != aProperty,
      "duplicate transitions for property");
  }
#endif
  if (haveCurrentTransition) {
    // If this new transition is replacing an existing transition that is running
    // on the compositor, we store select parameters from the replaced transition
    // so that later, once all scripts have run, we can update the start value
    // of the transition using TimeStamp::Now(). This allows us to avoid a
    // large jump when starting a new transition when the main thread lags behind
    // the compositor.
    if (oldPT &&
        oldPT->IsCurrent() &&
        oldPT->IsRunningOnCompositor() &&
        !oldPT->GetAnimation()->GetStartTime().IsNull() &&
        timeline == oldPT->GetAnimation()->GetTimeline()) {
      const AnimationPropertySegment& segment =
        oldPT->Properties()[0].mSegments[0];
      pt->mReplacedTransition.emplace(
        ElementPropertyTransition::ReplacedTransitionProperties({
          oldPT->GetAnimation()->GetStartTime().Value(),
          oldPT->GetAnimation()->PlaybackRate(),
          oldPT->SpecifiedTiming(),
          segment.mTimingFunction,
          segment.mFromValue,
          segment.mToValue
        })
      );
    }
    animations[currentIndex]->CancelFromStyle();
    oldPT = nullptr; // Clear pointer so it doesn't dangle
    animations[currentIndex] = animation;
  } else {
    if (!animations.AppendElement(animation)) {
      NS_WARNING("out of memory");
      return false;
    }
  }

  EffectSet* effectSet = EffectSet::GetEffectSet(aElement, aPseudoType);
  if (effectSet) {
    effectSet->UpdateAnimationGeneration(mPresContext);
  }

  return true;
}

