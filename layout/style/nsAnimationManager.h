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
#include "mozilla/dom/AnimationPlayer.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"

class nsCSSKeyframesRule;
class nsStyleContext;

namespace mozilla {
namespace css {
class Declaration;
} /* namespace css */

struct AnimationEventInfo {
  nsRefPtr<mozilla::dom::Element> mElement;
  mozilla::InternalAnimationEvent mEvent;

  AnimationEventInfo(mozilla::dom::Element *aElement,
                     const nsSubstring& aAnimationName,
                     uint32_t aMessage,
                     const mozilla::StickyTimeDuration& aElapsedTime,
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

class CSSAnimationPlayer MOZ_FINAL : public dom::AnimationPlayer
{
public:
 explicit CSSAnimationPlayer(dom::AnimationTimeline* aTimeline)
    : dom::AnimationPlayer(aTimeline)
    , mIsStylePaused(false)
    , mPauseShouldStick(false)
    , mLastNotification(LAST_NOTIFICATION_NONE)
  {
  }

  virtual CSSAnimationPlayer*
  AsCSSAnimationPlayer() MOZ_OVERRIDE { return this; }

  virtual void Play(UpdateFlags aUpdateFlags) MOZ_OVERRIDE;
  virtual void Pause(UpdateFlags aUpdateFlags) MOZ_OVERRIDE;

  void PlayFromStyle();
  void PauseFromStyle();

  bool IsStylePaused() const { return mIsStylePaused; }

  void QueueEvents(EventArray& aEventsToDispatch);

protected:
  virtual ~CSSAnimationPlayer() { }

  static nsString PseudoTypeAsString(nsCSSPseudoElements::Type aPseudoType);

  // When combining animation-play-state with play() / pause() the following
  // behavior applies:
  // 1. pause() is sticky and always overrides the underlying
  //    animation-play-state
  // 2. If animation-play-state is 'paused', play() will temporarily override
  //    it until animation-play-state next becomes 'running'.
  // 3. Calls to play() trigger finishing behavior but setting the
  //    animation-play-state to 'running' does not.
  //
  // This leads to five distinct states:
  //
  // A. Running
  // B. Running and temporarily overriding animation-play-state: paused
  // C. Paused and sticky overriding animation-play-state: running
  // D. Paused and sticky overriding animation-play-state: paused
  // E. Paused by animation-play-state
  //
  // C and D may seem redundant but they differ in how to respond to the
  // sequence: call play(), set animation-play-state: paused.
  //
  // C will transition to A then E leaving the animation paused.
  // D will transition to B then B leaving the animation running.
  //
  // A state transition chart is as follows:
  //
  //             A | B | C | D | E
  //   ---------------------------
  //   play()    A | B | A | B | B
  //   pause()   C | D | C | D | D
  //   'running' A | A | C | C | A
  //   'paused'  E | B | D | D | E
  //
  // The base class, AnimationPlayer already provides a boolean value,
  // mIsPaused which gives us two states. To this we add a further two booleans
  // to represent the states as follows.
  //
  // A. Running
  //    (!mIsPaused; !mIsStylePaused; !mPauseShouldStick)
  // B. Running and temporarily overriding animation-play-state: paused
  //    (!mIsPaused; mIsStylePaused; !mPauseShouldStick)
  // C. Paused and sticky overriding animation-play-state: running
  //    (mIsPaused; !mIsStylePaused; mPauseShouldStick)
  // D. Paused and sticky overriding animation-play-state: paused
  //    (mIsPaused; mIsStylePaused; mPauseShouldStick)
  // E. Paused by animation-play-state
  //    (mIsPaused; mIsStylePaused; !mPauseShouldStick)
  //
  // (That leaves 3 combinations of the boolean values that we never set because
  // they don't represent valid states.)
  bool mIsStylePaused;
  bool mPauseShouldStick;

  enum {
    LAST_NOTIFICATION_NONE = uint64_t(-1),
    LAST_NOTIFICATION_END = uint64_t(-2)
  };
  // One of the LAST_NOTIFICATION_* constants, or an integer for the iteration
  // whose start we last notified on.
  uint64_t mLastNotification;
};

} /* namespace mozilla */

class nsAnimationManager MOZ_FINAL
  : public mozilla::css::CommonAnimationManager
{
public:
  explicit nsAnimationManager(nsPresContext *aPresContext)
    : mozilla::css::CommonAnimationManager(aPresContext)
    , mObservingRefreshDriver(false)
  {
  }

  static mozilla::AnimationPlayerCollection*
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

  void UpdateStyleAndEvents(mozilla::AnimationPlayerCollection* aEA,
                            mozilla::TimeStamp aRefreshTime,
                            mozilla::EnsureStyleRuleFlags aFlags);
  void QueueEvents(mozilla::AnimationPlayerCollection* aEA,
                   mozilla::EventArray &aEventsToDispatch);

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

  mozilla::AnimationPlayerCollection*
  GetAnimationPlayers(mozilla::dom::Element *aElement,
                      nsCSSPseudoElements::Type aPseudoType,
                      bool aCreateIfNeeded);

protected:
  virtual void ElementCollectionRemoved() MOZ_OVERRIDE
  {
    CheckNeedsRefresh();
  }
  virtual void
  AddElementCollection(mozilla::AnimationPlayerCollection* aData) MOZ_OVERRIDE;

  /**
   * Check to see if we should stop or start observing the refresh driver
   */
  void CheckNeedsRefresh();

private:
  void BuildAnimations(nsStyleContext* aStyleContext,
                       mozilla::dom::Element* aTarget,
                       mozilla::dom::AnimationTimeline* aTimeline,
                       mozilla::AnimationPlayerPtrArray& aAnimations);
  bool BuildSegment(InfallibleTArray<mozilla::AnimationPropertySegment>&
                      aSegments,
                    nsCSSProperty aProperty,
                    const mozilla::StyleAnimation& aAnimation,
                    float aFromKey, nsStyleContext* aFromContext,
                    mozilla::css::Declaration* aFromDeclaration,
                    float aToKey, nsStyleContext* aToContext);
  nsIStyleRule* GetAnimationRule(mozilla::dom::Element* aElement,
                                 nsCSSPseudoElements::Type aPseudoType);

  // The guts of DispatchEvents
  void DoDispatchEvents();

  mozilla::EventArray mPendingEvents;

  bool mObservingRefreshDriver;
};

#endif /* !defined(nsAnimationManager_h_) */
