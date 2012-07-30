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
#include "mozilla/Preferences.h"
#include "nsThreadUtils.h"

class nsCSSKeyframesRule;

namespace mozilla {
namespace css {
class Declaration;
}
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

typedef InfallibleTArray<AnimationEventInfo> EventArray;

struct AnimationPropertySegment
{
  float mFromKey, mToKey;
  nsStyleAnimation::Value mFromValue, mToValue;
  mozilla::css::ComputedTimingFunction mTimingFunction;
};

struct AnimationProperty
{
  nsCSSProperty mProperty;
  InfallibleTArray<AnimationPropertySegment> mSegments;
};

/**
 * Data about one animation (i.e., one of the values of
 * 'animation-name') running on an element.
 */
struct ElementAnimation
{
  ElementAnimation()
    : mLastNotification(LAST_NOTIFICATION_NONE)
  {
  }

  nsString mName; // empty string for 'none'
  float mIterationCount; // NS_IEEEPositiveInfinity() means infinite
  PRUint8 mDirection;
  PRUint8 mFillMode;
  PRUint8 mPlayState;

  bool FillsForwards() const {
    return mFillMode == NS_STYLE_ANIMATION_FILL_MODE_BOTH ||
           mFillMode == NS_STYLE_ANIMATION_FILL_MODE_FORWARDS;
  }
  bool FillsBackwards() const {
    return mFillMode == NS_STYLE_ANIMATION_FILL_MODE_BOTH ||
           mFillMode == NS_STYLE_ANIMATION_FILL_MODE_BACKWARDS;
  }

  bool IsPaused() const {
    return mPlayState == NS_STYLE_ANIMATION_PLAY_STATE_PAUSED;
  }

  bool CanPerformOnCompositor(mozilla::dom::Element* aElement,
                              mozilla::TimeStamp aTime) const;

  mozilla::TimeStamp mStartTime; // with delay taken into account
  mozilla::TimeStamp mPauseStart;
  mozilla::TimeDuration mIterationDuration;

  enum {
    LAST_NOTIFICATION_NONE = PRUint32(-1),
    LAST_NOTIFICATION_END = PRUint32(-2)
  };
  // One of the above constants, or an integer for the iteration
  // whose start we last notified on.
  PRUint32 mLastNotification;

  InfallibleTArray<AnimationProperty> mProperties;
};

/**
 * Data about all of the animations running on an element.
 */
struct ElementAnimations : public mozilla::css::CommonElementAnimationData
{
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  ElementAnimations(mozilla::dom::Element *aElement, nsIAtom *aElementProperty,
                    nsAnimationManager *aAnimationManager);

  // This function takes as input the start time, duration, and direction of an
  // animation and returns the position in the current iteration.  Note that
  // this only works when we know that the animation is currently running.
  // This way of calling the function can be used from the compositor.  Note
  // that if the animation has not started yet, has already ended, or is paused,
  // it should not be run from the compositor.  When this function is called 
  // from the main thread, we need the actual ElementAnimation* in order to 
  // get correct animation-fill behavior and to fire animation events.
  // This function returns -1 for the position if the animation should not be
  // run (because it is not currently active and has no fill behavior.)
  static double GetPositionInIteration(TimeStamp aStartTime,
                                       TimeStamp aCurrentTime,
                                       TimeDuration aDuration,
                                       double aIterationCount,
                                       PRUint32 aDirection,
                                       bool IsForElement = true,
                                       ElementAnimation* aAnimation = nullptr,
                                       ElementAnimations* aEa = nullptr,
                                       EventArray* aEventsToDispatch = nullptr);

  void EnsureStyleRuleFor(TimeStamp aRefreshTime,
                          EventArray &aEventsToDispatch);

  bool IsForElement() const { // rather than for a pseudo-element
    return mElementProperty == nsGkAtoms::animationsProperty;
  }

  void PostRestyleForAnimation(nsPresContext *aPresContext) {
    nsRestyleHint styleHint = IsForElement() ? eRestyle_Self : eRestyle_Subtree;
    aPresContext->PresShell()->RestyleForAnimation(mElement, styleHint);
  }

  // True if this animation can be performed on the compositor thread.
  bool CanPerformOnCompositorThread() const;
  bool HasAnimationOfProperty(nsCSSProperty aProperty) const;
  // This style rule contains the style data for currently animating
  // values.  It only matches when styling with animation.  When we
  // style without animation, we need to not use it so that we can
  // detect any new changes; if necessary we restyle immediately
  // afterwards with animation.
  // NOTE: If we don't need to apply any styles, mStyleRule will be
  // null, but mStyleRuleRefreshTime will still be valid.
  nsRefPtr<mozilla::css::AnimValuesStyleRule> mStyleRule;
  // The refresh time associated with mStyleRule.
  TimeStamp mStyleRuleRefreshTime;

  // False when we know that our current style rule is valid
  // indefinitely into the future (because all of our animations are
  // either completed or paused).  May be invalidated by a style change.
  bool mNeedsRefreshes;

  InfallibleTArray<ElementAnimation> mAnimations;
};

class nsAnimationManager : public mozilla::css::CommonAnimationManager
{
public:
  nsAnimationManager(nsPresContext *aPresContext)
    : mozilla::css::CommonAnimationManager(aPresContext)
    , mKeyframesListIsDirty(true)
  {
    mKeyframesRules.Init(16); // FIXME: make infallible!
  }

  static ElementAnimations* GetAnimationsForCompositor(nsIContent* aContent,
                                                       nsCSSProperty aProperty)
  {
    if (!aContent->MayHaveAnimations())
      return nullptr;
    ElementAnimations* animations = static_cast<ElementAnimations*>(
      aContent->GetProperty(nsGkAtoms::animationsProperty));
    if (!animations)
      return nullptr;
    bool propertyMatches = animations->HasAnimationOfProperty(aProperty);
    return (propertyMatches && animations->CanPerformOnCompositorThread()) ?
      animations : nullptr;
  }

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
