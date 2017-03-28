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
#include "mozilla/dom/Animation.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"

class nsIGlobalObject;
class nsStyleContext;
struct nsStyleDisplay;
struct ServoComputedValues;

namespace mozilla {
struct ServoComputedValuesWithParent;
namespace css {
class Declaration;
} /* namespace css */
namespace dom {
class KeyframeEffectReadOnly;
class Promise;
} /* namespace dom */

enum class CSSPseudoElementType : uint8_t;
struct NonOwningAnimationTarget;

struct AnimationEventInfo {
  RefPtr<dom::Element> mElement;
  RefPtr<dom::Animation> mAnimation;
  InternalAnimationEvent mEvent;
  TimeStamp mTimeStamp;

  AnimationEventInfo(dom::Element* aElement,
                     CSSPseudoElementType aPseudoType,
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
    mEvent.mAnimationName = aAnimationName;
    mEvent.mElapsedTime = aElapsedTime.ToSeconds();
    mEvent.mPseudoElement =
      AnimationCollection<dom::CSSAnimation>::PseudoTypeAsString(aPseudoType);
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
    , mPreviousPhase(ComputedTiming::AnimationPhase::Idle)
    , mPreviousIteration(0)
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

    // We need to do this *after* calling CancelFromStyle() since
    // CancelFromStyle might synchronously trigger a cancel event for which
    // we need an owning element to target the event at.
    mOwningElement = OwningElementRef();
  }

  void Tick() override;
  void QueueEvents(StickyTimeDuration aActiveTime = StickyTimeDuration());

  bool IsStylePaused() const { return mIsStylePaused; }

  bool HasLowerCompositeOrderThan(const CSSAnimation& aOther) const;

  void SetAnimationIndex(uint64_t aIndex)
  {
    MOZ_ASSERT(IsTiedToMarkup());
    if (IsRelevant() &&
        mAnimationIndex != aIndex) {
      nsNodeUtils::AnimationChanged(this);
      PostUpdate();
    }
    mAnimationIndex = aIndex;
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

  void MaybeQueueCancelEvent(StickyTimeDuration aActiveTime) override {
    QueueEvents(aActiveTime);
  }

protected:
  virtual ~CSSAnimation()
  {
    MOZ_ASSERT(!mOwningElement.IsSet(), "Owning element should be cleared "
                                        "before a CSS animation is destroyed");
  }

  // Animation overrides
  void UpdateTiming(SeekFlag aSeekFlag,
                    SyncNotifyFlag aSyncNotifyFlag) override;

  // Returns the duration from the start of the animation's source effect's
  // active interval to the point where the animation actually begins playback.
  // This is zero unless the animation's source effect has a negative delay in
  // which case it is the absolute value of that delay.
  // This is used for setting the elapsedTime member of CSS AnimationEvents.
  TimeDuration InitialAdvance() const {
    return mEffect ?
           std::max(TimeDuration(), mEffect->SpecifiedTiming().mDelay * -1) :
           TimeDuration();
  }

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

  // Phase and current iteration from the previous time we queued events.
  // This is used to determine what new events to dispatch.
  ComputedTiming::AnimationPhase mPreviousPhase;
  uint64_t mPreviousIteration;
};

} /* namespace dom */

template <>
struct AnimationTypeTraits<dom::CSSAnimation>
{
  static nsIAtom* ElementPropertyAtom()
  {
    return nsGkAtoms::animationsProperty;
  }
  static nsIAtom* BeforePropertyAtom()
  {
    return nsGkAtoms::animationsOfBeforeProperty;
  }
  static nsIAtom* AfterPropertyAtom()
  {
    return nsGkAtoms::animationsOfAfterProperty;
  }
};

} /* namespace mozilla */

class nsAnimationManager final
  : public mozilla::CommonAnimationManager<mozilla::dom::CSSAnimation>
{
public:
  explicit nsAnimationManager(nsPresContext *aPresContext)
    : mozilla::CommonAnimationManager<mozilla::dom::CSSAnimation>(aPresContext)
  {
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsAnimationManager)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsAnimationManager)

  typedef mozilla::AnimationCollection<mozilla::dom::CSSAnimation>
    CSSAnimationCollection;
  typedef nsTArray<RefPtr<mozilla::dom::CSSAnimation>>
    OwningCSSAnimationPtrArray;

  /**
   * Update the set of animations on |aElement| based on |aStyleContext|.
   * If necessary, this will notify the corresponding EffectCompositor so
   * that it can update its animation rule.
   *
   * aStyleContext may be a style context for aElement or for its
   * :before or :after pseudo-element.
   */
  void UpdateAnimations(nsStyleContext* aStyleContext,
                        mozilla::dom::Element* aElement);

  /**
   * This function does the same thing as the above UpdateAnimations()
   * but with servo's computed values.
   */
  void UpdateAnimations(
    mozilla::dom::Element* aElement,
    mozilla::CSSPseudoElementType aPseudoType,
    const mozilla::ServoComputedValuesWithParent& aServoValues);

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
  void DispatchEvents()
  {
    RefPtr<nsAnimationManager> kungFuDeathGrip(this);
    mEventDispatcher.DispatchEvents(mPresContext);
  }
  void SortEvents()      { mEventDispatcher.SortEvents(); }
  void ClearEventQueue() { mEventDispatcher.ClearEventQueue(); }

protected:
  ~nsAnimationManager() override = default;

private:
  template<class BuilderType>
  void DoUpdateAnimations(
    const mozilla::NonOwningAnimationTarget& aTarget,
    const nsStyleDisplay& aStyleDisplay,
    BuilderType& aBuilder);

  mozilla::DelayedEventDispatcher<mozilla::AnimationEventInfo> mEventDispatcher;
};

#endif /* !defined(nsAnimationManager_h_) */
