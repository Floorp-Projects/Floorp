/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsAnimationManager_h_
#define nsAnimationManager_h_

#include "AnimationCommon.h"
#include "nsCSSPseudoElements.h"
#include "nsStyleContext.h"
#include "nsDataHashtable.h"
#include "nsGUIEvent.h"
#include "mozilla/TimeStamp.h"
#include "nsThreadUtils.h"

class nsCSSKeyframesRule;
struct AnimationPropertySegment;
struct ElementAnimation;
struct ElementAnimations;

namespace mozilla {
namespace css {
class Declaration;
}
}

class nsAnimationManager : public mozilla::css::CommonAnimationManager
{
public:
  nsAnimationManager(nsPresContext *aPresContext)
    : mozilla::css::CommonAnimationManager(aPresContext),
      mKeyframesListIsDirty(true)
  {
    mKeyframesRules.Init(16); // FIXME: make infallible!
  }

  struct AnimationEventInfo {
    nsRefPtr<mozilla::dom::Element> mElement;
    nsAnimationEvent mEvent;

    AnimationEventInfo(mozilla::dom::Element *aElement,
                       const nsString& aAnimationName,
                       PRUint32 aMessage, mozilla::TimeDuration aElapsedTime)
      : mElement(aElement),
        mEvent(true, aMessage, aAnimationName, aElapsedTime.ToSeconds())
    {
    }

    // nsAnimationEvent doesn't support copy-construction, so we need
    // to ourselves in order to work with nsTArray
    AnimationEventInfo(const AnimationEventInfo &aOther)
      : mElement(aOther.mElement),
        mEvent(true, aOther.mEvent.message,
               aOther.mEvent.animationName, aOther.mEvent.elapsedTime)
    {
    }
  };

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

  void KeyframesListIsDirty() {
    mKeyframesListIsDirty = true;
  }

  typedef InfallibleTArray<AnimationEventInfo> EventArray;

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

private:
  ElementAnimations* GetElementAnimations(mozilla::dom::Element *aElement,
                                          nsCSSPseudoElements::Type aPseudoType,
                                          bool aCreateIfNeeded);
  void BuildAnimations(nsStyleContext* aStyleContext,
                       InfallibleTArray<ElementAnimation>& aAnimations);
  bool BuildSegment(InfallibleTArray<AnimationPropertySegment>& aSegments,
                    nsCSSProperty aProperty, const nsAnimation& aAnimation,
                    float aFromKey, nsStyleContext* aFromContext,
                    mozilla::css::Declaration* aFromDeclaration,
                    float aToKey, nsStyleContext* aToContext);
  nsIStyleRule* GetAnimationRule(mozilla::dom::Element* aElement,
                                 nsCSSPseudoElements::Type aPseudoType);

  nsCSSKeyframesRule* KeyframesRuleFor(const nsSubstring& aName);

  // The guts of DispatchEvents
  void DoDispatchEvents();

  bool mKeyframesListIsDirty;
  nsDataHashtable<nsStringHashKey, nsCSSKeyframesRule*> mKeyframesRules;

  EventArray mPendingEvents;
};

#endif /* !defined(nsAnimationManager_h_) */
