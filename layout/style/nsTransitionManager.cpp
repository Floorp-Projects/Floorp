/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Code to start and animate CSS transitions. */

#include "nsTransitionManager.h"
#include "nsAnimationManager.h"
#include "nsIContent.h"
#include "nsStyleContext.h"
#include "nsCSSProps.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"
#include "nsRefreshDriver.h"
#include "nsRuleProcessorData.h"
#include "nsRuleWalker.h"
#include "nsCSSPropertySet.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/Element.h"
#include "nsIFrame.h"
#include "Layers.h"
#include "FrameLayerBuilder.h"
#include "nsDisplayList.h"
#include "nsStyleChangeList.h"
#include "nsStyleSet.h"
#include "RestyleManager.h"

using mozilla::TimeStamp;
using mozilla::TimeDuration;

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::css;

double
ElementPropertyTransition::CurrentValuePortion() const
{
  // It would be easy enough to handle finished transitions by using a time
  // fraction of 1 but currently we should not be called for finished
  // transitions.
  MOZ_ASSERT(!IsFinishedTransition(),
             "Getting the value portion of a finished transition");
  MOZ_ASSERT(!GetLocalTime().IsNull(),
             "Getting the value portion of an animation that's not being "
             "sampled");

  // Transitions use a fill mode of 'backwards' so GetComputedTiming will
  // never return a null time fraction due to being *before* the animation
  // interval. However, it might be possible that we're behind on flushing
  // causing us to get called *after* the animation interval. So, just in
  // case, we override the fill mode to 'both' to ensure the time fraction
  // is never null.
  AnimationTiming timingToUse = mTiming;
  timingToUse.mFillMode = NS_STYLE_ANIMATION_FILL_MODE_BOTH;
  ComputedTiming computedTiming = GetComputedTiming(timingToUse);

  MOZ_ASSERT(computedTiming.mTimeFraction != ComputedTiming::kNullTimeFraction,
             "Got a null time fraction for a fill mode of 'both'");
  MOZ_ASSERT(mProperties.Length() == 1,
             "Should have one animation property for a transition");
  MOZ_ASSERT(mProperties[0].mSegments.Length() == 1,
             "Animation property should have one segment for a transition");
  return mProperties[0].mSegments[0].mTimingFunction
         .GetValue(computedTiming.mTimeFraction);
}

/*****************************************************************************
 * nsTransitionManager                                                       *
 *****************************************************************************/

void
nsTransitionManager::UpdateThrottledStylesForSubtree(nsIContent* aContent,
                                                     nsStyleContext* aParentStyle,
                                                     nsStyleChangeList& aChangeList)
{
  dom::Element* element;
  if (aContent->IsElement()) {
    element = aContent->AsElement();
  } else {
    element = nullptr;
  }

  nsRefPtr<nsStyleContext> newStyle;

  ElementAnimationCollection* collection;
  if (element &&
      (collection =
        GetElementTransitions(element,
                              nsCSSPseudoElements::ePseudo_NotPseudoElement,
                              false))) {
    // re-resolve our style
    newStyle = UpdateThrottledStyle(element, aParentStyle, aChangeList);
    // remove the current transition from the working set
    collection->mFlushGeneration =
      mPresContext->RefreshDriver()->MostRecentRefresh();
  } else {
    newStyle = ReparentContent(aContent, aParentStyle);
  }

  // walk the children
  if (newStyle) {
    for (nsIContent *child = aContent->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      UpdateThrottledStylesForSubtree(child, newStyle, aChangeList);
    }
  }
}

IMPL_UPDATE_ALL_THROTTLED_STYLES_INTERNAL(nsTransitionManager,
                                          GetElementTransitions)

void
nsTransitionManager::UpdateAllThrottledStyles()
{
  if (PR_CLIST_IS_EMPTY(&mElementCollections)) {
    // no throttled transitions, leave early
    mPresContext->TickLastUpdateThrottledTransitionStyle();
    return;
  }

  if (mPresContext->ThrottledTransitionStyleIsUpToDate()) {
    // throttled transitions are up to date, leave early
    return;
  }

  mPresContext->TickLastUpdateThrottledTransitionStyle();
  UpdateAllThrottledStylesInternal();
}

void
nsTransitionManager::ElementCollectionRemoved()
{
  // If we have no transitions or animations left, remove ourselves from
  // the refresh driver.
  if (PR_CLIST_IS_EMPTY(&mElementCollections)) {
    mPresContext->RefreshDriver()->RemoveRefreshObserver(this, Flush_Style);
  }
}

void
nsTransitionManager::AddElementCollection(
  ElementAnimationCollection* aCollection)
{
  if (PR_CLIST_IS_EMPTY(&mElementCollections)) {
    // We need to observe the refresh driver.
    nsRefreshDriver *rd = mPresContext->RefreshDriver();
    rd->AddRefreshObserver(this, Flush_Style);
  }

  PR_INSERT_BEFORE(aCollection, &mElementCollections);
}

already_AddRefed<nsIStyleRule>
nsTransitionManager::StyleContextChanged(dom::Element *aElement,
                                         nsStyleContext *aOldStyleContext,
                                         nsStyleContext *aNewStyleContext)
{
  NS_PRECONDITION(aOldStyleContext->GetPseudo() ==
                      aNewStyleContext->GetPseudo(),
                  "pseudo type mismatch");
  // If we were called from ReparentStyleContext, this assertion would
  // actually fire.  If we need to be called from there, we can probably
  // just remove it; the condition probably isn't critical, although
  // it's worth thinking about some more.
  NS_PRECONDITION(aOldStyleContext->HasPseudoElementData() ==
                      aNewStyleContext->HasPseudoElementData(),
                  "pseudo type mismatch");

  if (!mPresContext->IsDynamic()) {
    // For print or print preview, ignore transitions.
    return nullptr;
  }

  // NOTE: Things in this function (and ConsiderStartingTransition)
  // should never call PeekStyleData because we don't preserve gotten
  // structs across reframes.

  // Return sooner (before the startedAny check below) for the most
  // common case: no transitions specified or running.
  const nsStyleDisplay *disp = aNewStyleContext->StyleDisplay();
  nsCSSPseudoElements::Type pseudoType = aNewStyleContext->GetPseudoType();
  if (pseudoType != nsCSSPseudoElements::ePseudo_NotPseudoElement) {
    if (pseudoType != nsCSSPseudoElements::ePseudo_before &&
        pseudoType != nsCSSPseudoElements::ePseudo_after) {
      return nullptr;
    }

    NS_ASSERTION((pseudoType == nsCSSPseudoElements::ePseudo_before &&
                  aElement->Tag() == nsGkAtoms::mozgeneratedcontentbefore) ||
                 (pseudoType == nsCSSPseudoElements::ePseudo_after &&
                  aElement->Tag() == nsGkAtoms::mozgeneratedcontentafter),
                 "Unexpected aElement coming through");

    // Else the element we want to use from now on is the element the
    // :before or :after is attached to.
    aElement = aElement->GetParent()->AsElement();
  }

  ElementAnimationCollection* collection =
    GetElementTransitions(aElement, pseudoType, false);
  if (!collection &&
      disp->mTransitionPropertyCount == 1 &&
      disp->mTransitions[0].GetDelay() == 0.0f &&
      disp->mTransitions[0].GetDuration() == 0.0f) {
    return nullptr;
  }


  if (aNewStyleContext->PresContext()->IsProcessingAnimationStyleChange()) {
    return nullptr;
  }

  if (aNewStyleContext->GetParent() &&
      aNewStyleContext->GetParent()->HasPseudoElementData()) {
    // Ignore transitions on things that inherit properties from
    // pseudo-elements.
    // FIXME (Bug 522599): Add tests for this.
    return nullptr;
  }

  NS_WARN_IF_FALSE(!nsLayoutUtils::AreAsyncAnimationsEnabled() ||
                     mPresContext->ThrottledTransitionStyleIsUpToDate(),
                   "throttled animations not up to date");

  // Per http://lists.w3.org/Archives/Public/www-style/2009Aug/0109.html
  // I'll consider only the transitions from the number of items in
  // 'transition-property' on down, and later ones will override earlier
  // ones (tracked using |whichStarted|).
  bool startedAny = false;
  nsCSSPropertySet whichStarted;
  for (uint32_t i = disp->mTransitionPropertyCount; i-- != 0; ) {
    const StyleTransition& t = disp->mTransitions[i];
    // Check delay and duration first, since they default to zero, and
    // when they're both zero, we can ignore the transition.
    if (t.GetDelay() != 0.0f || t.GetDuration() != 0.0f) {
      // We might have something to transition.  See if any of the
      // properties in question changed and are animatable.
      // FIXME: Would be good to find a way to share code between this
      // interpretation of transition-property and the one below.
      nsCSSProperty property = t.GetProperty();
      if (property == eCSSPropertyExtra_no_properties ||
          property == eCSSPropertyExtra_variable ||
          property == eCSSProperty_UNKNOWN) {
        // Nothing to do, but need to exclude this from cases below.
      } else if (property == eCSSPropertyExtra_all_properties) {
        for (nsCSSProperty p = nsCSSProperty(0);
             p < eCSSProperty_COUNT_no_shorthands;
             p = nsCSSProperty(p + 1)) {
          ConsiderStartingTransition(p, t, aElement, collection,
                                     aOldStyleContext, aNewStyleContext,
                                     &startedAny, &whichStarted);
        }
      } else if (nsCSSProps::IsShorthand(property)) {
        CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subprop, property) {
          ConsiderStartingTransition(*subprop, t, aElement, collection,
                                     aOldStyleContext, aNewStyleContext,
                                     &startedAny, &whichStarted);
        }
      } else {
        ConsiderStartingTransition(property, t, aElement, collection,
                                   aOldStyleContext, aNewStyleContext,
                                   &startedAny, &whichStarted);
      }
    }
  }

  // Stop any transitions for properties that are no longer in
  // 'transition-property'.
  // Also stop any transitions for properties that just changed (and are
  // still in the set of properties to transition), but we didn't just
  // start the transition because delay and duration are both zero.
  if (collection) {
    bool checkProperties =
      disp->mTransitions[0].GetProperty() != eCSSPropertyExtra_all_properties;
    nsCSSPropertySet allTransitionProperties;
    if (checkProperties) {
      for (uint32_t i = disp->mTransitionPropertyCount; i-- != 0; ) {
        const StyleTransition& t = disp->mTransitions[i];
        // FIXME: Would be good to find a way to share code between this
        // interpretation of transition-property and the one above.
        nsCSSProperty property = t.GetProperty();
        if (property == eCSSPropertyExtra_no_properties ||
            property == eCSSPropertyExtra_variable ||
            property == eCSSProperty_UNKNOWN) {
          // Nothing to do, but need to exclude this from cases below.
        } else if (property == eCSSPropertyExtra_all_properties) {
          for (nsCSSProperty p = nsCSSProperty(0);
               p < eCSSProperty_COUNT_no_shorthands;
               p = nsCSSProperty(p + 1)) {
            allTransitionProperties.AddProperty(p);
          }
        } else if (nsCSSProps::IsShorthand(property)) {
          CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subprop, property) {
            allTransitionProperties.AddProperty(*subprop);
          }
        } else {
          allTransitionProperties.AddProperty(property);
        }
      }
    }

    ElementAnimationPtrArray& animations = collection->mAnimations;
    uint32_t i = animations.Length();
    NS_ABORT_IF_FALSE(i != 0, "empty transitions list?");
    StyleAnimationValue currentValue;
    do {
      --i;
      ElementAnimation* animation = animations[i];
      MOZ_ASSERT(animation->mProperties.Length() == 1,
                 "Should have one animation property for a transition");
      MOZ_ASSERT(animation->mProperties[0].mSegments.Length() == 1,
                 "Animation property should have one segment for a transition");
      const AnimationProperty& prop = animation->mProperties[0];
      const AnimationPropertySegment& segment = prop.mSegments[0];
          // properties no longer in 'transition-property'
      if ((checkProperties &&
           !allTransitionProperties.HasProperty(prop.mProperty)) ||
          // properties whose computed values changed but delay and
          // duration are both zero
          !ExtractComputedValueForTransition(prop.mProperty, aNewStyleContext,
                                             currentValue) ||
          currentValue != segment.mToValue) {
        // stop the transition
        animations.RemoveElementAt(i);
        collection->UpdateAnimationGeneration(mPresContext);
      }
    } while (i != 0);

    if (animations.IsEmpty()) {
      collection->Destroy();
      collection = nullptr;
    }
  }

  if (!startedAny) {
    return nullptr;
  }

  NS_ABORT_IF_FALSE(collection, "must have element transitions if we started "
                                "any transitions");

  // In the CSS working group discussion (2009 Jul 15 telecon,
  // http://www.w3.org/mid/4A5E1470.4030904@inkedblade.net ) of
  // http://lists.w3.org/Archives/Public/www-style/2009Jun/0121.html ,
  // the working group decided that a transition property on an
  // element should not cause any transitions if the property change
  // is itself inheriting a value that is transitioning on an
  // ancestor.  So, to get the correct behavior, we continue the
  // restyle that caused this transition using a "covering" rule that
  // covers up any changes on which we started transitions, so that
  // descendants don't start their own transitions.  (In the case of
  // negative transition delay, this covering rule produces different
  // results than applying the transition rule immediately would).
  // Our caller is responsible for restyling again using this covering
  // rule.

  nsRefPtr<css::AnimValuesStyleRule> coverRule = new css::AnimValuesStyleRule;

  ElementAnimationPtrArray& animations = collection->mAnimations;
  for (uint32_t i = 0, i_end = animations.Length(); i < i_end; ++i) {
    ElementAnimation* animation = animations[i];
    MOZ_ASSERT(animation->mProperties.Length() == 1,
               "Should have one animation property for a transition");
    MOZ_ASSERT(animation->mProperties[0].mSegments.Length() == 1,
               "Animation property should have one segment for a transition");
    AnimationProperty& prop = animation->mProperties[0];
    AnimationPropertySegment& segment = prop.mSegments[0];
    if (whichStarted.HasProperty(prop.mProperty)) {
      coverRule->AddValue(prop.mProperty, segment.mFromValue);
    }
  }

  collection->mStyleRule = nullptr;

  return coverRule.forget();
}

void
nsTransitionManager::ConsiderStartingTransition(
  nsCSSProperty aProperty,
  const StyleTransition& aTransition,
  dom::Element* aElement,
  ElementAnimationCollection*& aElementTransitions,
  nsStyleContext* aOldStyleContext,
  nsStyleContext* aNewStyleContext,
  bool* aStartedAny,
  nsCSSPropertySet* aWhichStarted)
{
  // IsShorthand itself will assert if aProperty is not a property.
  NS_ABORT_IF_FALSE(!nsCSSProps::IsShorthand(aProperty),
                    "property out of range");
  NS_ASSERTION(!aElementTransitions ||
               aElementTransitions->mElement == aElement, "Element mismatch");

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

  dom::AnimationTimeline* timeline = aElement->OwnerDoc()->Timeline();
  nsRefPtr<ElementPropertyTransition> pt =
    new ElementPropertyTransition(timeline);

  StyleAnimationValue startValue, endValue, dummyValue;
  bool haveValues =
    ExtractComputedValueForTransition(aProperty, aOldStyleContext,
                                      startValue) &&
    ExtractComputedValueForTransition(aProperty, aNewStyleContext,
                                      endValue);

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
    ElementAnimationPtrArray& animations = aElementTransitions->mAnimations;
    for (size_t i = 0, i_end = animations.Length(); i < i_end; ++i) {
      MOZ_ASSERT(animations[i]->mProperties.Length() == 1,
                 "Should have one animation property for a transition");
      if (animations[i]->mProperties[0].mProperty == aProperty) {
        haveCurrentTransition = true;
        currentIndex = i;
        oldPT =
          aElementTransitions->mAnimations[currentIndex]->AsTransition();
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
  MOZ_ASSERT(!oldPT || oldPT->mProperties[0].mSegments.Length() == 1,
             "Should have one animation property segment for a transition");
  if (haveCurrentTransition && haveValues &&
      oldPT->mProperties[0].mSegments[0].mToValue == endValue) {
    // WalkTransitionRule already called RestyleForAnimation.
    return;
  }

  nsPresContext *presContext = aNewStyleContext->PresContext();

  if (!shouldAnimate) {
    if (haveCurrentTransition) {
      // We're in the middle of a transition, and just got a non-transition
      // style change to something that we can't animate.  This might happen
      // because we got a non-transition style change changing to the current
      // in-progress value (which is particularly easy to cause when we're
      // currently in the 'transition-delay').  It also might happen because we
      // just got a style change to a value that can't be interpolated.
      ElementAnimationPtrArray& animations = aElementTransitions->mAnimations;
      animations.RemoveElementAt(currentIndex);
      aElementTransitions->UpdateAnimationGeneration(mPresContext);

      if (animations.IsEmpty()) {
        aElementTransitions->Destroy();
        // |aElementTransitions| is now a dangling pointer!
        aElementTransitions = nullptr;
      }
      // WalkTransitionRule already called RestyleForAnimation.
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
  pt->mStartForReversingTest = startValue;
  pt->mReversePortion = 1.0;

  // If the new transition reverses an existing one, we'll need to
  // handle the timing differently.
  if (haveCurrentTransition &&
      !oldPT->IsFinishedTransition() &&
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

    pt->mStartForReversingTest = oldPT->mProperties[0].mSegments[0].mToValue;
    pt->mReversePortion = valuePortion;
  }

  AnimationProperty& prop = *pt->mProperties.AppendElement();
  prop.mProperty = aProperty;

  AnimationPropertySegment& segment = *prop.mSegments.AppendElement();
  segment.mFromValue = startValue;
  segment.mToValue = endValue;
  segment.mFromKey = 0;
  segment.mToKey = 1;
  segment.mTimingFunction.Init(tf);

  pt->mStartTime = timeline->GetCurrentTimeStamp();
  pt->mTiming.mIterationDuration = TimeDuration::FromMilliseconds(duration);
  pt->mTiming.mDelay = TimeDuration::FromMilliseconds(delay);
  pt->mTiming.mIterationCount = 1;
  pt->mTiming.mDirection = NS_STYLE_ANIMATION_DIRECTION_NORMAL;
  pt->mTiming.mFillMode = NS_STYLE_ANIMATION_FILL_MODE_BACKWARDS;
  pt->mPlayState = NS_STYLE_ANIMATION_PLAY_STATE_RUNNING;
  pt->mPauseStart = TimeStamp();

  if (!aElementTransitions) {
    aElementTransitions =
      GetElementTransitions(aElement, aNewStyleContext->GetPseudoType(),
                            true);
    if (!aElementTransitions) {
      NS_WARNING("allocating CommonAnimationManager failed");
      return;
    }
  }

  ElementAnimationPtrArray &animations = aElementTransitions->mAnimations;
#ifdef DEBUG
  for (uint32_t i = 0, i_end = animations.Length(); i < i_end; ++i) {
    NS_ABORT_IF_FALSE(animations[i]->mProperties.Length() == 1,
                      "Should have one animation property for a transition");
    NS_ABORT_IF_FALSE(i == currentIndex ||
                      animations[i]->mProperties[0].mProperty != aProperty,
                      "duplicate transitions for property");
  }
#endif
  if (haveCurrentTransition) {
    animations[currentIndex] = pt;
  } else {
    if (!animations.AppendElement(pt)) {
      NS_WARNING("out of memory");
      return;
    }
  }
  aElementTransitions->UpdateAnimationGeneration(mPresContext);
  aElementTransitions->PostRestyleForAnimation(presContext);

  *aStartedAny = true;
  aWhichStarted->AddProperty(aProperty);
}

ElementAnimationCollection*
nsTransitionManager::GetElementTransitions(
  dom::Element *aElement,
  nsCSSPseudoElements::Type aPseudoType,
  bool aCreateIfNeeded)
{
  if (!aCreateIfNeeded && PR_CLIST_IS_EMPTY(&mElementCollections)) {
    // Early return for the most common case.
    return nullptr;
  }

  nsIAtom *propName;
  if (aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement) {
    propName = nsGkAtoms::transitionsProperty;
  } else if (aPseudoType == nsCSSPseudoElements::ePseudo_before) {
    propName = nsGkAtoms::transitionsOfBeforeProperty;
  } else if (aPseudoType == nsCSSPseudoElements::ePseudo_after) {
    propName = nsGkAtoms::transitionsOfAfterProperty;
  } else {
    NS_ASSERTION(!aCreateIfNeeded,
                 "should never try to create transitions for pseudo "
                 "other than :before or :after");
    return nullptr;
  }
  ElementAnimationCollection* collection =
    static_cast<ElementAnimationCollection*>(aElement->GetProperty(propName));
  if (!collection && aCreateIfNeeded) {
    // FIXME: Consider arena-allocating?
    collection = new ElementAnimationCollection(aElement, propName, this,
      mPresContext->RefreshDriver()->MostRecentRefresh());
    nsresult rv =
      aElement->SetProperty(propName, collection,
                            &ElementAnimationCollection::PropertyDtor, false);
    if (NS_FAILED(rv)) {
      NS_WARNING("SetProperty failed");
      delete collection;
      return nullptr;
    }
    if (propName == nsGkAtoms::transitionsProperty) {
      aElement->SetMayHaveAnimations();
    }

    AddElementCollection(collection);
  }

  return collection;
}

/*
 * nsIStyleRuleProcessor implementation
 */

void
nsTransitionManager::WalkTransitionRule(
  ElementDependentRuleProcessorData* aData,
  nsCSSPseudoElements::Type aPseudoType)
{
  ElementAnimationCollection* collection =
    GetElementTransitions(aData->mElement, aPseudoType, false);
  if (!collection) {
    return;
  }

  if (!mPresContext->IsDynamic()) {
    // For print or print preview, ignore animations.
    return;
  }

  if (aData->mPresContext->IsProcessingRestyles() &&
      !aData->mPresContext->IsProcessingAnimationStyleChange()) {
    // If we're processing a normal style change rather than one from
    // animation, don't add the transition rule.  This allows us to
    // compute the new style value rather than having the transition
    // override it, so that we can start transitioning differently.

    // We need to immediately restyle with animation
    // after doing this.
    collection->PostRestyleForAnimation(mPresContext);
    return;
  }

  collection->mNeedsRefreshes = true;
  collection->EnsureStyleRuleFor(
    aData->mPresContext->RefreshDriver()->MostRecentRefresh(),
    EnsureStyleRule_IsNotThrottled);

  if (collection->mStyleRule) {
    aData->mRuleWalker->Forward(collection->mStyleRule);
  }
}

/* virtual */ void
nsTransitionManager::RulesMatching(ElementRuleProcessorData* aData)
{
  NS_ABORT_IF_FALSE(aData->mPresContext == mPresContext,
                    "pres context mismatch");
  WalkTransitionRule(aData,
                     nsCSSPseudoElements::ePseudo_NotPseudoElement);
}

/* virtual */ void
nsTransitionManager::RulesMatching(PseudoElementRuleProcessorData* aData)
{
  NS_ABORT_IF_FALSE(aData->mPresContext == mPresContext,
                    "pres context mismatch");

  // Note:  If we're the only thing keeping a pseudo-element frame alive
  // (per ProbePseudoStyleContext), we still want to keep it alive, so
  // this is ok.
  WalkTransitionRule(aData, aData->mPseudoType);
}

/* virtual */ void
nsTransitionManager::RulesMatching(AnonBoxRuleProcessorData* aData)
{
}

#ifdef MOZ_XUL
/* virtual */ void
nsTransitionManager::RulesMatching(XULTreeRuleProcessorData* aData)
{
}
#endif

/* virtual */ size_t
nsTransitionManager::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return CommonAnimationManager::SizeOfExcludingThis(aMallocSizeOf);
}

/* virtual */ size_t
nsTransitionManager::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

struct TransitionEventInfo {
  nsCOMPtr<nsIContent> mElement;
  InternalTransitionEvent mEvent;

  TransitionEventInfo(nsIContent *aElement, nsCSSProperty aProperty,
                      TimeDuration aDuration, const nsAString& aPseudoElement)
    : mElement(aElement)
    , mEvent(true, NS_TRANSITION_END)
  {
    // XXX Looks like nobody initialize WidgetEvent::time
    mEvent.propertyName =
      NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(aProperty));
    mEvent.elapsedTime = aDuration.ToSeconds();
    mEvent.pseudoElement = aPseudoElement;
  }

  // InternalTransitionEvent doesn't support copy-construction, so we need
  // to ourselves in order to work with nsTArray
  TransitionEventInfo(const TransitionEventInfo &aOther)
    : mElement(aOther.mElement)
    , mEvent(true, NS_TRANSITION_END)
  {
    mEvent.AssignTransitionEventData(aOther.mEvent, false);
  }
};

/* virtual */ void
nsTransitionManager::WillRefresh(mozilla::TimeStamp aTime)
{
  NS_ABORT_IF_FALSE(mPresContext,
                    "refresh driver should not notify additional observers "
                    "after pres context has been destroyed");
  if (!mPresContext->GetPresShell()) {
    // Someone might be keeping mPresContext alive past the point
    // where it has been torn down; don't bother doing anything in
    // this case.  But do get rid of all our transitions so we stop
    // triggering refreshes.
    RemoveAllElementCollections();
    return;
  }

  FlushTransitions(Can_Throttle);
}

void
nsTransitionManager::FlushTransitions(FlushFlags aFlags)
{
  if (PR_CLIST_IS_EMPTY(&mElementCollections)) {
    // no transitions, leave early
    return;
  }

  nsTArray<TransitionEventInfo> events;
  TimeStamp now = mPresContext->RefreshDriver()->MostRecentRefresh();
  bool didThrottle = false;
  // Trim transitions that have completed, post restyle events for frames that
  // are still transitioning, and start transitions with delays.
  {
    PRCList *next = PR_LIST_HEAD(&mElementCollections);
    while (next != &mElementCollections) {
      ElementAnimationCollection* collection =
        static_cast<ElementAnimationCollection*>(next);
      next = PR_NEXT_LINK(next);

      bool canThrottleTick = aFlags == Can_Throttle &&
        collection->CanPerformOnCompositorThread(
          ElementAnimationCollection::CanAnimateFlags(0)) &&
        collection->CanThrottleAnimation(now);

      NS_ABORT_IF_FALSE(collection->mElement->GetCrossShadowCurrentDoc() ==
                          mPresContext->Document(),
                        "Element::UnbindFromTree should have "
                        "destroyed the element transitions object");

      uint32_t i = collection->mAnimations.Length();
      NS_ABORT_IF_FALSE(i != 0, "empty transitions list?");
      bool transitionStartedOrEnded = false;
      do {
        --i;
        ElementAnimation* anim = collection->mAnimations[i];
        if (anim->IsFinishedTransition()) {
          // Actually remove transitions one throttle-able cycle after their
          // completion. We only clear on a throttle-able cycle because that
          // means it is a regular restyle tick and thus it is safe to discard
          // the transition. If the flush is not throttle-able, we might still
          // have new transitions left to process. See comment below.
          if (aFlags == Can_Throttle) {
            collection->mAnimations.RemoveElementAt(i);
          }
        } else {
          ComputedTiming computedTiming =
            anim->GetComputedTiming(anim->mTiming);
          if (computedTiming.mPhase == ComputedTiming::AnimationPhase_After) {
            MOZ_ASSERT(anim->mProperties.Length() == 1,
                       "Should have one animation property for a transition");
            nsCSSProperty prop = anim->mProperties[0].mProperty;
            if (nsCSSProps::PropHasFlags(prop, CSS_PROPERTY_REPORT_OTHER_NAME))
            {
              prop = nsCSSProps::OtherNameFor(prop);
            }
            events.AppendElement(
              TransitionEventInfo(collection->mElement, prop,
                                  anim->mTiming.mIterationDuration,
                                  collection->PseudoElement()));

            // Leave this transition in the list for one more refresh
            // cycle, since we haven't yet processed its style change, and
            // if we also have (already, or will have from processing
            // transitionend events or other refresh driver notifications)
            // a non-animation style change that would affect it, we need
            // to know not to start a new transition for the transition
            // from the almost-completed value to the final value.
            anim->SetFinishedTransition();
            collection->UpdateAnimationGeneration(mPresContext);
            transitionStartedOrEnded = true;
          } else if ((computedTiming.mPhase ==
                      ComputedTiming::AnimationPhase_Active) &&
                     canThrottleTick &&
                    !anim->mIsRunningOnCompositor) {
            // Start a transition with a delay where we should start the
            // transition proper.
            collection->UpdateAnimationGeneration(mPresContext);
            transitionStartedOrEnded = true;
          }
        }
      } while (i != 0);

      // We need to restyle even if the transition rule no longer
      // applies (in which case we just made it not apply).
      MOZ_ASSERT(collection->mElementProperty ==
                   nsGkAtoms::transitionsProperty ||
                 collection->mElementProperty ==
                   nsGkAtoms::transitionsOfBeforeProperty ||
                 collection->mElementProperty ==
                   nsGkAtoms::transitionsOfAfterProperty,
                 "Unexpected element property; might restyle too much");
      if (!canThrottleTick || transitionStartedOrEnded) {
        collection->PostRestyleForAnimation(mPresContext);
      } else {
        didThrottle = true;
      }

      if (collection->mAnimations.IsEmpty()) {
        collection->Destroy();
        // |collection| is now a dangling pointer!
        collection = nullptr;
      }
    }
  }

  if (didThrottle) {
    mPresContext->Document()->SetNeedStyleFlush();
  }

  for (uint32_t i = 0, i_end = events.Length(); i < i_end; ++i) {
    TransitionEventInfo &info = events[i];
    EventDispatcher::Dispatch(info.mElement, mPresContext, &info.mEvent);

    if (!mPresContext) {
      break;
    }
  }
}
