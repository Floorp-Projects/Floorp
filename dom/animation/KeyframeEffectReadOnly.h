/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_KeyframeEffectReadOnly_h
#define mozilla_dom_KeyframeEffectReadOnly_h

#include "nsChangeHint.h"
#include "nsCSSPropertyID.h"
#include "nsCSSPropertyIDSet.h"
#include "nsCSSValue.h"
#include "nsCycleCollectionParticipant.h"
#include "nsRefPtrHashtable.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"
#include "mozilla/AnimationPerformanceWarning.h"
#include "mozilla/AnimationPropertySegment.h"
#include "mozilla/AnimationTarget.h"
#include "mozilla/Attributes.h"
#include "mozilla/ComputedTimingFunction.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/Keyframe.h"
#include "mozilla/KeyframeEffectParams.h"
// RawServoDeclarationBlock and associated RefPtrTraits
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/AnimationEffectReadOnly.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Element.h"

struct JSContext;
class JSObject;
class nsIContent;
class nsIDocument;
class nsIFrame;
class nsIPresShell;

namespace mozilla {

class AnimValuesStyleRule;
enum class CSSPseudoElementType : uint8_t;
class ErrorResult;
struct AnimationRule;
struct TimingParams;
class EffectSet;
class ComputedStyle;

namespace dom {
class ElementOrCSSPseudoElement;
class GlobalObject;
class OwningElementOrCSSPseudoElement;
class UnrestrictedDoubleOrKeyframeAnimationOptions;
class UnrestrictedDoubleOrKeyframeEffectOptions;
enum class IterationCompositeOperation : uint8_t;
enum class CompositeOperation : uint8_t;
struct AnimationPropertyDetails;
}

struct AnimationProperty
{
  nsCSSPropertyID mProperty = eCSSProperty_UNKNOWN;

  // If true, the propery is currently being animated on the compositor.
  //
  // Note that when the owning Animation requests a non-throttled restyle, in
  // between calling RequestRestyle on its EffectCompositor and when the
  // restyle is performed, this member may temporarily become false even if
  // the animation remains on the layer after the restyle.
  //
  // **NOTE**: This member is not included when comparing AnimationProperty
  // objects for equality.
  bool mIsRunningOnCompositor = false;

  Maybe<AnimationPerformanceWarning> mPerformanceWarning;

  InfallibleTArray<AnimationPropertySegment> mSegments;

  // The copy constructor/assignment doesn't copy mIsRunningOnCompositor and
  // mPerformanceWarning.
  AnimationProperty() = default;
  AnimationProperty(const AnimationProperty& aOther)
    : mProperty(aOther.mProperty), mSegments(aOther.mSegments) { }
  AnimationProperty& operator=(const AnimationProperty& aOther)
  {
    mProperty = aOther.mProperty;
    mSegments = aOther.mSegments;
    return *this;
  }

  // NOTE: This operator does *not* compare the mIsRunningOnCompositor member.
  // This is because AnimationProperty objects are compared when recreating
  // CSS animations to determine if mutation observer change records need to
  // be created or not. However, at the point when these objects are compared
  // the mIsRunningOnCompositor will not have been set on the new objects so
  // we ignore this member to avoid generating spurious change records.
  bool operator==(const AnimationProperty& aOther) const
  {
    return mProperty == aOther.mProperty &&
           mSegments == aOther.mSegments;
  }
  bool operator!=(const AnimationProperty& aOther) const
  {
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
                         const Maybe<OwningAnimationTarget>& aTarget,
                         const TimingParams& aTiming,
                         const KeyframeEffectParams& aOptions);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(KeyframeEffectReadOnly,
                                                        AnimationEffectReadOnly)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  KeyframeEffectReadOnly* AsKeyframeEffect() override { return this; }

  // KeyframeEffectReadOnly interface
  static already_AddRefed<KeyframeEffectReadOnly>
  Constructor(const GlobalObject& aGlobal,
              const Nullable<ElementOrCSSPseudoElement>& aTarget,
              JS::Handle<JSObject*> aKeyframes,
              const UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
              ErrorResult& aRv);

  static already_AddRefed<KeyframeEffectReadOnly>
  Constructor(const GlobalObject& aGlobal,
              KeyframeEffectReadOnly& aSource,
              ErrorResult& aRv);

  void GetTarget(Nullable<OwningElementOrCSSPseudoElement>& aRv) const;
  Maybe<NonOwningAnimationTarget> GetTarget() const
  {
    Maybe<NonOwningAnimationTarget> result;
    if (mTarget) {
      result.emplace(*mTarget);
    }
    return result;
  }
  void GetKeyframes(JSContext*& aCx,
                    nsTArray<JSObject*>& aResult,
                    ErrorResult& aRv);
  void GetProperties(nsTArray<AnimationPropertyDetails>& aProperties,
                     ErrorResult& aRv) const;

  IterationCompositeOperation IterationComposite() const;
  CompositeOperation Composite() const;
  void NotifyAnimationTimingUpdated();
  void RequestRestyle(EffectCompositor::RestyleType aRestyleType);
  void SetAnimation(Animation* aAnimation) override;
  void SetKeyframes(JSContext* aContext, JS::Handle<JSObject*> aKeyframes,
                    ErrorResult& aRv);
  void SetKeyframes(nsTArray<Keyframe>&& aKeyframes,
                    const ComputedStyle* aStyle);

  // Returns true if the effect includes |aProperty| regardless of whether the
  // property is overridden by !important rule.
  bool HasAnimationOfProperty(nsCSSPropertyID aProperty) const;

  // GetEffectiveAnimationOfProperty returns AnimationProperty corresponding
  // to a given CSS property if the effect includes the property and the
  // property is not overridden by !important rules.
  // Also EffectiveAnimationOfProperty returns true under the same condition.
  //
  // NOTE: We don't currently check for !important rules for properties that
  // can't run on the compositor.
  bool HasEffectiveAnimationOfProperty(nsCSSPropertyID aProperty) const
  {
    return GetEffectiveAnimationOfProperty(aProperty) != nullptr;
  }
  const AnimationProperty* GetEffectiveAnimationOfProperty(
    nsCSSPropertyID aProperty) const;

  const InfallibleTArray<AnimationProperty>& Properties() const
  {
    return mProperties;
  }

  // Update |mProperties| by recalculating from |mKeyframes| using
  // |aComputedStyle| to resolve specified values.
  void UpdateProperties(const ComputedStyle* aComputedValues);

  // Update various bits of state related to running ComposeStyle().
  // We need to update this outside ComposeStyle() because we should avoid
  // mutating any state in ComposeStyle() since it might be called during
  // parallel traversal.
  void WillComposeStyle();

  // Updates |aComposeResult| with the animation values produced by this
  // AnimationEffect for the current time except any properties contained
  // in |aPropertiesToSkip|.
  void ComposeStyle(RawServoAnimationValueMap& aComposeResult,
                    const nsCSSPropertyIDSet& aPropertiesToSkip);


  // Returns true if at least one property is being animated on compositor.
  bool IsRunningOnCompositor() const;
  void SetIsRunningOnCompositor(nsCSSPropertyID aProperty, bool aIsRunning);
  void ResetIsRunningOnCompositor();

  // Returns true if this effect, applied to |aFrame|, contains properties
  // that mean we shouldn't run transform compositor animations on this element.
  //
  // For example, if we have an animation of geometric properties like 'left'
  // and 'top' on an element, we force all 'transform' animations running at
  // the same time on the same element to run on the main thread.
  //
  // When returning true, |aPerformanceWarning| stores the reason why
  // we shouldn't run the transform animations.
  bool ShouldBlockAsyncTransformAnimations(
    const nsIFrame* aFrame, AnimationPerformanceWarning::Type& aPerformanceWarning) const;
  bool HasGeometricProperties() const;
  bool AffectsGeometry() const override
  {
    return GetTarget() && HasGeometricProperties();
  }

  nsIDocument* GetRenderedDocument() const;
  nsIPresShell* GetPresShell() const;

  // Associates a warning with the animated property on the specified frame
  // indicating why, for example, the property could not be animated on the
  // compositor. |aParams| and |aParamsLength| are optional parameters which
  // will be used to generate a localized message for devtools.
  void SetPerformanceWarning(
    nsCSSPropertyID aProperty,
    const AnimationPerformanceWarning& aWarning);

  // Cumulative change hint on each segment for each property.
  // This is used for deciding the animation is paint-only.
  void CalculateCumulativeChangeHint(const ComputedStyle* aStyle);

  // Returns true if all of animation properties' change hints
  // can ignore painting if the animation is not visible.
  // See nsChangeHint_Hints_CanIgnoreIfNotVisible in nsChangeHint.h
  // in detail which change hint can be ignored.
  bool CanIgnoreIfNotVisible() const;

  // Returns true if the effect is current state and has scale animation.
  // |aFrame| is used for calculation of scale values.
  bool ContainsAnimatedScale(const nsIFrame* aFrame) const;

  AnimationValue BaseStyle(nsCSSPropertyID aProperty) const
  {
    AnimationValue result;
    bool hasProperty = false;
    // We cannot use getters_AddRefs on RawServoAnimationValue because it is
    // an incomplete type, so Get() doesn't work. Instead, use GetWeak, and
    // then assign the raw pointer to a RefPtr.
    result.mServo = mBaseStyleValuesForServo.GetWeak(aProperty, &hasProperty);
    MOZ_ASSERT(hasProperty || result.IsNull());
    return result;
  }

  static bool HasComputedTimingChanged(
    const ComputedTiming& aComputedTiming,
    IterationCompositeOperation aIterationComposite,
    const Nullable<double>& aProgressOnLastCompose,
    uint64_t aCurrentIterationOnLastCompose);

protected:
  KeyframeEffectReadOnly(nsIDocument* aDocument,
                         const Maybe<OwningAnimationTarget>& aTarget,
                         AnimationEffectTimingReadOnly* aTiming,
                         const KeyframeEffectParams& aOptions);

  ~KeyframeEffectReadOnly() override = default;

  static Maybe<OwningAnimationTarget>
  ConvertTarget(const Nullable<ElementOrCSSPseudoElement>& aTarget);

  template<class KeyframeEffectType, class OptionsType>
  static already_AddRefed<KeyframeEffectType>
  ConstructKeyframeEffect(const GlobalObject& aGlobal,
                          const Nullable<ElementOrCSSPseudoElement>& aTarget,
                          JS::Handle<JSObject*> aKeyframes,
                          const OptionsType& aOptions,
                          ErrorResult& aRv);

  template<class KeyframeEffectType>
  static already_AddRefed<KeyframeEffectType>
  ConstructKeyframeEffect(const GlobalObject& aGlobal,
                          KeyframeEffectReadOnly& aSource,
                          ErrorResult& aRv);

  // Build properties by recalculating from |mKeyframes| using |aComputedStyle|
  // to resolve specified values. This function also applies paced spacing if
  // needed.
  nsTArray<AnimationProperty> BuildProperties(const ComputedStyle* aStyle);

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

  // Remove the current effect target from its EffectSet.
  void UnregisterTarget();

  // Update the associated frame state bits so that, if necessary, a stacking
  // context will be created and the effect sent to the compositor.  We
  // typically need to do this when the properties referenced by the keyframe
  // have changed, or when the target frame might have changed.
  void MaybeUpdateFrameForCompositor();

  // Looks up the ComputedStyle associated with the target element, if any.
  // We need to be careful to *not* call this when we are updating the style
  // context. That's because calling GetComputedStyle when we are in the process
  // of building a ComputedStyle may trigger various forms of infinite
  // recursion.
  already_AddRefed<ComputedStyle> GetTargetComputedStyle();

  // A wrapper for marking cascade update according to the current
  // target and its effectSet.
  void MarkCascadeNeedsUpdate();

  void EnsureBaseStyles(const ComputedStyle* aComputedValues,
                        const nsTArray<AnimationProperty>& aProperties);

  // Stylo version of the above function that also first checks for an additive
  // value in |aProperty|'s list of segments.
  void EnsureBaseStyle(const AnimationProperty& aProperty,
                       nsPresContext* aPresContext,
                       const ComputedStyle* aComputedValues,
                       RefPtr<ComputedStyle>& aBaseComputedValues);

  Maybe<OwningAnimationTarget> mTarget;

  KeyframeEffectParams mEffectOptions;

  // The specified keyframes.
  nsTArray<Keyframe>          mKeyframes;

  // A set of per-property value arrays, derived from |mKeyframes|.
  nsTArray<AnimationProperty> mProperties;

  // The computed progress last time we composed the style rule. This is
  // used to detect when the progress is not changing (e.g. due to a step
  // timing function) so we can avoid unnecessary style updates.
  Nullable<double> mProgressOnLastCompose;

  // The purpose of this value is the same as mProgressOnLastCompose but
  // this is used to detect when the current iteration is not changing
  // in the case when iterationComposite is accumulate.
  uint64_t mCurrentIterationOnLastCompose = 0;

  // We need to track when we go to or from being "in effect" since
  // we need to re-evaluate the cascade of animations when that changes.
  bool mInEffectOnLastAnimationTimingUpdate;

  // The non-animated values for properties in this effect that contain at
  // least one animation value that is composited with the underlying value
  // (i.e. it uses the additive or accumulate composite mode).
  nsRefPtrHashtable<nsUint32HashKey, RawServoAnimationValue>
    mBaseStyleValuesForServo;

  // True if this effect is in the EffectSet for its target element. This is
  // used as an optimization to avoid unnecessary hashmap lookups on the
  // EffectSet.
  bool mInEffectSet = false;

private:
  nsChangeHint mCumulativeChangeHint;

  void ComposeStyleRule(RawServoAnimationValueMap& aAnimationValues,
                        const AnimationProperty& aProperty,
                        const AnimationPropertySegment& aSegment,
                        const ComputedTiming& aComputedTiming);


  already_AddRefed<ComputedStyle> CreateComputedStyleForAnimationValue(
    nsCSSPropertyID aProperty,
    const AnimationValue& aValue,
    nsPresContext* aPresContext,
    const ComputedStyle* aBaseComputedStyle);

  nsIFrame* GetAnimationFrame() const;

  bool CanThrottle() const;
  bool CanThrottleTransformChanges(const nsIFrame& aFrame) const;
  bool CanThrottleTransformChangesInScrollable(nsIFrame& aFrame) const;

  // Returns true if the computedTiming has changed since the last
  // composition.
  bool HasComputedTimingChanged() const;

  // Returns true unless Gecko limitations prevent performing transform
  // animations for |aFrame|. When returning true, the reason for the
  // limitation is stored in |aOutPerformanceWarning|.
  static bool CanAnimateTransformOnCompositor(
    const nsIFrame* aFrame,
    AnimationPerformanceWarning::Type& aPerformanceWarning);
  static bool IsGeometricProperty(const nsCSSPropertyID aProperty);

  static const TimeDuration OverflowRegionRefreshInterval();

  void UpdateEffectSet(mozilla::EffectSet* aEffectSet = nullptr) const;

  // Returns true if this effect has transform and the transform might affect
  // the overflow region.
  // This function is used for updating scroll bars or notifying intersection
  // observers reflected by the transform.
  bool HasTransformThatMightAffectOverflow() const
  {
    return mCumulativeChangeHint & (nsChangeHint_UpdatePostTransformOverflow |
                                    nsChangeHint_AddOrRemoveTransform |
                                    nsChangeHint_UpdateTransformLayer);
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_KeyframeEffectReadOnly_h
