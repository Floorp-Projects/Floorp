/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSAnimation_h
#define mozilla_dom_CSSAnimation_h

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "mozilla/dom/MutationObservers.h"
#include "mozilla/StyleAnimationValue.h"
#include "AnimationCommon.h"

namespace mozilla {
// Properties of CSS Animations that can be overridden by the Web Animations API
// in a manner that means we should ignore subsequent changes to markup for that
// property.
enum class CSSAnimationProperties {
  None = 0,
  Keyframes = 1 << 0,
  Duration = 1 << 1,
  IterationCount = 1 << 2,
  Direction = 1 << 3,
  Delay = 1 << 4,
  FillMode = 1 << 5,
  Composition = 1 << 6,
  Effect = Keyframes | Duration | IterationCount | Direction | Delay |
           FillMode | Composition,
  PlayState = 1 << 7,
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(CSSAnimationProperties)

namespace dom {

class CSSAnimation final : public Animation {
 public:
  explicit CSSAnimation(nsIGlobalObject* aGlobal, nsAtom* aAnimationName)
      : dom::Animation(aGlobal),
        mAnimationName(aAnimationName),
        mNeedsNewAnimationIndexWhenRun(false),
        mPreviousPhase(ComputedTiming::AnimationPhase::Idle),
        mPreviousIteration(0) {
    // We might need to drop this assertion once we add a script-accessible
    // constructor but for animations generated from CSS markup the
    // animation-name should never be empty.
    MOZ_ASSERT(mAnimationName != nsGkAtoms::_empty,
               "animation-name should not be 'none'");
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  CSSAnimation* AsCSSAnimation() override { return this; }
  const CSSAnimation* AsCSSAnimation() const override { return this; }

  // CSSAnimation interface
  void GetAnimationName(nsString& aRetVal) const {
    mAnimationName->ToString(aRetVal);
  }

  nsAtom* AnimationName() const { return mAnimationName; }

  // Animation interface overrides
  void SetEffect(AnimationEffect* aEffect) override;
  void SetStartTimeAsDouble(const Nullable<double>& aStartTime) override;
  Promise* GetReady(ErrorResult& aRv) override;
  void Reverse(ErrorResult& aRv) override;

  // NOTE: tabbrowser.xml currently relies on the fact that reading the
  // currentTime of a CSSAnimation does *not* flush style (whereas reading the
  // playState does). If CSS Animations 2 specifies that reading currentTime
  // also flushes style we will need to find another way to detect canceled
  // animations in tabbrowser.xml. On the other hand, if CSS Animations 2
  // specifies that reading playState does *not* flush style (and we drop the
  // following override), then we should update tabbrowser.xml to check
  // the playState instead.
  AnimationPlayState PlayStateFromJS() const override;
  bool PendingFromJS() const override;
  void PlayFromJS(ErrorResult& aRv) override;
  void PauseFromJS(ErrorResult& aRv) override;

  void PlayFromStyle();
  void PauseFromStyle();
  void CancelFromStyle(PostRestyleMode aPostRestyle) {
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

    Animation::Cancel(aPostRestyle);

    // We need to do this *after* calling Cancel() since
    // Cancel() might synchronously trigger a cancel event for which
    // we need an owning element to target the event at.
    mOwningElement = OwningElementRef();
  }

  void Tick(TickState&) override;
  void QueueEvents(
      const StickyTimeDuration& aActiveTime = StickyTimeDuration());

  bool HasLowerCompositeOrderThan(const CSSAnimation& aOther) const;

  void SetAnimationIndex(uint64_t aIndex) {
    MOZ_ASSERT(IsTiedToMarkup());
    if (IsRelevant() && mAnimationIndex != aIndex) {
      MutationObservers::NotifyAnimationChanged(this);
      PostUpdate();
    }
    mAnimationIndex = aIndex;
  }

  // Sets the owning element which is used for determining the composite
  // order of CSSAnimation objects generated from CSS markup.
  //
  // @see mOwningElement
  void SetOwningElement(const OwningElementRef& aElement) {
    mOwningElement = aElement;
  }
  // True for animations that are generated from CSS markup and continue to
  // reflect changes to that markup.
  bool IsTiedToMarkup() const { return mOwningElement.IsSet(); }

  void MaybeQueueCancelEvent(const StickyTimeDuration& aActiveTime) override {
    QueueEvents(aActiveTime);
  }

  CSSAnimationProperties GetOverriddenProperties() const {
    return mOverriddenProperties;
  }
  void AddOverriddenProperties(CSSAnimationProperties aProperties) {
    mOverriddenProperties |= aProperties;
  }

 protected:
  virtual ~CSSAnimation() {
    MOZ_ASSERT(!mOwningElement.IsSet(),
               "Owning element should be cleared "
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
    return mEffect ? std::max(TimeDuration(),
                              mEffect->NormalizedTiming().Delay() * -1)
                   : TimeDuration();
  }

  RefPtr<nsAtom> mAnimationName;

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

  // When true, indicates that when this animation next leaves the idle state,
  // its animation index should be updated.
  bool mNeedsNewAnimationIndexWhenRun;

  // Phase and current iteration from the previous time we queued events.
  // This is used to determine what new events to dispatch.
  ComputedTiming::AnimationPhase mPreviousPhase;
  uint64_t mPreviousIteration;

  // Properties that would normally be defined by the cascade but which have
  // since been explicitly set via the Web Animations API.
  CSSAnimationProperties mOverriddenProperties = CSSAnimationProperties::None;
};

// A subclass of KeyframeEffect that reports when specific properties have been
// overridden via the Web Animations API.
class CSSAnimationKeyframeEffect : public KeyframeEffect {
 public:
  CSSAnimationKeyframeEffect(Document* aDocument,
                             OwningAnimationTarget&& aTarget,
                             TimingParams&& aTiming,
                             const KeyframeEffectParams& aOptions)
      : KeyframeEffect(aDocument, std::move(aTarget), std::move(aTiming),
                       aOptions) {}

  void GetTiming(EffectTiming& aRetVal) const override;
  void GetComputedTimingAsDict(ComputedEffectTiming& aRetVal) const override;
  void UpdateTiming(const OptionalEffectTiming& aTiming,
                    ErrorResult& aRv) override;
  void SetKeyframes(JSContext* aContext, JS::Handle<JSObject*> aKeyframes,
                    ErrorResult& aRv) override;
  void SetComposite(const CompositeOperation& aComposite) override;

 private:
  CSSAnimation* GetOwningCSSAnimation() {
    return mAnimation ? mAnimation->AsCSSAnimation() : nullptr;
  }
  const CSSAnimation* GetOwningCSSAnimation() const {
    return mAnimation ? mAnimation->AsCSSAnimation() : nullptr;
  }

  // Flushes styles if our owning animation is a CSSAnimation
  void MaybeFlushUnanimatedStyle() const;
};

}  // namespace dom

}  // namespace mozilla

#endif  // mozilla_dom_CSSAnimation_h
