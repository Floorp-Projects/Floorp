/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_KeyframeEffect_h
#define mozilla_dom_KeyframeEffect_h

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCSSPseudoElements.h"
#include "nsIDocument.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"
#include "mozilla/ComputedTimingFunction.h" // ComputedTimingFunction
#include "mozilla/LayerAnimationInfo.h"     // LayerAnimations::kRecords
#include "mozilla/StickyTimeDuration.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/AnimationEffectReadOnly.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/KeyframeBinding.h"
#include "mozilla/dom/Nullable.h"

struct JSContext;
class nsCSSPropertySet;
class nsIContent;
class nsIDocument;
class nsIFrame;
class nsPresContext;

namespace mozilla {

struct AnimationCollection;
class AnimValuesStyleRule;

namespace dom {
struct ComputedTimingProperties;
class UnrestrictedDoubleOrKeyframeEffectOptions;
enum class IterationCompositeOperation : uint32_t;
enum class CompositeOperation : uint32_t;
}

/**
 * Input timing parameters.
 *
 * Eventually this will represent all the input timing parameters specified
 * by content but for now it encapsulates just the subset of those
 * parameters passed to GetPositionInIteration.
 */
struct AnimationTiming
{
  TimeDuration mIterationDuration;
  TimeDuration mDelay;
  float mIterationCount; // mozilla::PositiveInfinity<float>() means infinite
  dom::PlaybackDirection mDirection;
  dom::FillMode mFillMode;

  bool FillsForwards() const;
  bool FillsBackwards() const;
  bool operator==(const AnimationTiming& aOther) const {
    return mIterationDuration == aOther.mIterationDuration &&
           mDelay == aOther.mDelay &&
           mIterationCount == aOther.mIterationCount &&
           mDirection == aOther.mDirection &&
           mFillMode == aOther.mFillMode;
  }
  bool operator!=(const AnimationTiming& aOther) const {
    return !(*this == aOther);
  }
};

/**
 * Stores the results of calculating the timing properties of an animation
 * at a given sample time.
 */
struct ComputedTiming
{
  // The total duration of the animation including all iterations.
  // Will equal StickyTimeDuration::Forever() if the animation repeats
  // indefinitely.
  StickyTimeDuration  mActiveDuration;
  // Progress towards the end of the current iteration. If the effect is
  // being sampled backwards, this will go from 1.0 to 0.0.
  // Will be null if the animation is neither animating nor
  // filling at the sampled time.
  Nullable<double>    mProgress;
  // Zero-based iteration index (meaningless if mProgress is null).
  uint64_t            mCurrentIteration = 0;

  enum class AnimationPhase {
    Null,   // Not sampled (null sample time)
    Before, // Sampled prior to the start of the active interval
    Active, // Sampled within the active interval
    After   // Sampled after (or at) the end of the active interval
  };
  AnimationPhase      mPhase = AnimationPhase::Null;
};

struct AnimationPropertySegment
{
  float mFromKey, mToKey;
  StyleAnimationValue mFromValue, mToValue;
  ComputedTimingFunction mTimingFunction;

  bool operator==(const AnimationPropertySegment& aOther) const {
    return mFromKey == aOther.mFromKey &&
           mToKey == aOther.mToKey &&
           mFromValue == aOther.mFromValue &&
           mToValue == aOther.mToValue &&
           mTimingFunction == aOther.mTimingFunction;
  }
  bool operator!=(const AnimationPropertySegment& aOther) const {
    return !(*this == aOther);
  }
};

struct AnimationProperty
{
  nsCSSProperty mProperty = eCSSProperty_UNKNOWN;

  // Does this property win in the CSS Cascade?
  //
  // For CSS transitions, this is true as long as a CSS animation on the
  // same property and element is not running, in which case we set this
  // to false so that the animation (lower in the cascade) can win.  We
  // then use this to decide whether to apply the style both in the CSS
  // cascade and for OMTA.
  //
  // For CSS Animations, which are overridden by !important rules in the
  // cascade, we actually determine this from the CSS cascade
  // computations, and then use it for OMTA.
  // **NOTE**: For CSS animations, we only bother setting mWinsInCascade
  // accurately for properties that we can animate on the compositor.
  // For other properties, we make it always be true.
  // **NOTE 2**: This member is not included when comparing AnimationProperty
  // objects for equality.
  bool mWinsInCascade = true;

  // If true, the propery is currently being animated on the compositor.
  //
  // Note that when the owning Animation requests a non-throttled restyle, in
  // between calling RequestRestyle on its AnimationCollection and when the
  // restyle is performed, this member may temporarily become false even if
  // the animation remains on the layer after the restyle.
  bool mIsRunningOnCompositor = false;

  InfallibleTArray<AnimationPropertySegment> mSegments;

  // NOTE: This operator does *not* compare the mWinsInCascade member *or* the
  // mIsRunningOnCompositor member.
  // This is because AnimationProperty objects are compared when recreating
  // CSS animations to determine if mutation observer change records need to
  // be created or not. However, at the point when these objects are compared
  // neither the mWinsInCascade nor the mIsRunningOnCompositor will have been
  // set on the new objects so we ignore these members to avoid generating
  // spurious change records.
  bool operator==(const AnimationProperty& aOther) const {
    return mProperty == aOther.mProperty &&
           mSegments == aOther.mSegments;
  }
  bool operator!=(const AnimationProperty& aOther) const {
    return !(*this == aOther);
  }
};

struct ElementPropertyTransition;

namespace dom {

class Animation;

class KeyframeEffectReadOnly : public AnimationEffectReadOnly
{
public:
  KeyframeEffectReadOnly(nsIDocument* aDocument,
                         Element* aTarget,
                         nsCSSPseudoElements::Type aPseudoType,
                         const AnimationTiming& aTiming);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(KeyframeEffectReadOnly,
                                                        AnimationEffectReadOnly)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  virtual ElementPropertyTransition* AsTransition() { return nullptr; }
  virtual const ElementPropertyTransition* AsTransition() const
  {
    return nullptr;
  }

  // KeyframeEffectReadOnly interface
  static already_AddRefed<KeyframeEffectReadOnly>
  Constructor(const GlobalObject& aGlobal,
              Element* aTarget,
              const Optional<JS::Handle<JSObject*>>& aFrames,
              const UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
              ErrorResult& aRv);
  Element* GetTarget() const {
    // Currently we never return animations from the API whose effect
    // targets a pseudo-element so this should never be called when
    // mPseudoType is not 'none' (see bug 1174575).
    MOZ_ASSERT(mPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement,
               "Requesting the target of a KeyframeEffect that targets a"
               " pseudo-element is not yet supported.");
    return mTarget;
  }
  void GetFrames(JSContext*& aCx,
                 nsTArray<JSObject*>& aResult,
                 ErrorResult& aRv);

  // Temporary workaround to return both the target element and pseudo-type
  // until we implement PseudoElement (bug 1174575).
  void GetTarget(Element*& aTarget,
                 nsCSSPseudoElements::Type& aPseudoType) const {
    aTarget = mTarget;
    aPseudoType = mPseudoType;
  }

  IterationCompositeOperation IterationComposite() const;
  CompositeOperation Composite() const;
  void GetSpacing(nsString& aRetVal) const {
    aRetVal.AssignLiteral("distribute");
  }

  const AnimationTiming& Timing() const { return mTiming; }
  AnimationTiming& Timing() { return mTiming; }
  void SetTiming(const AnimationTiming& aTiming);
  void NotifyAnimationTimingUpdated();

  Nullable<TimeDuration> GetLocalTime() const;

  // This function takes as input the timing parameters of an animation and
  // returns the computed timing at the specified local time.
  //
  // The local time may be null in which case only static parameters such as the
  // active duration are calculated. All other members of the returned object
  // are given a null/initial value.
  //
  // This function returns a null mProgress member of the return value
  // if the animation should not be run
  // (because it is not currently active and is not filling at this time).
  static ComputedTiming
  GetComputedTimingAt(const Nullable<TimeDuration>& aLocalTime,
                      const AnimationTiming& aTiming);

  // Shortcut for that gets the computed timing using the current local time as
  // calculated from the timeline time.
  ComputedTiming
  GetComputedTiming(const AnimationTiming* aTiming = nullptr) const
  {
    return GetComputedTimingAt(GetLocalTime(), aTiming ? *aTiming : mTiming);
  }

  void
  GetComputedTimingAsDict(ComputedTimingProperties& aRetVal) const override;

  // Return the duration of the active interval for the given timing parameters.
  static StickyTimeDuration
  ActiveDuration(const AnimationTiming& aTiming);

  bool IsInPlay() const;
  bool IsCurrent() const;
  bool IsInEffect() const;

  void SetAnimation(Animation* aAnimation);
  Animation* GetAnimation() const { return mAnimation; }

  const AnimationProperty*
  GetAnimationOfProperty(nsCSSProperty aProperty) const;
  bool HasAnimationOfProperty(nsCSSProperty aProperty) const {
    return GetAnimationOfProperty(aProperty) != nullptr;
  }
  bool HasAnimationOfProperties(const nsCSSProperty* aProperties,
                                size_t aPropertyCount) const;
  const InfallibleTArray<AnimationProperty>& Properties() const {
    return mProperties;
  }
  InfallibleTArray<AnimationProperty>& Properties() {
    return mProperties;
  }

  // Updates |aStyleRule| with the animation values produced by this
  // AnimationEffect for the current time except any properties already
  // contained in |aSetProperties|.
  // Any updated properties are added to |aSetProperties|.
  void ComposeStyle(RefPtr<AnimValuesStyleRule>& aStyleRule,
                    nsCSSPropertySet& aSetProperties);
  // Returns true if at least one property is being animated on compositor.
  bool IsRunningOnCompositor() const;
  void SetIsRunningOnCompositor(nsCSSProperty aProperty, bool aIsRunning);

  bool CanThrottle() const;

  // Returns true if this effect, applied to |aFrame|, contains
  // properties that mean we shouldn't run *any* compositor animations on this
  // element.
  //
  // For example, if we have an animation of geometric properties like 'left'
  // and 'top' on an element, we force all 'transform' and 'opacity' animations
  // running at the same time on the same element to run on the main thread.
  //
  // Similarly, some transform animations cannot be run on the compositor and
  // when that is the case we simply disable all compositor animations
  // on the same element.
  //
  // Bug 1218620 - It seems like we don't need to be this restrictive. Wouldn't
  // it be ok to do 'opacity' animations on the compositor in either case?
  bool ShouldBlockCompositorAnimations(const nsIFrame* aFrame) const;

  nsIDocument* GetRenderedDocument() const;
  nsPresContext* GetPresContext() const;

  inline AnimationCollection* GetCollection() const;

protected:
  virtual ~KeyframeEffectReadOnly();
  void ResetIsRunningOnCompositor();

  // This effect is registered with its target element so long as:
  //
  // (a) It has a target element, and
  // (b) It is "relevant" (i.e. yet to finish but not idle, or finished but
  //     filling forwards)
  //
  // As a result, we need to make sure this gets called whenever anything
  // changes with regards to this effects's timing including changes to the
  // owning Animation's timing.
  void UpdateTargetRegistration();

  static AnimationTiming ConvertKeyframeEffectOptions(
    const UnrestrictedDoubleOrKeyframeEffectOptions& aOptions);

  static void BuildAnimationPropertyList(
    JSContext* aCx,
    Element* aTarget,
    const Optional<JS::Handle<JSObject*>>& aFrames,
    InfallibleTArray<AnimationProperty>& aResult,
    ErrorResult& aRv);

  nsCOMPtr<Element> mTarget;
  RefPtr<Animation> mAnimation;

  AnimationTiming mTiming;
  nsCSSPseudoElements::Type mPseudoType;

  InfallibleTArray<AnimationProperty> mProperties;

private:
  nsIFrame* GetAnimationFrame() const;

  bool CanThrottleTransformChanges(nsIFrame& aFrame) const;

  // Returns true unless Gecko limitations prevent performing transform
  // animations for |aFrame|. Any limitations that are encountered are
  // logged using |aContent| to describe the affected content.
  // If |aContent| is nullptr, no logging is performed
  static bool CanAnimateTransformOnCompositor(const nsIFrame* aFrame,
                                              const nsIContent* aContent);
  static bool IsGeometricProperty(const nsCSSProperty aProperty);

  static const TimeDuration OverflowRegionRefreshInterval();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_KeyframeEffect_h
