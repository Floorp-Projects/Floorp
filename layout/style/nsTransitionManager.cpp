/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Code to start and animate CSS transitions. */

#include "nsTransitionManager.h"
#include "nsIContent.h"
#include "nsStyleContext.h"
#include "nsCSSProps.h"
#include "mozilla/TimeStamp.h"
#include "nsRefreshDriver.h"
#include "nsRuleProcessorData.h"
#include "nsIStyleRule.h"
#include "nsRuleWalker.h"
#include "nsRuleData.h"
#include "gfxColor.h"
#include "nsCSSPropertySet.h"
#include "nsStyleAnimation.h"
#include "nsEventDispatcher.h"
#include "nsGUIEvent.h"
#include "mozilla/dom/Element.h"
#include "nsIFrame.h"
#include "nsCSSFrameConstructor.h"

using mozilla::TimeStamp;
using mozilla::TimeDuration;

namespace dom = mozilla::dom;
namespace css = mozilla::css;

ElementTransitions::ElementTransitions(mozilla::dom::Element *aElement, nsIAtom *aElementProperty,
                                       nsTransitionManager *aTransitionManager)
  : CommonElementAnimationData(aElement, aElementProperty,
                               aTransitionManager)
{
}

double
ElementPropertyTransition::ValuePortionFor(TimeStamp aRefreshTime) const
{
  // Set |timePortion| to the portion of the way we are through the time
  // input to the transition's timing function (always within the range
  // 0-1).
  double duration = mDuration.ToSeconds();
  NS_ABORT_IF_FALSE(duration >= 0.0, "negative duration forbidden");
  double timePortion;
  if (duration == 0.0) {
    // When duration is zero, we can still have a transition when delay
    // is nonzero.  mStartTime already incorporates delay.
    if (aRefreshTime >= mStartTime) {
      timePortion = 1.0;
    } else {
      timePortion = 0.0;
    }
  } else {
    timePortion = (aRefreshTime - mStartTime).ToSeconds() / duration;
    if (timePortion < 0.0)
      timePortion = 0.0; // use start value during transition-delay
    if (timePortion > 1.0)
      timePortion = 1.0; // we might be behind on flushing
  }

  return mTimingFunction.GetValue(timePortion);
}

static void
ElementTransitionsPropertyDtor(void           *aObject,
                               nsIAtom        *aPropertyName,
                               void           *aPropertyValue,
                               void           *aData)
{
  ElementTransitions *et = static_cast<ElementTransitions*>(aPropertyValue);
#ifdef DEBUG
  NS_ABORT_IF_FALSE(!et->mCalledPropertyDtor, "can't call dtor twice");
  et->mCalledPropertyDtor = true;
#endif
  delete et;
}

void
ElementTransitions::EnsureStyleRuleFor(TimeStamp aRefreshTime)
{
  if (!mStyleRule || mStyleRuleRefreshTime != aRefreshTime) {
    mStyleRule = new css::AnimValuesStyleRule();
    mStyleRuleRefreshTime = aRefreshTime;

    for (PRUint32 i = 0, i_end = mPropertyTransitions.Length(); i < i_end; ++i)
    {
      ElementPropertyTransition &pt = mPropertyTransitions[i];
      if (pt.IsRemovedSentinel()) {
        continue;
      }

      nsStyleAnimation::Value *val = mStyleRule->AddEmptyValue(pt.mProperty);

      double valuePortion = pt.ValuePortionFor(aRefreshTime);
#ifdef DEBUG
      bool ok =
#endif
        nsStyleAnimation::Interpolate(pt.mProperty,
                                      pt.mStartValue, pt.mEndValue,
                                      valuePortion, *val);
      NS_ABORT_IF_FALSE(ok, "could not interpolate values");
    }
  }
}

bool
ElementPropertyTransition::CanPerformOnCompositor(mozilla::dom::Element* aElement,
                                                  TimeStamp aTime) const {
  return css::CommonElementAnimationData::
    CanAnimatePropertyOnCompositor(aElement, mProperty) && !IsRemovedSentinel() &&
    mStartTime < aTime && aTime < mStartTime + mDuration;
}

bool
ElementTransitions::HasTransitionOfProperty(nsCSSProperty aProperty) const
{
  for (PRUint32 tranIdx = mPropertyTransitions.Length(); tranIdx-- != 0; ) {
    if (aProperty == mPropertyTransitions[tranIdx].mProperty) {
      return true;
    }
  }
  return false;
}

bool
ElementTransitions::CanPerformOnCompositorThread() const
{
  for (PRUint32 i = 0, i_end = mPropertyTransitions.Length(); i < i_end; ++i) {
    const ElementPropertyTransition &pt = mPropertyTransitions[i];
    if (pt.IsRemovedSentinel()) {
      continue;
    }
    if (!css::CommonElementAnimationData::CanAnimatePropertyOnCompositor(mElement,
                                                                         pt.mProperty)) {
      return false;
    }
  }
  return true;
}

/*****************************************************************************
 * nsTransitionManager                                                       *
 *****************************************************************************/

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

  // NOTE: Things in this function (and ConsiderStartingTransition)
  // should never call PeekStyleData because we don't preserve gotten
  // structs across reframes.

  // Return sooner (before the startedAny check below) for the most
  // common case: no transitions specified or running.
  const nsStyleDisplay *disp = aNewStyleContext->GetStyleDisplay();
  nsCSSPseudoElements::Type pseudoType = aNewStyleContext->GetPseudoType();
  if (pseudoType != nsCSSPseudoElements::ePseudo_NotPseudoElement) {
    if (pseudoType != nsCSSPseudoElements::ePseudo_before &&
        pseudoType != nsCSSPseudoElements::ePseudo_after) {
      return nsnull;
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

  ElementTransitions *et =
      GetElementTransitions(aElement, pseudoType, false);
  if (!et &&
      disp->mTransitionPropertyCount == 1 &&
      disp->mTransitions[0].GetDelay() == 0.0f &&
      disp->mTransitions[0].GetDuration() == 0.0f) {
    return nsnull;
  }

  if (aNewStyleContext->PresContext()->IsProcessingAnimationStyleChange()) {
    return nsnull;
  }

  if (aNewStyleContext->GetParent() &&
      aNewStyleContext->GetParent()->HasPseudoElementData()) {
    // Ignore transitions on things that inherit properties from
    // pseudo-elements.
    // FIXME (Bug 522599): Add tests for this.
    return nsnull;
  }

  // Per http://lists.w3.org/Archives/Public/www-style/2009Aug/0109.html
  // I'll consider only the transitions from the number of items in
  // 'transition-property' on down, and later ones will override earlier
  // ones (tracked using |whichStarted|).
  bool startedAny = false;
  nsCSSPropertySet whichStarted;
  for (PRUint32 i = disp->mTransitionPropertyCount; i-- != 0; ) {
    const nsTransition& t = disp->mTransitions[i];
    // Check delay and duration first, since they default to zero, and
    // when they're both zero, we can ignore the transition.
    if (t.GetDelay() != 0.0f || t.GetDuration() != 0.0f) {
      // We might have something to transition.  See if any of the
      // properties in question changed and are animatable.
      // FIXME: Would be good to find a way to share code between this
      // interpretation of transition-property and the one below.
      nsCSSProperty property = t.GetProperty();
      if (property == eCSSPropertyExtra_no_properties ||
          property == eCSSProperty_UNKNOWN) {
        // Nothing to do, but need to exclude this from cases below.
      } else if (property == eCSSPropertyExtra_all_properties) {
        for (nsCSSProperty p = nsCSSProperty(0);
             p < eCSSProperty_COUNT_no_shorthands;
             p = nsCSSProperty(p + 1)) {
          ConsiderStartingTransition(p, t, aElement, et,
                                     aOldStyleContext, aNewStyleContext,
                                     &startedAny, &whichStarted);
        }
      } else if (nsCSSProps::IsShorthand(property)) {
        CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subprop, property) {
          ConsiderStartingTransition(*subprop, t, aElement, et,
                                     aOldStyleContext, aNewStyleContext,
                                     &startedAny, &whichStarted);
        }
      } else {
        ConsiderStartingTransition(property, t, aElement, et,
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
  if (et) {
    bool checkProperties =
      disp->mTransitions[0].GetProperty() != eCSSPropertyExtra_all_properties;
    nsCSSPropertySet allTransitionProperties;
    if (checkProperties) {
      for (PRUint32 i = disp->mTransitionPropertyCount; i-- != 0; ) {
        const nsTransition& t = disp->mTransitions[i];
        // FIXME: Would be good to find a way to share code between this
        // interpretation of transition-property and the one above.
        nsCSSProperty property = t.GetProperty();
        if (property == eCSSPropertyExtra_no_properties ||
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

    nsTArray<ElementPropertyTransition> &pts = et->mPropertyTransitions;
    PRUint32 i = pts.Length();
    NS_ABORT_IF_FALSE(i != 0, "empty transitions list?");
    nsStyleAnimation::Value currentValue;
    do {
      --i;
      ElementPropertyTransition &pt = pts[i];
          // properties no longer in 'transition-property'
      if ((checkProperties &&
           !allTransitionProperties.HasProperty(pt.mProperty)) ||
          // properties whose computed values changed but delay and
          // duration are both zero
          !ExtractComputedValueForTransition(pt.mProperty, aNewStyleContext,
                                             currentValue) ||
          currentValue != pt.mEndValue) {
        // stop the transition
        pts.RemoveElementAt(i);
      }
    } while (i != 0);

    if (pts.IsEmpty()) {
      et->Destroy();
      et = nsnull;
    }
  }

  if (!startedAny) {
    return nsnull;
  }

  NS_ABORT_IF_FALSE(et, "must have element transitions if we started "
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

  nsTArray<ElementPropertyTransition> &pts = et->mPropertyTransitions;
  for (PRUint32 i = 0, i_end = pts.Length(); i < i_end; ++i) {
    ElementPropertyTransition &pt = pts[i];
    if (whichStarted.HasProperty(pt.mProperty)) {
      coverRule->AddValue(pt.mProperty, pt.mStartValue);
    }
  }

  return coverRule.forget();
}

void
nsTransitionManager::ConsiderStartingTransition(nsCSSProperty aProperty,
                       const nsTransition& aTransition,
                       dom::Element *aElement,
                       ElementTransitions *&aElementTransitions,
                       nsStyleContext *aOldStyleContext,
                       nsStyleContext *aNewStyleContext,
                       bool *aStartedAny,
                       nsCSSPropertySet *aWhichStarted)
{
  // IsShorthand itself will assert if aProperty is not a property.
  NS_ABORT_IF_FALSE(!nsCSSProps::IsShorthand(aProperty),
                    "property out of range");

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

  ElementPropertyTransition pt;
  nsStyleAnimation::Value dummyValue;
  bool haveValues =
    ExtractComputedValueForTransition(aProperty, aOldStyleContext,
                                      pt.mStartValue) &&
    ExtractComputedValueForTransition(aProperty, aNewStyleContext,
                                      pt.mEndValue);

  bool haveChange = pt.mStartValue != pt.mEndValue;
  bool haveOMTA = false;
  if (!aNewStyleContext->GetPseudoType()) {
    ElementTransitions* et = nsTransitionManager::GetTransitions(aElement);
    if (et) {
      haveOMTA = et->CanPerformOnCompositorThread();
    }
  }

  bool shouldAnimate =
    haveValues &&
    (haveChange || haveOMTA) &&
    // Check that we can interpolate between these values
    // (If this is ever a performance problem, we could add a
    // CanInterpolate method, but it seems fine for now.)
    nsStyleAnimation::Interpolate(aProperty, pt.mStartValue, pt.mEndValue,
                                  0.5, dummyValue);

  PRUint32 currentIndex = nsTArray<ElementPropertyTransition>::NoIndex;
  if (aElementTransitions) {
    nsTArray<ElementPropertyTransition> &pts =
      aElementTransitions->mPropertyTransitions;
    for (PRUint32 i = 0, i_end = pts.Length(); i < i_end; ++i) {
      if (pts[i].mProperty == aProperty) {
        currentIndex = i;
        break;
      }
    }
  }

  nsPresContext *presContext = aNewStyleContext->PresContext();

  if (!shouldAnimate) {
    nsTArray<ElementPropertyTransition> &pts =
      aElementTransitions->mPropertyTransitions;
    if (currentIndex != nsTArray<ElementPropertyTransition>::NoIndex &&
        (!haveValues || pts[currentIndex].mEndValue != pt.mEndValue)) {
      // We're in the middle of a transition, but just got a
      // non-transition style change changing to exactly the
      // current in-progress value.   (This is quite easy to cause
      // using 'transition-delay'.)
      //
      // We also check that this current in-progress value is different
      // from the end value; we don't want to cancel a transition that
      // is almost done (and whose current value rounds to its end
      // value) just because we got an unrelated style change.
      pts.RemoveElementAt(currentIndex);
      if (pts.IsEmpty()) {
        aElementTransitions->Destroy();
        // |aElementTransitions| is now a dangling pointer!
        aElementTransitions = nsnull;
      }
      // WalkTransitionRule already called RestyleForAnimation.
    }
    return;
  }

  TimeStamp mostRecentRefresh =
    presContext->RefreshDriver()->MostRecentRefresh();

  const nsTimingFunction &tf = aTransition.GetTimingFunction();
  float delay = aTransition.GetDelay();
  float duration = aTransition.GetDuration();
  if (duration < 0.0) {
    // The spec says a negative duration is treated as zero.
    duration = 0.0;
  }
  pt.mStartForReversingTest = pt.mStartValue;
  pt.mReversePortion = 1.0;

  // We need to check two things if we have a currently running
  // transition for this property.
  if (currentIndex != nsTArray<ElementPropertyTransition>::NoIndex) {
    const ElementPropertyTransition &oldPT =
      aElementTransitions->mPropertyTransitions[currentIndex];

    if (oldPT.mEndValue == pt.mEndValue) {
      // If we got a style change that changed the value to the endpoint
      // of the currently running transition, we don't want to interrupt
      // its timing function.
      // WalkTransitionRule already called RestyleForAnimation.
      return;
    }

    // If the new transition reverses the old one, we'll need to handle
    // the timing differently.
    if (!oldPT.IsRemovedSentinel() &&
        oldPT.mStartForReversingTest == pt.mEndValue) {
      // Compute the appropriate negative transition-delay such that right
      // now we'd end up at the current position.
      double valuePortion =
        oldPT.ValuePortionFor(mostRecentRefresh) * oldPT.mReversePortion +
        (1.0 - oldPT.mReversePortion);
      // A timing function with negative y1 (or y2!) might make
      // valuePortion negative.  In this case, we still want to apply our
      // reversing logic based on relative distances, not make duration
      // negative.
      if (valuePortion < 0.0)
        valuePortion = -valuePortion;
      // A timing function with y2 (or y1!) greater than one might
      // advance past its terminal value.  It's probably a good idea to
      // clamp valuePortion to be at most one to preserve the invariant
      // that a transition will complete within at most its specified
      // time.
      if (valuePortion > 1.0)
        valuePortion = 1.0;

      // Negative delays are essentially part of the transition
      // function, so reduce them along with the duration, but don't
      // reduce positive delays.
      if (delay < 0.0f)
        delay *= valuePortion;

      duration *= valuePortion;

      pt.mStartForReversingTest = oldPT.mEndValue;
      pt.mReversePortion = valuePortion;
    }
  }

  pt.mProperty = aProperty;
  pt.mStartTime = mostRecentRefresh + TimeDuration::FromMilliseconds(delay);
  pt.mDuration = TimeDuration::FromMilliseconds(duration);
  pt.mTimingFunction.Init(tf);
  if (!aElementTransitions) {
    aElementTransitions =
      GetElementTransitions(aElement, aNewStyleContext->GetPseudoType(),
                            true);
    if (!aElementTransitions) {
      NS_WARNING("allocating ElementTransitions failed");
      return;
    }
  }

  nsTArray<ElementPropertyTransition> &pts =
    aElementTransitions->mPropertyTransitions;
#ifdef DEBUG
  for (PRUint32 i = 0, i_end = pts.Length(); i < i_end; ++i) {
    NS_ABORT_IF_FALSE(i == currentIndex ||
                      pts[i].mProperty != aProperty,
                      "duplicate transitions for property");
  }
#endif
  if (currentIndex != nsTArray<ElementPropertyTransition>::NoIndex) {
    pts[currentIndex] = pt;
  } else {
    if (!pts.AppendElement(pt)) {
      NS_WARNING("out of memory");
      return;
    }
  }

  nsRestyleHint hint =
    aNewStyleContext->GetPseudoType() ==
      nsCSSPseudoElements::ePseudo_NotPseudoElement ?
    eRestyle_Self : eRestyle_Subtree;
  presContext->PresShell()->RestyleForAnimation(aElement, hint);
  // XXXdz: invalidate the frame here, once animations are throttled.

  *aStartedAny = true;
  aWhichStarted->AddProperty(aProperty);
}

ElementTransitions*
nsTransitionManager::GetElementTransitions(dom::Element *aElement,
                                           nsCSSPseudoElements::Type aPseudoType,
                                           bool aCreateIfNeeded)
{
  if (!aCreateIfNeeded && PR_CLIST_IS_EMPTY(&mElementData)) {
    // Early return for the most common case.
    return nsnull;
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
    return nsnull;
  }
  ElementTransitions *et = static_cast<ElementTransitions*>(
                             aElement->GetProperty(propName));
  if (!et && aCreateIfNeeded) {
    // FIXME: Consider arena-allocating?
    et = new ElementTransitions(aElement, propName, this);
    nsresult rv = aElement->SetProperty(propName, et,
                                        ElementTransitionsPropertyDtor, nsnull);
    if (NS_FAILED(rv)) {
      NS_WARNING("SetProperty failed");
      delete et;
      return nsnull;
    }
    if (propName == nsGkAtoms::transitionsProperty) {
      aElement->SetMayHaveAnimations();
    }

    AddElementData(et);
  }

  return et;
}

/*
 * nsIStyleRuleProcessor implementation
 */

void
nsTransitionManager::WalkTransitionRule(RuleProcessorData* aData,
                                        nsCSSPseudoElements::Type aPseudoType)
{
  ElementTransitions *et =
    GetElementTransitions(aData->mElement, aPseudoType, false);
  if (!et) {
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
    if (et) {
      nsRestyleHint hint =
        aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement ?
        eRestyle_Self : eRestyle_Subtree;
      mPresContext->PresShell()->RestyleForAnimation(aData->mElement, hint);
    }
    return;
  }

  et->EnsureStyleRuleFor(
    aData->mPresContext->RefreshDriver()->MostRecentRefresh());

  aData->mRuleWalker->Forward(et->mStyleRule);
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
nsTransitionManager::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return CommonAnimationManager::SizeOfExcludingThis(aMallocSizeOf);
}

/* virtual */ size_t
nsTransitionManager::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

struct TransitionEventInfo {
  nsCOMPtr<nsIContent> mElement;
  nsTransitionEvent mEvent;

  TransitionEventInfo(nsIContent *aElement, nsCSSProperty aProperty,
                      TimeDuration aDuration)
    : mElement(aElement),
      mEvent(true, NS_TRANSITION_END,
             NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(aProperty)),
             aDuration.ToSeconds())
  {
  }

  // nsTransitionEvent doesn't support copy-construction, so we need
  // to ourselves in order to work with nsTArray
  TransitionEventInfo(const TransitionEventInfo &aOther)
    : mElement(aOther.mElement),
      mEvent(true, NS_TRANSITION_END,
             aOther.mEvent.propertyName, aOther.mEvent.elapsedTime)
  {
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
    RemoveAllElementData();
    return;
  }

  nsTArray<TransitionEventInfo> events;

  // Trim transitions that have completed, and post restyle events for
  // frames that are still transitioning.
  {
    PRCList *next = PR_LIST_HEAD(&mElementData);
    while (next != &mElementData) {
      ElementTransitions *et = static_cast<ElementTransitions*>(next);
      next = PR_NEXT_LINK(next);

      NS_ABORT_IF_FALSE(et->mElement->GetCurrentDoc() ==
                          mPresContext->Document(),
                        "nsGenericElement::UnbindFromTree should have "
                        "destroyed the element transitions object");

      PRUint32 i = et->mPropertyTransitions.Length();
      NS_ABORT_IF_FALSE(i != 0, "empty transitions list?");
      do {
        --i;
        ElementPropertyTransition &pt = et->mPropertyTransitions[i];
        if (pt.IsRemovedSentinel()) {
          // Actually remove transitions one cycle after their
          // completion.  See comment below.
          et->mPropertyTransitions.RemoveElementAt(i);
        } else if (pt.mStartTime + pt.mDuration <= aTime) {
          // Fire transitionend events only for transitions on elements
          // and not those on pseudo-elements, since we can't target an
          // event at pseudo-elements.
          if (et->mElementProperty == nsGkAtoms::transitionsProperty) {
            nsCSSProperty prop = pt.mProperty;
            if (nsCSSProps::PropHasFlags(prop, CSS_PROPERTY_REPORT_OTHER_NAME))
            {
              prop = nsCSSProps::OtherNameFor(prop);
            }
            events.AppendElement(
              TransitionEventInfo(et->mElement, prop, pt.mDuration));
          }

          // Leave this transition in the list for one more refresh
          // cycle, since we haven't yet processed its style change, and
          // if we also have (already, or will have from processing
          // transitionend events or other refresh driver notifications)
          // a non-animation style change that would affect it, we need
          // to know not to start a new transition for the transition
          // from the almost-completed value to the final value.
          pt.SetRemovedSentinel();
        }
      } while (i != 0);

      // We need to restyle even if the transition rule no longer
      // applies (in which case we just made it not apply).
      NS_ASSERTION(et->mElementProperty == nsGkAtoms::transitionsProperty ||
                   et->mElementProperty == nsGkAtoms::transitionsOfBeforeProperty ||
                   et->mElementProperty == nsGkAtoms::transitionsOfAfterProperty,
                   "Unexpected element property; might restyle too much");
      nsRestyleHint hint = et->mElementProperty == nsGkAtoms::transitionsProperty ?
        eRestyle_Self : eRestyle_Subtree;
      mPresContext->PresShell()->RestyleForAnimation(et->mElement, hint);
      // XXXdz: if we have started a transition since the last tick and are
      // performing the transition off the main thread, we need to invalidate
      // the frame once we start throttling animation ticks.

      if (et->mPropertyTransitions.IsEmpty()) {
        et->Destroy();
        // |et| is now a dangling pointer!
        et = nsnull;
      }
    }
  }

  // We might have removed transitions above.
  ElementDataRemoved();

  for (PRUint32 i = 0, i_end = events.Length(); i < i_end; ++i) {
    TransitionEventInfo &info = events[i];
    nsEventDispatcher::Dispatch(info.mElement, mPresContext, &info.mEvent);

    if (!mPresContext) {
      break;
    }
  }
}
