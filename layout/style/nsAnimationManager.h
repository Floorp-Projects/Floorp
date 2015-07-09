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
#include "mozilla/dom/Animation.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"

class nsIGlobalObject;
class nsStyleContext;

namespace mozilla {
namespace css {
class Declaration;
} /* namespace css */
namespace dom {
class Promise;
} /* namespace dom */

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

namespace dom {

class CSSAnimation final : public Animation
{
public:
 explicit CSSAnimation(nsIGlobalObject* aGlobal,
                       const nsSubstring& aAnimationName)
    : dom::Animation(aGlobal)
    , mAnimationName(aAnimationName)
    , mIsStylePaused(false)
    , mPauseShouldStick(false)
    , mPreviousPhaseOrIteration(PREVIOUS_PHASE_BEFORE)
  {
    // We might need to drop this assertion once we add a script-accessible
    // constructor but for animations generated from CSS markup the
    // animation-name should never be empty.
    MOZ_ASSERT(!mAnimationName.IsEmpty(), "animation-name should not be empty");
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  CSSAnimation* AsCSSAnimation() override { return this; }
  const CSSAnimation* AsCSSAnimation() const override { return this; }

  // CSSAnimation interface
  void GetAnimationName(nsString& aRetVal) const { aRetVal = mAnimationName; }

  // Alternative to GetAnimationName that returns a reference to the member
  // for more efficient internal usage.
  const nsString& AnimationName() const { return mAnimationName; }

  // Animation interface overrides
  virtual Promise* GetReady(ErrorResult& aRv) override;
  virtual void Play(ErrorResult& aRv, LimitBehavior aLimitBehavior) override;
  virtual void Pause(ErrorResult& aRv) override;

  virtual AnimationPlayState PlayStateFromJS() const override;
  virtual void PlayFromJS(ErrorResult& aRv) override;

  void PlayFromStyle();
  void PauseFromStyle();
  void CancelFromStyle() override
  {
    mOwningElement = OwningElementRef();
    Animation::CancelFromStyle();
    MOZ_ASSERT(mSequenceNum == kUnsequenced);
  }

  bool IsStylePaused() const { return mIsStylePaused; }

  bool HasLowerCompositeOrderThan(const Animation& aOther) const override;
  bool IsUsingCustomCompositeOrder() const override
  {
    return mOwningElement.IsSet();
  }

  void SetAnimationIndex(uint64_t aIndex)
  {
    MOZ_ASSERT(IsUsingCustomCompositeOrder());
    mSequenceNum = aIndex;
  }
  void CopyAnimationIndex(const CSSAnimation& aOther)
  {
    MOZ_ASSERT(IsUsingCustomCompositeOrder() &&
               aOther.IsUsingCustomCompositeOrder());
    mSequenceNum = aOther.mSequenceNum;
  }

  void QueueEvents(EventArray& aEventsToDispatch);

  // Returns the element or pseudo-element whose animation-name property
  // this CSSAnimation corresponds to (if any). This is used for determining
  // the relative composite order of animations generated from CSS markup.
  //
  // Typically this will be the same as the target element of the keyframe
  // effect associated with this animation. However, it can differ in the
  // following circumstances:
  //
  // a) If script removes or replaces the effect of this animation,
  // b) If this animation is cancelled (e.g. by updating the
  //    animation-name property or removing the owning element from the
  //    document),
  // c) If this object is generated from script using the CSSAnimation
  //    constructor.
  //
  // For (b) and (c) the returned owning element will return !IsSet().
  const OwningElementRef& OwningElement() const { return mOwningElement; }

  // Sets the owning element which is used for determining the composite
  // order of CSSAnimation objects generated from CSS markup.
  //
  // @see OwningElement()
  void SetOwningElement(const OwningElementRef& aElement)
  {
    mOwningElement = aElement;
  }

  // Is this animation currently in effect for the purposes of computing
  // mWinsInCascade.  (In general, this can be computed from the timing
  // function.  This boolean remembers the state as of the last time we
  // called UpdateCascadeResults so we know if it changes and we need to
  // call UpdateCascadeResults again.)
  bool mInEffectForCascadeResults;

protected:
  virtual ~CSSAnimation()
  {
    MOZ_ASSERT(!mOwningElement.IsSet(), "Owning element should be cleared "
                                        "before a CSS animation is destroyed");
  }
  virtual css::CommonAnimationManager* GetAnimationManager() const override;

  static nsString PseudoTypeAsString(nsCSSPseudoElements::Type aPseudoType);

  nsString mAnimationName;

  // The (pseudo-)element whose computed animation-name refers to this
  // animation (if any).
  OwningElementRef mOwningElement;

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
  // The base class, Animation already provides a boolean value,
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
    PREVIOUS_PHASE_BEFORE = uint64_t(-1),
    PREVIOUS_PHASE_AFTER = uint64_t(-2)
  };
  // One of the PREVIOUS_PHASE_* constants, or an integer for the iteration
  // whose start we last notified on.
  uint64_t mPreviousPhaseOrIteration;
};

} /* namespace dom */
} /* namespace mozilla */

class nsAnimationManager final
  : public mozilla::css::CommonAnimationManager
{
public:
  explicit nsAnimationManager(nsPresContext *aPresContext)
    : mozilla::css::CommonAnimationManager(aPresContext)
  {
  }

  void UpdateStyleAndEvents(mozilla::AnimationCollection* aEA,
                            mozilla::TimeStamp aRefreshTime,
                            mozilla::EnsureStyleRuleFlags aFlags);
  void QueueEvents(mozilla::AnimationCollection* aEA,
                   mozilla::EventArray &aEventsToDispatch);

  void MaybeUpdateCascadeResults(mozilla::AnimationCollection* aCollection);

  // nsIStyleRuleProcessor (parts)
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE override;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE override;

  // nsARefreshObserver
  virtual void WillRefresh(mozilla::TimeStamp aTime) override;

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

protected:
  virtual nsIAtom* GetAnimationsAtom() override {
    return nsGkAtoms::animationsProperty;
  }
  virtual nsIAtom* GetAnimationsBeforeAtom() override {
    return nsGkAtoms::animationsOfBeforeProperty;
  }
  virtual nsIAtom* GetAnimationsAfterAtom() override {
    return nsGkAtoms::animationsOfAfterProperty;
  }
  virtual bool IsAnimationManager() override {
    return true;
  }

private:
  void BuildAnimations(nsStyleContext* aStyleContext,
                       mozilla::dom::Element* aTarget,
                       mozilla::dom::AnimationTimeline* aTimeline,
                       mozilla::AnimationPtrArray& aAnimations);
  bool BuildSegment(InfallibleTArray<mozilla::AnimationPropertySegment>&
                      aSegments,
                    nsCSSProperty aProperty,
                    const mozilla::StyleAnimation& aAnimation,
                    float aFromKey, nsStyleContext* aFromContext,
                    mozilla::css::Declaration* aFromDeclaration,
                    float aToKey, nsStyleContext* aToContext);

  static void UpdateCascadeResults(nsStyleContext* aStyleContext,
                                   mozilla::AnimationCollection*
                                     aElementAnimations);

  // The guts of DispatchEvents
  void DoDispatchEvents();

  mozilla::EventArray mPendingEvents;
};

#endif /* !defined(nsAnimationManager_h_) */
