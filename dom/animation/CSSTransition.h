/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSTransition_h
#define mozilla_dom_CSSTransition_h

#include "mozilla/dom/Animation.h"
#include "mozilla/StyleAnimationValue.h"
#include "AnimationCommon.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class CSSTransition final : public Animation {
 public:
  explicit CSSTransition(nsIGlobalObject* aGlobal)
      : dom::Animation(aGlobal),
        mPreviousTransitionPhase(TransitionPhase::Idle),
        mNeedsNewAnimationIndexWhenRun(false),
        mTransitionProperty(eCSSProperty_UNKNOWN) {}

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  CSSTransition* AsCSSTransition() override { return this; }
  const CSSTransition* AsCSSTransition() const override { return this; }

  // CSSTransition interface
  void GetTransitionProperty(nsString& aRetVal) const;

  // Animation interface overrides
  AnimationPlayState PlayStateFromJS() const override;
  bool PendingFromJS() const override;
  void PlayFromJS(ErrorResult& aRv) override;

  // A variant of Play() that avoids posting style updates since this method
  // is expected to be called whilst already updating style.
  void PlayFromStyle() {
    ErrorResult rv;
    PlayNoUpdate(rv, Animation::LimitBehavior::Continue);
    // play() should not throw when LimitBehavior is Continue
    MOZ_ASSERT(!rv.Failed(), "Unexpected exception playing transition");
  }

  void CancelFromStyle(PostRestyleMode aPostRestyle) {
    // The animation index to use for compositing will be established when
    // this transition next transitions out of the idle state but we still
    // update it now so that the sort order of this transition remains
    // defined until that moment.
    //
    // See longer explanation in CSSAnimation::CancelFromStyle.
    mAnimationIndex = sNextAnimationIndex++;
    mNeedsNewAnimationIndexWhenRun = true;

    Animation::Cancel(aPostRestyle);

    // It is important we do this *after* calling Cancel().
    // This is because Cancel() will end up posting a restyle and
    // that restyle should target the *transitions* level of the cascade.
    // However, once we clear the owning element, CascadeLevel() will begin
    // returning CascadeLevel::Animations.
    mOwningElement = OwningElementRef();
  }

  void SetEffectFromStyle(AnimationEffect* aEffect);

  void Tick() override;

  nsCSSPropertyID TransitionProperty() const;
  AnimationValue ToValue() const;

  bool HasLowerCompositeOrderThan(const CSSTransition& aOther) const;
  EffectCompositor::CascadeLevel CascadeLevel() const override {
    return IsTiedToMarkup() ? EffectCompositor::CascadeLevel::Transitions
                            : EffectCompositor::CascadeLevel::Animations;
  }

  void SetCreationSequence(uint64_t aIndex) {
    MOZ_ASSERT(IsTiedToMarkup());
    mAnimationIndex = aIndex;
  }

  // Sets the owning element which is used for determining the composite
  // oder of CSSTransition objects generated from CSS markup.
  //
  // @see mOwningElement
  void SetOwningElement(const OwningElementRef& aElement) {
    mOwningElement = aElement;
  }
  // True for transitions that are generated from CSS markup and continue to
  // reflect changes to that markup.
  bool IsTiedToMarkup() const { return mOwningElement.IsSet(); }

  // Return the animation current time based on a given TimeStamp, a given
  // start time and a given playbackRate on a given timeline.  This is useful
  // when we estimate the current animated value running on the compositor
  // because the animation on the compositor may be running ahead while
  // main-thread is busy.
  static Nullable<TimeDuration> GetCurrentTimeAt(
      const AnimationTimeline& aTimeline, const TimeStamp& aBaseTime,
      const TimeDuration& aStartTime, double aPlaybackRate);

  void MaybeQueueCancelEvent(const StickyTimeDuration& aActiveTime) override {
    QueueEvents(aActiveTime);
  }

  // Compute the portion of the *value* space that we should be through
  // at the current time.  (The input to the transition timing function
  // has time units, the output has value units.)
  double CurrentValuePortion() const;

  const AnimationValue& StartForReversingTest() const {
    return mStartForReversingTest;
  }
  double ReversePortion() const { return mReversePortion; }

  void SetReverseParameters(AnimationValue&& aStartForReversingTest,
                            double aReversePortion) {
    mStartForReversingTest = std::move(aStartForReversingTest);
    mReversePortion = aReversePortion;
  }

  struct ReplacedTransitionProperties {
    TimeDuration mStartTime;
    double mPlaybackRate;
    TimingParams mTiming;
    Maybe<ComputedTimingFunction> mTimingFunction;
    AnimationValue mFromValue, mToValue;
  };
  void SetReplacedTransition(
      ReplacedTransitionProperties&& aReplacedTransition) {
    mReplacedTransition.emplace(std::move(aReplacedTransition));
  }

  // For a new transition interrupting an existing transition on the
  // compositor, update the start value to match the value of the replaced
  // transitions at the current time.
  void UpdateStartValueFromReplacedTransition();

 protected:
  virtual ~CSSTransition() {
    MOZ_ASSERT(!mOwningElement.IsSet(),
               "Owning element should be cleared "
               "before a CSS transition is destroyed");
  }

  // Animation overrides
  void UpdateTiming(SeekFlag aSeekFlag,
                    SyncNotifyFlag aSyncNotifyFlag) override;

  void QueueEvents(const StickyTimeDuration& activeTime = StickyTimeDuration());

  enum class TransitionPhase;

  // The (pseudo-)element whose computed transition-property refers to this
  // transition (if any).
  //
  // This is used for determining the relative composite order of transitions
  // generated from CSS markup.
  //
  // Typically this will be the same as the target element of the keyframe
  // effect associated with this transition. However, it can differ in the
  // following circumstances:
  //
  // a) If script removes or replaces the effect of this transition,
  // b) If this transition is cancelled (e.g. by updating the
  //    transition-property or removing the owning element from the document),
  // c) If this object is generated from script using the CSSTransition
  //    constructor.
  //
  // For (b) and (c) the owning element will return !IsSet().
  OwningElementRef mOwningElement;

  // The 'transition phase' used to determine which transition events need
  // to be queued on this tick.
  // See: https://drafts.csswg.org/css-transitions-2/#transition-phase
  enum class TransitionPhase {
    Idle = static_cast<int>(ComputedTiming::AnimationPhase::Idle),
    Before = static_cast<int>(ComputedTiming::AnimationPhase::Before),
    Active = static_cast<int>(ComputedTiming::AnimationPhase::Active),
    After = static_cast<int>(ComputedTiming::AnimationPhase::After),
    Pending
  };
  TransitionPhase mPreviousTransitionPhase;

  // When true, indicates that when this transition next leaves the idle state,
  // its animation index should be updated.
  bool mNeedsNewAnimationIndexWhenRun;

  // Store the transition property and to-value here since we need that
  // information in order to determine if there is an existing transition
  // for a given style change. We can't store that information on the
  // effect however since it can be replaced using the Web Animations API.
  nsCSSPropertyID mTransitionProperty;
  AnimationValue mTransitionToValue;

  // This is the start value to be used for a check for whether a
  // transition is being reversed.  Normally the same as
  // mEffect->mProperties[0].mSegments[0].mFromValue, except when this
  // transition started as the reversal of another in-progress transition
  // or when the effect has been mutated using the Web Animations API.
  //
  // Needed so we can handle two reverses in a row.
  AnimationValue mStartForReversingTest;

  // Likewise, the portion (in value space) of the "full" reversed
  // transition that we're actually covering.  For example, if a :hover
  // effect has a transition that moves the element 10px to the right
  // (by changing 'left' from 0px to 10px), and the mouse moves in to
  // the element (starting the transition) but then moves out after the
  // transition has advanced 4px, the second transition (from 10px/4px
  // to 0px) will have mReversePortion of 0.4.  (If the mouse then moves
  // in again when the transition is back to 2px, the mReversePortion
  // for the third transition (from 0px/2px to 10px) will be 0.8.
  double mReversePortion = 1.0;

  Maybe<ReplacedTransitionProperties> mReplacedTransition;
};

}  // namespace dom

template <>
struct AnimationTypeTraits<dom::CSSTransition> {
  static nsAtom* ElementPropertyAtom() {
    return nsGkAtoms::transitionsProperty;
  }
  static nsAtom* BeforePropertyAtom() {
    return nsGkAtoms::transitionsOfBeforeProperty;
  }
  static nsAtom* AfterPropertyAtom() {
    return nsGkAtoms::transitionsOfAfterProperty;
  }
  static nsAtom* MarkerPropertyAtom() {
    return nsGkAtoms::transitionsOfMarkerProperty;
  }
};

}  // namespace mozilla

#endif  // mozilla_dom_CSSTransition_h
