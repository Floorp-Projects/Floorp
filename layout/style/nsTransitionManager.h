/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Code to start and animate CSS transitions. */

#ifndef nsTransitionManager_h_
#define nsTransitionManager_h_

#include "AnimationCommon.h"
#include "nsCSSPseudoElements.h"

class nsStyleContext;
class nsPresContext;
class nsCSSPropertySet;
struct nsTransition;

/*****************************************************************************
 * Per-Element data                                                          *
 *****************************************************************************/

struct ElementPropertyTransition
{
  ElementPropertyTransition() {}

  nsCSSProperty mProperty;
  nsStyleAnimation::Value mStartValue, mEndValue;
  mozilla::TimeStamp mStartTime; // actual start plus transition delay

  // data from the relevant nsTransition
  mozilla::TimeDuration mDuration;
  mozilla::css::ComputedTimingFunction mTimingFunction;

  // This is the start value to be used for a check for whether a
  // transition is being reversed.  Normally the same as mStartValue,
  // except when this transition started as the reversal of another
  // in-progress transition.  Needed so we can handle two reverses in a
  // row.
  nsStyleAnimation::Value mStartForReversingTest;
  // Likewise, the portion (in value space) of the "full" reversed
  // transition that we're actually covering.  For example, if a :hover
  // effect has a transition that moves the element 10px to the right
  // (by changing 'left' from 0px to 10px), and the mouse moves in to
  // the element (starting the transition) but then moves out after the
  // transition has advanced 4px, the second transition (from 10px/4px
  // to 0px) will have mReversePortion of 0.4.  (If the mouse then moves
  // in again when the transition is back to 2px, the mReversePortion
  // for the third transition (from 0px/2px to 10px) will be 0.8.
  double mReversePortion;

  // Compute the portion of the *value* space that we should be through
  // at the given time.  (The input to the transition timing function
  // has time units, the output has value units.)
  double ValuePortionFor(mozilla::TimeStamp aRefreshTime) const;

  bool IsRemovedSentinel() const
  {
    return mStartTime.IsNull();
  }

  void SetRemovedSentinel()
  {
    // assign the null time stamp
    mStartTime = mozilla::TimeStamp();
  }

  bool CanPerformOnCompositor(mozilla::dom::Element* aElement,
                              mozilla::TimeStamp aTime) const;
};

struct ElementTransitions : public mozilla::css::CommonElementAnimationData
{
  ElementTransitions(mozilla::dom::Element *aElement, nsIAtom *aElementProperty,
                     nsTransitionManager *aTransitionManager);

  void EnsureStyleRuleFor(mozilla::TimeStamp aRefreshTime);


  bool HasTransitionOfProperty(nsCSSProperty aProperty) const;
  // True if this animation can be performed on the compositor thread.
  bool CanPerformOnCompositorThread() const;
  // Either zero or one for each CSS property:
  nsTArray<ElementPropertyTransition> mPropertyTransitions;

  // This style rule overrides style data with the currently
  // transitioning value for an element that is executing a transition.
  // It only matches when styling with animation.  When we style without
  // animation, we need to not use it so that we can detect any new
  // changes; if necessary we restyle immediately afterwards with
  // animation.
  nsRefPtr<mozilla::css::AnimValuesStyleRule> mStyleRule;
  // The refresh time associated with mStyleRule.
  mozilla::TimeStamp mStyleRuleRefreshTime;
};



class nsTransitionManager : public mozilla::css::CommonAnimationManager
{
public:
  nsTransitionManager(nsPresContext *aPresContext)
    : mozilla::css::CommonAnimationManager(aPresContext)
  {
  }

  static ElementTransitions* GetTransitions(nsIContent* aContent) {
    return static_cast<ElementTransitions*>
      (aContent->GetProperty(nsGkAtoms::transitionsProperty));
  }

  static ElementTransitions*
    GetTransitionsForCompositor(nsIContent* aContent,
                                nsCSSProperty aProperty)
  {
    if (!aContent->MayHaveAnimations())
      return nsnull;
    ElementTransitions* transitions = GetTransitions(aContent);
    if (!transitions ||
        !transitions->HasTransitionOfProperty(aProperty) ||
        !transitions->CanPerformOnCompositorThread()) {
      return nsnull;
    }
    return transitions;
  }

  /**
   * StyleContextChanged
   *
   * To be called from nsFrameManager::ReResolveStyleContext when the
   * style of an element has changed, to initiate transitions from
   * that style change.  For style contexts with :before and :after
   * pseudos, aElement is expected to be the generated before/after
   * element.
   *
   * It may return a "cover rule" (see CoverTransitionStartStyleRule) to
   * cover up some of the changes for the duration of the restyling of
   * descendants.  If it does, this function will take care of causing
   * the necessary restyle afterwards, but the caller must restyle the
   * element *again* with the original sequence of rules plus the
   * returned cover rule as the most specific rule.
   */
  already_AddRefed<nsIStyleRule>
    StyleContextChanged(mozilla::dom::Element *aElement,
                        nsStyleContext *aOldStyleContext,
                        nsStyleContext *aNewStyleContext);

  // nsIStyleRuleProcessor (parts)
  virtual void RulesMatching(ElementRuleProcessorData* aData);
  virtual void RulesMatching(PseudoElementRuleProcessorData* aData);
  virtual void RulesMatching(AnonBoxRuleProcessorData* aData);
#ifdef MOZ_XUL
  virtual void RulesMatching(XULTreeRuleProcessorData* aData);
#endif
  virtual NS_MUST_OVERRIDE size_t
    SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const MOZ_OVERRIDE;
  virtual NS_MUST_OVERRIDE size_t
    SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const MOZ_OVERRIDE;

  // nsARefreshObserver
  virtual void WillRefresh(mozilla::TimeStamp aTime);

private:
  void ConsiderStartingTransition(nsCSSProperty aProperty,
                                  const nsTransition& aTransition,
                                  mozilla::dom::Element *aElement,
                                  ElementTransitions *&aElementTransitions,
                                  nsStyleContext *aOldStyleContext,
                                  nsStyleContext *aNewStyleContext,
                                  bool *aStartedAny,
                                  nsCSSPropertySet *aWhichStarted);
  ElementTransitions* GetElementTransitions(mozilla::dom::Element *aElement,
                                            nsCSSPseudoElements::Type aPseudoType,
                                            bool aCreateIfNeeded);
  void WalkTransitionRule(RuleProcessorData* aData,
                          nsCSSPseudoElements::Type aPseudoType);
};

#endif /* !defined(nsTransitionManager_h_) */
