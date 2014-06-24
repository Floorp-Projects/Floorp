/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Code to start and animate CSS transitions. */

#ifndef nsTransitionManager_h_
#define nsTransitionManager_h_

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "AnimationCommon.h"
#include "nsCSSPseudoElements.h"

class nsStyleContext;
class nsPresContext;
class nsCSSPropertySet;
struct nsTransition;
struct ElementDependentRuleProcessorData;

/*****************************************************************************
 * Per-Element data                                                          *
 *****************************************************************************/

struct ElementPropertyTransition : public mozilla::ElementAnimation
{
  virtual ElementPropertyTransition* AsTransition() { return this; }
  virtual const ElementPropertyTransition* AsTransition() const { return this; }

  // This is the start value to be used for a check for whether a
  // transition is being reversed.  Normally the same as
  // mProperties[0].mSegments[0].mFromValue, except when this transition
  // started as the reversal of another in-progress transition.
  // Needed so we can handle two reverses in a row.
  mozilla::StyleAnimationValue mStartForReversingTest;
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
};

class nsTransitionManager MOZ_FINAL
  : public mozilla::css::CommonAnimationManager
{
public:
  nsTransitionManager(nsPresContext *aPresContext)
    : mozilla::css::CommonAnimationManager(aPresContext)
  {
  }

  static mozilla::css::CommonElementAnimationData*
  GetTransitions(nsIContent* aContent) {
    return static_cast<CommonElementAnimationData*>
      (aContent->GetProperty(nsGkAtoms::transitionsProperty));
  }

  // Returns true if aContent or any of its ancestors has a transition.
  static bool ContentOrAncestorHasTransition(nsIContent* aContent) {
    do {
      if (GetTransitions(aContent)) {
        return true;
      }
    } while ((aContent = aContent->GetParent()));

    return false;
  }

  typedef mozilla::css::CommonElementAnimationData CommonElementAnimationData;

  static CommonElementAnimationData*
  GetAnimationsForCompositor(nsIContent* aContent, nsCSSProperty aProperty)
  {
    return mozilla::css::CommonAnimationManager::GetAnimationsForCompositor(
      aContent, nsGkAtoms::transitionsProperty, aProperty);
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
  virtual void RulesMatching(ElementRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual void RulesMatching(PseudoElementRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual void RulesMatching(AnonBoxRuleProcessorData* aData) MOZ_OVERRIDE;
#ifdef MOZ_XUL
  virtual void RulesMatching(XULTreeRuleProcessorData* aData) MOZ_OVERRIDE;
#endif
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
    MOZ_MUST_OVERRIDE MOZ_OVERRIDE;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
    MOZ_MUST_OVERRIDE MOZ_OVERRIDE;

  // nsARefreshObserver
  virtual void WillRefresh(mozilla::TimeStamp aTime) MOZ_OVERRIDE;

  void FlushTransitions(FlushFlags aFlags);

  // Performs a 'mini-flush' to make styles from throttled transitions
  // up-to-date prior to processing an unrelated style change, so that
  // any transitions triggered by that style change produce correct
  // results.
  //
  // In more detail:  when we're able to run animations on the
  // compositor, we sometimes "throttle" these animations by skipping
  // updating style data on the main thread.  However, whenever we
  // process a normal (non-animation) style change, any changes in
  // computed style on elements that have transition-* properties set
  // may need to trigger new transitions; this process requires knowing
  // both the old and new values of the property.  To do this correctly,
  // we need to have an up-to-date *old* value of the property on the
  // primary frame.  So the purpose of the mini-flush is to update the
  // style for all throttled transitions and animations to the current
  // animation state without making any other updates, so that when we
  // process the queued style updates we'll have correct old data to
  // compare against.  When we do this, we don't bother touching frames
  // other than primary frames.
  void UpdateAllThrottledStyles();

  CommonElementAnimationData* GetElementTransitions(
    mozilla::dom::Element *aElement,
    nsCSSPseudoElements::Type aPseudoType,
    bool aCreateIfNeeded);

protected:
  virtual void ElementDataRemoved() MOZ_OVERRIDE;
  virtual void AddElementData(mozilla::css::CommonElementAnimationData* aData) MOZ_OVERRIDE;

private:
  void ConsiderStartingTransition(nsCSSProperty aProperty,
                                  const nsTransition& aTransition,
                                  mozilla::dom::Element* aElement,
                                  CommonElementAnimationData*& aElementTransitions,
                                  nsStyleContext* aOldStyleContext,
                                  nsStyleContext* aNewStyleContext,
                                  bool* aStartedAny,
                                  nsCSSPropertySet* aWhichStarted);
  void WalkTransitionRule(ElementDependentRuleProcessorData* aData,
                          nsCSSPseudoElements::Type aPseudoType);
  // Update the animated styles of an element and its descendants.
  // If the element has a transition, it is flushed back to its primary frame.
  // If the element does not have a transition, then its style is reparented.
  void UpdateThrottledStylesForSubtree(nsIContent* aContent,
                                       nsStyleContext* aParentStyle,
                                       nsStyleChangeList &aChangeList);
  void UpdateAllThrottledStylesInternal();
};

#endif /* !defined(nsTransitionManager_h_) */
