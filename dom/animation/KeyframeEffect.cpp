/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/KeyframeEffect.h"

#include "FrameLayerBuilder.h"
#include "mozilla/dom/Animation.h"
#include "mozilla/dom/KeyframeAnimationOptionsBinding.h"
// For UnrestrictedDoubleOrKeyframeAnimationOptions;
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "mozilla/dom/MutationObservers.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/ComputedStyleInlines.h"
#include "mozilla/EffectSet.h"
#include "mozilla/FloatingPoint.h"  // For IsFinite
#include "mozilla/LayerAnimationInfo.h"
#include "mozilla/LookAndFeel.h"  // For LookAndFeel::GetInt
#include "mozilla/KeyframeUtils.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "Layers.h"              // For Layer
#include "nsComputedDOMStyle.h"  // nsComputedDOMStyle::GetComputedStyle
#include "nsContentUtils.h"
#include "nsCSSPropertyIDSet.h"
#include "nsCSSProps.h"             // For nsCSSProps::PropHasFlags
#include "nsCSSPseudoElements.h"    // For PseudoStyleType
#include "nsDOMMutationObserver.h"  // For nsAutoAnimationMutationBatch
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "nsPresContextInlines.h"
#include "nsRefreshDriver.h"

namespace mozilla {

void AnimationProperty::SetPerformanceWarning(
    const AnimationPerformanceWarning& aWarning, const dom::Element* aElement) {
  if (mPerformanceWarning && *mPerformanceWarning == aWarning) {
    return;
  }

  mPerformanceWarning = Some(aWarning);

  nsAutoString localizedString;
  if (StaticPrefs::layers_offmainthreadcomposition_log_animations() &&
      mPerformanceWarning->ToLocalizedString(localizedString)) {
    nsAutoCString logMessage = NS_ConvertUTF16toUTF8(localizedString);
    AnimationUtils::LogAsyncAnimationFailure(logMessage, aElement);
  }
}

bool PropertyValuePair::operator==(const PropertyValuePair& aOther) const {
  if (mProperty != aOther.mProperty) {
    return false;
  }
  if (mServoDeclarationBlock == aOther.mServoDeclarationBlock) {
    return true;
  }
  if (!mServoDeclarationBlock || !aOther.mServoDeclarationBlock) {
    return false;
  }
  return Servo_DeclarationBlock_Equals(mServoDeclarationBlock,
                                       aOther.mServoDeclarationBlock);
}

namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(KeyframeEffect, AnimationEffect,
                                   mTarget.mElement)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(KeyframeEffect, AnimationEffect)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(KeyframeEffect)
NS_INTERFACE_MAP_END_INHERITING(AnimationEffect)

NS_IMPL_ADDREF_INHERITED(KeyframeEffect, AnimationEffect)
NS_IMPL_RELEASE_INHERITED(KeyframeEffect, AnimationEffect)

KeyframeEffect::KeyframeEffect(Document* aDocument,
                               OwningAnimationTarget&& aTarget,
                               TimingParams&& aTiming,
                               const KeyframeEffectParams& aOptions)
    : AnimationEffect(aDocument, std::move(aTiming)),
      mTarget(std::move(aTarget)),
      mEffectOptions(aOptions),
      mCumulativeChangeHint(nsChangeHint(0)) {}

JSObject* KeyframeEffect::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return KeyframeEffect_Binding::Wrap(aCx, this, aGivenProto);
}

IterationCompositeOperation KeyframeEffect::IterationComposite() const {
  return mEffectOptions.mIterationComposite;
}

void KeyframeEffect::SetIterationComposite(
    const IterationCompositeOperation& aIterationComposite) {
  if (mEffectOptions.mIterationComposite == aIterationComposite) {
    return;
  }

  if (mAnimation && mAnimation->IsRelevant()) {
    MutationObservers::NotifyAnimationChanged(mAnimation);
  }

  mEffectOptions.mIterationComposite = aIterationComposite;
  RequestRestyle(EffectCompositor::RestyleType::Layer);
}

CompositeOperation KeyframeEffect::Composite() const {
  return mEffectOptions.mComposite;
}

void KeyframeEffect::SetComposite(const CompositeOperation& aComposite) {
  if (mEffectOptions.mComposite == aComposite) {
    return;
  }

  mEffectOptions.mComposite = aComposite;

  if (mAnimation && mAnimation->IsRelevant()) {
    MutationObservers::NotifyAnimationChanged(mAnimation);
  }

  if (mTarget) {
    RefPtr<ComputedStyle> computedStyle = GetTargetComputedStyle(Flush::None);
    if (computedStyle) {
      UpdateProperties(computedStyle);
    }
  }
}

void KeyframeEffect::NotifySpecifiedTimingUpdated() {
  // Use the same document for a pseudo element and its parent element.
  // Use nullptr if we don't have mTarget, so disable the mutation batch.
  nsAutoAnimationMutationBatch mb(mTarget ? mTarget.mElement->OwnerDoc()
                                          : nullptr);

  if (mAnimation) {
    mAnimation->NotifyEffectTimingUpdated();

    if (mAnimation->IsRelevant()) {
      MutationObservers::NotifyAnimationChanged(mAnimation);
    }

    RequestRestyle(EffectCompositor::RestyleType::Layer);
  }
}

void KeyframeEffect::NotifyAnimationTimingUpdated(
    PostRestyleMode aPostRestyle) {
  UpdateTargetRegistration();

  // If the effect is not relevant it will be removed from the target
  // element's effect set. However, effects not in the effect set
  // will not be included in the set of candidate effects for running on
  // the compositor and hence they won't have their compositor status
  // updated. As a result, we need to make sure we clear their compositor
  // status here.
  bool isRelevant = mAnimation && mAnimation->IsRelevant();
  if (!isRelevant) {
    ResetIsRunningOnCompositor();
  }

  // Request restyle if necessary.
  if (aPostRestyle == PostRestyleMode::IfNeeded && mAnimation &&
      !mProperties.IsEmpty() && HasComputedTimingChanged()) {
    EffectCompositor::RestyleType restyleType =
        CanThrottle() ? EffectCompositor::RestyleType::Throttled
                      : EffectCompositor::RestyleType::Standard;
    RequestRestyle(restyleType);
  }

  // Detect changes to "in effect" status since we need to recalculate the
  // animation cascade for this element whenever that changes.
  // Note that updating mInEffectOnLastAnimationTimingUpdate has to be done
  // after above CanThrottle() call since the function uses the flag inside it.
  bool inEffect = IsInEffect();
  if (inEffect != mInEffectOnLastAnimationTimingUpdate) {
    MarkCascadeNeedsUpdate();
    mInEffectOnLastAnimationTimingUpdate = inEffect;
  }

  // If we're no longer "in effect", our ComposeStyle method will never be
  // called and we will never have a chance to update mProgressOnLastCompose
  // and mCurrentIterationOnLastCompose.
  // We clear them here to ensure that if we later become "in effect" we will
  // request a restyle (above).
  if (!inEffect) {
    mProgressOnLastCompose.SetNull();
    mCurrentIterationOnLastCompose = 0;
  }
}

static bool KeyframesEqualIgnoringComputedOffsets(
    const nsTArray<Keyframe>& aLhs, const nsTArray<Keyframe>& aRhs) {
  if (aLhs.Length() != aRhs.Length()) {
    return false;
  }

  for (size_t i = 0, len = aLhs.Length(); i < len; ++i) {
    const Keyframe& a = aLhs[i];
    const Keyframe& b = aRhs[i];
    if (a.mOffset != b.mOffset || a.mTimingFunction != b.mTimingFunction ||
        a.mPropertyValues != b.mPropertyValues) {
      return false;
    }
  }
  return true;
}

// https://drafts.csswg.org/web-animations/#dom-keyframeeffect-setkeyframes
void KeyframeEffect::SetKeyframes(JSContext* aContext,
                                  JS::Handle<JSObject*> aKeyframes,
                                  ErrorResult& aRv) {
  nsTArray<Keyframe> keyframes = KeyframeUtils::GetKeyframesFromObject(
      aContext, mDocument, aKeyframes, "KeyframeEffect.setKeyframes", aRv);
  if (aRv.Failed()) {
    return;
  }

  RefPtr<ComputedStyle> style = GetTargetComputedStyle(Flush::None);
  SetKeyframes(std::move(keyframes), style);
}

void KeyframeEffect::SetKeyframes(nsTArray<Keyframe>&& aKeyframes,
                                  const ComputedStyle* aStyle) {
  if (KeyframesEqualIgnoringComputedOffsets(aKeyframes, mKeyframes)) {
    return;
  }

  mKeyframes = std::move(aKeyframes);
  KeyframeUtils::DistributeKeyframes(mKeyframes);

  if (mAnimation && mAnimation->IsRelevant()) {
    MutationObservers::NotifyAnimationChanged(mAnimation);
  }

  // We need to call UpdateProperties() unless the target element doesn't have
  // style (e.g. the target element is not associated with any document).
  if (aStyle) {
    UpdateProperties(aStyle);
  }
}

void KeyframeEffect::ReplaceTransitionStartValue(AnimationValue&& aStartValue) {
  if (!aStartValue.mServo) {
    return;
  }

  // A typical transition should have a single property and a single segment.
  //
  // (And for atypical transitions, that is, those updated by script, we don't
  // apply the replacing behavior.)
  if (mProperties.Length() != 1 || mProperties[0].mSegments.Length() != 1) {
    return;
  }

  // Likewise, check that the keyframes are of the expected shape.
  if (mKeyframes.Length() != 2 || mKeyframes[0].mPropertyValues.Length() != 1) {
    return;
  }

  // Check that the value we are about to substitute in is actually for the
  // same property.
  if (Servo_AnimationValue_GetPropertyId(aStartValue.mServo) !=
      mProperties[0].mProperty) {
    return;
  }

  mKeyframes[0].mPropertyValues[0].mServoDeclarationBlock =
      Servo_AnimationValue_Uncompute(aStartValue.mServo).Consume();
  mProperties[0].mSegments[0].mFromValue = std::move(aStartValue);
}

static bool IsEffectiveProperty(const EffectSet& aEffects,
                                nsCSSPropertyID aProperty) {
  return !aEffects.PropertiesWithImportantRules().HasProperty(aProperty) ||
         !aEffects.PropertiesForAnimationsLevel().HasProperty(aProperty);
}

const AnimationProperty* KeyframeEffect::GetEffectiveAnimationOfProperty(
    nsCSSPropertyID aProperty, const EffectSet& aEffects) const {
  MOZ_ASSERT(mTarget &&
             &aEffects == EffectSet::GetEffectSet(mTarget.mElement,
                                                  mTarget.mPseudoType));

  for (const AnimationProperty& property : mProperties) {
    if (aProperty != property.mProperty) {
      continue;
    }

    const AnimationProperty* result = nullptr;
    // Only include the property if it is not overridden by !important rules in
    // the transitions level.
    if (IsEffectiveProperty(aEffects, property.mProperty)) {
      result = &property;
    }
    return result;
  }
  return nullptr;
}

bool KeyframeEffect::HasEffectiveAnimationOfPropertySet(
    const nsCSSPropertyIDSet& aPropertySet, const EffectSet& aEffectSet) const {
  for (const AnimationProperty& property : mProperties) {
    if (aPropertySet.HasProperty(property.mProperty) &&
        IsEffectiveProperty(aEffectSet, property.mProperty)) {
      return true;
    }
  }
  return false;
}

nsCSSPropertyIDSet KeyframeEffect::GetPropertiesForCompositor(
    EffectSet& aEffects, const nsIFrame* aFrame) const {
  MOZ_ASSERT(&aEffects ==
             EffectSet::GetEffectSet(mTarget.mElement, mTarget.mPseudoType));

  nsCSSPropertyIDSet properties;

  if (!mAnimation || !mAnimation->IsRelevant()) {
    return properties;
  }

  static constexpr nsCSSPropertyIDSet compositorAnimatables =
      nsCSSPropertyIDSet::CompositorAnimatables();
  static constexpr nsCSSPropertyIDSet transformLikeProperties =
      nsCSSPropertyIDSet::TransformLikeProperties();

  nsCSSPropertyIDSet transformSet;
  AnimationPerformanceWarning::Type dummyWarning;

  for (const AnimationProperty& property : mProperties) {
    if (!compositorAnimatables.HasProperty(property.mProperty)) {
      continue;
    }

    // Transform-like properties are combined together on the compositor so we
    // need to evaluate them as a group. We build up a separate set here then
    // evaluate it as a separate step below.
    if (transformLikeProperties.HasProperty(property.mProperty)) {
      transformSet.AddProperty(property.mProperty);
      continue;
    }

    KeyframeEffect::MatchForCompositor matchResult = IsMatchForCompositor(
        nsCSSPropertyIDSet{property.mProperty}, aFrame, aEffects, dummyWarning);
    if (matchResult ==
            KeyframeEffect::MatchForCompositor::NoAndBlockThisProperty ||
        matchResult == KeyframeEffect::MatchForCompositor::No) {
      continue;
    }
    properties.AddProperty(property.mProperty);
  }

  if (!transformSet.IsEmpty()) {
    KeyframeEffect::MatchForCompositor matchResult =
        IsMatchForCompositor(transformSet, aFrame, aEffects, dummyWarning);
    if (matchResult == KeyframeEffect::MatchForCompositor::Yes ||
        matchResult == KeyframeEffect::MatchForCompositor::IfNeeded) {
      properties |= transformSet;
    }
  }

  return properties;
}

nsCSSPropertyIDSet KeyframeEffect::GetPropertySet() const {
  nsCSSPropertyIDSet result;

  for (const AnimationProperty& property : mProperties) {
    result.AddProperty(property.mProperty);
  }

  return result;
}

#ifdef DEBUG
bool SpecifiedKeyframeArraysAreEqual(const nsTArray<Keyframe>& aA,
                                     const nsTArray<Keyframe>& aB) {
  if (aA.Length() != aB.Length()) {
    return false;
  }

  for (size_t i = 0; i < aA.Length(); i++) {
    const Keyframe& a = aA[i];
    const Keyframe& b = aB[i];
    if (a.mOffset != b.mOffset || a.mTimingFunction != b.mTimingFunction ||
        a.mPropertyValues != b.mPropertyValues) {
      return false;
    }
  }

  return true;
}
#endif

static bool HasCurrentColor(
    const nsTArray<AnimationPropertySegment>& aSegments) {
  for (const AnimationPropertySegment& segment : aSegments) {
    if ((!segment.mFromValue.IsNull() && segment.mFromValue.IsCurrentColor()) ||
        (!segment.mToValue.IsNull() && segment.mToValue.IsCurrentColor())) {
      return true;
    }
  }
  return false;
}
void KeyframeEffect::UpdateProperties(const ComputedStyle* aStyle) {
  MOZ_ASSERT(aStyle);

  nsTArray<AnimationProperty> properties = BuildProperties(aStyle);

  bool propertiesChanged = mProperties != properties;

  // We need to update base styles even if any properties are not changed at all
  // since base styles might have been changed due to parent style changes, etc.
  bool baseStylesChanged = false;
  EnsureBaseStyles(aStyle, properties,
                   !propertiesChanged ? &baseStylesChanged : nullptr);

  if (!propertiesChanged) {
    if (baseStylesChanged) {
      RequestRestyle(EffectCompositor::RestyleType::Layer);
    }
    // Check if we need to update the cumulative change hint because we now have
    // style data.
    if (mNeedsStyleData && mTarget && mTarget.mElement->HasServoData()) {
      CalculateCumulativeChangeHint(aStyle);
    }
    return;
  }

  // Preserve the state of the mIsRunningOnCompositor flag.
  nsCSSPropertyIDSet runningOnCompositorProperties;

  for (const AnimationProperty& property : mProperties) {
    if (property.mIsRunningOnCompositor) {
      runningOnCompositorProperties.AddProperty(property.mProperty);
    }
  }

  mProperties = std::move(properties);
  UpdateEffectSet();

  mHasCurrentColor = false;

  for (AnimationProperty& property : mProperties) {
    property.mIsRunningOnCompositor =
        runningOnCompositorProperties.HasProperty(property.mProperty);

    if (property.mProperty == eCSSProperty_background_color &&
        !mHasCurrentColor) {
      if (HasCurrentColor(property.mSegments)) {
        mHasCurrentColor = true;
        break;
      }
    }
  }

  CalculateCumulativeChangeHint(aStyle);

  MarkCascadeNeedsUpdate();

  if (mAnimation) {
    mAnimation->NotifyEffectPropertiesUpdated();
  }

  RequestRestyle(EffectCompositor::RestyleType::Layer);
}

void KeyframeEffect::EnsureBaseStyles(
    const ComputedStyle* aComputedValues,
    const nsTArray<AnimationProperty>& aProperties, bool* aBaseStylesChanged) {
  if (aBaseStylesChanged != nullptr) {
    *aBaseStylesChanged = false;
  }

  if (!mTarget) {
    return;
  }

  BaseValuesHashmap previousBaseStyles;
  if (aBaseStylesChanged != nullptr) {
    previousBaseStyles = std::move(mBaseValues);
  }

  mBaseValues.Clear();

  nsPresContext* presContext =
      nsContentUtils::GetContextForContent(mTarget.mElement);
  // If |aProperties| is empty we're not going to dereference |presContext| so
  // we don't care if it is nullptr.
  //
  // We could just return early when |aProperties| is empty and save looking up
  // the pres context, but that won't save any effort normally since we don't
  // call this function if we have no keyframes to begin with. Furthermore, the
  // case where |presContext| is nullptr is so rare (we've only ever seen in
  // fuzzing, and even then we've never been able to reproduce it reliably)
  // it's not worth the runtime cost of an extra branch.
  MOZ_ASSERT(presContext || aProperties.IsEmpty(),
             "Typically presContext should not be nullptr but if it is"
             " we should have also failed to calculate the computed values"
             " passed-in as aProperties");

  RefPtr<ComputedStyle> baseComputedStyle;
  for (const AnimationProperty& property : aProperties) {
    EnsureBaseStyle(property, presContext, aComputedValues, baseComputedStyle);
  }

  if (aBaseStylesChanged != nullptr) {
    for (auto iter = mBaseValues.Iter(); !iter.Done(); iter.Next()) {
      if (AnimationValue(iter.Data()) !=
          AnimationValue(previousBaseStyles.Get(iter.Key()))) {
        *aBaseStylesChanged = true;
        break;
      }
    }
  }
}

void KeyframeEffect::EnsureBaseStyle(
    const AnimationProperty& aProperty, nsPresContext* aPresContext,
    const ComputedStyle* aComputedStyle,
    RefPtr<ComputedStyle>& aBaseComputedStyle) {
  bool hasAdditiveValues = false;

  for (const AnimationPropertySegment& segment : aProperty.mSegments) {
    if (!segment.HasReplaceableValues()) {
      hasAdditiveValues = true;
      break;
    }
  }

  if (!hasAdditiveValues) {
    return;
  }

  if (!aBaseComputedStyle) {
    MOZ_ASSERT(mTarget, "Should have a valid target");

    Element* animatingElement = EffectCompositor::GetElementToRestyle(
        mTarget.mElement, mTarget.mPseudoType);
    if (!animatingElement) {
      return;
    }
    aBaseComputedStyle = aPresContext->StyleSet()->GetBaseContextForElement(
        animatingElement, aComputedStyle);
  }
  RefPtr<RawServoAnimationValue> baseValue =
      Servo_ComputedValues_ExtractAnimationValue(aBaseComputedStyle,
                                                 aProperty.mProperty)
          .Consume();
  mBaseValues.Put(aProperty.mProperty, std::move(baseValue));
}

void KeyframeEffect::WillComposeStyle() {
  ComputedTiming computedTiming = GetComputedTiming();
  mProgressOnLastCompose = computedTiming.mProgress;
  mCurrentIterationOnLastCompose = computedTiming.mCurrentIteration;
}

void KeyframeEffect::ComposeStyleRule(
    RawServoAnimationValueMap& aAnimationValues,
    const AnimationProperty& aProperty,
    const AnimationPropertySegment& aSegment,
    const ComputedTiming& aComputedTiming) {
  auto* opaqueTable =
      reinterpret_cast<RawServoAnimationValueTable*>(&mBaseValues);
  Servo_AnimationCompose(&aAnimationValues, opaqueTable, aProperty.mProperty,
                         &aSegment, &aProperty.mSegments.LastElement(),
                         &aComputedTiming, mEffectOptions.mIterationComposite);
}

void KeyframeEffect::ComposeStyle(RawServoAnimationValueMap& aComposeResult,
                                  const nsCSSPropertyIDSet& aPropertiesToSkip) {
  ComputedTiming computedTiming = GetComputedTiming();

  // If the progress is null, we don't have fill data for the current
  // time so we shouldn't animate.
  if (computedTiming.mProgress.IsNull()) {
    return;
  }

  for (size_t propIdx = 0, propEnd = mProperties.Length(); propIdx != propEnd;
       ++propIdx) {
    const AnimationProperty& prop = mProperties[propIdx];

    MOZ_ASSERT(prop.mSegments[0].mFromKey == 0.0, "incorrect first from key");
    MOZ_ASSERT(prop.mSegments[prop.mSegments.Length() - 1].mToKey == 1.0,
               "incorrect last to key");

    if (aPropertiesToSkip.HasProperty(prop.mProperty)) {
      continue;
    }

    MOZ_ASSERT(prop.mSegments.Length() > 0,
               "property should not be in animations if it has no segments");

    // FIXME: Maybe cache the current segment?
    const AnimationPropertySegment *segment = prop.mSegments.Elements(),
                                   *segmentEnd =
                                       segment + prop.mSegments.Length();
    while (segment->mToKey <= computedTiming.mProgress.Value()) {
      MOZ_ASSERT(segment->mFromKey <= segment->mToKey, "incorrect keys");
      if ((segment + 1) == segmentEnd) {
        break;
      }
      ++segment;
      MOZ_ASSERT(segment->mFromKey == (segment - 1)->mToKey, "incorrect keys");
    }
    MOZ_ASSERT(segment->mFromKey <= segment->mToKey, "incorrect keys");
    MOZ_ASSERT(segment >= prop.mSegments.Elements() &&
                   size_t(segment - prop.mSegments.Elements()) <
                       prop.mSegments.Length(),
               "out of array bounds");

    ComposeStyleRule(aComposeResult, prop, *segment, computedTiming);
  }

  // If the animation produces a change hint that affects the overflow region,
  // we need to record the current time to unthrottle the animation
  // periodically when the animation is being throttled because it's scrolled
  // out of view.
  if (HasPropertiesThatMightAffectOverflow()) {
    nsPresContext* presContext =
        nsContentUtils::GetContextForContent(mTarget.mElement);
    EffectSet* effectSet =
        EffectSet::GetEffectSet(mTarget.mElement, mTarget.mPseudoType);
    if (presContext && effectSet) {
      TimeStamp now = presContext->RefreshDriver()->MostRecentRefresh();
      effectSet->UpdateLastOverflowAnimationSyncTime(now);
    }
  }
}

bool KeyframeEffect::IsRunningOnCompositor() const {
  // We consider animation is running on compositor if there is at least
  // one property running on compositor.
  // Animation.IsRunningOnCompotitor will return more fine grained
  // information in bug 1196114.
  for (const AnimationProperty& property : mProperties) {
    if (property.mIsRunningOnCompositor) {
      return true;
    }
  }
  return false;
}

void KeyframeEffect::SetIsRunningOnCompositor(nsCSSPropertyID aProperty,
                                              bool aIsRunning) {
  MOZ_ASSERT(
      nsCSSProps::PropHasFlags(aProperty, CSSPropFlags::CanAnimateOnCompositor),
      "Property being animated on compositor is a recognized "
      "compositor-animatable property");
  for (AnimationProperty& property : mProperties) {
    if (property.mProperty == aProperty) {
      property.mIsRunningOnCompositor = aIsRunning;
      // We currently only set a performance warning message when animations
      // cannot be run on the compositor, so if this animation is running
      // on the compositor we don't need a message.
      if (aIsRunning) {
        property.mPerformanceWarning.reset();
      }
      return;
    }
  }
}

void KeyframeEffect::SetIsRunningOnCompositor(
    const nsCSSPropertyIDSet& aPropertySet, bool aIsRunning) {
  for (AnimationProperty& property : mProperties) {
    if (aPropertySet.HasProperty(property.mProperty)) {
      MOZ_ASSERT(nsCSSProps::PropHasFlags(property.mProperty,
                                          CSSPropFlags::CanAnimateOnCompositor),
                 "Property being animated on compositor is a recognized "
                 "compositor-animatable property");
      property.mIsRunningOnCompositor = aIsRunning;
      // We currently only set a performance warning message when animations
      // cannot be run on the compositor, so if this animation is running
      // on the compositor we don't need a message.
      if (aIsRunning) {
        property.mPerformanceWarning.reset();
      }
    }
  }
}

void KeyframeEffect::ResetIsRunningOnCompositor() {
  for (AnimationProperty& property : mProperties) {
    property.mIsRunningOnCompositor = false;
  }
}

static bool IsSupportedPseudoForWebAnimation(PseudoStyleType aType) {
  // FIXME: Bug 1615469: Support first-line and first-letter for Web Animation.
  return aType == PseudoStyleType::before || aType == PseudoStyleType::after ||
         aType == PseudoStyleType::marker;
}

static const KeyframeEffectOptions& KeyframeEffectOptionsFromUnion(
    const UnrestrictedDoubleOrKeyframeEffectOptions& aOptions) {
  MOZ_ASSERT(aOptions.IsKeyframeEffectOptions());
  return aOptions.GetAsKeyframeEffectOptions();
}

static const KeyframeEffectOptions& KeyframeEffectOptionsFromUnion(
    const UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions) {
  MOZ_ASSERT(aOptions.IsKeyframeAnimationOptions());
  return aOptions.GetAsKeyframeAnimationOptions();
}

template <class OptionsType>
static KeyframeEffectParams KeyframeEffectParamsFromUnion(
    const OptionsType& aOptions, CallerType aCallerType, ErrorResult& aRv) {
  KeyframeEffectParams result;
  if (aOptions.IsUnrestrictedDouble()) {
    return result;
  }

  const KeyframeEffectOptions& options =
      KeyframeEffectOptionsFromUnion(aOptions);

  // If dom.animations-api.compositing.enabled is turned off,
  // iterationComposite and composite are the default value 'replace' in the
  // dictionary.
  result.mIterationComposite = options.mIterationComposite;
  result.mComposite = options.mComposite;

  result.mPseudoType = PseudoStyleType::NotPseudo;
  if (DOMStringIsNull(options.mPseudoElement)) {
    return result;
  }

  RefPtr<nsAtom> pseudoAtom =
      nsCSSPseudoElements::GetPseudoAtom(options.mPseudoElement);
  if (!pseudoAtom) {
    // Per the spec, we throw SyntaxError for syntactically invalid pseudos.
    aRv.ThrowSyntaxError(
        nsPrintfCString("'%s' is a syntactically invalid pseudo-element.",
                        NS_ConvertUTF16toUTF8(options.mPseudoElement).get()));
    return result;
  }

  result.mPseudoType = nsCSSPseudoElements::GetPseudoType(
      pseudoAtom, CSSEnabledState::ForAllContent);
  if (!IsSupportedPseudoForWebAnimation(result.mPseudoType)) {
    // Per the spec, we throw SyntaxError for unsupported pseudos.
    aRv.ThrowSyntaxError(
        nsPrintfCString("'%s' is an unsupported pseudo-element.",
                        NS_ConvertUTF16toUTF8(options.mPseudoElement).get()));
  }

  return result;
}

template <class OptionsType>
/* static */
already_AddRefed<KeyframeEffect> KeyframeEffect::ConstructKeyframeEffect(
    const GlobalObject& aGlobal, Element* aTarget,
    JS::Handle<JSObject*> aKeyframes, const OptionsType& aOptions,
    ErrorResult& aRv) {
  // We should get the document from `aGlobal` instead of the current Realm
  // to make this works in Xray case.
  //
  // In all non-Xray cases, `aGlobal` matches the current Realm, so this
  // matches the spec behavior.
  //
  // In Xray case, the new objects should be created using the document of
  // the target global, but the KeyframeEffect constructors are called in the
  // caller's compartment to access `aKeyframes` object.
  Document* doc = AnimationUtils::GetDocumentFromGlobal(aGlobal.Get());
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  KeyframeEffectParams effectOptions =
      KeyframeEffectParamsFromUnion(aOptions, aGlobal.CallerType(), aRv);
  // An invalid Pseudo-element aborts all further steps.
  if (aRv.Failed()) {
    return nullptr;
  }

  TimingParams timingParams =
      TimingParams::FromOptionsUnion(aOptions, doc, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<KeyframeEffect> effect = new KeyframeEffect(
      doc, OwningAnimationTarget(aTarget, effectOptions.mPseudoType),
      std::move(timingParams), effectOptions);

  effect->SetKeyframes(aGlobal.Context(), aKeyframes, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return effect.forget();
}

nsTArray<AnimationProperty> KeyframeEffect::BuildProperties(
    const ComputedStyle* aStyle) {
  MOZ_ASSERT(aStyle);

  nsTArray<AnimationProperty> result;
  // If mTarget is false (i.e. mTarget.mElement is null), return an empty
  // property array.
  if (!mTarget) {
    return result;
  }

  // When GetComputedKeyframeValues or GetAnimationPropertiesFromKeyframes
  // calculate computed values from |mKeyframes|, they could possibly
  // trigger a subsequent restyle in which we rebuild animations. If that
  // happens we could find that |mKeyframes| is overwritten while it is
  // being iterated over. Normally that shouldn't happen but just in case we
  // make a copy of |mKeyframes| first and iterate over that instead.
  auto keyframesCopy(mKeyframes.Clone());

  result = KeyframeUtils::GetAnimationPropertiesFromKeyframes(
      keyframesCopy, mTarget.mElement, aStyle, mEffectOptions.mComposite);

#ifdef DEBUG
  MOZ_ASSERT(SpecifiedKeyframeArraysAreEqual(mKeyframes, keyframesCopy),
             "Apart from the computed offset members, the keyframes array"
             " should not be modified");
#endif

  mKeyframes.SwapElements(keyframesCopy);
  return result;
}

template <typename FrameEnumFunc>
static void EnumerateContinuationsOrIBSplitSiblings(nsIFrame* aFrame,
                                                    FrameEnumFunc&& aFunc) {
  while (aFrame) {
    aFunc(aFrame);
    aFrame = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(aFrame);
  }
}

void KeyframeEffect::UpdateTarget(Element* aElement,
                                  PseudoStyleType aPseudoType) {
  OwningAnimationTarget newTarget(aElement, aPseudoType);

  if (mTarget == newTarget) {
    // Assign the same target, skip it.
    return;
  }

  if (mTarget) {
    UnregisterTarget();
    ResetIsRunningOnCompositor();

    RequestRestyle(EffectCompositor::RestyleType::Layer);

    nsAutoAnimationMutationBatch mb(mTarget.mElement->OwnerDoc());
    if (mAnimation) {
      MutationObservers::NotifyAnimationRemoved(mAnimation);
    }
  }

  mTarget = newTarget;

  if (mTarget) {
    UpdateTargetRegistration();
    RefPtr<ComputedStyle> computedStyle = GetTargetComputedStyle(Flush::None);
    if (computedStyle) {
      UpdateProperties(computedStyle);
    }

    RequestRestyle(EffectCompositor::RestyleType::Layer);

    nsAutoAnimationMutationBatch mb(mTarget.mElement->OwnerDoc());
    if (mAnimation) {
      MutationObservers::NotifyAnimationAdded(mAnimation);
      mAnimation->ReschedulePendingTasks();
    }
  }

  if (mAnimation) {
    mAnimation->NotifyEffectTargetUpdated();
  }
}

void KeyframeEffect::UpdateTargetRegistration() {
  if (!mTarget) {
    return;
  }

  bool isRelevant = mAnimation && mAnimation->IsRelevant();

  // Animation::IsRelevant() returns a cached value. It only updates when
  // something calls Animation::UpdateRelevance. Whenever our timing changes,
  // we should be notifying our Animation before calling this, so
  // Animation::IsRelevant() should be up-to-date by the time we get here.
  MOZ_ASSERT(isRelevant ==
                 ((IsCurrent() || IsInEffect()) && mAnimation &&
                  mAnimation->ReplaceState() != AnimationReplaceState::Removed),
             "Out of date Animation::IsRelevant value");

  if (isRelevant && !mInEffectSet) {
    EffectSet* effectSet =
        EffectSet::GetOrCreateEffectSet(mTarget.mElement, mTarget.mPseudoType);
    effectSet->AddEffect(*this);
    mInEffectSet = true;
    UpdateEffectSet(effectSet);
    nsIFrame* frame = GetPrimaryFrame();
    EnumerateContinuationsOrIBSplitSiblings(
        frame, [](nsIFrame* aFrame) { aFrame->MarkNeedsDisplayItemRebuild(); });
  } else if (!isRelevant && mInEffectSet) {
    UnregisterTarget();
  }
}

void KeyframeEffect::UnregisterTarget() {
  if (!mInEffectSet) {
    return;
  }

  EffectSet* effectSet =
      EffectSet::GetEffectSet(mTarget.mElement, mTarget.mPseudoType);
  MOZ_ASSERT(effectSet,
             "If mInEffectSet is true, there must be an EffectSet"
             " on the target element");
  mInEffectSet = false;
  if (effectSet) {
    effectSet->RemoveEffect(*this);

    if (effectSet->IsEmpty()) {
      EffectSet::DestroyEffectSet(mTarget.mElement, mTarget.mPseudoType);
    }
  }
  nsIFrame* frame = GetPrimaryFrame();
  EnumerateContinuationsOrIBSplitSiblings(
      frame, [](nsIFrame* aFrame) { aFrame->MarkNeedsDisplayItemRebuild(); });
}

void KeyframeEffect::RequestRestyle(
    EffectCompositor::RestyleType aRestyleType) {
  if (!mTarget) {
    return;
  }
  nsPresContext* presContext =
      nsContentUtils::GetContextForContent(mTarget.mElement);
  if (presContext && mAnimation) {
    presContext->EffectCompositor()->RequestRestyle(
        mTarget.mElement, mTarget.mPseudoType, aRestyleType,
        mAnimation->CascadeLevel());
  }
}

already_AddRefed<ComputedStyle> KeyframeEffect::GetTargetComputedStyle(
    Flush aFlushType) const {
  if (!GetRenderedDocument()) {
    return nullptr;
  }

  MOZ_ASSERT(mTarget,
             "Should only have a document when we have a target element");

  nsAtom* pseudo = PseudoStyle::IsPseudoElement(mTarget.mPseudoType)
                       ? nsCSSPseudoElements::GetPseudoAtom(mTarget.mPseudoType)
                       : nullptr;

  OwningAnimationTarget kungfuDeathGrip(mTarget.mElement, mTarget.mPseudoType);

  return aFlushType == Flush::Style
             ? nsComputedDOMStyle::GetComputedStyle(mTarget.mElement, pseudo)
             : nsComputedDOMStyle::GetComputedStyleNoFlush(mTarget.mElement,
                                                           pseudo);
}

#ifdef DEBUG
void DumpAnimationProperties(
    const RawServoStyleSet* aRawSet,
    nsTArray<AnimationProperty>& aAnimationProperties) {
  for (auto& p : aAnimationProperties) {
    printf("%s\n", nsCString(nsCSSProps::GetStringValue(p.mProperty)).get());
    for (auto& s : p.mSegments) {
      nsString fromValue, toValue;
      s.mFromValue.SerializeSpecifiedValue(p.mProperty, aRawSet, fromValue);
      s.mToValue.SerializeSpecifiedValue(p.mProperty, aRawSet, toValue);
      printf("  %f..%f: %s..%s\n", s.mFromKey, s.mToKey,
             NS_ConvertUTF16toUTF8(fromValue).get(),
             NS_ConvertUTF16toUTF8(toValue).get());
    }
  }
}
#endif

/* static */
already_AddRefed<KeyframeEffect> KeyframeEffect::Constructor(
    const GlobalObject& aGlobal, Element* aTarget,
    JS::Handle<JSObject*> aKeyframes,
    const UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
    ErrorResult& aRv) {
  return ConstructKeyframeEffect(aGlobal, aTarget, aKeyframes, aOptions, aRv);
}

/* static */
already_AddRefed<KeyframeEffect> KeyframeEffect::Constructor(
    const GlobalObject& aGlobal, Element* aTarget,
    JS::Handle<JSObject*> aKeyframes,
    const UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
    ErrorResult& aRv) {
  return ConstructKeyframeEffect(aGlobal, aTarget, aKeyframes, aOptions, aRv);
}

/* static */
already_AddRefed<KeyframeEffect> KeyframeEffect::Constructor(
    const GlobalObject& aGlobal, KeyframeEffect& aSource, ErrorResult& aRv) {
  Document* doc = AnimationUtils::GetCurrentRealmDocument(aGlobal.Context());
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Create a new KeyframeEffect object with aSource's target,
  // iteration composite operation, composite operation, and spacing mode.
  // The constructor creates a new AnimationEffect object by
  // aSource's TimingParams.
  // Note: we don't need to re-throw exceptions since the value specified on
  //       aSource's timing object can be assumed valid.
  RefPtr<KeyframeEffect> effect = new KeyframeEffect(
      doc, OwningAnimationTarget(aSource.mTarget),
      TimingParams(aSource.SpecifiedTiming()), aSource.mEffectOptions);
  // Copy cumulative change hint. mCumulativeChangeHint should be the same as
  // the source one because both of targets are the same.
  effect->mCumulativeChangeHint = aSource.mCumulativeChangeHint;

  // Copy aSource's keyframes and animation properties.
  // Note: We don't call SetKeyframes directly, which might revise the
  //       computed offsets and rebuild the animation properties.
  effect->mKeyframes = aSource.mKeyframes.Clone();
  effect->mProperties = aSource.mProperties.Clone();
  for (auto iter = aSource.mBaseValues.ConstIter(); !iter.Done(); iter.Next()) {
    // XXX Should this use non-const Iter() and then pass
    // std::move(iter.Data())? Otherwise aSource might be a const&...
    effect->mBaseValues.Put(iter.Key(), RefPtr{iter.Data()});
  }
  return effect.forget();
}

void KeyframeEffect::SetPseudoElement(const nsAString& aPseudoElement,
                                      ErrorResult& aRv) {
  PseudoStyleType pseudoType = PseudoStyleType::NotPseudo;
  if (DOMStringIsNull(aPseudoElement)) {
    UpdateTarget(mTarget.mElement, pseudoType);
    return;
  }

  // Note: GetPseudoAtom() also returns nullptr for the null string,
  // so we handle null case before this.
  RefPtr<nsAtom> pseudoAtom =
      nsCSSPseudoElements::GetPseudoAtom(aPseudoElement);
  if (!pseudoAtom) {
    // Per the spec, we throw SyntaxError for syntactically invalid pseudos.
    aRv.ThrowSyntaxError(
        nsPrintfCString("'%s' is a syntactically invalid pseudo-element.",
                        NS_ConvertUTF16toUTF8(aPseudoElement).get()));
    return;
  }

  pseudoType = nsCSSPseudoElements::GetPseudoType(
      pseudoAtom, CSSEnabledState::ForAllContent);
  if (!IsSupportedPseudoForWebAnimation(pseudoType)) {
    // Per the spec, we throw SyntaxError for unsupported pseudos.
    aRv.ThrowSyntaxError(
        nsPrintfCString("'%s' is an unsupported pseudo-element.",
                        NS_ConvertUTF16toUTF8(aPseudoElement).get()));
    return;
  }

  UpdateTarget(mTarget.mElement, pseudoType);
}

static void CreatePropertyValue(
    nsCSSPropertyID aProperty, float aOffset,
    const Maybe<ComputedTimingFunction>& aTimingFunction,
    const AnimationValue& aValue, dom::CompositeOperation aComposite,
    const RawServoStyleSet* aRawSet, AnimationPropertyValueDetails& aResult) {
  aResult.mOffset = aOffset;

  if (!aValue.IsNull()) {
    nsString stringValue;
    aValue.SerializeSpecifiedValue(aProperty, aRawSet, stringValue);
    aResult.mValue.Construct(stringValue);
  }

  if (aTimingFunction) {
    aResult.mEasing.Construct();
    aTimingFunction->AppendToString(aResult.mEasing.Value());
  } else {
    aResult.mEasing.Construct(NS_LITERAL_STRING("linear"));
  }

  aResult.mComposite = aComposite;
}

void KeyframeEffect::GetProperties(
    nsTArray<AnimationPropertyDetails>& aProperties, ErrorResult& aRv) const {
  const RawServoStyleSet* rawSet =
      mDocument->StyleSetForPresShellOrMediaQueryEvaluation()->RawSet();

  for (const AnimationProperty& property : mProperties) {
    AnimationPropertyDetails propertyDetails;
    propertyDetails.mProperty =
        NS_ConvertASCIItoUTF16(nsCSSProps::GetStringValue(property.mProperty));
    propertyDetails.mRunningOnCompositor = property.mIsRunningOnCompositor;

    nsAutoString localizedString;
    if (property.mPerformanceWarning &&
        property.mPerformanceWarning->ToLocalizedString(localizedString)) {
      propertyDetails.mWarning.Construct(localizedString);
    }

    if (!propertyDetails.mValues.SetCapacity(property.mSegments.Length(),
                                             mozilla::fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    for (size_t segmentIdx = 0, segmentLen = property.mSegments.Length();
         segmentIdx < segmentLen; segmentIdx++) {
      const AnimationPropertySegment& segment = property.mSegments[segmentIdx];

      binding_detail::FastAnimationPropertyValueDetails fromValue;
      CreatePropertyValue(property.mProperty, segment.mFromKey,
                          segment.mTimingFunction, segment.mFromValue,
                          segment.mFromComposite, rawSet, fromValue);
      // We don't apply timing functions for zero-length segments, so
      // don't return one here.
      if (segment.mFromKey == segment.mToKey) {
        fromValue.mEasing.Reset();
      }
      // Even though we called SetCapacity before, this could fail, since we
      // might add multiple elements to propertyDetails.mValues for an element
      // of property.mSegments in the cases mentioned below.
      if (!propertyDetails.mValues.AppendElement(fromValue,
                                                 mozilla::fallible)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }

      // Normally we can ignore the to-value for this segment since it is
      // identical to the from-value from the next segment. However, we need
      // to add it if either:
      // a) this is the last segment, or
      // b) the next segment's from-value differs.
      if (segmentIdx == segmentLen - 1 ||
          property.mSegments[segmentIdx + 1].mFromValue != segment.mToValue) {
        binding_detail::FastAnimationPropertyValueDetails toValue;
        CreatePropertyValue(property.mProperty, segment.mToKey, Nothing(),
                            segment.mToValue, segment.mToComposite, rawSet,
                            toValue);
        // It doesn't really make sense to have a timing function on the
        // last property value or before a sudden jump so we just drop the
        // easing property altogether.
        toValue.mEasing.Reset();
        if (!propertyDetails.mValues.AppendElement(toValue,
                                                   mozilla::fallible)) {
          aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
          return;
        }
      }
    }

    aProperties.AppendElement(propertyDetails);
  }
}

void KeyframeEffect::GetKeyframes(JSContext* aCx, nsTArray<JSObject*>& aResult,
                                  ErrorResult& aRv) const {
  MOZ_ASSERT(aResult.IsEmpty());
  MOZ_ASSERT(!aRv.Failed());

  if (!aResult.SetCapacity(mKeyframes.Length(), mozilla::fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  bool isCSSAnimation = mAnimation && mAnimation->AsCSSAnimation();

  // For Servo, when we have CSS Animation @keyframes with variables, we convert
  // shorthands to longhands if needed, and store a reference to the unparsed
  // value. When it comes time to serialize, however, what do you serialize for
  // a longhand that comes from a variable reference in a shorthand? Servo says,
  // "an empty string" which is not particularly helpful.
  //
  // We should just store shorthands as-is (bug 1391537) and then return the
  // variable references, but for now, since we don't do that, and in order to
  // be consistent with Gecko, we just expand the variables (assuming we have
  // enough context to do so). For that we need to grab the ComputedStyle so we
  // know what custom property values to provide.
  RefPtr<ComputedStyle> computedStyle;
  if (isCSSAnimation) {
    // The following will flush style but that's ok since if you update
    // a variable's computed value, you expect to see that updated value in the
    // result of getKeyframes().
    //
    // If we don't have a target, the following will return null. In that case
    // we might end up returning variables as-is or empty string. That should be
    // acceptable however, since such a case is rare and this is only
    // short-term (and unshipped) behavior until bug 1391537 is fixed.
    computedStyle = GetTargetComputedStyle(Flush::Style);
  }

  const RawServoStyleSet* rawSet =
      mDocument->StyleSetForPresShellOrMediaQueryEvaluation()->RawSet();

  for (const Keyframe& keyframe : mKeyframes) {
    // Set up a dictionary object for the explicit members
    BaseComputedKeyframe keyframeDict;
    if (keyframe.mOffset) {
      keyframeDict.mOffset.SetValue(keyframe.mOffset.value());
    }
    MOZ_ASSERT(keyframe.mComputedOffset != Keyframe::kComputedOffsetNotSet,
               "Invalid computed offset");
    keyframeDict.mComputedOffset.Construct(keyframe.mComputedOffset);
    if (keyframe.mTimingFunction) {
      keyframeDict.mEasing.Truncate();
      keyframe.mTimingFunction.ref().AppendToString(keyframeDict.mEasing);
    }  // else if null, leave easing as its default "linear".

    // With the pref off (i.e. dom.animations-api.compositing.enabled:false),
    // the dictionary-to-JS conversion will skip this member entirely.
    keyframeDict.mComposite = keyframe.mComposite;

    JS::Rooted<JS::Value> keyframeJSValue(aCx);
    if (!ToJSValue(aCx, keyframeDict, &keyframeJSValue)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    RefPtr<RawServoDeclarationBlock> customProperties;
    // A workaround for CSS Animations in servo backend, custom properties in
    // keyframe are stored in a servo's declaration block. Find the declaration
    // block to resolve CSS variables in the keyframe.
    // This workaround will be solved by bug 1391537.
    if (isCSSAnimation) {
      for (const PropertyValuePair& propertyValue : keyframe.mPropertyValues) {
        if (propertyValue.mProperty ==
            nsCSSPropertyID::eCSSPropertyExtra_variable) {
          customProperties = propertyValue.mServoDeclarationBlock;
          break;
        }
      }
    }

    JS::Rooted<JSObject*> keyframeObject(aCx, &keyframeJSValue.toObject());
    for (const PropertyValuePair& propertyValue : keyframe.mPropertyValues) {
      nsAutoString stringValue;
      // Don't serialize the custom properties for this keyframe.
      if (propertyValue.mProperty ==
          nsCSSPropertyID::eCSSPropertyExtra_variable) {
        continue;
      }
      if (propertyValue.mServoDeclarationBlock) {
        Servo_DeclarationBlock_SerializeOneValue(
            propertyValue.mServoDeclarationBlock, propertyValue.mProperty,
            &stringValue, computedStyle, customProperties, rawSet);
      } else {
        RawServoAnimationValue* value =
            mBaseValues.GetWeak(propertyValue.mProperty);

        if (value) {
          Servo_AnimationValue_Serialize(value, propertyValue.mProperty, rawSet,
                                         &stringValue);
        }
      }

      // Basically, we need to do the mapping:
      // * eCSSProperty_offset => "cssOffset"
      // * eCSSProperty_float => "cssFloat"
      // This means if property refers to the CSS "offset"/"float" property,
      // return the string "cssOffset"/"cssFloat". (So avoid overlapping
      // "offset" property in BaseKeyframe.)
      // https://drafts.csswg.org/web-animations/#property-name-conversion
      const char* name = nullptr;
      switch (propertyValue.mProperty) {
        case nsCSSPropertyID::eCSSProperty_offset:
          name = "cssOffset";
          break;
        case nsCSSPropertyID::eCSSProperty_float:
          // FIXME: Bug 1582314: Should handle cssFloat manually if we remove it
          // from nsCSSProps::PropertyIDLName().
        default:
          name = nsCSSProps::PropertyIDLName(propertyValue.mProperty);
      }

      JS::Rooted<JS::Value> value(aCx);
      if (!ToJSValue(aCx, stringValue, &value) ||
          !JS_DefineProperty(aCx, keyframeObject, name, value,
                             JSPROP_ENUMERATE)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return;
      }
    }

    aResult.AppendElement(keyframeObject);
  }
}

/* static */ const TimeDuration
KeyframeEffect::OverflowRegionRefreshInterval() {
  // The amount of time we can wait between updating throttled animations
  // on the main thread that influence the overflow region.
  static const TimeDuration kOverflowRegionRefreshInterval =
      TimeDuration::FromMilliseconds(200);

  return kOverflowRegionRefreshInterval;
}

static bool IsDefinitivelyInvisibleDueToOpacity(const nsIFrame& aFrame) {
  if (!aFrame.Style()->IsInOpacityZeroSubtree()) {
    return false;
  }

  // Find the root of the opacity: 0 subtree.
  const nsIFrame* root = &aFrame;
  while (true) {
    auto* parent = root->GetInFlowParent();
    if (!parent || !parent->Style()->IsInOpacityZeroSubtree()) {
      break;
    }
    root = parent;
  }

  MOZ_ASSERT(root && root->Style()->IsInOpacityZeroSubtree());

  // If aFrame is the root of the opacity: zero subtree, we can't prove we can
  // optimize it away, because it may have an opacity animation itself.
  if (root == &aFrame) {
    return false;
  }

  // Even if we're in an opacity: zero subtree, if the root of the subtree may
  // have an opacity animation, we can't optimize us away, as we may become
  // visible ourselves.
  return !root->HasAnimationOfOpacity();
}

static bool CanOptimizeAwayDueToOpacity(const KeyframeEffect& aEffect,
                                        const nsIFrame& aFrame) {
  if (!aFrame.Style()->IsInOpacityZeroSubtree()) {
    return false;
  }
  if (IsDefinitivelyInvisibleDueToOpacity(aFrame)) {
    return true;
  }
  return !aEffect.HasOpacityChange();
}

bool KeyframeEffect::CanThrottleIfNotVisible(nsIFrame& aFrame) const {
  // Unless we are newly in-effect, we can throttle the animation if the
  // animation is paint only and the target frame is out of view or the document
  // is in background tabs.
  if (!mInEffectOnLastAnimationTimingUpdate || !CanIgnoreIfNotVisible()) {
    return false;
  }

  PresShell* presShell = GetPresShell();
  if (presShell && !presShell->IsActive()) {
    return true;
  }

  const bool isVisibilityHidden =
      !aFrame.IsVisibleOrMayHaveVisibleDescendants();
  const bool canOptimizeAwayVisibility =
      isVisibilityHidden && !HasVisibilityChange();

  const bool invisible = canOptimizeAwayVisibility ||
                         CanOptimizeAwayDueToOpacity(*this, aFrame) ||
                         aFrame.IsScrolledOutOfView();
  if (!invisible) {
    return false;
  }

  // If there are no overflow change hints, we don't need to worry about
  // unthrottling the animation periodically to update scrollbar positions for
  // the overflow region.
  if (!HasPropertiesThatMightAffectOverflow()) {
    return true;
  }

  // Don't throttle finite animations since the animation might suddenly
  // come into view and if it was throttled it will be out-of-sync.
  if (HasFiniteActiveDuration()) {
    return false;
  }

  return isVisibilityHidden ? CanThrottleOverflowChangesInScrollable(aFrame)
                            : CanThrottleOverflowChanges(aFrame);
}

bool KeyframeEffect::CanThrottle() const {
  // Unthrottle if we are not in effect or current. This will be the case when
  // our owning animation has finished, is idle, or when we are in the delay
  // phase (but without a backwards fill). In each case the computed progress
  // value produced on each tick will be the same so we will skip requesting
  // unnecessary restyles in NotifyAnimationTimingUpdated. Any calls we *do* get
  // here will be because of a change in state (e.g. we are newly finished or
  // newly no longer in effect) in which case we shouldn't throttle the sample.
  if (!IsInEffect() || !IsCurrent()) {
    return false;
  }

  nsIFrame* frame = GetStyleFrame();
  if (!frame) {
    // There are two possible cases here.
    // a) No target element
    // b) The target element has no frame, e.g. because it is in a display:none
    //    subtree.
    // In either case we can throttle the animation because there is no
    // need to update on the main thread.
    return true;
  }

  if (CanThrottleIfNotVisible(*frame)) {
    return true;
  }

  EffectSet* effectSet = nullptr;
  for (const AnimationProperty& property : mProperties) {
    if (!property.mIsRunningOnCompositor) {
      return false;
    }

    MOZ_ASSERT(nsCSSPropertyIDSet::CompositorAnimatables().HasProperty(
                   property.mProperty),
               "The property should be able to run on the compositor");
    if (!effectSet) {
      effectSet =
          EffectSet::GetEffectSet(mTarget.mElement, mTarget.mPseudoType);
      MOZ_ASSERT(effectSet,
                 "CanThrottle should be called on an effect "
                 "associated with a target element");
    }
    MOZ_ASSERT(HasEffectiveAnimationOfProperty(property.mProperty, *effectSet),
               "There should be an effective animation of the property while "
               "it is marked as being run on the compositor");

    DisplayItemType displayItemType =
        LayerAnimationInfo::GetDisplayItemTypeForProperty(property.mProperty);

    // Note that AnimationInfo::GetGenarationFromFrame() is supposed to work
    // with the primary frame instead of the style frame.
    Maybe<uint64_t> generation = layers::AnimationInfo::GetGenerationFromFrame(
        GetPrimaryFrame(), displayItemType);
    // Unthrottle if the animation needs to be brought up to date
    if (!generation || effectSet->GetAnimationGeneration() != *generation) {
      return false;
    }

    // If this is a transform animation that affects the overflow region,
    // we should unthrottle the animation periodically.
    if (HasPropertiesThatMightAffectOverflow() &&
        !CanThrottleOverflowChangesInScrollable(*frame)) {
      return false;
    }
  }

  return true;
}

bool KeyframeEffect::CanThrottleOverflowChanges(const nsIFrame& aFrame) const {
  TimeStamp now = aFrame.PresContext()->RefreshDriver()->MostRecentRefresh();

  EffectSet* effectSet =
      EffectSet::GetEffectSet(mTarget.mElement, mTarget.mPseudoType);
  MOZ_ASSERT(effectSet,
             "CanOverflowTransformChanges is expected to be called"
             " on an effect in an effect set");
  MOZ_ASSERT(mAnimation,
             "CanOverflowTransformChanges is expected to be called"
             " on an effect with a parent animation");
  TimeStamp lastSyncTime = effectSet->LastOverflowAnimationSyncTime();
  // If this animation can cause overflow, we can throttle some of the ticks.
  return (!lastSyncTime.IsNull() &&
          (now - lastSyncTime) < OverflowRegionRefreshInterval());
}

bool KeyframeEffect::CanThrottleOverflowChangesInScrollable(
    nsIFrame& aFrame) const {
  // If the target element is not associated with any documents, we don't care
  // it.
  Document* doc = GetRenderedDocument();
  if (!doc) {
    return true;
  }

  bool hasIntersectionObservers = doc->HasIntersectionObservers();

  // If we know that the animation cannot cause overflow,
  // we can just disable flushes for this animation.

  // If we don't show scrollbars and have no intersection observers, we don't
  // care about overflow.
  if (LookAndFeel::GetInt(LookAndFeel::eIntID_ShowHideScrollbars) == 0 &&
      !hasIntersectionObservers) {
    return true;
  }

  if (CanThrottleOverflowChanges(aFrame)) {
    return true;
  }

  // If we have any intersection observers, we unthrottle this transform
  // animation periodically.
  if (hasIntersectionObservers) {
    return false;
  }

  // If the nearest scrollable ancestor has overflow:hidden,
  // we don't care about overflow.
  nsIScrollableFrame* scrollable =
      nsLayoutUtils::GetNearestScrollableFrame(&aFrame);
  if (!scrollable) {
    return true;
  }

  ScrollStyles ss = scrollable->GetScrollStyles();
  if (ss.mVertical == StyleOverflow::Hidden &&
      ss.mHorizontal == StyleOverflow::Hidden &&
      scrollable->GetLogicalScrollPosition() == nsPoint(0, 0)) {
    return true;
  }

  return false;
}

nsIFrame* KeyframeEffect::GetStyleFrame() const {
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) {
    return nullptr;
  }

  return nsLayoutUtils::GetStyleFrame(frame);
}

nsIFrame* KeyframeEffect::GetPrimaryFrame() const {
  nsIFrame* frame = nullptr;
  if (!mTarget) {
    return frame;
  }

  if (mTarget.mPseudoType == PseudoStyleType::before) {
    frame = nsLayoutUtils::GetBeforeFrame(mTarget.mElement);
  } else if (mTarget.mPseudoType == PseudoStyleType::after) {
    frame = nsLayoutUtils::GetAfterFrame(mTarget.mElement);
  } else if (mTarget.mPseudoType == PseudoStyleType::marker) {
    frame = nsLayoutUtils::GetMarkerFrame(mTarget.mElement);
  } else {
    frame = mTarget.mElement->GetPrimaryFrame();
    MOZ_ASSERT(mTarget.mPseudoType == PseudoStyleType::NotPseudo,
               "unknown mTarget.mPseudoType");
  }

  return frame;
}

Document* KeyframeEffect::GetRenderedDocument() const {
  if (!mTarget) {
    return nullptr;
  }
  return mTarget.mElement->GetComposedDoc();
}

PresShell* KeyframeEffect::GetPresShell() const {
  Document* doc = GetRenderedDocument();
  if (!doc) {
    return nullptr;
  }
  return doc->GetPresShell();
}

/* static */
bool KeyframeEffect::IsGeometricProperty(const nsCSSPropertyID aProperty) {
  MOZ_ASSERT(!nsCSSProps::IsShorthand(aProperty),
             "Property should be a longhand property");

  switch (aProperty) {
    case eCSSProperty_bottom:
    case eCSSProperty_height:
    case eCSSProperty_left:
    case eCSSProperty_margin_bottom:
    case eCSSProperty_margin_left:
    case eCSSProperty_margin_right:
    case eCSSProperty_margin_top:
    case eCSSProperty_padding_bottom:
    case eCSSProperty_padding_left:
    case eCSSProperty_padding_right:
    case eCSSProperty_padding_top:
    case eCSSProperty_right:
    case eCSSProperty_top:
    case eCSSProperty_width:
      return true;
    default:
      return false;
  }
}

/* static */
bool KeyframeEffect::CanAnimateTransformOnCompositor(
    const nsIFrame* aFrame,
    AnimationPerformanceWarning::Type& aPerformanceWarning /* out */) {
  // In some cases, such as when we are simply collecting all the compositor
  // animations regardless of the frame on which they run in order to calculate
  // change hints, |aFrame| will be the style frame. However, even in that case
  // we should look at the primary frame since that is where the transform will
  // be applied.
  const nsIFrame* primaryFrame =
      nsLayoutUtils::GetPrimaryFrameFromStyleFrame(aFrame);

  // Note that testing BackfaceIsHidden() is not a sufficient test for
  // what we need for animating backface-visibility correctly if we
  // remove the above test for Extend3DContext(); that would require
  // looking at backface-visibility on descendants as well. See bug 1186204.
  if (primaryFrame->BackfaceIsHidden()) {
    aPerformanceWarning =
        AnimationPerformanceWarning::Type::TransformBackfaceVisibilityHidden;
    return false;
  }
  // Async 'transform' animations of aFrames with SVG transforms is not
  // supported.  See bug 779599.
  if (primaryFrame->IsSVGTransformed()) {
    aPerformanceWarning = AnimationPerformanceWarning::Type::TransformSVG;
    return false;
  }

  return true;
}

bool KeyframeEffect::ShouldBlockAsyncTransformAnimations(
    const nsIFrame* aFrame, const nsCSSPropertyIDSet& aPropertySet,
    AnimationPerformanceWarning::Type& aPerformanceWarning /* out */) const {
  EffectSet* effectSet =
      EffectSet::GetEffectSet(mTarget.mElement, mTarget.mPseudoType);
  // The various transform properties ('transform', 'scale' etc.) get combined
  // on the compositor.
  //
  // As a result, if we have an animation of 'scale' and 'translate', but the
  // 'translate' property is covered by an !important rule, we will not be
  // able to combine the result on the compositor since we won't have the
  // !important rule to incorporate. In that case we should run all the
  // transform-related animations on the main thread (where we have the
  // !important rule).
  nsCSSPropertyIDSet blockedProperties =
      effectSet->PropertiesWithImportantRules().Intersect(
          effectSet->PropertiesForAnimationsLevel());
  if (blockedProperties.Intersects(aPropertySet)) {
    aPerformanceWarning =
        AnimationPerformanceWarning::Type::TransformIsBlockedByImportantRules;
    return true;
  }

  const bool enableMainthreadSynchronizationWithGeometricAnimations =
      StaticPrefs::
          dom_animations_mainthread_synchronization_with_geometric_animations();

  for (const AnimationProperty& property : mProperties) {
    // If there is a property for animations level that is overridden by
    // !important rules, it should not block other animations from running
    // on the compositor.
    // NOTE: We don't currently check for !important rules for properties that
    // don't run on the compositor. As result such properties (e.g. margin-left)
    // can still block async animations even if they are overridden by
    // !important rules.
    if (effectSet &&
        effectSet->PropertiesWithImportantRules().HasProperty(
            property.mProperty) &&
        effectSet->PropertiesForAnimationsLevel().HasProperty(
            property.mProperty)) {
      continue;
    }
    // Check for geometric properties
    if (enableMainthreadSynchronizationWithGeometricAnimations &&
        IsGeometricProperty(property.mProperty)) {
      aPerformanceWarning =
          AnimationPerformanceWarning::Type::TransformWithGeometricProperties;
      return true;
    }

    // Check for unsupported transform animations
    if (LayerAnimationInfo::GetCSSPropertiesFor(DisplayItemType::TYPE_TRANSFORM)
            .HasProperty(property.mProperty)) {
      if (!CanAnimateTransformOnCompositor(aFrame, aPerformanceWarning)) {
        return true;
      }
    }
  }

  return false;
}

bool KeyframeEffect::HasGeometricProperties() const {
  for (const AnimationProperty& property : mProperties) {
    if (IsGeometricProperty(property.mProperty)) {
      return true;
    }
  }

  return false;
}

void KeyframeEffect::SetPerformanceWarning(
    const nsCSSPropertyIDSet& aPropertySet,
    const AnimationPerformanceWarning& aWarning) {
  nsCSSPropertyIDSet curr = aPropertySet;
  for (AnimationProperty& property : mProperties) {
    if (!curr.HasProperty(property.mProperty)) {
      continue;
    }
    property.SetPerformanceWarning(aWarning, mTarget.mElement);
    curr.RemoveProperty(property.mProperty);
    if (curr.IsEmpty()) {
      return;
    }
  }
}

already_AddRefed<ComputedStyle>
KeyframeEffect::CreateComputedStyleForAnimationValue(
    nsCSSPropertyID aProperty, const AnimationValue& aValue,
    nsPresContext* aPresContext, const ComputedStyle* aBaseComputedStyle) {
  MOZ_ASSERT(aBaseComputedStyle,
             "CreateComputedStyleForAnimationValue needs to be called "
             "with a valid ComputedStyle");

  Element* elementForResolve = EffectCompositor::GetElementToRestyle(
      mTarget.mElement, mTarget.mPseudoType);
  // The element may be null if, for example, we target a pseudo-element that no
  // longer exists.
  if (!elementForResolve) {
    return nullptr;
  }

  ServoStyleSet* styleSet = aPresContext->StyleSet();
  return styleSet->ResolveServoStyleByAddingAnimation(
      elementForResolve, aBaseComputedStyle, aValue.mServo);
}

void KeyframeEffect::CalculateCumulativeChangeHint(
    const ComputedStyle* aComputedStyle) {
  mCumulativeChangeHint = nsChangeHint(0);
  mNeedsStyleData = false;

  nsPresContext* presContext =
      mTarget ? nsContentUtils::GetContextForContent(mTarget.mElement)
              : nullptr;
  if (!presContext) {
    // Change hints make no sense if we're not rendered.
    //
    // Actually, we cannot even post them anywhere.
    mNeedsStyleData = true;
    return;
  }

  for (const AnimationProperty& property : mProperties) {
    // For opacity property we don't produce any change hints that are not
    // included in nsChangeHint_Hints_CanIgnoreIfNotVisible so we can throttle
    // opacity animations regardless of the change they produce.  This
    // optimization is particularly important since it allows us to throttle
    // opacity animations with missing 0%/100% keyframes.
    if (property.mProperty == eCSSProperty_opacity) {
      continue;
    }

    for (const AnimationPropertySegment& segment : property.mSegments) {
      // In case composite operation is not 'replace' or value is null,
      // we can't throttle animations which will not cause any layout changes
      // on invisible elements because we can't calculate the change hint for
      // such properties until we compose it.
      if (!segment.HasReplaceableValues()) {
        if (!nsCSSPropertyIDSet::TransformLikeProperties().HasProperty(
                property.mProperty)) {
          mCumulativeChangeHint = ~nsChangeHint_Hints_CanIgnoreIfNotVisible;
          return;
        }
        // We try a little harder to optimize transform animations simply
        // because they are so common (the second-most commonly animated
        // property at the time of writing).  So if we encounter a transform
        // segment that needs composing with the underlying value, we just add
        // all the change hints a transform animation is known to be able to
        // generate.
        mCumulativeChangeHint |=
            nsChangeHint_ComprehensiveAddOrRemoveTransform |
            nsChangeHint_UpdatePostTransformOverflow |
            nsChangeHint_UpdateTransformLayer;
        continue;
      }

      RefPtr<ComputedStyle> fromContext = CreateComputedStyleForAnimationValue(
          property.mProperty, segment.mFromValue, presContext, aComputedStyle);
      if (!fromContext) {
        mCumulativeChangeHint = ~nsChangeHint_Hints_CanIgnoreIfNotVisible;
        mNeedsStyleData = true;
        return;
      }

      RefPtr<ComputedStyle> toContext = CreateComputedStyleForAnimationValue(
          property.mProperty, segment.mToValue, presContext, aComputedStyle);
      if (!toContext) {
        mCumulativeChangeHint = ~nsChangeHint_Hints_CanIgnoreIfNotVisible;
        mNeedsStyleData = true;
        return;
      }

      uint32_t equalStructs = 0;
      nsChangeHint changeHint =
          fromContext->CalcStyleDifference(*toContext, &equalStructs);

      mCumulativeChangeHint |= changeHint;
    }
  }
}

void KeyframeEffect::SetAnimation(Animation* aAnimation) {
  if (mAnimation == aAnimation) {
    return;
  }

  // Restyle for the old animation.
  RequestRestyle(EffectCompositor::RestyleType::Layer);

  mAnimation = aAnimation;

  // The order of these function calls is important:
  // NotifyAnimationTimingUpdated() need the updated mIsRelevant flag to check
  // if it should create the effectSet or not, and MarkCascadeNeedsUpdate()
  // needs a valid effectSet, so we should call them in this order.
  if (mAnimation) {
    mAnimation->UpdateRelevance();
  }
  NotifyAnimationTimingUpdated(PostRestyleMode::IfNeeded);
  if (mAnimation) {
    MarkCascadeNeedsUpdate();
  }
}

bool KeyframeEffect::CanIgnoreIfNotVisible() const {
  if (!StaticPrefs::dom_animations_offscreen_throttling()) {
    return false;
  }

  // FIXME: For further sophisticated optimization we need to check
  // change hint on the segment corresponding to computedTiming.progress.
  return NS_IsHintSubset(mCumulativeChangeHint,
                         nsChangeHint_Hints_CanIgnoreIfNotVisible);
}

void KeyframeEffect::MarkCascadeNeedsUpdate() {
  if (!mTarget) {
    return;
  }

  EffectSet* effectSet =
      EffectSet::GetEffectSet(mTarget.mElement, mTarget.mPseudoType);
  if (!effectSet) {
    return;
  }
  effectSet->MarkCascadeNeedsUpdate();
}

/* static */
bool KeyframeEffect::HasComputedTimingChanged(
    const ComputedTiming& aComputedTiming,
    IterationCompositeOperation aIterationComposite,
    const Nullable<double>& aProgressOnLastCompose,
    uint64_t aCurrentIterationOnLastCompose) {
  // Typically we don't need to request a restyle if the progress hasn't
  // changed since the last call to ComposeStyle. The one exception is if the
  // iteration composite mode is 'accumulate' and the current iteration has
  // changed, since that will often produce a different result.
  return aComputedTiming.mProgress != aProgressOnLastCompose ||
         (aIterationComposite == IterationCompositeOperation::Accumulate &&
          aComputedTiming.mCurrentIteration != aCurrentIterationOnLastCompose);
}

bool KeyframeEffect::HasComputedTimingChanged() const {
  ComputedTiming computedTiming = GetComputedTiming();
  return HasComputedTimingChanged(
      computedTiming, mEffectOptions.mIterationComposite,
      mProgressOnLastCompose, mCurrentIterationOnLastCompose);
}

bool KeyframeEffect::ContainsAnimatedScale(const nsIFrame* aFrame) const {
  // For display:table content, transform animations run on the table wrapper
  // frame. If we are being passed a frame that doesn't support transforms
  // (i.e. the inner table frame) we could just return false, but it possibly
  // means we looked up the wrong EffectSet so for now we just assert instead.
  MOZ_ASSERT(aFrame && aFrame->IsFrameOfType(nsIFrame::eSupportsCSSTransforms),
             "We should be passed a frame that supports transforms");

  if (!IsCurrent()) {
    return false;
  }

  if (!mAnimation ||
      mAnimation->ReplaceState() == AnimationReplaceState::Removed) {
    return false;
  }

  for (const AnimationProperty& prop : mProperties) {
    if (prop.mProperty != eCSSProperty_transform &&
        prop.mProperty != eCSSProperty_scale &&
        prop.mProperty != eCSSProperty_rotate) {
      continue;
    }

    AnimationValue baseStyle = BaseStyle(prop.mProperty);
    if (!baseStyle.IsNull()) {
      gfx::Size size = baseStyle.GetScaleValue(aFrame);
      if (size != gfx::Size(1.0f, 1.0f)) {
        return true;
      }
    }

    // This is actually overestimate because there are some cases that combining
    // the base value and from/to value produces 1:1 scale. But it doesn't
    // really matter.
    for (const AnimationPropertySegment& segment : prop.mSegments) {
      if (!segment.mFromValue.IsNull()) {
        gfx::Size from = segment.mFromValue.GetScaleValue(aFrame);
        if (from != gfx::Size(1.0f, 1.0f)) {
          return true;
        }
      }
      if (!segment.mToValue.IsNull()) {
        gfx::Size to = segment.mToValue.GetScaleValue(aFrame);
        if (to != gfx::Size(1.0f, 1.0f)) {
          return true;
        }
      }
    }
  }

  return false;
}

void KeyframeEffect::UpdateEffectSet(EffectSet* aEffectSet) const {
  if (!mInEffectSet) {
    return;
  }

  EffectSet* effectSet =
      aEffectSet
          ? aEffectSet
          : EffectSet::GetEffectSet(mTarget.mElement, mTarget.mPseudoType);
  if (!effectSet) {
    return;
  }

  nsIFrame* styleFrame = GetStyleFrame();
  if (HasAnimationOfPropertySet(nsCSSPropertyIDSet::OpacityProperties())) {
    effectSet->SetMayHaveOpacityAnimation();
    EnumerateContinuationsOrIBSplitSiblings(styleFrame, [](nsIFrame* aFrame) {
      aFrame->SetMayHaveOpacityAnimation();
    });
  }

  nsIFrame* primaryFrame = GetPrimaryFrame();
  if (HasAnimationOfPropertySet(
          nsCSSPropertyIDSet::TransformLikeProperties())) {
    effectSet->SetMayHaveTransformAnimation();
    // For table frames, it's not clear if we should iterate over the
    // continuations of the table wrapper or the inner table frame.
    //
    // Fortunately, this is not currently an issue because we only split tables
    // when printing (page breaks) where we don't run animations.
    EnumerateContinuationsOrIBSplitSiblings(primaryFrame, [](nsIFrame* aFrame) {
      aFrame->SetMayHaveTransformAnimation();
    });
  }
}

KeyframeEffect::MatchForCompositor KeyframeEffect::IsMatchForCompositor(
    const nsCSSPropertyIDSet& aPropertySet, const nsIFrame* aFrame,
    const EffectSet& aEffects,
    AnimationPerformanceWarning::Type& aPerformanceWarning /* out */) const {
  MOZ_ASSERT(mAnimation);

  if (!mAnimation->IsRelevant()) {
    return KeyframeEffect::MatchForCompositor::No;
  }

  if (mAnimation->ShouldBeSynchronizedWithMainThread(aPropertySet, aFrame,
                                                     aPerformanceWarning)) {
    // For a given |aFrame|, we don't want some animations of |aProperty| to
    // run on the compositor and others to run on the main thread, so if any
    // need to be synchronized with the main thread, run them all there.
    return KeyframeEffect::MatchForCompositor::NoAndBlockThisProperty;
  }

  if (!HasEffectiveAnimationOfPropertySet(aPropertySet, aEffects)) {
    return KeyframeEffect::MatchForCompositor::No;
  }

  // If we know that the animation is not visible, we don't need to send the
  // animation to the compositor.
  if (!aFrame->IsVisibleOrMayHaveVisibleDescendants() ||
      IsDefinitivelyInvisibleDueToOpacity(*aFrame) ||
      aFrame->IsScrolledOutOfView()) {
    return KeyframeEffect::MatchForCompositor::NoAndBlockThisProperty;
  }

  if (aPropertySet.HasProperty(eCSSProperty_background_color)) {
    if (!StaticPrefs::gfx_omta_background_color()) {
      return KeyframeEffect::MatchForCompositor::No;
    }
  }

  // We can't run this background color animation on the compositor if there
  // is any `current-color` keyframe.
  if (mHasCurrentColor) {
    aPerformanceWarning = AnimationPerformanceWarning::Type::HasCurrentColor;
    return KeyframeEffect::MatchForCompositor::NoAndBlockThisProperty;
  }

  return mAnimation->IsPlaying() ? KeyframeEffect::MatchForCompositor::Yes
                                 : KeyframeEffect::MatchForCompositor::IfNeeded;
}

}  // namespace dom
}  // namespace mozilla
