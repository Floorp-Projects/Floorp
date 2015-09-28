/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsAnimationManager_h_
#define nsAnimationManager_h_

#include "mozilla/Attributes.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventForwards.h"
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
  nsRefPtr<dom::Element> mElement;
  nsRefPtr<dom::Animation> mAnimation;
  InternalAnimationEvent mEvent;
  TimeStamp mTimeStamp;

  AnimationEventInfo(dom::Element* aElement,
                     nsCSSPseudoElements::Type aPseudoType,
                     EventMessage aMessage,
                     const nsSubstring& aAnimationName,
                     const StickyTimeDuration& aElapsedTime,
                     const TimeStamp& aTimeStamp,
                     dom::Animation* aAnimation)
    : mElement(aElement)
    , mAnimation(aAnimation)
    , mEvent(true, aMessage)
    , mTimeStamp(aTimeStamp)
  {
    // XXX Looks like nobody initialize WidgetEvent::time
    mEvent.animationName = aAnimationName;
    mEvent.elapsedTime = aElapsedTime.ToSeconds();
    mEvent.pseudoElement = AnimationCollection::PseudoTypeAsString(aPseudoType);
  }

  // InternalAnimationEvent doesn't support copy-construction, so we need
  // to ourselves in order to work with nsTArray
  AnimationEventInfo(const AnimationEventInfo& aOther)
    : mElement(aOther.mElement)
    , mAnimation(aOther.mAnimation)
    , mEvent(true, aOther.mEvent.mMessage)
    , mTimeStamp(aOther.mTimeStamp)
  {
    mEvent.AssignAnimationEventData(aOther.mEvent, false);
  }
};

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
    , mNeedsNewAnimationIndexWhenRun(false)
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

    // When an animation is disassociated with style it enters an odd state
    // where its composite order is undefined until it first transitions
    // out of the idle state.
    //
    // Even if the composite order isn't defined we don't want it to be random
    // in case we need to determine the order to dispatch events associated
    // with an animation in this state. To solve this we treat the animation as
    // if it had been added to the end of the global animation list so that
    // its sort order is defined. We'll update this index again once the
    // animation leaves the idle state.
    mAnimationIndex = sNextAnimationIndex++;
    mNeedsNewAnimationIndexWhenRun = true;

    Animation::CancelFromStyle();
  }

  void Tick() override;
  void QueueEvents();
  bool HasEndEventToQueue() const override;

  bool IsStylePaused() const { return mIsStylePaused; }

  bool HasLowerCompositeOrderThan(const Animation& aOther) const override;

  void SetAnimationIndex(uint64_t aIndex)
  {
    MOZ_ASSERT(IsTiedToMarkup());
    mAnimationIndex = aIndex;
  }
  void CopyAnimationIndex(const CSSAnimation& aOther)
  {
    MOZ_ASSERT(IsTiedToMarkup() && aOther.IsTiedToMarkup());
    mAnimationIndex = aOther.mAnimationIndex;
  }

  // Sets the owning element which is used for determining the composite
  // order of CSSAnimation objects generated from CSS markup.
  //
  // @see mOwningElement
  void SetOwningElement(const OwningElementRef& aElement)
  {
    mOwningElement = aElement;
  }
  // True for animations that are generated from CSS markup and continue to
  // reflect changes to that markup.
  bool IsTiedToMarkup() const { return mOwningElement.IsSet(); }

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

  // Animation overrides
  CommonAnimationManager* GetAnimationManager() const override;
  void UpdateTiming(SeekFlag aSeekFlag,
                    SyncNotifyFlag aSyncNotifyFlag) override;

  // Returns the duration from the start of the animation's source effect's
  // active interval to the point where the animation actually begins playback.
  // This is zero unless the animation's source effect has a negative delay in
  // which // case it is the absolute value of that delay.
  // This is used for setting the elapsedTime member of CSS AnimationEvents.
  TimeDuration InitialAdvance() const {
    return mEffect ?
           std::max(TimeDuration(), mEffect->Timing().mDelay * -1) :
           TimeDuration();
  }
  // Converts an AnimationEvent's elapsedTime value to an equivalent TimeStamp
  // that can be used to sort events by when they occurred.
  TimeStamp ElapsedTimeToTimeStamp(const StickyTimeDuration& aElapsedTime)
    const;

  nsString mAnimationName;

  // The (pseudo-)element whose computed animation-name refers to this
  // animation (if any).
  //
  // This is used for determining the relative composite order of animations
  // generated from CSS markup.
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
  // For (b) and (c) the owning element will return !IsSet().
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

  // When true, indicates that when this animation next leaves the idle state,
  // its animation index should be updated.
  bool mNeedsNewAnimationIndexWhenRun;

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
  : public mozilla::CommonAnimationManager
{
public:
  explicit nsAnimationManager(nsPresContext *aPresContext)
    : mozilla::CommonAnimationManager(aPresContext)
  {
  }

  NS_DECL_CYCLE_COLLECTION_CLASS(nsAnimationManager)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  void MaybeUpdateCascadeResults(mozilla::AnimationCollection* aCollection);

  // nsIStyleRuleProcessor (parts)
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE override;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE override;

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
   * Add a pending event.
   */
  void QueueEvent(mozilla::AnimationEventInfo&& aEventInfo)
  {
    mEventDispatcher.QueueEvent(
      mozilla::Forward<mozilla::AnimationEventInfo>(aEventInfo));
  }

  /**
   * Dispatch any pending events.  We accumulate animationend and
   * animationiteration events only during refresh driver notifications
   * (and dispatch them at the end of such notifications), but we
   * accumulate animationstart events at other points when style
   * contexts are created.
   */
  void DispatchEvents()  { mEventDispatcher.DispatchEvents(mPresContext); }
  void SortEvents()      { mEventDispatcher.SortEvents(); }
  void ClearEventQueue() { mEventDispatcher.ClearEventQueue(); }

  // Stop animations on the element. This method takes the real element
  // rather than the element for the generated content for animations on
  // ::before and ::after.
  void StopAnimationsForElement(mozilla::dom::Element* aElement,
                                nsCSSPseudoElements::Type aPseudoType);

  bool IsAnimationManager() override {
    return true;
  }

protected:
  virtual ~nsAnimationManager() {}

  virtual nsIAtom* GetAnimationsAtom() override {
    return nsGkAtoms::animationsProperty;
  }
  virtual nsIAtom* GetAnimationsBeforeAtom() override {
    return nsGkAtoms::animationsOfBeforeProperty;
  }
  virtual nsIAtom* GetAnimationsAfterAtom() override {
    return nsGkAtoms::animationsOfAfterProperty;
  }

  mozilla::DelayedEventDispatcher<mozilla::AnimationEventInfo> mEventDispatcher;

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
};

#endif /* !defined(nsAnimationManager_h_) */
