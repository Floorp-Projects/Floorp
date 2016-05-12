/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/KeyframeEffect.h"

#include "mozilla/dom/AnimatableBinding.h"
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/LookAndFeel.h" // For LookAndFeel::GetInt
#include "mozilla/KeyframeUtils.h"
#include "mozilla/StyleAnimationValue.h"
#include "Layers.h" // For Layer
#include "nsComputedDOMStyle.h" // nsComputedDOMStyle::GetStyleContextForElement
#include "nsCSSPropertySet.h"
#include "nsCSSProps.h" // For nsCSSProps::PropHasFlags
#include "nsCSSPseudoElements.h" // For CSSPseudoElementType
#include "nsDOMMutationObserver.h" // For nsAutoAnimationMutationBatch

namespace mozilla {

// Helper functions for generating a ComputedTimingProperties dictionary
static void
GetComputedTimingDictionary(const ComputedTiming& aComputedTiming,
                            const Nullable<TimeDuration>& aLocalTime,
                            const TimingParams& aTiming,
                            dom::ComputedTimingProperties& aRetVal)
{
  // AnimationEffectTimingProperties
  aRetVal.mDelay = aTiming.mDelay.ToMilliseconds();
  aRetVal.mEndDelay = aTiming.mEndDelay.ToMilliseconds();
  aRetVal.mFill = aComputedTiming.mFill;
  aRetVal.mIterations = aComputedTiming.mIterations;
  aRetVal.mIterationStart = aComputedTiming.mIterationStart;
  aRetVal.mDuration.SetAsUnrestrictedDouble() =
    aComputedTiming.mDuration.ToMilliseconds();
  aRetVal.mDirection = aTiming.mDirection;

  // ComputedTimingProperties
  aRetVal.mActiveDuration = aComputedTiming.mActiveDuration.ToMilliseconds();
  aRetVal.mEndTime = aComputedTiming.mEndTime.ToMilliseconds();
  aRetVal.mLocalTime = AnimationUtils::TimeDurationToDouble(aLocalTime);
  aRetVal.mProgress = aComputedTiming.mProgress;

  if (!aRetVal.mProgress.IsNull()) {
    // Convert the returned currentIteration into Infinity if we set
    // (uint64_t) aComputedTiming.mCurrentIteration to UINT64_MAX
    double iteration = aComputedTiming.mCurrentIteration == UINT64_MAX
                     ? PositiveInfinity<double>()
                     : static_cast<double>(aComputedTiming.mCurrentIteration);
    aRetVal.mCurrentIteration.SetValue(iteration);
  }
}

namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(KeyframeEffectReadOnly,
                                   AnimationEffectReadOnly,
                                   mTarget,
                                   mAnimation,
                                   mTiming)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(KeyframeEffectReadOnly,
                                               AnimationEffectReadOnly)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(KeyframeEffectReadOnly)
NS_INTERFACE_MAP_END_INHERITING(AnimationEffectReadOnly)

NS_IMPL_ADDREF_INHERITED(KeyframeEffectReadOnly, AnimationEffectReadOnly)
NS_IMPL_RELEASE_INHERITED(KeyframeEffectReadOnly, AnimationEffectReadOnly)

KeyframeEffectReadOnly::KeyframeEffectReadOnly(
  nsIDocument* aDocument,
  const Maybe<OwningAnimationTarget>& aTarget,
  const TimingParams& aTiming)
  : KeyframeEffectReadOnly(aDocument, aTarget,
                           new AnimationEffectTimingReadOnly(aDocument,
                                                             aTiming))
{
}

KeyframeEffectReadOnly::KeyframeEffectReadOnly(
  nsIDocument* aDocument,
  const Maybe<OwningAnimationTarget>& aTarget,
  AnimationEffectTimingReadOnly* aTiming)
  : AnimationEffectReadOnly(aDocument)
  , mTarget(aTarget)
  , mTiming(aTiming)
  , mInEffectOnLastAnimationTimingUpdate(false)
{
  MOZ_ASSERT(aTiming);
}

JSObject*
KeyframeEffectReadOnly::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto)
{
  return KeyframeEffectReadOnlyBinding::Wrap(aCx, this, aGivenProto);
}

IterationCompositeOperation
KeyframeEffectReadOnly::IterationComposite() const
{
  return IterationCompositeOperation::Replace;
}

CompositeOperation
KeyframeEffectReadOnly::Composite() const
{
  return CompositeOperation::Replace;
}

already_AddRefed<AnimationEffectTimingReadOnly>
KeyframeEffectReadOnly::Timing() const
{
  RefPtr<AnimationEffectTimingReadOnly> temp(mTiming);
  return temp.forget();
}

void
KeyframeEffectReadOnly::SetSpecifiedTiming(const TimingParams& aTiming)
{
  if (mTiming->AsTimingParams() == aTiming) {
    return;
  }
  mTiming->SetTimingParams(aTiming);
  if (mAnimation) {
    mAnimation->NotifyEffectTimingUpdated();
  }
  // NotifyEffectTimingUpdated will eventually cause
  // NotifyAnimationTimingUpdated to be called on this object which will
  // update our registration with the target element.
}

void
KeyframeEffectReadOnly::NotifyAnimationTimingUpdated()
{
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

  // Detect changes to "in effect" status since we need to recalculate the
  // animation cascade for this element whenever that changes.
  bool inEffect = IsInEffect();
  if (inEffect != mInEffectOnLastAnimationTimingUpdate) {
    if (mTarget) {
      EffectSet* effectSet = EffectSet::GetEffectSet(mTarget->mElement,
                                                     mTarget->mPseudoType);
      if (effectSet) {
        effectSet->MarkCascadeNeedsUpdate();
      }
    }
    mInEffectOnLastAnimationTimingUpdate = inEffect;
  }

  // Request restyle if necessary.
  //
  // Bug 1216843: When we implement iteration composite modes, we need to
  // also detect if the current iteration has changed.
  if (mAnimation &&
      !mProperties.IsEmpty() &&
      GetComputedTiming().mProgress != mProgressOnLastCompose) {
    EffectCompositor::RestyleType restyleType =
      CanThrottle() ?
      EffectCompositor::RestyleType::Throttled :
      EffectCompositor::RestyleType::Standard;
    RequestRestyle(restyleType);
  }

  // If we're no longer "in effect", our ComposeStyle method will never be
  // called and we will never have a chance to update mProgressOnLastCompose.
  // We clear mProgressOnLastCompose here to ensure that if we later become
  // "in effect" we will request a restyle (above).
  if (!inEffect) {
     mProgressOnLastCompose.SetNull();
  }
}

Nullable<TimeDuration>
KeyframeEffectReadOnly::GetLocalTime() const
{
  // Since the *animation* start time is currently always zero, the local
  // time is equal to the parent time.
  Nullable<TimeDuration> result;
  if (mAnimation) {
    result = mAnimation->GetCurrentTime();
  }
  return result;
}

void
KeyframeEffectReadOnly::GetComputedTimingAsDict(
    ComputedTimingProperties& aRetVal) const
{
  const Nullable<TimeDuration> currentTime = GetLocalTime();
  GetComputedTimingDictionary(GetComputedTimingAt(currentTime,
                                                  SpecifiedTiming()),
                              currentTime,
                              SpecifiedTiming(),
                              aRetVal);
}

ComputedTiming
KeyframeEffectReadOnly::GetComputedTimingAt(
    const Nullable<TimeDuration>& aLocalTime,
    const TimingParams& aTiming)
{
  const StickyTimeDuration zeroDuration;

  // Always return the same object to benefit from return-value optimization.
  ComputedTiming result;

  if (aTiming.mDuration) {
    MOZ_ASSERT(aTiming.mDuration.ref() >= zeroDuration,
               "Iteration duration should be positive");
    result.mDuration = aTiming.mDuration.ref();
  }

  MOZ_ASSERT(aTiming.mIterations >= 0.0 && !IsNaN(aTiming.mIterations),
             "mIterations should be nonnegative & finite, as ensured by "
             "ValidateIterations or CSSParser");
  result.mIterations = aTiming.mIterations;

  MOZ_ASSERT(aTiming.mIterationStart >= 0.0,
             "mIterationStart should be nonnegative, as ensured by "
             "ValidateIterationStart");
  result.mIterationStart = aTiming.mIterationStart;

  result.mActiveDuration = aTiming.ActiveDuration();
  result.mEndTime = aTiming.EndTime();
  result.mFill = aTiming.mFill == dom::FillMode::Auto ?
                 dom::FillMode::None :
                 aTiming.mFill;

  // The default constructor for ComputedTiming sets all other members to
  // values consistent with an animation that has not been sampled.
  if (aLocalTime.IsNull()) {
    return result;
  }
  const TimeDuration& localTime = aLocalTime.Value();

  // Calculate the time within the active interval.
  // https://w3c.github.io/web-animations/#active-time
  StickyTimeDuration activeTime;
  if (localTime >=
        std::min(StickyTimeDuration(aTiming.mDelay + result.mActiveDuration),
                 result.mEndTime)) {
    result.mPhase = ComputedTiming::AnimationPhase::After;
    if (!result.FillsForwards()) {
      // The animation isn't active or filling at this time.
      return result;
    }
    activeTime = result.mActiveDuration;
  } else if (localTime <
               std::min(StickyTimeDuration(aTiming.mDelay), result.mEndTime)) {
    result.mPhase = ComputedTiming::AnimationPhase::Before;
    if (!result.FillsBackwards()) {
      // The animation isn't active or filling at this time.
      return result;
    }
    // activeTime is zero
  } else {
    MOZ_ASSERT(result.mActiveDuration != zeroDuration,
               "How can we be in the middle of a zero-duration interval?");
    result.mPhase = ComputedTiming::AnimationPhase::Active;
    activeTime = localTime - aTiming.mDelay;
  }

  // Convert active time to a multiple of iterations.
  // https://w3c.github.io/web-animations/#overall-progress
  double overallProgress;
  if (result.mDuration == zeroDuration) {
    overallProgress = result.mPhase == ComputedTiming::AnimationPhase::Before
                      ? 0.0
                      : result.mIterations;
  } else {
    overallProgress = activeTime / result.mDuration;
  }

  // Factor in iteration start offset.
  if (IsFinite(overallProgress)) {
    overallProgress += result.mIterationStart;
  }

  // Determine the 0-based index of the current iteration.
  // https://w3c.github.io/web-animations/#current-iteration
  result.mCurrentIteration =
    IsInfinite(result.mIterations) &&
      result.mPhase == ComputedTiming::AnimationPhase::After
    ? UINT64_MAX // In GetComputedTimingDictionary(),
                 // we will convert this into Infinity
    : static_cast<uint64_t>(overallProgress);

  // Convert the overall progress to a fraction of a single iteration--the
  // simply iteration progress.
  // https://w3c.github.io/web-animations/#simple-iteration-progress
  double progress = IsFinite(overallProgress)
                    ? fmod(overallProgress, 1.0)
                    : fmod(result.mIterationStart, 1.0);

  // When we finish exactly at the end of an iteration we need to report
  // the end of the final iteration and not the start of the next iteration.
  if (result.mPhase == ComputedTiming::AnimationPhase::After &&
      progress == 0.0 &&
      result.mIterations != 0.0) {
    progress = 1.0;
    if (result.mCurrentIteration != UINT64_MAX) {
      result.mCurrentIteration--;
    }
  }

  // Factor in the direction.
  bool thisIterationReverse = false;
  switch (aTiming.mDirection) {
    case PlaybackDirection::Normal:
      thisIterationReverse = false;
      break;
    case PlaybackDirection::Reverse:
      thisIterationReverse = true;
      break;
    case PlaybackDirection::Alternate:
      thisIterationReverse = (result.mCurrentIteration & 1) == 1;
      break;
    case PlaybackDirection::Alternate_reverse:
      thisIterationReverse = (result.mCurrentIteration & 1) == 0;
      break;
    default:
      MOZ_ASSERT(true, "Unknown PlaybackDirection type");
  }
  if (thisIterationReverse) {
    progress = 1.0 - progress;
  }

  // Calculate the 'before flag' which we use when applying step timing
  // functions.
  if ((result.mPhase == ComputedTiming::AnimationPhase::After &&
       thisIterationReverse) ||
      (result.mPhase == ComputedTiming::AnimationPhase::Before &&
       !thisIterationReverse)) {
    result.mBeforeFlag = ComputedTimingFunction::BeforeFlag::Set;
  }

  // Apply the easing.
  if (aTiming.mFunction) {
    progress = aTiming.mFunction->GetValue(progress, result.mBeforeFlag);
  }

  MOZ_ASSERT(IsFinite(progress), "Progress value should be finite");
  result.mProgress.SetValue(progress);
  return result;
}

// https://w3c.github.io/web-animations/#in-play
bool
KeyframeEffectReadOnly::IsInPlay() const
{
  if (!mAnimation || mAnimation->PlayState() == AnimationPlayState::Finished) {
    return false;
  }

  return GetComputedTiming().mPhase == ComputedTiming::AnimationPhase::Active;
}

// https://w3c.github.io/web-animations/#current
bool
KeyframeEffectReadOnly::IsCurrent() const
{
  if (!mAnimation || mAnimation->PlayState() == AnimationPlayState::Finished) {
    return false;
  }

  ComputedTiming computedTiming = GetComputedTiming();
  return computedTiming.mPhase == ComputedTiming::AnimationPhase::Before ||
         computedTiming.mPhase == ComputedTiming::AnimationPhase::Active;
}

// https://w3c.github.io/web-animations/#in-effect
bool
KeyframeEffectReadOnly::IsInEffect() const
{
  ComputedTiming computedTiming = GetComputedTiming();
  return !computedTiming.mProgress.IsNull();
}

void
KeyframeEffectReadOnly::SetAnimation(Animation* aAnimation)
{
  mAnimation = aAnimation;
  NotifyAnimationTimingUpdated();
}

static bool
KeyframesEqualIgnoringComputedOffsets(const nsTArray<Keyframe>& aLhs,
                                      const nsTArray<Keyframe>& aRhs)
{
  if (aLhs.Length() != aRhs.Length()) {
    return false;
  }

  for (size_t i = 0, len = aLhs.Length(); i < len; ++i) {
    const Keyframe& a = aLhs[i];
    const Keyframe& b = aRhs[i];
    if (a.mOffset != b.mOffset ||
        a.mTimingFunction != b.mTimingFunction ||
        a.mPropertyValues != b.mPropertyValues) {
      return false;
    }
  }
  return true;
}

// https://w3c.github.io/web-animations/#dom-keyframeeffect-setframes
void
KeyframeEffectReadOnly::SetFrames(JSContext* aContext,
                                  JS::Handle<JSObject*> aFrames,
                                  ErrorResult& aRv)
{
  nsIDocument* doc = AnimationUtils::GetCurrentRealmDocument(aContext);
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsTArray<Keyframe> keyframes =
    KeyframeUtils::GetKeyframesFromObject(aContext, aFrames, aRv);
  if (aRv.Failed()) {
    return;
  }

  RefPtr<nsStyleContext> styleContext;
  nsIPresShell* shell = doc->GetShell();
  if (shell && mTarget) {
    nsIAtom* pseudo =
      mTarget->mPseudoType < CSSPseudoElementType::Count ?
      nsCSSPseudoElements::GetPseudoAtom(mTarget->mPseudoType) : nullptr;
    styleContext =
      nsComputedDOMStyle::GetStyleContextForElement(mTarget->mElement,
                                                    pseudo, shell);
  }

  SetFrames(Move(keyframes), styleContext);
}

void
KeyframeEffectReadOnly::SetFrames(nsTArray<Keyframe>&& aFrames,
                                  nsStyleContext* aStyleContext)
{
  if (KeyframesEqualIgnoringComputedOffsets(aFrames, mFrames)) {
    return;
  }

  mFrames = Move(aFrames);
  KeyframeUtils::ApplyDistributeSpacing(mFrames);

  if (mAnimation && mAnimation->IsRelevant()) {
    nsNodeUtils::AnimationChanged(mAnimation);
  }

  if (aStyleContext) {
    UpdateProperties(aStyleContext);
  }
}

const AnimationProperty*
KeyframeEffectReadOnly::GetAnimationOfProperty(nsCSSProperty aProperty) const
{
  for (size_t propIdx = 0, propEnd = mProperties.Length();
       propIdx != propEnd; ++propIdx) {
    if (aProperty == mProperties[propIdx].mProperty) {
      const AnimationProperty* result = &mProperties[propIdx];
      if (!result->mWinsInCascade) {
        result = nullptr;
      }
      return result;
    }
  }
  return nullptr;
}

bool
KeyframeEffectReadOnly::HasAnimationOfProperties(
                          const nsCSSProperty* aProperties,
                          size_t aPropertyCount) const
{
  for (size_t i = 0; i < aPropertyCount; i++) {
    if (HasAnimationOfProperty(aProperties[i])) {
      return true;
    }
  }
  return false;
}

void
KeyframeEffectReadOnly::UpdateProperties(nsStyleContext* aStyleContext)
{
  MOZ_ASSERT(aStyleContext);

  nsTArray<AnimationProperty> properties;
  if (mTarget) {
    properties =
      KeyframeUtils::GetAnimationPropertiesFromKeyframes(aStyleContext,
                                                         mTarget->mElement,
                                                         mTarget->mPseudoType,
                                                         mFrames);
  }

  if (mProperties == properties) {
    return;
  }

  // Preserve the state of mWinsInCascade and mIsRunningOnCompositor flags.
  nsCSSPropertySet winningInCascadeProperties;
  nsCSSPropertySet runningOnCompositorProperties;

  for (const AnimationProperty& property : mProperties) {
    if (property.mWinsInCascade) {
      winningInCascadeProperties.AddProperty(property.mProperty);
    }
    if (property.mIsRunningOnCompositor) {
      runningOnCompositorProperties.AddProperty(property.mProperty);
    }
  }

  mProperties = Move(properties);

  for (AnimationProperty& property : mProperties) {
    property.mWinsInCascade =
      winningInCascadeProperties.HasProperty(property.mProperty);
    property.mIsRunningOnCompositor =
      runningOnCompositorProperties.HasProperty(property.mProperty);
  }

  if (mTarget) {
    EffectSet* effectSet = EffectSet::GetEffectSet(mTarget->mElement,
                                                   mTarget->mPseudoType);
    if (effectSet) {
      effectSet->MarkCascadeNeedsUpdate();
    }

    RequestRestyle(EffectCompositor::RestyleType::Layer);
  }
}

void
KeyframeEffectReadOnly::ComposeStyle(RefPtr<AnimValuesStyleRule>& aStyleRule,
                                     nsCSSPropertySet& aSetProperties)
{
  ComputedTiming computedTiming = GetComputedTiming();
  mProgressOnLastCompose = computedTiming.mProgress;

  // If the progress is null, we don't have fill data for the current
  // time so we shouldn't animate.
  if (computedTiming.mProgress.IsNull()) {
    return;
  }

  for (size_t propIdx = 0, propEnd = mProperties.Length();
       propIdx != propEnd; ++propIdx)
  {
    const AnimationProperty& prop = mProperties[propIdx];

    MOZ_ASSERT(prop.mSegments[0].mFromKey == 0.0, "incorrect first from key");
    MOZ_ASSERT(prop.mSegments[prop.mSegments.Length() - 1].mToKey == 1.0,
               "incorrect last to key");

    if (aSetProperties.HasProperty(prop.mProperty)) {
      // Animations are composed by EffectCompositor by iterating
      // from the last animation to first. For animations targetting the
      // same property, the later one wins. So if this property is already set,
      // we should not override it.
      continue;
    }

    if (!prop.mWinsInCascade) {
      // This isn't the winning declaration, so don't add it to style.
      // For transitions, this is important, because it's how we
      // implement the rule that CSS transitions don't run when a CSS
      // animation is running on the same property and element.  For
      // animations, this is only skipping things that will otherwise be
      // overridden.
      continue;
    }

    aSetProperties.AddProperty(prop.mProperty);

    MOZ_ASSERT(prop.mSegments.Length() > 0,
               "property should not be in animations if it has no segments");

    // FIXME: Maybe cache the current segment?
    const AnimationPropertySegment *segment = prop.mSegments.Elements(),
                                *segmentEnd = segment + prop.mSegments.Length();
    while (segment->mToKey <= computedTiming.mProgress.Value()) {
      MOZ_ASSERT(segment->mFromKey <= segment->mToKey, "incorrect keys");
      if ((segment+1) == segmentEnd) {
        break;
      }
      ++segment;
      MOZ_ASSERT(segment->mFromKey == (segment-1)->mToKey, "incorrect keys");
    }
    MOZ_ASSERT(segment->mFromKey <= segment->mToKey, "incorrect keys");
    MOZ_ASSERT(segment >= prop.mSegments.Elements() &&
               size_t(segment - prop.mSegments.Elements()) <
                 prop.mSegments.Length(),
               "out of array bounds");

    if (!aStyleRule) {
      // Allocate the style rule now that we know we have animation data.
      aStyleRule = new AnimValuesStyleRule();
    }

    // Special handling for zero-length segments
    if (segment->mToKey == segment->mFromKey) {
      if (computedTiming.mProgress.Value() < 0) {
        aStyleRule->AddValue(prop.mProperty, segment->mFromValue);
      } else {
        aStyleRule->AddValue(prop.mProperty, segment->mToValue);
      }
      continue;
    }

    double positionInSegment =
      (computedTiming.mProgress.Value() - segment->mFromKey) /
      (segment->mToKey - segment->mFromKey);
    double valuePosition =
      ComputedTimingFunction::GetPortion(segment->mTimingFunction,
                                         positionInSegment,
                                         computedTiming.mBeforeFlag);

    StyleAnimationValue val;
    if (StyleAnimationValue::Interpolate(prop.mProperty,
                                         segment->mFromValue,
                                         segment->mToValue,
                                         valuePosition, val)) {
      aStyleRule->AddValue(prop.mProperty, Move(val));
    } else if (valuePosition < 0.5) {
      aStyleRule->AddValue(prop.mProperty, segment->mFromValue);
    } else {
      aStyleRule->AddValue(prop.mProperty, segment->mToValue);
    }
  }
}

bool
KeyframeEffectReadOnly::IsRunningOnCompositor() const
{
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

void
KeyframeEffectReadOnly::SetIsRunningOnCompositor(nsCSSProperty aProperty,
                                                 bool aIsRunning)
{
  MOZ_ASSERT(nsCSSProps::PropHasFlags(aProperty,
                                      CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR),
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

KeyframeEffectReadOnly::~KeyframeEffectReadOnly()
{
}

static Maybe<OwningAnimationTarget>
ConvertTarget(const Nullable<ElementOrCSSPseudoElement>& aTarget)
{
  // Return value optimization.
  Maybe<OwningAnimationTarget> result;

  if (aTarget.IsNull()) {
    return result;
  }

  const ElementOrCSSPseudoElement& target = aTarget.Value();
  MOZ_ASSERT(target.IsElement() || target.IsCSSPseudoElement(),
             "Uninitialized target");

  if (target.IsElement()) {
    result.emplace(&target.GetAsElement());
  } else {
    RefPtr<Element> elem = target.GetAsCSSPseudoElement().ParentElement();
    result.emplace(elem, target.GetAsCSSPseudoElement().GetType());
  }
  return result;
}

template <class KeyframeEffectType, class OptionsType>
/* static */ already_AddRefed<KeyframeEffectType>
KeyframeEffectReadOnly::ConstructKeyframeEffect(
    const GlobalObject& aGlobal,
    const Nullable<ElementOrCSSPseudoElement>& aTarget,
    JS::Handle<JSObject*> aFrames,
    const OptionsType& aOptions,
    ErrorResult& aRv)
{
  nsIDocument* doc = AnimationUtils::GetCurrentRealmDocument(aGlobal.Context());
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  TimingParams timingParams =
    TimingParams::FromOptionsUnion(aOptions, doc, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  Maybe<OwningAnimationTarget> target = ConvertTarget(aTarget);
  RefPtr<KeyframeEffectType> effect =
    new KeyframeEffectType(doc, target, timingParams);

  effect->SetFrames(aGlobal.Context(), aFrames, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return effect.forget();
}

void
KeyframeEffectReadOnly::ResetIsRunningOnCompositor()
{
  for (AnimationProperty& property : mProperties) {
    property.mIsRunningOnCompositor = false;
  }
}

void
KeyframeEffectReadOnly::ResetWinsInCascade()
{
  for (AnimationProperty& property : mProperties) {
    property.mWinsInCascade = false;
  }
}

void
KeyframeEffectReadOnly::UpdateTargetRegistration()
{
  if (!mTarget) {
    return;
  }

  bool isRelevant = mAnimation && mAnimation->IsRelevant();

  // Animation::IsRelevant() returns a cached value. It only updates when
  // something calls Animation::UpdateRelevance. Whenever our timing changes,
  // we should be notifying our Animation before calling this, so
  // Animation::IsRelevant() should be up-to-date by the time we get here.
  MOZ_ASSERT(isRelevant == IsCurrent() || IsInEffect(),
             "Out of date Animation::IsRelevant value");

  if (isRelevant) {
    EffectSet* effectSet =
      EffectSet::GetOrCreateEffectSet(mTarget->mElement, mTarget->mPseudoType);
    effectSet->AddEffect(*this);
  } else {
    UnregisterTarget();
  }
}

void
KeyframeEffectReadOnly::UnregisterTarget()
{
  EffectSet* effectSet =
    EffectSet::GetEffectSet(mTarget->mElement, mTarget->mPseudoType);
  if (effectSet) {
    effectSet->RemoveEffect(*this);
    if (effectSet->IsEmpty()) {
      EffectSet::DestroyEffectSet(mTarget->mElement, mTarget->mPseudoType);
    }
  }
}

void
KeyframeEffectReadOnly::RequestRestyle(
  EffectCompositor::RestyleType aRestyleType)
{
  nsPresContext* presContext = GetPresContext();
  if (presContext && mTarget && mAnimation) {
    presContext->EffectCompositor()->
      RequestRestyle(mTarget->mElement, mTarget->mPseudoType,
                     aRestyleType, mAnimation->CascadeLevel());
  }
}

#ifdef DEBUG
void
DumpAnimationProperties(nsTArray<AnimationProperty>& aAnimationProperties)
{
  for (auto& p : aAnimationProperties) {
    printf("%s\n", nsCSSProps::GetStringValue(p.mProperty).get());
    for (auto& s : p.mSegments) {
      nsString fromValue, toValue;
      StyleAnimationValue::UncomputeValue(p.mProperty,
                                          s.mFromValue,
                                          fromValue);
      StyleAnimationValue::UncomputeValue(p.mProperty,
                                          s.mToValue,
                                          toValue);
      printf("  %f..%f: %s..%s\n", s.mFromKey, s.mToKey,
             NS_ConvertUTF16toUTF8(fromValue).get(),
             NS_ConvertUTF16toUTF8(toValue).get());
    }
  }
}
#endif

/* static */ already_AddRefed<KeyframeEffectReadOnly>
KeyframeEffectReadOnly::Constructor(
    const GlobalObject& aGlobal,
    const Nullable<ElementOrCSSPseudoElement>& aTarget,
    JS::Handle<JSObject*> aFrames,
    const UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
    ErrorResult& aRv)
{
  return ConstructKeyframeEffect<KeyframeEffectReadOnly>(aGlobal, aTarget,
                                                         aFrames, aOptions,
                                                         aRv);
}

void
KeyframeEffectReadOnly::GetTarget(
    Nullable<OwningElementOrCSSPseudoElement>& aRv) const
{
  if (!mTarget) {
    aRv.SetNull();
    return;
  }

  switch (mTarget->mPseudoType) {
    case CSSPseudoElementType::before:
    case CSSPseudoElementType::after:
      aRv.SetValue().SetAsCSSPseudoElement() =
        CSSPseudoElement::GetCSSPseudoElement(mTarget->mElement,
                                              mTarget->mPseudoType);
      break;

    case CSSPseudoElementType::NotPseudo:
      aRv.SetValue().SetAsElement() = mTarget->mElement;
      break;

    default:
      NS_NOTREACHED("Animation of unsupported pseudo-type");
      aRv.SetNull();
  }
}

static void
CreatePropertyValue(nsCSSProperty aProperty,
                    float aOffset,
                    const Maybe<ComputedTimingFunction>& aTimingFunction,
                    const StyleAnimationValue& aValue,
                    AnimationPropertyValueDetails& aResult)
{
  aResult.mOffset = aOffset;

  nsString stringValue;
  StyleAnimationValue::UncomputeValue(aProperty, aValue, stringValue);
  aResult.mValue = stringValue;

  if (aTimingFunction) {
    aResult.mEasing.Construct();
    aTimingFunction->AppendToString(aResult.mEasing.Value());
  } else {
    aResult.mEasing.Construct(NS_LITERAL_STRING("linear"));
  }

  aResult.mComposite = CompositeOperation::Replace;
}

void
KeyframeEffectReadOnly::GetProperties(
    nsTArray<AnimationPropertyDetails>& aProperties,
    ErrorResult& aRv) const
{
  for (const AnimationProperty& property : mProperties) {
    AnimationPropertyDetails propertyDetails;
    propertyDetails.mProperty =
      NS_ConvertASCIItoUTF16(nsCSSProps::GetStringValue(property.mProperty));
    propertyDetails.mRunningOnCompositor = property.mIsRunningOnCompositor;

    nsXPIDLString localizedString;
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
         segmentIdx < segmentLen;
         segmentIdx++)
    {
      const AnimationPropertySegment& segment = property.mSegments[segmentIdx];

      binding_detail::FastAnimationPropertyValueDetails fromValue;
      CreatePropertyValue(property.mProperty, segment.mFromKey,
                          segment.mTimingFunction, segment.mFromValue,
                          fromValue);
      // We don't apply timing functions for zero-length segments, so
      // don't return one here.
      if (segment.mFromKey == segment.mToKey) {
        fromValue.mEasing.Reset();
      }
      // The following won't fail since we have already allocated the capacity
      // above.
      propertyDetails.mValues.AppendElement(fromValue, mozilla::fallible);

      // Normally we can ignore the to-value for this segment since it is
      // identical to the from-value from the next segment. However, we need
      // to add it if either:
      // a) this is the last segment, or
      // b) the next segment's from-value differs.
      if (segmentIdx == segmentLen - 1 ||
          property.mSegments[segmentIdx + 1].mFromValue != segment.mToValue) {
        binding_detail::FastAnimationPropertyValueDetails toValue;
        CreatePropertyValue(property.mProperty, segment.mToKey,
                            Nothing(), segment.mToValue, toValue);
        // It doesn't really make sense to have a timing function on the
        // last property value or before a sudden jump so we just drop the
        // easing property altogether.
        toValue.mEasing.Reset();
        propertyDetails.mValues.AppendElement(toValue, mozilla::fallible);
      }
    }

    aProperties.AppendElement(propertyDetails);
  }
}

void
KeyframeEffectReadOnly::GetFrames(JSContext*& aCx,
                                  nsTArray<JSObject*>& aResult,
                                  ErrorResult& aRv)
{
  MOZ_ASSERT(aResult.IsEmpty());
  MOZ_ASSERT(!aRv.Failed());

  if (!aResult.SetCapacity(mFrames.Length(), mozilla::fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  for (const Keyframe& keyframe : mFrames) {
    // Set up a dictionary object for the explicit members
    BaseComputedKeyframe keyframeDict;
    if (keyframe.mOffset) {
      keyframeDict.mOffset.SetValue(keyframe.mOffset.value());
    }
    keyframeDict.mComputedOffset.Construct(keyframe.mComputedOffset);
    if (keyframe.mTimingFunction) {
      keyframeDict.mEasing.Truncate();
      keyframe.mTimingFunction.ref().AppendToString(keyframeDict.mEasing);
    } // else if null, leave easing as its default "linear".

    JS::Rooted<JS::Value> keyframeJSValue(aCx);
    if (!ToJSValue(aCx, keyframeDict, &keyframeJSValue)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    JS::Rooted<JSObject*> keyframeObject(aCx, &keyframeJSValue.toObject());
    for (const PropertyValuePair& propertyValue : keyframe.mPropertyValues) {

      const char* name = nsCSSProps::PropertyIDLName(propertyValue.mProperty);

      // nsCSSValue::AppendToString does not accept shorthands properties but
      // works with token stream values if we pass eCSSProperty_UNKNOWN as
      // the property.
      nsCSSProperty propertyForSerializing =
        nsCSSProps::IsShorthand(propertyValue.mProperty)
        ? eCSSProperty_UNKNOWN
        : propertyValue.mProperty;

      nsAutoString stringValue;
      propertyValue.mValue.AppendToString(
        propertyForSerializing, stringValue, nsCSSValue::eNormalized);

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
KeyframeEffectReadOnly::OverflowRegionRefreshInterval()
{
  // The amount of time we can wait between updating throttled animations
  // on the main thread that influence the overflow region.
  static const TimeDuration kOverflowRegionRefreshInterval =
    TimeDuration::FromMilliseconds(200);

  return kOverflowRegionRefreshInterval;
}

bool
KeyframeEffectReadOnly::CanThrottle() const
{
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

  nsIFrame* frame = GetAnimationFrame();
  if (!frame) {
    // There are two possible cases here.
    // a) No target element
    // b) The target element has no frame, e.g. because it is in a display:none
    //    subtree.
    // In either case we can throttle the animation because there is no
    // need to update on the main thread.
    return true;
  }

  // First we need to check layer generation and transform overflow
  // prior to the property.mIsRunningOnCompositor check because we should
  // occasionally unthrottle these animations even if the animations are
  // already running on compositor.
  for (const LayerAnimationInfo::Record& record :
        LayerAnimationInfo::sRecords) {
    // Skip properties that are overridden in the cascade.
    // (GetAnimationOfProperty, as called by HasAnimationOfProperty,
    // only returns an animation if it currently wins in the cascade.)
    if (!HasAnimationOfProperty(record.mProperty)) {
      continue;
    }

    EffectSet* effectSet = EffectSet::GetEffectSet(mTarget->mElement,
                                                   mTarget->mPseudoType);
    MOZ_ASSERT(effectSet, "CanThrottle should be called on an effect "
                          "associated with a target element");
    layers::Layer* layer =
      FrameLayerBuilder::GetDedicatedLayer(frame, record.mLayerType);
    // Unthrottle if the layer needs to be brought up to date
    if (!layer ||
        effectSet->GetAnimationGeneration() !=
          layer->GetAnimationGeneration()) {
      return false;
    }

    // If this is a transform animation that affects the overflow region,
    // we should unthrottle the animation periodically.
    if (record.mProperty == eCSSProperty_transform &&
        !CanThrottleTransformChanges(*frame)) {
      return false;
    }
  }

  for (const AnimationProperty& property : mProperties) {
    if (!property.mIsRunningOnCompositor) {
      return false;
    }
  }

  return true;
}

bool
KeyframeEffectReadOnly::CanThrottleTransformChanges(nsIFrame& aFrame) const
{
  // If we know that the animation cannot cause overflow,
  // we can just disable flushes for this animation.

  // If we don't show scrollbars, we don't care about overflow.
  if (LookAndFeel::GetInt(LookAndFeel::eIntID_ShowHideScrollbars) == 0) {
    return true;
  }

  nsPresContext* presContext = GetPresContext();
  // CanThrottleTransformChanges is only called as part of a refresh driver tick
  // in which case we expect to has a pres context.
  MOZ_ASSERT(presContext);

  TimeStamp now =
    presContext->RefreshDriver()->MostRecentRefresh();

  EffectSet* effectSet = EffectSet::GetEffectSet(mTarget->mElement,
                                                 mTarget->mPseudoType);
  MOZ_ASSERT(effectSet, "CanThrottleTransformChanges is expected to be called"
                        " on an effect in an effect set");
  MOZ_ASSERT(mAnimation, "CanThrottleTransformChanges is expected to be called"
                         " on an effect with a parent animation");
  TimeStamp animationRuleRefreshTime =
    effectSet->AnimationRuleRefreshTime(mAnimation->CascadeLevel());
  // If this animation can cause overflow, we can throttle some of the ticks.
  if (!animationRuleRefreshTime.IsNull() &&
      (now - animationRuleRefreshTime) < OverflowRegionRefreshInterval()) {
    return true;
  }

  // If the nearest scrollable ancestor has overflow:hidden,
  // we don't care about overflow.
  nsIScrollableFrame* scrollable =
    nsLayoutUtils::GetNearestScrollableFrame(&aFrame);
  if (!scrollable) {
    return true;
  }

  ScrollbarStyles ss = scrollable->GetScrollbarStyles();
  if (ss.mVertical == NS_STYLE_OVERFLOW_HIDDEN &&
      ss.mHorizontal == NS_STYLE_OVERFLOW_HIDDEN &&
      scrollable->GetLogicalScrollPosition() == nsPoint(0, 0)) {
    return true;
  }

  return false;
}

nsIFrame*
KeyframeEffectReadOnly::GetAnimationFrame() const
{
  if (!mTarget) {
    return nullptr;
  }

  nsIFrame* frame = mTarget->mElement->GetPrimaryFrame();
  if (!frame) {
    return nullptr;
  }

  if (mTarget->mPseudoType == CSSPseudoElementType::before) {
    frame = nsLayoutUtils::GetBeforeFrame(frame);
  } else if (mTarget->mPseudoType == CSSPseudoElementType::after) {
    frame = nsLayoutUtils::GetAfterFrame(frame);
  } else {
    MOZ_ASSERT(mTarget->mPseudoType == CSSPseudoElementType::NotPseudo,
               "unknown mTarget->mPseudoType");
  }
  if (!frame) {
    return nullptr;
  }

  return nsLayoutUtils::GetStyleFrame(frame);
}

nsIDocument*
KeyframeEffectReadOnly::GetRenderedDocument() const
{
  if (!mTarget) {
    return nullptr;
  }
  return mTarget->mElement->GetComposedDoc();
}

nsPresContext*
KeyframeEffectReadOnly::GetPresContext() const
{
  nsIDocument* doc = GetRenderedDocument();
  if (!doc) {
    return nullptr;
  }
  nsIPresShell* shell = doc->GetShell();
  if (!shell) {
    return nullptr;
  }
  return shell->GetPresContext();
}

/* static */ bool
KeyframeEffectReadOnly::IsGeometricProperty(
  const nsCSSProperty aProperty)
{
  switch (aProperty) {
    case eCSSProperty_bottom:
    case eCSSProperty_height:
    case eCSSProperty_left:
    case eCSSProperty_right:
    case eCSSProperty_top:
    case eCSSProperty_width:
      return true;
    default:
      return false;
  }
}

/* static */ bool
KeyframeEffectReadOnly::CanAnimateTransformOnCompositor(
  const nsIFrame* aFrame,
  AnimationPerformanceWarning::Type& aPerformanceWarning)
{
  // Disallow OMTA for preserve-3d transform. Note that we check the style property
  // rather than Extend3DContext() since that can recurse back into this function
  // via HasOpacity(). See bug 779598.
  if (aFrame->Combines3DTransformWithAncestors() ||
      aFrame->StyleDisplay()->mTransformStyle == NS_STYLE_TRANSFORM_STYLE_PRESERVE_3D) {
    aPerformanceWarning = AnimationPerformanceWarning::Type::TransformPreserve3D;
    return false;
  }
  // Note that testing BackfaceIsHidden() is not a sufficient test for
  // what we need for animating backface-visibility correctly if we
  // remove the above test for Extend3DContext(); that would require
  // looking at backface-visibility on descendants as well. See bug 1186204.
  if (aFrame->StyleDisplay()->BackfaceIsHidden()) {
    aPerformanceWarning =
      AnimationPerformanceWarning::Type::TransformBackfaceVisibilityHidden;
    return false;
  }
  // Async 'transform' animations of aFrames with SVG transforms is not
  // supported.  See bug 779599.
  if (aFrame->IsSVGTransformed()) {
    aPerformanceWarning = AnimationPerformanceWarning::Type::TransformSVG;
    return false;
  }

  return true;
}

bool
KeyframeEffectReadOnly::ShouldBlockAsyncTransformAnimations(
  const nsIFrame* aFrame,
  AnimationPerformanceWarning::Type& aPerformanceWarning) const
{
  // We currently only expect this method to be called when this effect
  // is attached to a playing Animation. If that ever changes we'll need
  // to update this to only return true when that is the case since paused,
  // filling, cancelled Animations etc. shouldn't stop other Animations from
  // running on the compositor.
  MOZ_ASSERT(mAnimation && mAnimation->IsPlaying());

  for (const AnimationProperty& property : mProperties) {
    // If a property is overridden in the CSS cascade, it should not block other
    // animations from running on the compositor.
    if (!property.mWinsInCascade) {
      continue;
    }
    // Check for geometric properties
    if (IsGeometricProperty(property.mProperty)) {
      aPerformanceWarning =
        AnimationPerformanceWarning::Type::TransformWithGeometricProperties;
      return true;
    }

    // Check for unsupported transform animations
    if (property.mProperty == eCSSProperty_transform) {
      if (!CanAnimateTransformOnCompositor(aFrame,
                                           aPerformanceWarning)) {
        return true;
      }
    }
  }

  return false;
}

void
KeyframeEffectReadOnly::SetPerformanceWarning(
  nsCSSProperty aProperty,
  const AnimationPerformanceWarning& aWarning)
{
  for (AnimationProperty& property : mProperties) {
    if (property.mProperty == aProperty &&
        (!property.mPerformanceWarning ||
         *property.mPerformanceWarning != aWarning)) {
      property.mPerformanceWarning = Some(aWarning);

      nsXPIDLString localizedString;
      if (nsLayoutUtils::IsAnimationLoggingEnabled() &&
          property.mPerformanceWarning->ToLocalizedString(localizedString)) {
        nsAutoCString logMessage = NS_ConvertUTF16toUTF8(localizedString);
        AnimationUtils::LogAsyncAnimationFailure(logMessage, mTarget->mElement);
      }
      return;
    }
  }
}

//---------------------------------------------------------------------
//
// KeyframeEffect
//
//---------------------------------------------------------------------

KeyframeEffect::KeyframeEffect(nsIDocument* aDocument,
                               const Maybe<OwningAnimationTarget>& aTarget,
                               const TimingParams& aTiming)
  : KeyframeEffectReadOnly(aDocument, aTarget,
                           new AnimationEffectTiming(aDocument, aTiming, this))
{
}

JSObject*
KeyframeEffect::WrapObject(JSContext* aCx,
                           JS::Handle<JSObject*> aGivenProto)
{
  return KeyframeEffectBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<KeyframeEffect>
KeyframeEffect::Constructor(
    const GlobalObject& aGlobal,
    const Nullable<ElementOrCSSPseudoElement>& aTarget,
    JS::Handle<JSObject*> aFrames,
    const UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
    ErrorResult& aRv)
{
  return ConstructKeyframeEffect<KeyframeEffect>(aGlobal, aTarget, aFrames,
                                                 aOptions, aRv);
}

/* static */ already_AddRefed<KeyframeEffect>
KeyframeEffect::Constructor(
    const GlobalObject& aGlobal,
    const Nullable<ElementOrCSSPseudoElement>& aTarget,
    JS::Handle<JSObject*> aFrames,
    const UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
    ErrorResult& aRv)
{
  return ConstructKeyframeEffect<KeyframeEffect>(aGlobal, aTarget, aFrames,
                                                 aOptions, aRv);
}

void
KeyframeEffect::NotifySpecifiedTimingUpdated()
{
  // Use the same document for a pseudo element and its parent element.
  // Use nullptr if we don't have mTarget, so disable the mutation batch.
  nsAutoAnimationMutationBatch mb(mTarget ? mTarget->mElement->OwnerDoc()
                                          : nullptr);

  if (mAnimation) {
    mAnimation->NotifyEffectTimingUpdated();

    if (mAnimation->IsRelevant()) {
      nsNodeUtils::AnimationChanged(mAnimation);
    }

    RequestRestyle(EffectCompositor::RestyleType::Layer);
  }
}

void
KeyframeEffect::SetTarget(const Nullable<ElementOrCSSPseudoElement>& aTarget)
{
  Maybe<OwningAnimationTarget> newTarget = ConvertTarget(aTarget);
  if (mTarget == newTarget) {
    // Assign the same target, skip it.
    return;
  }

  if (mTarget) {
    UnregisterTarget();
    ResetIsRunningOnCompositor();
    ResetWinsInCascade();

    RequestRestyle(EffectCompositor::RestyleType::Layer);

    nsAutoAnimationMutationBatch mb(mTarget->mElement->OwnerDoc());
    if (mAnimation) {
      nsNodeUtils::AnimationRemoved(mAnimation);
    }
  }

  mTarget = newTarget;

  if (mTarget) {
    UpdateTargetRegistration();
    MaybeUpdateProperties();

    RequestRestyle(EffectCompositor::RestyleType::Layer);

    nsAutoAnimationMutationBatch mb(mTarget->mElement->OwnerDoc());
    if (mAnimation) {
      nsNodeUtils::AnimationAdded(mAnimation);
    }
  }
}

KeyframeEffect::~KeyframeEffect()
{
  // mTiming is cycle collected, so we have to do null check first even though
  // mTiming shouldn't be null during the lifetime of KeyframeEffect.
  if (mTiming) {
    mTiming->Unlink();
  }
}

void
KeyframeEffect::MaybeUpdateProperties()
{
  if (!mTarget) {
    return;
  }

  nsIDocument* doc = mTarget->mElement->OwnerDoc();
  if (!doc) {
    return;
  }

  nsIAtom* pseudo = mTarget->mPseudoType < CSSPseudoElementType::Count ?
                    nsCSSPseudoElements::GetPseudoAtom(mTarget->mPseudoType) :
                    nullptr;
  RefPtr<nsStyleContext> styleContext =
    nsComputedDOMStyle::GetStyleContextForElement(mTarget->mElement, pseudo,
                                                  doc->GetShell());
  if (!styleContext) {
    return;
  }

  UpdateProperties(styleContext);
}

} // namespace dom
} // namespace mozilla
