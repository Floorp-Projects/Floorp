/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Code to start and animate CSS transitions. */

#include "nsTransitionManager.h"
#include "nsAnimationManager.h"
#include "mozilla/dom/CSSTransitionBinding.h"

#include "nsIContent.h"
#include "nsStyleContext.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"
#include "nsRefreshDriver.h"
#include "nsRuleProcessorData.h"
#include "nsRuleWalker.h"
#include "nsCSSPropertyIDSet.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EffectSet.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/DocumentTimeline.h"
#include "mozilla/dom/Element.h"
#include "nsIFrame.h"
#include "Layers.h"
#include "FrameLayerBuilder.h"
#include "nsCSSProps.h"
#include "nsCSSPseudoElements.h"
#include "nsDisplayList.h"
#include "nsStyleChangeList.h"
#include "nsStyleSet.h"
#include "mozilla/RestyleManagerHandle.h"
#include "mozilla/RestyleManagerHandleInlines.h"
#include "nsDOMMutationObserver.h"

using mozilla::TimeStamp;
using mozilla::TimeDuration;
using mozilla::dom::Animation;
using mozilla::dom::AnimationPlayState;
using mozilla::dom::CSSTransition;
using mozilla::dom::KeyframeEffectReadOnly;

using namespace mozilla;
using namespace mozilla::css;

typedef mozilla::ComputedTiming::AnimationPhase AnimationPhase;

namespace {
struct TransitionEventParams {
  EventMessage mMessage;
  StickyTimeDuration mElapsedTime;
  TimeStamp mTimeStamp;
};
} // anonymous namespace

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
  timingToUse.mFill = dom::FillMode::Both;
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
                                      CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR),
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
    StyleAnimationValue startValue;
    if (StyleAnimationValue::Interpolate(mProperties[0].mProperty,
                                         mReplacedTransition->mFromValue,
                                         mReplacedTransition->mToValue,
                                         valuePosition, startValue)) {
      MOZ_ASSERT(mProperties.Length() == 1 &&
                 mProperties[0].mSegments.Length() == 1,
                 "The transition should have one property and one segment");
      nsCSSValue cssValue;
      DebugOnly<bool> uncomputeResult =
        StyleAnimationValue::UncomputeValue(mProperties[0].mProperty,
                                            startValue,
                                            cssValue);
      mProperties[0].mSegments[0].mFromValue = Move(startValue);
      MOZ_ASSERT(uncomputeResult, "UncomputeValue should not fail");
      MOZ_ASSERT(mKeyframes.Length() == 2,
          "Transitions should have exactly two animation keyframes");
      MOZ_ASSERT(mKeyframes[0].mPropertyValues.Length() == 1,
          "Transitions should have exactly one property in their first "
          "frame");
      mKeyframes[0].mPropertyValues[0].mValue = cssValue;
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
  FlushStyle();
  return Animation::PlayStateFromJS();
}

void
CSSTransition::PlayFromJS(ErrorResult& aRv)
{
  FlushStyle();
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
CSSTransition::QueueEvents()
{
  if (!mEffect ||
      !mOwningElement.IsSet()) {
    return;
  }

  dom::Element* owningElement;
  CSSPseudoElementType owningPseudoType;
  mOwningElement.GetElement(owningElement, owningPseudoType);
  MOZ_ASSERT(owningElement, "Owning element should be set");

  nsPresContext* presContext = mOwningElement.GetRenderedPresContext();
  if (!presContext) {
    return;
  }

  ComputedTiming computedTiming = mEffect->GetComputedTiming();
  const StickyTimeDuration zeroDuration;
  StickyTimeDuration intervalStartTime =
    std::max(std::min(StickyTimeDuration(-mEffect->SpecifiedTiming().mDelay),
                      computedTiming.mActiveDuration), zeroDuration);
  StickyTimeDuration intervalEndTime =
    std::max(std::min((EffectEnd() - mEffect->SpecifiedTiming().mDelay),
                      computedTiming.mActiveDuration), zeroDuration);

  // TimeStamps to use for ordering the events when they are dispatched. We
  // use a TimeStamp so we can compare events produced by different elements,
  // perhaps even with different timelines.
  // The zero timestamp is for transitionrun events where we ignore the delay
  // for the purpose of ordering events.
  TimeStamp zeroTimeStamp   = AnimationTimeToTimeStamp(zeroDuration);
  TimeStamp startTimeStamp  = ElapsedTimeToTimeStamp(intervalStartTime);
  TimeStamp endTimeStamp    = ElapsedTimeToTimeStamp(intervalEndTime);

  TransitionPhase currentPhase;
  if (mPendingState != PendingState::NotPending &&
      (mPreviousTransitionPhase == TransitionPhase::Idle ||
       mPreviousTransitionPhase == TransitionPhase::Pending))
  {
    currentPhase = TransitionPhase::Pending;
  } else {
    currentPhase = static_cast<TransitionPhase>(computedTiming.mPhase);
  }

  AutoTArray<TransitionEventParams, 3> events;

  // Handle cancel events firts
  if (mPreviousTransitionPhase != TransitionPhase::Idle &&
      currentPhase == TransitionPhase::Idle) {
    // FIXME: bug 1264125: We will need to get active time when cancelling
    //                     the transition.
    StickyTimeDuration activeTime(0);
    TimeStamp activeTimeStamp = ElapsedTimeToTimeStamp(activeTime);
    events.AppendElement(TransitionEventParams{ eTransitionCancel,
                                                activeTime,
                                                activeTimeStamp });
  }

  // All other events
  switch (mPreviousTransitionPhase) {
    case TransitionPhase::Idle:
      if (currentPhase == TransitionPhase::Pending ||
          currentPhase == TransitionPhase::Before) {
        events.AppendElement(TransitionEventParams{ eTransitionRun,
                                                    intervalStartTime,
                                                    zeroTimeStamp });
      } else if (currentPhase == TransitionPhase::Active) {
        events.AppendElement(TransitionEventParams{ eTransitionRun,
                                                    intervalStartTime,
                                                    zeroTimeStamp });
        events.AppendElement(TransitionEventParams{ eTransitionStart,
                                                    intervalStartTime,
                                                    startTimeStamp });
      } else if (currentPhase == TransitionPhase::After) {
        events.AppendElement(TransitionEventParams{ eTransitionRun,
                                                    intervalStartTime,
                                                    zeroTimeStamp });
        events.AppendElement(TransitionEventParams{ eTransitionStart,
                                                    intervalStartTime,
                                                    startTimeStamp });
        events.AppendElement(TransitionEventParams{ eTransitionEnd,
                                                    intervalEndTime,
                                                    endTimeStamp });
      }
      break;

    case TransitionPhase::Pending:
    case TransitionPhase::Before:
      if (currentPhase == TransitionPhase::Active) {
        events.AppendElement(TransitionEventParams{ eTransitionStart,
                                                    intervalStartTime,
                                                    startTimeStamp });
      } else if (currentPhase == TransitionPhase::After) {
        events.AppendElement(TransitionEventParams{ eTransitionStart,
                                                    intervalStartTime,
                                                    startTimeStamp });
        events.AppendElement(TransitionEventParams{ eTransitionEnd,
                                                    intervalEndTime,
                                                    endTimeStamp });
      }
      break;

    case TransitionPhase::Active:
      if (currentPhase == TransitionPhase::After) {
        events.AppendElement(TransitionEventParams{ eTransitionEnd,
                                                    intervalEndTime,
                                                    endTimeStamp });
      } else if (currentPhase == TransitionPhase::Before) {
        events.AppendElement(TransitionEventParams{ eTransitionEnd,
                                                    intervalStartTime,
                                                    startTimeStamp });
      }
      break;

    case TransitionPhase::After:
      if (currentPhase == TransitionPhase::Active) {
        events.AppendElement(TransitionEventParams{ eTransitionStart,
                                                    intervalEndTime,
                                                    startTimeStamp });
      } else if (currentPhase == TransitionPhase::Before) {
        events.AppendElement(TransitionEventParams{ eTransitionStart,
                                                    intervalEndTime,
                                                    startTimeStamp });
        events.AppendElement(TransitionEventParams{ eTransitionEnd,
                                                    intervalStartTime,
                                                    endTimeStamp });
      }
      break;
  }
  mPreviousTransitionPhase = currentPhase;

  nsTransitionManager* manager = presContext->TransitionManager();
  for (const TransitionEventParams& evt : events) {
    manager->QueueEvent(TransitionEventInfo(owningElement, owningPseudoType,
                                            evt.mMessage,
                                            TransitionProperty(),
                                            evt.mElapsedTime,
                                            evt.mTimeStamp,
                                            this));
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

StyleAnimationValue
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
CSSTransition::SetEffectFromStyle(dom::AnimationEffectReadOnly* aEffect)
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

NS_IMPL_CYCLE_COLLECTION(nsTransitionManager, mEventDispatcher)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsTransitionManager, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsTransitionManager, Release)

static inline bool
ExtractNonDiscreteComputedValue(nsCSSPropertyID aProperty,
                                nsStyleContext* aStyleContext,
                                StyleAnimationValue& aComputedValue)
{
  return (nsCSSProps::kAnimTypeTable[aProperty] != eStyleAnimType_Discrete ||
          aProperty == eCSSProperty_visibility) &&
         StyleAnimationValue::ExtractComputedValue(aProperty, aStyleContext,
                                                   aComputedValue);
}

void
nsTransitionManager::StyleContextChanged(dom::Element *aElement,
                                         nsStyleContext *aOldStyleContext,
                                         RefPtr<nsStyleContext>* aNewStyleContext /* inout */)
{
  nsStyleContext* newStyleContext = *aNewStyleContext;

  NS_PRECONDITION(aOldStyleContext->GetPseudo() == newStyleContext->GetPseudo(),
                  "pseudo type mismatch");

  if (mInAnimationOnlyStyleUpdate) {
    // If we're doing an animation-only style update, return, since the
    // purpose of an animation-only style update is to update only the
    // animation styles so that we don't consider style changes
    // resulting from changes in the animation time for starting a
    // transition.
    return;
  }

  if (!mPresContext->IsDynamic()) {
    // For print or print preview, ignore transitions.
    return;
  }

  if (aOldStyleContext->HasPseudoElementData() !=
      newStyleContext->HasPseudoElementData()) {
    // If the old style context and new style context differ in terms of
    // whether they're inside ::first-letter, ::first-line, or similar,
    // bail.  We can't hit this codepath for normal style changes
    // involving moving frames around the boundaries of these
    // pseudo-elements since we don't call StyleContextChanged from
    // ReparentStyleContext.  However, we can hit this codepath during
    // the handling of transitions that start across reframes.
    //
    // While there isn't an easy *perfect* way to handle this case, err
    // on the side of missing some transitions that we ought to have
    // rather than having bogus transitions that we shouldn't.
    //
    // We could consider changing this handling, although it's worth
    // thinking about whether the code below could do anything weird in
    // this case.
    return;
  }

  // NOTE: Things in this function (and ConsiderInitiatingTransition)
  // should never call PeekStyleData because we don't preserve gotten
  // structs across reframes.

  // Return sooner (before the startedAny check below) for the most
  // common case: no transitions specified or running.
  const nsStyleDisplay *disp = newStyleContext->StyleDisplay();
  CSSPseudoElementType pseudoType = newStyleContext->GetPseudoType();
  if (pseudoType != CSSPseudoElementType::NotPseudo) {
    if (pseudoType != CSSPseudoElementType::before &&
        pseudoType != CSSPseudoElementType::after) {
      return;
    }

    NS_ASSERTION((pseudoType == CSSPseudoElementType::before &&
                  aElement->IsGeneratedContentContainerForBefore()) ||
                 (pseudoType == CSSPseudoElementType::after &&
                  aElement->IsGeneratedContentContainerForAfter()),
                 "Unexpected aElement coming through");

    // Else the element we want to use from now on is the element the
    // :before or :after is attached to.
    aElement = aElement->GetParent()->AsElement();
  }

  CSSTransitionCollection* collection =
    CSSTransitionCollection::GetAnimationCollection(aElement, pseudoType);
  if (!collection &&
      disp->mTransitionPropertyCount == 1 &&
      disp->mTransitions[0].GetCombinedDuration() <= 0.0f) {
    return;
  }

  MOZ_ASSERT(mPresContext->RestyleManager()->IsGecko(),
             "ServoRestyleManager should not use nsTransitionManager "
             "for transitions");
  if (collection &&
      collection->mCheckGeneration ==
        mPresContext->RestyleManager()->AsGecko()->GetAnimationGeneration()) {
    // When we start a new transition, we immediately post a restyle.
    // If the animation generation on the collection is current, that
    // means *this* is that restyle, since we bump the animation
    // generation on the restyle manager whenever there's a real style
    // change (i.e., one where mInAnimationOnlyStyleUpdate isn't true,
    // which causes us to return above).  Thus we shouldn't do anything.
    return;
  }
  if (newStyleContext->GetParent() &&
      newStyleContext->GetParent()->HasPseudoElementData()) {
    // Ignore transitions on things that inherit properties from
    // pseudo-elements.
    // FIXME (Bug 522599): Add tests for this.
    return;
  }

  NS_WARNING_ASSERTION(
    !mPresContext->EffectCompositor()->HasThrottledStyleUpdates(),
    "throttled animations not up to date");

  // Compute what the css-transitions spec calls the "after-change
  // style", which is the new style without any data from transitions,
  // but still inheriting from data that contains transitions that are
  // not stopping or starting right now.
  RefPtr<nsStyleContext> afterChangeStyle;
  if (collection) {
    MOZ_ASSERT(mPresContext->StyleSet()->IsGecko(),
               "ServoStyleSets should not use nsTransitionManager "
               "for transitions");
    nsStyleSet* styleSet = mPresContext->StyleSet()->AsGecko();
    afterChangeStyle =
      styleSet->ResolveStyleWithoutAnimation(aElement, newStyleContext,
                                             eRestyle_CSSTransitions);
  } else {
    afterChangeStyle = newStyleContext;
  }

  nsAutoAnimationMutationBatch mb(aElement->OwnerDoc());

  DebugOnly<bool> startedAny = false;
  // We don't have to update transitions if display:none, although we will
  // cancel them after restyling.
  if (!afterChangeStyle->IsInDisplayNoneSubtree()) {
    startedAny = UpdateTransitions(disp, aElement, collection,
                                   aOldStyleContext, afterChangeStyle);
  }

  MOZ_ASSERT(!startedAny || collection,
             "must have element transitions if we started any transitions");

  EffectCompositor::CascadeLevel cascadeLevel =
    EffectCompositor::CascadeLevel::Transitions;

  if (collection) {
    collection->UpdateCheckGeneration(mPresContext);
    mPresContext->EffectCompositor()->MaybeUpdateAnimationRule(aElement,
                                                               pseudoType,
                                                               cascadeLevel,
                                                               newStyleContext);
  }

  // We want to replace the new style context with the after-change style.
  *aNewStyleContext = afterChangeStyle;
  if (collection) {
    // Since we have transition styles, we have to undo this replacement.
    // The check of collection->mCheckGeneration against the restyle
    // manager's GetAnimationGeneration() will ensure that we don't go
    // through the rest of this function again when we do.
    mPresContext->EffectCompositor()->PostRestyleForAnimation(aElement,
                                                              pseudoType,
                                                              cascadeLevel);
  }
}

bool
nsTransitionManager::UpdateTransitions(
  const nsStyleDisplay* aDisp,
  dom::Element* aElement,
  CSSTransitionCollection*& aElementTransitions,
  nsStyleContext* aOldStyleContext,
  nsStyleContext* aNewStyleContext)
{
  MOZ_ASSERT(aDisp, "Null nsStyleDisplay");
  MOZ_ASSERT(!aElementTransitions ||
             aElementTransitions->mElement == aElement, "Element mismatch");

  // Per http://lists.w3.org/Archives/Public/www-style/2009Aug/0109.html
  // I'll consider only the transitions from the number of items in
  // 'transition-property' on down, and later ones will override earlier
  // ones (tracked using |whichStarted|).
  bool startedAny = false;
  nsCSSPropertyIDSet whichStarted;
  for (uint32_t i = aDisp->mTransitionPropertyCount; i-- != 0; ) {
    const StyleTransition& t = aDisp->mTransitions[i];
    // Check the combined duration (combination of delay and duration)
    // first, since it defaults to zero, which means we can ignore the
    // transition.
    if (t.GetCombinedDuration() > 0.0f) {
      // We might have something to transition.  See if any of the
      // properties in question changed and are animatable.
      // FIXME: Would be good to find a way to share code between this
      // interpretation of transition-property and the one below.
      nsCSSPropertyID property = t.GetProperty();
      if (property == eCSSPropertyExtra_no_properties ||
          property == eCSSPropertyExtra_variable ||
          property == eCSSProperty_UNKNOWN) {
        // Nothing to do, but need to exclude this from cases below.
      } else if (property == eCSSPropertyExtra_all_properties) {
        for (nsCSSPropertyID p = nsCSSPropertyID(0);
             p < eCSSProperty_COUNT_no_shorthands;
             p = nsCSSPropertyID(p + 1)) {
          ConsiderInitiatingTransition(p, t, aElement, aElementTransitions,
                                       aOldStyleContext, aNewStyleContext,
                                       &startedAny, &whichStarted);
        }
      } else if (nsCSSProps::IsShorthand(property)) {
        CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subprop, property,
                                             CSSEnabledState::eForAllContent)
        {
          ConsiderInitiatingTransition(*subprop, t, aElement,
                                       aElementTransitions,
                                       aOldStyleContext, aNewStyleContext,
                                       &startedAny, &whichStarted);
        }
      } else {
        ConsiderInitiatingTransition(property, t, aElement, aElementTransitions,
                                     aOldStyleContext, aNewStyleContext,
                                     &startedAny, &whichStarted);
      }
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
      aDisp->mTransitions[0].GetProperty() != eCSSPropertyExtra_all_properties;
    nsCSSPropertyIDSet allTransitionProperties;
    if (checkProperties) {
      for (uint32_t i = aDisp->mTransitionPropertyCount; i-- != 0; ) {
        const StyleTransition& t = aDisp->mTransitions[i];
        // FIXME: Would be good to find a way to share code between this
        // interpretation of transition-property and the one above.
        nsCSSPropertyID property = t.GetProperty();
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
    StyleAnimationValue currentValue;
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
                                           aNewStyleContext, currentValue) ||
          currentValue != anim->ToValue()) {
        // stop the transition
        if (anim->HasCurrentEffect()) {
          EffectSet* effectSet =
            EffectSet::GetEffectSet(aElement,
                                    aNewStyleContext->GetPseudoType());
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

void
nsTransitionManager::ConsiderInitiatingTransition(
  nsCSSPropertyID aProperty,
  const StyleTransition& aTransition,
  dom::Element* aElement,
  CSSTransitionCollection*& aElementTransitions,
  nsStyleContext* aOldStyleContext,
  nsStyleContext* aNewStyleContext,
  bool* aStartedAny,
  nsCSSPropertyIDSet* aWhichStarted)
{
  // IsShorthand itself will assert if aProperty is not a property.
  MOZ_ASSERT(!nsCSSProps::IsShorthand(aProperty),
             "property out of range");
  NS_ASSERTION(!aElementTransitions ||
               aElementTransitions->mElement == aElement, "Element mismatch");

  // Ignore disabled properties. We can arrive here if the transition-property
  // is 'all' and the disabled property has a default value which derives value
  // from another property, e.g. color.
  if (!nsCSSProps::IsEnabled(aProperty, CSSEnabledState::eForAllContent)) {
    return;
  }

  if (aWhichStarted->HasProperty(aProperty)) {
    // A later item in transition-property already started a
    // transition for this property, so we ignore this one.
    // See comment above and
    // http://lists.w3.org/Archives/Public/www-style/2009Aug/0109.html .
    return;
  }

  if (nsCSSProps::kAnimTypeTable[aProperty] == eStyleAnimType_None) {
    return;
  }

  dom::DocumentTimeline* timeline = aElement->OwnerDoc()->Timeline();

  StyleAnimationValue startValue, endValue, dummyValue;
  bool haveValues =
    ExtractNonDiscreteComputedValue(aProperty, aOldStyleContext, startValue) &&
    ExtractNonDiscreteComputedValue(aProperty, aNewStyleContext, endValue);

  bool haveChange = startValue != endValue;

  bool shouldAnimate =
    haveValues &&
    haveChange &&
    // Check that we can interpolate between these values
    // (If this is ever a performance problem, we could add a
    // CanInterpolate method, but it seems fine for now.)
    StyleAnimationValue::Interpolate(aProperty, startValue, endValue,
                                     0.5, dummyValue);

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
    return;
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
      EffectSet* effectSet =
        EffectSet::GetEffectSet(aElement, aNewStyleContext->GetPseudoType());
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
    return;
  }

  const nsTimingFunction &tf = aTransition.GetTimingFunction();
  float delay = aTransition.GetDelay();
  float duration = aTransition.GetDuration();
  if (duration < 0.0) {
    // The spec says a negative duration is treated as zero.
    duration = 0.0;
  }

  StyleAnimationValue startForReversingTest = startValue;
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

  TimingParams timing;
  timing.mDuration.emplace(StickyTimeDuration::FromMilliseconds(duration));
  timing.mDelay = TimeDuration::FromMilliseconds(delay);
  timing.mIterations = 1.0;
  timing.mDirection = dom::PlaybackDirection::Normal;
  timing.mFill = dom::FillMode::Backwards;

  // aElement is non-null here, so we emplace it directly.
  Maybe<OwningAnimationTarget> target;
  target.emplace(aElement, aNewStyleContext->GetPseudoType());
  KeyframeEffectParams effectOptions;
  RefPtr<ElementPropertyTransition> pt =
    new ElementPropertyTransition(aElement->OwnerDoc(), target, timing,
                                  startForReversingTest, reversePortion,
                                  effectOptions);

  pt->SetKeyframes(GetTransitionKeyframes(aNewStyleContext, aProperty,
                                          Move(startValue), Move(endValue), tf),
                   aNewStyleContext);

  MOZ_ASSERT(mPresContext->RestyleManager()->IsGecko(),
             "ServoRestyleManager should not use nsTransitionManager "
             "for transitions");

  RefPtr<CSSTransition> animation =
    new CSSTransition(mPresContext->Document()->GetScopeObject());
  animation->SetOwningElement(
    OwningElementRef(*aElement, aNewStyleContext->GetPseudoType()));
  animation->SetTimelineNoUpdate(timeline);
  animation->SetCreationSequence(
    mPresContext->RestyleManager()->AsGecko()->GetAnimationGeneration());
  animation->SetEffectFromStyle(pt);
  animation->PlayFromStyle();

  if (!aElementTransitions) {
    bool createdCollection = false;
    aElementTransitions =
      CSSTransitionCollection::GetOrCreateAnimationCollection(
        aElement, aNewStyleContext->GetPseudoType(), &createdCollection);
    if (!aElementTransitions) {
      MOZ_ASSERT(!createdCollection, "outparam should agree with return value");
      NS_WARNING("allocating collection failed");
      return;
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
      return;
    }
  }

  EffectSet* effectSet =
    EffectSet::GetEffectSet(aElement, aNewStyleContext->GetPseudoType());
  if (effectSet) {
    effectSet->UpdateAnimationGeneration(mPresContext);
  }

  *aStartedAny = true;
  aWhichStarted->AddProperty(aProperty);
}

static Keyframe&
AppendKeyframe(double aOffset, nsCSSPropertyID aProperty,
               StyleAnimationValue&& aValue, nsTArray<Keyframe>& aKeyframes)
{
  Keyframe& frame = *aKeyframes.AppendElement();
  frame.mOffset.emplace(aOffset);
  PropertyValuePair& pv = *frame.mPropertyValues.AppendElement();
  pv.mProperty = aProperty;
  DebugOnly<bool> uncomputeResult =
    StyleAnimationValue::UncomputeValue(aProperty, Move(aValue), pv.mValue);
  MOZ_ASSERT(uncomputeResult,
              "Unable to get specified value from computed value");
  return frame;
}

nsTArray<Keyframe>
nsTransitionManager::GetTransitionKeyframes(
    nsStyleContext* aStyleContext,
    nsCSSPropertyID aProperty,
    StyleAnimationValue&& aStartValue,
    StyleAnimationValue&& aEndValue,
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

void
nsTransitionManager::PruneCompletedTransitions(mozilla::dom::Element* aElement,
                                               CSSPseudoElementType aPseudoType,
                                               nsStyleContext* aNewStyleContext)
{
  CSSTransitionCollection* collection =
    CSSTransitionCollection::GetAnimationCollection(aElement, aPseudoType);
  if (!collection) {
    return;
  }

  // Remove any finished transitions whose style doesn't match the new
  // style.
  // This is similar to some of the work that happens near the end of
  // nsTransitionManager::StyleContextChanged.
  // FIXME (bug 1158431): Really, we should also cancel running
  // transitions whose destination doesn't match as well.
  OwningCSSTransitionPtrArray& animations = collection->mAnimations;
  size_t i = animations.Length();
  MOZ_ASSERT(i != 0, "empty transitions list?");
  do {
    --i;
    CSSTransition* anim = animations[i];

    if (anim->HasCurrentEffect()) {
      continue;
    }

    // Since effect is a finished transition, we know it didn't
    // influence style.
    StyleAnimationValue currentValue;
    if (!ExtractNonDiscreteComputedValue(anim->TransitionProperty(),
                                         aNewStyleContext, currentValue) ||
        currentValue != anim->ToValue()) {
      anim->CancelFromStyle();
      animations.RemoveElementAt(i);
    }
  } while (i != 0);

  if (collection->mAnimations.IsEmpty()) {
    collection->Destroy();
    // |collection| is now a dangling pointer!
    collection = nullptr;
  }
}

void
nsTransitionManager::StopTransitionsForElement(
  mozilla::dom::Element* aElement,
  mozilla::CSSPseudoElementType aPseudoType)
{
  MOZ_ASSERT(aElement);
  CSSTransitionCollection* collection =
    CSSTransitionCollection::GetAnimationCollection(aElement, aPseudoType);
  if (!collection) {
    return;
  }

  nsAutoAnimationMutationBatch mb(aElement->OwnerDoc());
  collection->Destroy();
}
