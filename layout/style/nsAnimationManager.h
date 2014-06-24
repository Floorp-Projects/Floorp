/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsAnimationManager_h_
#define nsAnimationManager_h_

#include "mozilla/Attributes.h"
#include "mozilla/ContentEvents.h"
#include "AnimationCommon.h"
#include "nsCSSPseudoElements.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"

class nsCSSKeyframesRule;
class nsStyleContext;

namespace mozilla {
namespace css {
class Declaration;
}
}

struct AnimationEventInfo {
  nsRefPtr<mozilla::dom::Element> mElement;
  mozilla::InternalAnimationEvent mEvent;

  AnimationEventInfo(mozilla::dom::Element *aElement,
                     const nsString& aAnimationName,
                     uint32_t aMessage, mozilla::TimeDuration aElapsedTime,
                     const nsAString& aPseudoElement)
    : mElement(aElement), mEvent(true, aMessage)
  {
    // XXX Looks like nobody initialize WidgetEvent::time
    mEvent.animationName = aAnimationName;
    mEvent.elapsedTime = aElapsedTime.ToSeconds();
    mEvent.pseudoElement = aPseudoElement;
  }

  // InternalAnimationEvent doesn't support copy-construction, so we need
  // to ourselves in order to work with nsTArray
  AnimationEventInfo(const AnimationEventInfo &aOther)
    : mElement(aOther.mElement), mEvent(true, aOther.mEvent.message)
  {
    mEvent.AssignAnimationEventData(aOther.mEvent, false);
  }
};

typedef InfallibleTArray<AnimationEventInfo> EventArray;

class nsAnimationManager MOZ_FINAL
  : public mozilla::css::CommonAnimationManager
{
public:
  nsAnimationManager(nsPresContext *aPresContext)
    : mozilla::css::CommonAnimationManager(aPresContext)
    , mObservingRefreshDriver(false)
  {
  }

  static mozilla::css::CommonElementAnimationData*
  GetAnimationsForCompositor(nsIContent* aContent, nsCSSProperty aProperty)
  {
    return mozilla::css::CommonAnimationManager::GetAnimationsForCompositor(
      aContent, nsGkAtoms::animationsProperty, aProperty);
  }

  // Returns true if aContent or any of its ancestors has an animation.
  static bool ContentOrAncestorHasAnimation(nsIContent* aContent) {
    do {
      if (aContent->GetProperty(nsGkAtoms::animationsProperty)) {
        return true;
      }
    } while ((aContent = aContent->GetParent()));

    return false;
  }

  void UpdateStyleAndEvents(mozilla::css::CommonElementAnimationData* aEA,
                            mozilla::TimeStamp aRefreshTime,
                            mozilla::EnsureStyleRuleFlags aFlags);
  void GetEventsAt(mozilla::css::CommonElementAnimationData* aEA,
                   TimeStamp aRefreshTime,
                   EventArray &aEventsToDispatch);

  // nsIStyleRuleProcessor (parts)
  virtual void RulesMatching(ElementRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual void RulesMatching(PseudoElementRuleProcessorData* aData) MOZ_OVERRIDE;
  virtual void RulesMatching(AnonBoxRuleProcessorData* aData) MOZ_OVERRIDE;
#ifdef MOZ_XUL
  virtual void RulesMatching(XULTreeRuleProcessorData* aData) MOZ_OVERRIDE;
#endif
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE MOZ_OVERRIDE;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE MOZ_OVERRIDE;

  // nsARefreshObserver
  virtual void WillRefresh(mozilla::TimeStamp aTime) MOZ_OVERRIDE;

  void FlushAnimations(FlushFlags aFlags);

  /**
   * Return the style rule that RulesMatching should add for
   * aStyleContext.  This might be different from what RulesMatching
   * actually added during aStyleContext's construction because the
   * element's animation-name may have changed.  (However, this does
   * return null during the non-animation restyling phase, as
   * RulesMatching does.)
   *
   * aStyleContext may be a style context for aElement or for its
   * :before or :after pseudo-element.
   */
  nsIStyleRule* CheckAnimationRule(nsStyleContext* aStyleContext,
                                   mozilla::dom::Element* aElement);

  /**
   * Dispatch any pending events.  We accumulate animationend and
   * animationiteration events only during refresh driver notifications
   * (and dispatch them at the end of such notifications), but we
   * accumulate animationstart events at other points when style
   * contexts are created.
   */
  void DispatchEvents() {
    // Fast-path the common case: no events
    if (!mPendingEvents.IsEmpty()) {
      DoDispatchEvents();
    }
  }

  mozilla::css::CommonElementAnimationData*
  GetElementAnimations(mozilla::dom::Element *aElement,
                       nsCSSPseudoElements::Type aPseudoType,
                       bool aCreateIfNeeded);

  // Updates styles on throttled animations. See note on nsTransitionManager
  void UpdateAllThrottledStyles();

protected:
  virtual void ElementDataRemoved() MOZ_OVERRIDE
  {
    CheckNeedsRefresh();
  }
  virtual void AddElementData(mozilla::css::CommonElementAnimationData* aData) MOZ_OVERRIDE;

  /**
   * Check to see if we should stop or start observing the refresh driver
   */
  void CheckNeedsRefresh();

private:
  void BuildAnimations(nsStyleContext* aStyleContext,
                       mozilla::ElementAnimationPtrArray& aAnimations);
  bool BuildSegment(InfallibleTArray<mozilla::AnimationPropertySegment>&
                      aSegments,
                    nsCSSProperty aProperty, const nsAnimation& aAnimation,
                    float aFromKey, nsStyleContext* aFromContext,
                    mozilla::css::Declaration* aFromDeclaration,
                    float aToKey, nsStyleContext* aToContext);
  nsIStyleRule* GetAnimationRule(mozilla::dom::Element* aElement,
                                 nsCSSPseudoElements::Type aPseudoType);

  // Update the animated styles of an element and its descendants.
  // If the element has an animation, it is flushed back to its primary frame.
  // If the element does not have an animation, then its style is reparented.
  void UpdateThrottledStylesForSubtree(nsIContent* aContent,
                                       nsStyleContext* aParentStyle,
                                       nsStyleChangeList &aChangeList);
  void UpdateAllThrottledStylesInternal();

  // The guts of DispatchEvents
  void DoDispatchEvents();

  EventArray mPendingEvents;

  bool mObservingRefreshDriver;
};

#endif /* !defined(nsAnimationManager_h_) */
