/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/KeyframeEffect.h"

#include "mozilla/dom/AnimationEffectReadOnlyBinding.h"
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "mozilla/dom/PropertyIndexedKeyframesBinding.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/LookAndFeel.h" // For LookAndFeel::GetInt
#include "mozilla/StyleAnimationValue.h"
#include "AnimationCommon.h"
#include "Layers.h" // For Layer
#include "nsCSSParser.h"
#include "nsCSSPropertySet.h"
#include "nsCSSProps.h" // For nsCSSProps::PropHasFlags
#include "nsCSSValue.h"
#include "nsStyleUtil.h"
#include <algorithm>    // std::max

namespace mozilla {

bool
AnimationTiming::FillsForwards() const
{
  return mFillMode == dom::FillMode::Both ||
         mFillMode == dom::FillMode::Forwards;
}

bool
AnimationTiming::FillsBackwards() const
{
  return mFillMode == dom::FillMode::Both ||
         mFillMode == dom::FillMode::Backwards;
}

// Helper functions for generating a ComputedTimingProperties dictionary
static void
GetComputedTimingDictionary(const ComputedTiming& aComputedTiming,
                            const Nullable<TimeDuration>& aLocalTime,
                            const AnimationTiming& aTiming,
                            dom::ComputedTimingProperties& aRetVal)
{
  // AnimationEffectTimingProperties
  aRetVal.mDelay = aTiming.mDelay.ToMilliseconds();
  aRetVal.mFill = aTiming.mFillMode;
  aRetVal.mIterations = aTiming.mIterationCount;
  aRetVal.mDuration.SetAsUnrestrictedDouble() = aTiming.mIterationDuration.ToMilliseconds();
  aRetVal.mDirection = aTiming.mDirection;

  // ComputedTimingProperties
  aRetVal.mActiveDuration = aComputedTiming.mActiveDuration.ToMilliseconds();
  aRetVal.mEndTime
    = std::max(aRetVal.mDelay + aRetVal.mActiveDuration + aRetVal.mEndDelay, 0.0);
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
                                   mAnimation)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(KeyframeEffectReadOnly,
                                               AnimationEffectReadOnly)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(KeyframeEffectReadOnly)
NS_INTERFACE_MAP_END_INHERITING(AnimationEffectReadOnly)

NS_IMPL_ADDREF_INHERITED(KeyframeEffectReadOnly, AnimationEffectReadOnly)
NS_IMPL_RELEASE_INHERITED(KeyframeEffectReadOnly, AnimationEffectReadOnly)

KeyframeEffectReadOnly::KeyframeEffectReadOnly(
  nsIDocument* aDocument,
  Element* aTarget,
  nsCSSPseudoElements::Type aPseudoType,
  const AnimationTiming& aTiming)
  : AnimationEffectReadOnly(aDocument)
  , mTarget(aTarget)
  , mTiming(aTiming)
  , mPseudoType(aPseudoType)
{
  MOZ_ASSERT(aTarget, "null animation target is not yet supported");
  ResetIsRunningOnCompositor();
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

void
KeyframeEffectReadOnly::SetTiming(const AnimationTiming& aTiming)
{
  if (mTiming == aTiming) {
    return;
  }
  mTiming = aTiming;
  if (mAnimation) {
    mAnimation->NotifyEffectTimingUpdated();
  }
  // NotifyEffectTimingUpdated will eventually cause
  // NotifyAnimationTimingUpdated to be called on this object which will
  // update our registration with the target element.
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
KeyframeEffectReadOnly::GetComputedTimingAsDict(ComputedTimingProperties& aRetVal) const
{
  const Nullable<TimeDuration> currentTime = GetLocalTime();
  GetComputedTimingDictionary(GetComputedTimingAt(currentTime, mTiming),
                              currentTime,
                              mTiming,
                              aRetVal);
}

ComputedTiming
KeyframeEffectReadOnly::GetComputedTimingAt(
                          const Nullable<TimeDuration>& aLocalTime,
                          const AnimationTiming& aTiming)
{
  const TimeDuration zeroDuration;

  // Currently we expect negative durations to be picked up during CSS
  // parsing but when we start receiving timing parameters from other sources
  // we will need to clamp negative durations here.
  // For now, if we're hitting this it probably means we're overflowing
  // integer arithmetic in mozilla::TimeStamp.
  MOZ_ASSERT(aTiming.mIterationDuration >= zeroDuration,
             "Expecting iteration duration >= 0");

  // Always return the same object to benefit from return-value optimization.
  ComputedTiming result;

  result.mActiveDuration = ActiveDuration(aTiming);

  // The default constructor for ComputedTiming sets all other members to
  // values consistent with an animation that has not been sampled.
  if (aLocalTime.IsNull()) {
    return result;
  }
  const TimeDuration& localTime = aLocalTime.Value();

  // When we finish exactly at the end of an iteration we need to report
  // the end of the final iteration and not the start of the next iteration
  // so we set up a flag for that case.
  bool isEndOfFinalIteration = false;

  // Get the normalized time within the active interval.
  StickyTimeDuration activeTime;
  if (localTime >= aTiming.mDelay + result.mActiveDuration) {
    result.mPhase = ComputedTiming::AnimationPhase::After;
    if (!aTiming.FillsForwards()) {
      // The animation isn't active or filling at this time.
      result.mProgress.SetNull();
      return result;
    }
    activeTime = result.mActiveDuration;
    // Note that infinity == floor(infinity) so this will also be true when we
    // have finished an infinitely repeating animation of zero duration.
    isEndOfFinalIteration =
      aTiming.mIterationCount != 0.0 &&
      aTiming.mIterationCount == floor(aTiming.mIterationCount);
  } else if (localTime < aTiming.mDelay) {
    result.mPhase = ComputedTiming::AnimationPhase::Before;
    if (!aTiming.FillsBackwards()) {
      // The animation isn't active or filling at this time.
      result.mProgress.SetNull();
      return result;
    }
    // activeTime is zero
  } else {
    MOZ_ASSERT(result.mActiveDuration != zeroDuration,
               "How can we be in the middle of a zero-duration interval?");
    result.mPhase = ComputedTiming::AnimationPhase::Active;
    activeTime = localTime - aTiming.mDelay;
  }

  // Get the position within the current iteration.
  StickyTimeDuration iterationTime;
  if (aTiming.mIterationDuration != zeroDuration) {
    iterationTime = isEndOfFinalIteration
                    ? StickyTimeDuration(aTiming.mIterationDuration)
                    : activeTime % aTiming.mIterationDuration;
  } /* else, iterationTime is zero */

  // Determine the 0-based index of the current iteration.
  if (isEndOfFinalIteration) {
    result.mCurrentIteration =
      aTiming.mIterationCount == NS_IEEEPositiveInfinity()
      ? UINT64_MAX // In GetComputedTimingDictionary(), we will convert this
                   // into Infinity.
      : static_cast<uint64_t>(aTiming.mIterationCount) - 1;
  } else if (activeTime == zeroDuration) {
    // If the active time is zero we're either in the first iteration
    // (including filling backwards) or we have finished an animation with an
    // iteration duration of zero that is filling forwards (but we're not at
    // the exact end of an iteration since we deal with that above).
    result.mCurrentIteration =
      result.mPhase == ComputedTiming::AnimationPhase::After
      ? static_cast<uint64_t>(aTiming.mIterationCount) // floor
      : 0;
  } else {
    result.mCurrentIteration =
      static_cast<uint64_t>(activeTime / aTiming.mIterationDuration); // floor
  }

  // Normalize the iteration time into a fraction of the iteration duration.
  if (result.mPhase == ComputedTiming::AnimationPhase::Before) {
    result.mProgress.SetValue(0.0);
  } else if (result.mPhase == ComputedTiming::AnimationPhase::After) {
    double progress = isEndOfFinalIteration
                      ? 1.0
                      : fmod(aTiming.mIterationCount, 1.0f);
    result.mProgress.SetValue(progress);
  } else {
    // We are in the active phase so the iteration duration can't be zero.
    MOZ_ASSERT(aTiming.mIterationDuration != zeroDuration,
               "In the active phase of a zero-duration animation?");
    double progress = aTiming.mIterationDuration == TimeDuration::Forever()
                      ? 0.0
                      : iterationTime / aTiming.mIterationDuration;
    result.mProgress.SetValue(progress);
  }

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
    result.mProgress.SetValue(1.0 - result.mProgress.Value());
  }

  return result;
}

StickyTimeDuration
KeyframeEffectReadOnly::ActiveDuration(const AnimationTiming& aTiming)
{
  if (aTiming.mIterationCount == mozilla::PositiveInfinity<float>()) {
    // An animation that repeats forever has an infinite active duration
    // unless its iteration duration is zero, in which case it has a zero
    // active duration.
    const StickyTimeDuration zeroDuration;
    return aTiming.mIterationDuration == zeroDuration
           ? zeroDuration
           : StickyTimeDuration::Forever();
  }
  return StickyTimeDuration(
    aTiming.mIterationDuration.MultDouble(aTiming.mIterationCount));
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
KeyframeEffectReadOnly::ComposeStyle(RefPtr<AnimValuesStyleRule>& aStyleRule,
                                     nsCSSPropertySet& aSetProperties)
{
  ComputedTiming computedTiming = GetComputedTiming();

  // If the progress is null, we don't have fill data for the current
  // time so we shouldn't animate.
  if (computedTiming.mProgress.IsNull()) {
    return;
  }

  MOZ_ASSERT(!computedTiming.mProgress.IsNull() &&
             0.0 <= computedTiming.mProgress.Value() &&
             computedTiming.mProgress.Value() <= 1.0,
             "iteration progress should be in [0-1]");

  for (size_t propIdx = 0, propEnd = mProperties.Length();
       propIdx != propEnd; ++propIdx)
  {
    const AnimationProperty& prop = mProperties[propIdx];

    MOZ_ASSERT(prop.mSegments[0].mFromKey == 0.0, "incorrect first from key");
    MOZ_ASSERT(prop.mSegments[prop.mSegments.Length() - 1].mToKey == 1.0,
               "incorrect last to key");

    if (aSetProperties.HasProperty(prop.mProperty)) {
      // Animations are composed by AnimationCollection by iterating
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
    while (segment->mToKey < computedTiming.mProgress.Value()) {
      MOZ_ASSERT(segment->mFromKey < segment->mToKey, "incorrect keys");
      ++segment;
      if (segment == segmentEnd) {
        MOZ_ASSERT_UNREACHABLE("incorrect iteration progress");
        break; // in order to continue in outer loop (just below)
      }
      MOZ_ASSERT(segment->mFromKey == (segment-1)->mToKey, "incorrect keys");
    }
    if (segment == segmentEnd) {
      continue;
    }
    MOZ_ASSERT(segment->mFromKey < segment->mToKey, "incorrect keys");
    MOZ_ASSERT(segment >= prop.mSegments.Elements() &&
               size_t(segment - prop.mSegments.Elements()) <
                 prop.mSegments.Length(),
               "out of array bounds");

    if (!aStyleRule) {
      // Allocate the style rule now that we know we have animation data.
      aStyleRule = new AnimValuesStyleRule();
    }

    double positionInSegment =
      (computedTiming.mProgress.Value() - segment->mFromKey) /
      (segment->mToKey - segment->mFromKey);
    double valuePosition =
      segment->mTimingFunction.GetValue(positionInSegment);

    StyleAnimationValue *val = aStyleRule->AddEmptyValue(prop.mProperty);

#ifdef DEBUG
    bool result =
#endif
      StyleAnimationValue::Interpolate(prop.mProperty,
                                       segment->mFromValue,
                                       segment->mToValue,
                                       valuePosition, *val);
    MOZ_ASSERT(result, "interpolate must succeed now");
  }
}

bool
KeyframeEffectReadOnly::IsPropertyRunningOnCompositor(
  nsCSSProperty aProperty) const
{
  const auto& info = LayerAnimationInfo::sRecords;
  for (size_t i = 0; i < ArrayLength(mIsPropertyRunningOnCompositor); i++) {
    if (info[i].mProperty == aProperty) {
      return mIsPropertyRunningOnCompositor[i];
    }
  }
  return false;
}

bool
KeyframeEffectReadOnly::IsRunningOnCompositor() const
{
  // We consider animation is running on compositor if there is at least
  // one property running on compositor.
  // Animation.IsRunningOnCompotitor will return more fine grained
  // information in bug 1196114.
  for (bool isPropertyRunningOnCompositor : mIsPropertyRunningOnCompositor) {
    if (isPropertyRunningOnCompositor) {
      return true;
    }
  }
  return false;
}

void
KeyframeEffectReadOnly::SetIsRunningOnCompositor(nsCSSProperty aProperty,
                                                 bool aIsRunning)
{
  static_assert(
    MOZ_ARRAY_LENGTH(LayerAnimationInfo::sRecords) ==
      MOZ_ARRAY_LENGTH(mIsPropertyRunningOnCompositor),
    "The length of mIsPropertyRunningOnCompositor should equal to"
    "the length of LayserAnimationInfo::sRecords");
  MOZ_ASSERT(nsCSSProps::PropHasFlags(aProperty,
                                      CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR),
             "Property being animated on compositor is a recognized "
             "compositor-animatable property");
  const auto& info = LayerAnimationInfo::sRecords;
  for (size_t i = 0; i < ArrayLength(mIsPropertyRunningOnCompositor); i++) {
    if (info[i].mProperty == aProperty) {
      mIsPropertyRunningOnCompositor[i] = aIsRunning;
      return;
    }
  }
}

KeyframeEffectReadOnly::~KeyframeEffectReadOnly()
{
}

void
KeyframeEffectReadOnly::ResetIsRunningOnCompositor()
{
  for (bool& isPropertyRunningOnCompositor : mIsPropertyRunningOnCompositor) {
    isPropertyRunningOnCompositor = false;
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
    EffectSet* effectSet = EffectSet::GetOrCreateEffectSet(mTarget,
                                                           mPseudoType);
    effectSet->AddEffect(*this);
  } else {
    EffectSet* effectSet = EffectSet::GetEffectSet(mTarget, mPseudoType);
    if (effectSet) {
      effectSet->RemoveEffect(*this);
    }
    // Any effects not in the effect set will not be included in the set of
    // candidate effects for running on the compositor and hence they won't
    // have their compositor status updated so we should do that now.
    for (bool& isRunningOnCompositor : mIsPropertyRunningOnCompositor) {
      isRunningOnCompositor = false;
    }
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

// Extract an iteration duration from an UnrestrictedDoubleOrXXX object.
template <typename T>
static TimeDuration
GetIterationDuration(const T& aDuration) {
  // Always return the same object to benefit from return-value optimization.
  TimeDuration result;
  if (aDuration.IsUnrestrictedDouble()) {
    double durationMs = aDuration.GetAsUnrestrictedDouble();
    if (!IsNaN(durationMs) && durationMs >= 0.0f) {
      result = TimeDuration::FromMilliseconds(durationMs);
    }
  }
  // else, aDuration should be zero
  return result;
}

/* static */ AnimationTiming
KeyframeEffectReadOnly::ConvertKeyframeEffectOptions(
    const UnrestrictedDoubleOrKeyframeEffectOptions& aOptions)
{
  AnimationTiming animationTiming;

  if (aOptions.IsKeyframeEffectOptions()) {
    const KeyframeEffectOptions& opt = aOptions.GetAsKeyframeEffectOptions();

    animationTiming.mIterationDuration = GetIterationDuration(opt.mDuration);
    animationTiming.mDelay = TimeDuration::FromMilliseconds(opt.mDelay);
    // FIXME: Covert mIterationCount to a valid value.
    // Bug 1214536 should revise this and keep the original value, so
    // AnimationTimingEffectReadOnly can get the original iterations.
    animationTiming.mIterationCount = (IsNaN(opt.mIterations) ||
                                      opt.mIterations < 0.0f) ?
                                        1.0f :
                                        opt.mIterations;
    animationTiming.mDirection = opt.mDirection;
    // FIXME: We should store original value.
    animationTiming.mFillMode = (opt.mFill == FillMode::Auto) ?
                                  FillMode::None :
                                  opt.mFill;
  } else {
    animationTiming.mIterationDuration = GetIterationDuration(aOptions);
    animationTiming.mDelay = TimeDuration(0);
    animationTiming.mIterationCount = 1.0f;
    animationTiming.mDirection = PlaybackDirection::Normal;
    animationTiming.mFillMode = FillMode::None;
  }
  return animationTiming;
}

/**
 * A property and StyleAnimationValue pair.
 */
struct KeyframeValue
{
  nsCSSProperty mProperty;
  StyleAnimationValue mValue;
};

/**
 * Represents a relative position for a value in a keyframe animation.
 */
enum class ValuePosition
{
  First,  // value at 0 used for reverse filling
  Left,   // value coming in to a given offset
  Right,  // value coming out from a given offset
  Last    // value at 1 used for forward filling
};

/**
 * A single value in a keyframe animation, used by GetFrames to produce a
 * minimal set of Keyframe objects.
 */
struct OrderedKeyframeValueEntry : KeyframeValue
{
  float mOffset;
  const ComputedTimingFunction* mTimingFunction;
  ValuePosition mPosition;

  bool SameKeyframe(const OrderedKeyframeValueEntry& aOther) const
  {
    return mOffset == aOther.mOffset &&
           !!mTimingFunction == !!aOther.mTimingFunction &&
           (!mTimingFunction || *mTimingFunction == *aOther.mTimingFunction) &&
           mPosition == aOther.mPosition;
  }

  struct ForKeyframeGenerationComparator
  {
    static bool Equals(const OrderedKeyframeValueEntry& aLhs,
                       const OrderedKeyframeValueEntry& aRhs)
    {
      return aLhs.SameKeyframe(aRhs) &&
             aLhs.mProperty == aRhs.mProperty;
    }
    static bool LessThan(const OrderedKeyframeValueEntry& aLhs,
                         const OrderedKeyframeValueEntry& aRhs)
    {
      // First, sort by offset.
      if (aLhs.mOffset != aRhs.mOffset) {
        return aLhs.mOffset < aRhs.mOffset;
      }

      // Second, by position.
      if (aLhs.mPosition != aRhs.mPosition) {
        return aLhs.mPosition < aRhs.mPosition;
      }

      // Third, by easing.
      if (aLhs.mTimingFunction) {
        if (aRhs.mTimingFunction) {
          int32_t order = aLhs.mTimingFunction->Compare(*aRhs.mTimingFunction);
          if (order != 0) {
            return order < 0;
          }
        } else {
          return true;
        }
      } else {
        if (aRhs.mTimingFunction) {
          return false;
        }
      }

      // Last, by property IDL name.
      return nsCSSProps::PropertyIDLNameSortPosition(aLhs.mProperty) <
             nsCSSProps::PropertyIDLNameSortPosition(aRhs.mProperty);
    }
  };
};

/**
 * Data for a segment in a keyframe animation of a given property
 * whose value is a StyleAnimationValue.
 *
 * KeyframeValueEntry is used in BuildAnimationPropertyListFromKeyframeSequence
 * to gather data for each individual segment described by an author-supplied
 * an IDL sequence<Keyframe> value so that they can be parsed into mProperties.
 */
struct KeyframeValueEntry : KeyframeValue
{
  float mOffset;
  ComputedTimingFunction mTimingFunction;

  struct PropertyOffsetComparator
  {
    static bool Equals(const KeyframeValueEntry& aLhs,
                       const KeyframeValueEntry& aRhs)
    {
      return aLhs.mProperty == aRhs.mProperty &&
             aLhs.mOffset == aRhs.mOffset;
    }
    static bool LessThan(const KeyframeValueEntry& aLhs,
                         const KeyframeValueEntry& aRhs)
    {
      // First, sort by property IDL name.
      int32_t order = nsCSSProps::PropertyIDLNameSortPosition(aLhs.mProperty) -
                      nsCSSProps::PropertyIDLNameSortPosition(aRhs.mProperty);
      if (order != 0) {
        return order < 0;
      }

      // Then, by offset.
      return aLhs.mOffset < aRhs.mOffset;
    }
  };
};

/**
 * A property-values pair obtained from the open-ended properties
 * discovered on a Keyframe or PropertyIndexedKeyframes object.
 *
 * Single values (as required by Keyframe, and as also supported
 * on PropertyIndexedKeyframes) are stored as the only element in
 * mValues.
 */
struct PropertyValuesPair
{
  nsCSSProperty mProperty;
  nsTArray<nsString> mValues;

  class PropertyPriorityComparator
  {
  public:
    PropertyPriorityComparator()
      : mSubpropertyCountInitialized(false) {}

    bool Equals(const PropertyValuesPair& aLhs,
                const PropertyValuesPair& aRhs) const
    {
      return aLhs.mProperty == aRhs.mProperty;
    }

    bool LessThan(const PropertyValuesPair& aLhs,
                  const PropertyValuesPair& aRhs) const
    {
      bool isShorthandLhs = nsCSSProps::IsShorthand(aLhs.mProperty);
      bool isShorthandRhs = nsCSSProps::IsShorthand(aRhs.mProperty);

      if (isShorthandLhs) {
        if (isShorthandRhs) {
          // First, sort shorthands by the number of longhands they have.
          uint32_t subpropCountLhs = SubpropertyCount(aLhs.mProperty);
          uint32_t subpropCountRhs = SubpropertyCount(aRhs.mProperty);
          if (subpropCountLhs != subpropCountRhs) {
            return subpropCountLhs < subpropCountRhs;
          }
          // Otherwise, sort by IDL name below.
        } else {
          // Put longhands before shorthands.
          return false;
        }
      } else {
        if (isShorthandRhs) {
          // Put longhands before shorthands.
          return true;
        }
      }
      // For two longhand properties, or two shorthand with the same number
      // of longhand components, sort by IDL name.
      return nsCSSProps::PropertyIDLNameSortPosition(aLhs.mProperty) <
             nsCSSProps::PropertyIDLNameSortPosition(aRhs.mProperty);
    }

    uint32_t SubpropertyCount(nsCSSProperty aProperty) const
    {
      if (!mSubpropertyCountInitialized) {
        PodZero(&mSubpropertyCount);
        mSubpropertyCountInitialized = true;
      }
      if (mSubpropertyCount[aProperty] == 0) {
        uint32_t count = 0;
        CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(
            p, aProperty, nsCSSProps::eEnabledForAllContent) {
          ++count;
        }
        mSubpropertyCount[aProperty] = count;
      }
      return mSubpropertyCount[aProperty];
    }

  private:
    // Cache of shorthand subproperty counts.
    mutable RangedArray<
      uint32_t,
      eCSSProperty_COUNT_no_shorthands,
      eCSSProperty_COUNT - eCSSProperty_COUNT_no_shorthands> mSubpropertyCount;
    mutable bool mSubpropertyCountInitialized;
  };
};

/**
 * The result of parsing a JS object as a Keyframe dictionary
 * and getting its property-value pairs from its open-ended
 * properties.
 */
struct OffsetIndexedKeyframe
{
  binding_detail::FastKeyframe mKeyframeDict;
  nsTArray<PropertyValuesPair> mPropertyValuePairs;
};

/**
 * Parses a CSS <single-transition-timing-function> value from
 * aEasing into a ComputedTimingFunction.  If parsing fails, aResult will
 * be set to 'linear'.
 */
static void
ParseEasing(Element* aTarget,
            const nsAString& aEasing,
            ComputedTimingFunction& aResult)
{
  nsIDocument* doc = aTarget->OwnerDoc();

  nsCSSValue value;
  nsCSSParser parser;
  parser.ParseLonghandProperty(eCSSProperty_animation_timing_function,
                               aEasing,
                               doc->GetDocumentURI(),
                               doc->GetDocumentURI(),
                               doc->NodePrincipal(),
                               value);

  switch (value.GetUnit()) {
    case eCSSUnit_List: {
      const nsCSSValueList* list = value.GetListValue();
      if (list->mNext) {
        // don't support a list of timing functions
        break;
      }
      switch (list->mValue.GetUnit()) {
        case eCSSUnit_Enumerated:
        case eCSSUnit_Cubic_Bezier:
        case eCSSUnit_Steps: {
          nsTimingFunction timingFunction;
          nsRuleNode::ComputeTimingFunction(list->mValue, timingFunction);
          aResult.Init(timingFunction);
          return;
        }
        default:
          MOZ_ASSERT_UNREACHABLE("unexpected animation-timing-function list "
                                 "item unit");
        break;
      }
      break;
    }
    case eCSSUnit_Null:
    case eCSSUnit_Inherit:
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
    case eCSSUnit_TokenStream:
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unexpected animation-timing-function unit");
      break;
  }

  aResult.Init(nsTimingFunction(NS_STYLE_TRANSITION_TIMING_FUNCTION_LINEAR));
}

/**
 * An additional property (for a property-values pair) found on a Keyframe
 * or PropertyIndexedKeyframes object.
 */
struct AdditionalProperty
{
  nsCSSProperty mProperty;
  size_t mJsidIndex;        // Index into |ids| in GetPropertyValuesPairs.

  struct PropertyComparator
  {
    bool Equals(const AdditionalProperty& aLhs,
                const AdditionalProperty& aRhs) const
    {
      return aLhs.mProperty == aRhs.mProperty;
    }
    bool LessThan(const AdditionalProperty& aLhs,
                  const AdditionalProperty& aRhs) const
    {
      return nsCSSProps::PropertyIDLNameSortPosition(aLhs.mProperty) <
             nsCSSProps::PropertyIDLNameSortPosition(aRhs.mProperty);
    }
  };
};

/**
 * Converts aValue to DOMString and appends it to aValues.
 */
static bool
AppendValueAsString(JSContext* aCx,
                    nsTArray<nsString>& aValues,
                    JS::Handle<JS::Value> aValue)
{
  return ConvertJSValueToString(aCx, aValue, eStringify, eStringify,
                                *aValues.AppendElement());
}

// For the aAllowList parameter of AppendStringOrStringSequence and
// GetPropertyValuesPairs.
enum class ListAllowance { eDisallow, eAllow };

/**
 * Converts aValue to DOMString, if aAllowLists is eDisallow, or
 * to (DOMString or sequence<DOMString>) if aAllowLists is aAllow.
 * The resulting strings are appended to aValues.
 */
static bool
AppendStringOrStringSequenceToArray(JSContext* aCx,
                                    JS::Handle<JS::Value> aValue,
                                    ListAllowance aAllowLists,
                                    nsTArray<nsString>& aValues)
{
  if (aAllowLists == ListAllowance::eAllow && aValue.isObject()) {
    // The value is an object, and we want to allow lists; convert
    // aValue to (DOMString or sequence<DOMString>).
    JS::ForOfIterator iter(aCx);
    if (!iter.init(aValue, JS::ForOfIterator::AllowNonIterable)) {
      return false;
    }
    if (iter.valueIsIterable()) {
      // If the object is iterable, convert it to sequence<DOMString>.
      JS::Rooted<JS::Value> element(aCx);
      for (;;) {
        bool done;
        if (!iter.next(&element, &done)) {
          return false;
        }
        if (done) {
          break;
        }
        if (!AppendValueAsString(aCx, aValues, element)) {
          return false;
        }
      }
      return true;
    }
  }

  // Either the object is not iterable, or aAllowLists doesn't want
  // a list; convert it to DOMString.
  if (!AppendValueAsString(aCx, aValues, aValue)) {
    return false;
  }

  return true;
}

/**
 * Reads the property-values pairs from the specified JS object.
 *
 * @param aObject The JS object to look at.
 * @param aAllowLists If eAllow, values will be converted to
 *   (DOMString or sequence<DOMString); if eDisallow, values
 *   will be converted to DOMString.
 * @param aResult The array into which the enumerated property-values
 *   pairs will be stored.
 * @return false on failure or JS exception thrown while interacting
 *   with aObject; true otherwise.
 */
static bool
GetPropertyValuesPairs(JSContext* aCx,
                       JS::Handle<JSObject*> aObject,
                       ListAllowance aAllowLists,
                       nsTArray<PropertyValuesPair>& aResult)
{
  nsTArray<AdditionalProperty> properties;

  // Iterate over all the properties on aObject and append an
  // entry to properties for them.
  //
  // We don't compare the jsids that we encounter with those for
  // the explicit dictionary members, since we know that none
  // of the CSS property IDL names clash with them.
  JS::Rooted<JS::IdVector> ids(aCx, JS::IdVector(aCx));
  if (!JS_Enumerate(aCx, aObject, &ids)) {
    return false;
  }
  for (size_t i = 0, n = ids.length(); i < n; i++) {
    nsAutoJSString propName;
    if (!propName.init(aCx, ids[i])) {
      return false;
    }
    nsCSSProperty property =
      nsCSSProps::LookupPropertyByIDLName(propName,
                                          nsCSSProps::eEnabledForAllContent);
    if (property != eCSSProperty_UNKNOWN &&
        nsCSSProps::kAnimTypeTable[property] != eStyleAnimType_None) {
      AdditionalProperty* p = properties.AppendElement();
      p->mProperty = property;
      p->mJsidIndex = i;
    }
  }

  // Sort the entries by IDL name and then get each value and
  // convert it either to a DOMString or to a
  // (DOMString or sequence<DOMString>), depending on aAllowLists,
  // and build up aResult.
  properties.Sort(AdditionalProperty::PropertyComparator());

  for (AdditionalProperty& p : properties) {
    JS::Rooted<JS::Value> value(aCx);
    if (!JS_GetPropertyById(aCx, aObject, ids[p.mJsidIndex], &value)) {
      return false;
    }
    PropertyValuesPair* pair = aResult.AppendElement();
    pair->mProperty = p.mProperty;
    if (!AppendStringOrStringSequenceToArray(aCx, value, aAllowLists,
                                             pair->mValues)) {
      return false;
    }
  }

  return true;
}

/**
 * Converts a JS object wrapped by the given JS::ForIfIterator to an
 * IDL sequence<Keyframe> and stores the resulting OffsetIndexedKeyframe
 * objects in aResult.
 */
static bool
ConvertKeyframeSequence(JSContext* aCx,
                        JS::ForOfIterator& aIterator,
                        nsTArray<OffsetIndexedKeyframe>& aResult)
{
  JS::Rooted<JS::Value> value(aCx);
  for (;;) {
    bool done;
    if (!aIterator.next(&value, &done)) {
      return false;
    }
    if (done) {
      break;
    }
    // Each value found when iterating the object must be an object
    // or null/undefined (which gets treated as a default {} dictionary
    // value).
    if (!value.isObject() && !value.isNullOrUndefined()) {
      ThrowErrorMessage(aCx, MSG_NOT_OBJECT,
                        "Element of sequence<Keyframes> argument");
      return false;
    }
    // Convert the JS value into a Keyframe dictionary value.
    OffsetIndexedKeyframe* keyframe = aResult.AppendElement();
    if (!keyframe->mKeyframeDict.Init(
          aCx, value, "Element of sequence<Keyframes> argument")) {
      return false;
    }
    // Look for additional property-values pairs on the object.
    if (value.isObject()) {
      JS::Rooted<JSObject*> object(aCx, &value.toObject());
      if (!GetPropertyValuesPairs(aCx, object,
                                  ListAllowance::eDisallow,
                                  keyframe->mPropertyValuePairs)) {
        return false;
      }
    }
  }
  return true;
}

/**
 * Checks that the given keyframes are loosely ordered (each keyframe's
 * offset that is not null is greater than or equal to the previous
 * non-null offset) and that all values are within the range [0.0, 1.0].
 *
 * @return true if the keyframes' offsets are correctly ordered and
 *   within range; false otherwise.
 */
static bool
HasValidOffsets(const nsTArray<OffsetIndexedKeyframe>& aKeyframes)
{
  double offset = 0.0;
  for (const OffsetIndexedKeyframe& keyframe : aKeyframes) {
    if (!keyframe.mKeyframeDict.mOffset.IsNull()) {
      double thisOffset = keyframe.mKeyframeDict.mOffset.Value();
      if (thisOffset < offset || thisOffset > 1.0f) {
        return false;
      }
      offset = thisOffset;
    }
  }
  return true;
}

/**
 * Fills in any null offsets for the given keyframes by applying the
 * "distribute" spacing algorithm.
 *
 * http://w3c.github.io/web-animations/#distribute-keyframe-spacing-mode
 */
static void
ApplyDistributeSpacing(nsTArray<OffsetIndexedKeyframe>& aKeyframes)
{
  // If the first or last keyframes have an unspecified offset,
  // fill them in with 0% and 100%.  If there is only a single keyframe,
  // then it gets 100%.
  if (aKeyframes.LastElement().mKeyframeDict.mOffset.IsNull()) {
    aKeyframes.LastElement().mKeyframeDict.mOffset.SetValue(1.0);
  }
  if (aKeyframes[0].mKeyframeDict.mOffset.IsNull()) {
    aKeyframes[0].mKeyframeDict.mOffset.SetValue(0.0);
  }

  // Fill in remaining missing offsets.
  size_t i = 0;
  while (i < aKeyframes.Length() - 1) {
    MOZ_ASSERT(!aKeyframes[i].mKeyframeDict.mOffset.IsNull());
    double start = aKeyframes[i].mKeyframeDict.mOffset.Value();
    size_t j = i + 1;
    while (aKeyframes[j].mKeyframeDict.mOffset.IsNull()) {
      ++j;
    }
    double end = aKeyframes[j].mKeyframeDict.mOffset.Value();
    size_t n = j - i;
    for (size_t k = 1; k < n; ++k) {
      double offset = start + double(k) / n * (end - start);
      aKeyframes[i + k].mKeyframeDict.mOffset.SetValue(offset);
    }
    i = j;
  }
}

/**
 * Splits out each property's keyframe animation segment information
 * from the OffsetIndexedKeyframe objects into an array of KeyframeValueEntry.
 *
 * The easing string value in OffsetIndexedKeyframe objects is parsed
 * into a ComputedTimingFunction value in the corresponding KeyframeValueEntry
 * objects.
 *
 * @param aTarget The target of the animation.
 * @param aKeyframes The keyframes to read.
 * @param aResult The array to append the resulting KeyframeValueEntry
 *   objects to.
 */
static void
GenerateValueEntries(Element* aTarget,
                     nsTArray<OffsetIndexedKeyframe>& aKeyframes,
                     nsTArray<KeyframeValueEntry>& aResult,
                     ErrorResult& aRv)
{
  nsCSSPropertySet properties;              // All properties encountered.
  nsCSSPropertySet propertiesWithFromValue; // Those with a defined 0% value.
  nsCSSPropertySet propertiesWithToValue;   // Those with a defined 100% value.

  for (OffsetIndexedKeyframe& keyframe : aKeyframes) {
    float offset = float(keyframe.mKeyframeDict.mOffset.Value());
    ComputedTimingFunction easing;
    ParseEasing(aTarget, keyframe.mKeyframeDict.mEasing, easing);
    // We ignore keyframe.mKeyframeDict.mComposite since we don't support
    // composite modes on keyframes yet.

    // keyframe.mPropertyValuePairs is currently sorted by CSS property IDL
    // name, since that was the order we read the properties from the JS
    // object.  Re-sort the list so that longhand properties appear before
    // shorthands, and with shorthands all appearing in increasing order of
    // number of components.  For two longhand properties, or two shorthands
    // with the same number of components, sort by IDL name.
    //
    // Example orderings that result from this:
    //
    //   margin-left, margin
    //
    // and:
    //
    //   border-top-color, border-color, border-top, border
    //
    // This allows us to prioritize values specified by longhands (or smaller
    // shorthand subsets) when longhands and shorthands are both specified
    // on the one keyframe.
    keyframe.mPropertyValuePairs.Sort(
        PropertyValuesPair::PropertyPriorityComparator());

    nsCSSPropertySet propertiesOnThisKeyframe;
    for (const PropertyValuesPair& pair : keyframe.mPropertyValuePairs) {
      MOZ_ASSERT(pair.mValues.Length() == 1,
                 "ConvertKeyframeSequence should have parsed single "
                 "DOMString values from the property-values pairs");
      // Parse the property's string value and produce a KeyframeValueEntry (or
      // more than one, for shorthands) for it.
      nsTArray<PropertyStyleAnimationValuePair> values;
      if (StyleAnimationValue::ComputeValues(pair.mProperty,
                                             nsCSSProps::eEnabledForAllContent,
                                             aTarget,
                                             pair.mValues[0],
                                             /* aUseSVGMode */ false,
                                             values)) {
        for (auto& value : values) {
          // If we already got a value for this property on the keyframe,
          // skip this one.
          if (propertiesOnThisKeyframe.HasProperty(value.mProperty)) {
            continue;
          }

          KeyframeValueEntry* entry = aResult.AppendElement();
          entry->mOffset = offset;
          entry->mProperty = value.mProperty;
          entry->mValue = value.mValue;
          entry->mTimingFunction = easing;

          if (offset == 0.0) {
            propertiesWithFromValue.AddProperty(value.mProperty);
          } else if (offset == 1.0) {
            propertiesWithToValue.AddProperty(value.mProperty);
          }
          propertiesOnThisKeyframe.AddProperty(value.mProperty);
          properties.AddProperty(value.mProperty);
        }
      }
    }
  }

  // We don't support additive segments and so can't support missing properties
  // using their underlying value in 0% and 100% keyframes.  Throw an exception
  // until we do support this.
  if (!propertiesWithFromValue.Equals(properties) ||
      !propertiesWithToValue.Equals(properties)) {
    aRv.Throw(NS_ERROR_DOM_ANIM_MISSING_PROPS_ERR);
    return;
  }
}

/**
 * Builds an array of AnimationProperty objects to represent the keyframe
 * animation segments in aEntries.
 */
static void
BuildSegmentsFromValueEntries(nsTArray<KeyframeValueEntry>& aEntries,
                              nsTArray<AnimationProperty>& aResult)
{
  if (aEntries.IsEmpty()) {
    return;
  }

  // Sort the KeyframeValueEntry objects so that all entries for a given
  // property are together, and the entries are sorted by offset otherwise.
  std::stable_sort(aEntries.begin(), aEntries.end(),
                   &KeyframeValueEntry::PropertyOffsetComparator::LessThan);

  MOZ_ASSERT(aEntries[0].mOffset == 0.0f);
  MOZ_ASSERT(aEntries.LastElement().mOffset == 1.0f);

  // For a given index i, we want to generate a segment from aEntries[i]
  // to aEntries[j], if:
  //
  //   * j > i,
  //   * aEntries[i + 1]'s offset/property is different from aEntries[i]'s, and
  //   * aEntries[j - 1]'s offset/property is different from aEntries[j]'s.
  //
  // That will eliminate runs of same offset/property values where there's no
  // point generating zero length segments in the middle of the animation.
  //
  // Additionally we need to generate a zero length segment at offset 0 and at
  // offset 1, if we have multiple values for a given property at that offset,
  // since we need to retain the very first and very last value so they can
  // be used for reverse and forward filling.

  nsCSSProperty lastProperty = eCSSProperty_UNKNOWN;
  AnimationProperty* animationProperty = nullptr;

  size_t i = 0, n = aEntries.Length();

  while (i + 1 < n) {
    // Starting from i, determine the next [i, j] interval from which to
    // generate a segment.
    size_t j;
    if (aEntries[i].mOffset == 0.0f && aEntries[i + 1].mOffset == 0.0f) {
      // We need to generate an initial zero-length segment.
      MOZ_ASSERT(aEntries[i].mProperty == aEntries[i + 1].mProperty);
      j = i + 1;
      while (aEntries[j + 1].mOffset == 0.0f) {
        MOZ_ASSERT(aEntries[j].mProperty == aEntries[j + 1].mProperty);
        ++j;
      }
    } else if (aEntries[i].mOffset == 1.0f) {
      if (aEntries[i + 1].mOffset == 1.0f) {
        // We need to generate a final zero-length segment.
        MOZ_ASSERT(aEntries[i].mProperty == aEntries[i].mProperty);
        j = i + 1;
        while (j + 1 < n && aEntries[j + 1].mOffset == 1.0f) {
          MOZ_ASSERT(aEntries[j].mProperty == aEntries[j + 1].mProperty);
          ++j;
        }
      } else {
        // New property.
        MOZ_ASSERT(aEntries[i + 1].mOffset == 0.0f);
        MOZ_ASSERT(aEntries[i].mProperty != aEntries[i + 1].mProperty);
        ++i;
        continue;
      }
    } else {
      while (aEntries[i].mOffset == aEntries[i + 1].mOffset &&
             aEntries[i].mProperty == aEntries[i + 1].mProperty) {
        ++i;
      }
      j = i + 1;
    }

    // If we've moved on to a new property, create a new AnimationProperty
    // to insert segments into.
    if (aEntries[i].mProperty != lastProperty) {
      MOZ_ASSERT(aEntries[i].mOffset == 0.0f);
      animationProperty = aResult.AppendElement();
      animationProperty->mProperty = aEntries[i].mProperty;
      animationProperty->mWinsInCascade = true;
      lastProperty = aEntries[i].mProperty;
    }

    MOZ_ASSERT(animationProperty, "animationProperty should be valid pointer.");

    // Now generate the segment.
    AnimationPropertySegment* segment =
      animationProperty->mSegments.AppendElement();
    segment->mFromKey   = aEntries[i].mOffset;
    segment->mToKey     = aEntries[j].mOffset;
    segment->mFromValue = aEntries[i].mValue;
    segment->mToValue   = aEntries[j].mValue;
    segment->mTimingFunction = aEntries[i].mTimingFunction;

    i = j;
  }
}

/**
 * Converts a JS object to an IDL sequence<Keyframe> and builds an
 * array of AnimationProperty objects for the keyframe animation
 * that it specifies.
 *
 * @param aTarget The target of the animation.
 * @param aIterator An already-initialized ForOfIterator for the JS
 *   object to iterate over as a sequence.
 * @param aResult The array into which the resulting AnimationProperty
 *   objects will be appended.
 */
static void
BuildAnimationPropertyListFromKeyframeSequence(
    JSContext* aCx,
    Element* aTarget,
    JS::ForOfIterator& aIterator,
    nsTArray<AnimationProperty>& aResult,
    ErrorResult& aRv)
{
  // Convert the object in aIterator to sequence<Keyframe>, producing
  // an array of OffsetIndexedKeyframe objects.
  nsAutoTArray<OffsetIndexedKeyframe,4> keyframes;
  if (!ConvertKeyframeSequence(aCx, aIterator, keyframes)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // If the sequence<> had zero elements, we won't generate any
  // keyframes.
  if (keyframes.IsEmpty()) {
    return;
  }

  // Check that the keyframes are loosely sorted and with values all
  // between 0% and 100%.
  if (!HasValidOffsets(keyframes)) {
    aRv.ThrowTypeError<MSG_INVALID_KEYFRAME_OFFSETS>();
    return;
  }

  // Fill in 0%/100% values if the first/element keyframes don't have
  // a specified offset, and evenly space those that have a missing
  // offset.  (We don't support paced spacing yet.)
  ApplyDistributeSpacing(keyframes);

  // Convert the OffsetIndexedKeyframes into a list of KeyframeValueEntry
  // objects.
  nsTArray<KeyframeValueEntry> entries;
  GenerateValueEntries(aTarget, keyframes, entries, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Finally, build an array of AnimationProperty objects in aResult
  // corresponding to the entries.
  BuildSegmentsFromValueEntries(entries, aResult);
}

/**
 * Converts a JS object to an IDL PropertyIndexedKeyframes and builds an
 * array of AnimationProperty objects for the keyframe animation
 * that it specifies.
 *
 * @param aTarget The target of the animation.
 * @param aValue The JS object.
 * @param aResult The array into which the resulting AnimationProperty
 *   objects will be appended.
 */
static void
BuildAnimationPropertyListFromPropertyIndexedKeyframes(
    JSContext* aCx,
    Element* aTarget,
    JS::Handle<JS::Value> aValue,
    InfallibleTArray<AnimationProperty>& aResult,
    ErrorResult& aRv)
{
  MOZ_ASSERT(aValue.isObject());

  // Convert the object to a PropertyIndexedKeyframes dictionary to
  // get its explicit dictionary members.
  binding_detail::FastPropertyIndexedKeyframes keyframes;
  if (!keyframes.Init(aCx, aValue, "PropertyIndexedKeyframes argument",
                      false)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  ComputedTimingFunction easing;
  ParseEasing(aTarget, keyframes.mEasing, easing);

  // We ignore easing.mComposite since we don't support composite modes on
  // keyframes yet.

  // Get all the property--value-list pairs off the object.
  JS::Rooted<JSObject*> object(aCx, &aValue.toObject());
  nsTArray<PropertyValuesPair> propertyValuesPairs;
  if (!GetPropertyValuesPairs(aCx, object, ListAllowance::eAllow,
                              propertyValuesPairs)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // We must keep track of which properties we've already generated
  // an AnimationProperty since the author could have specified both a
  // shorthand and one of its component longhands on the
  // PropertyIndexedKeyframes.
  nsCSSPropertySet properties;

  // Create AnimationProperty objects for each PropertyValuesPair, applying
  // the "distribute" spacing algorithm to the segments.
  for (const PropertyValuesPair& pair : propertyValuesPairs) {
    size_t count = pair.mValues.Length();
    if (count == 0) {
      // No animation values for this property.
      continue;
    }
    if (count == 1) {
      // We don't support additive segments and so can't support an
      // animation that goes from the underlying value to this
      // specified value.  Throw an exception until we do support this.
      aRv.Throw(NS_ERROR_DOM_ANIM_MISSING_PROPS_ERR);
      return;
    }

    // If we find an invalid value, we don't create a segment for it, but
    // we adjust the surrounding segments so that the timing of the segments
    // is the same as if we did support it.  For example, animating with
    // values ["red", "green", "yellow", "invalid", "blue"] will generate
    // segments with this timing:
    //
    //   0.00 -> 0.25 : red -> green
    //   0.25 -> 0.50 : green -> yellow
    //   0.50 -> 1.00 : yellow -> blue
    //
    // With future spec clarifications we might decide to preserve the invalid
    // value on the segment and make the animation code deal with the invalid
    // value instead.
    nsTArray<PropertyStyleAnimationValuePair> fromValues;
    float fromKey = 0.0f;
    if (!StyleAnimationValue::ComputeValues(pair.mProperty,
                                            nsCSSProps::eEnabledForAllContent,
                                            aTarget,
                                            pair.mValues[0],
                                            /* aUseSVGMode */ false,
                                            fromValues)) {
      // We need to throw for an invalid first value, since that would imply an
      // additive animation, which we don't support yet.
      aRv.Throw(NS_ERROR_DOM_ANIM_MISSING_PROPS_ERR);
      return;
    }

    if (fromValues.IsEmpty()) {
      // All longhand components of a shorthand pair.mProperty must be disabled.
      continue;
    }

    // Create AnimationProperty objects for each property that had a
    // value computed.  When pair.mProperty is a longhand, it is just
    // that property.  When pair.mProperty is a shorthand, we'll have
    // one property per longhand component.
    nsTArray<size_t> animationPropertyIndexes;
    animationPropertyIndexes.SetLength(fromValues.Length());
    for (size_t i = 0, n = fromValues.Length(); i < n; ++i) {
      nsCSSProperty p = fromValues[i].mProperty;
      bool found = false;
      if (properties.HasProperty(p)) {
        // We have already dealt with this property.  Look up and
        // overwrite the old AnimationProperty object.
        for (size_t j = 0, m = aResult.Length(); j < m; ++j) {
          if (aResult[j].mProperty == p) {
            aResult[j].mSegments.Clear();
            animationPropertyIndexes[i] = j;
            found = true;
            break;
          }
        }
        MOZ_ASSERT(found, "properties is inconsistent with aResult");
      }
      if (!found) {
        // This is the first time we've encountered this property.
        animationPropertyIndexes[i] = aResult.Length();
        AnimationProperty* animationProperty = aResult.AppendElement();
        animationProperty->mProperty = p;
        animationProperty->mWinsInCascade = true;
        properties.AddProperty(p);
      }
    }

    double portion = 1.0 / (count - 1);
    for (size_t i = 0; i < count - 1; ++i) {
      nsTArray<PropertyStyleAnimationValuePair> toValues;
      float toKey = (i + 1) * portion;
      if (!StyleAnimationValue::ComputeValues(pair.mProperty,
                                              nsCSSProps::eEnabledForAllContent,
                                              aTarget,
                                              pair.mValues[i + 1],
                                              /* aUseSVGMode */ false,
                                              toValues)) {
        if (i + 1 == count - 1) {
          // We need to throw for an invalid last value, since that would
          // imply an additive animation, which we don't support yet.
          aRv.Throw(NS_ERROR_DOM_ANIM_MISSING_PROPS_ERR);
          return;
        }
        // Otherwise, skip the segment.
        continue;
      }
      MOZ_ASSERT(toValues.Length() == fromValues.Length(),
                 "should get the same number of properties as the last time "
                 "we called ComputeValues for pair.mProperty");
      for (size_t j = 0, n = toValues.Length(); j < n; ++j) {
        size_t index = animationPropertyIndexes[j];
        AnimationPropertySegment* segment =
          aResult[index].mSegments.AppendElement();
        segment->mFromKey = fromKey;
        segment->mFromValue = fromValues[j].mValue;
        segment->mToKey = toKey;
        segment->mToValue = toValues[j].mValue;
        segment->mTimingFunction = easing;
      }
      fromValues = Move(toValues);
      fromKey = toKey;
    }
  }
}

/**
 * Converts a JS value to an IDL
 * (PropertyIndexedKeyframes or sequence<Keyframe>) value and builds an
 * array of AnimationProperty objects for the keyframe animation
 * that it specifies.
 *
 * @param aTarget The target of the animation, used to resolve style
 *   for a property's underlying value if needed.
 * @param aFrames The JS value, provided as an optional IDL |object?| value,
 *   that is the keyframe list specification.
 * @param aResult The array into which the resulting AnimationProperty
 *   objects will be appended.
 */
/* static */ void
KeyframeEffectReadOnly::BuildAnimationPropertyList(
    JSContext* aCx,
    Element* aTarget,
    const Optional<JS::Handle<JSObject*>>& aFrames,
    InfallibleTArray<AnimationProperty>& aResult,
    ErrorResult& aRv)
{
  MOZ_ASSERT(aResult.IsEmpty());

  // A frame list specification in the IDL is:
  //
  // (PropertyIndexedKeyframes or sequence<Keyframe> or SharedKeyframeList)
  //
  // We don't support SharedKeyframeList yet, but we do the other two.  We
  // manually implement the parts of JS-to-IDL union conversion algorithm
  // from the Web IDL spec, since we have to represent this an object? so
  // we can look at the open-ended set of properties on a
  // PropertyIndexedKeyframes or Keyframe.

  if (!aFrames.WasPassed() || !aFrames.Value().get()) {
    // The argument was omitted, or was explicitly null.  In both cases,
    // the default dictionary value for PropertyIndexedKeyframes would
    // result in no keyframes.
    return;
  }

  // At this point we know we have an object.  We try to convert it to a
  // sequence<Keyframe> first, and if that fails due to not being iterable,
  // we try to convert it to PropertyIndexedKeyframes.
  JS::Rooted<JS::Value> objectValue(aCx, JS::ObjectValue(*aFrames.Value()));
  JS::ForOfIterator iter(aCx);
  if (!iter.init(objectValue, JS::ForOfIterator::AllowNonIterable)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (iter.valueIsIterable()) {
    BuildAnimationPropertyListFromKeyframeSequence(aCx, aTarget, iter,
                                                   aResult, aRv);
  } else {
    BuildAnimationPropertyListFromPropertyIndexedKeyframes(aCx, aTarget,
                                                           objectValue, aResult,
                                                           aRv);
  }
}

/* static */ already_AddRefed<KeyframeEffectReadOnly>
KeyframeEffectReadOnly::Constructor(
    const GlobalObject& aGlobal,
    Element* aTarget,
    const Optional<JS::Handle<JSObject*>>& aFrames,
    const UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
    ErrorResult& aRv)
{
  if (!aTarget) {
    // We don't support null targets yet.
    aRv.Throw(NS_ERROR_DOM_ANIM_NO_TARGET_ERR);
    return nullptr;
  }

  AnimationTiming timing = ConvertKeyframeEffectOptions(aOptions);

  InfallibleTArray<AnimationProperty> animationProperties;
  BuildAnimationPropertyList(aGlobal.Context(), aTarget, aFrames,
                             animationProperties, aRv);

  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<KeyframeEffectReadOnly> effect =
    new KeyframeEffectReadOnly(aTarget->OwnerDoc(), aTarget,
                               nsCSSPseudoElements::ePseudo_NotPseudoElement,
                               timing);
  effect->mProperties = Move(animationProperties);
  return effect.forget();
}

void
KeyframeEffectReadOnly::GetFrames(JSContext*& aCx,
                                  nsTArray<JSObject*>& aResult,
                                  ErrorResult& aRv)
{
  nsTArray<OrderedKeyframeValueEntry> entries;

  for (const AnimationProperty& property : mProperties) {
    for (size_t i = 0, n = property.mSegments.Length(); i < n; i++) {
      const AnimationPropertySegment& segment = property.mSegments[i];

      // We append the mFromValue for each segment.  If the mToValue
      // differs from the following segment's mFromValue, or if we're on
      // the last segment, then we append the mToValue as well.
      //
      // Each value is annotated with whether it is a "first", "left", "right",
      // or "last" value.  "left" and "right" values represent the value coming
      // in to and out of a given offset, in the middle of an animation.  For
      // most segments, the mToValue is the "left" and the following segment's
      // mFromValue is the "right".  The "first" and "last" values are the
      // additional values assigned to offset 0 or 1 for reverse and forward
      // filling.  These annotations are used to ensure multiple values for a
      // given property are sorted correctly and that we do not merge Keyframes
      // with different values for the same offset.

      OrderedKeyframeValueEntry* entry = entries.AppendElement();
      entry->mProperty = property.mProperty;
      entry->mValue = segment.mFromValue;
      entry->mOffset = segment.mFromKey;
      entry->mTimingFunction = &segment.mTimingFunction;
      entry->mPosition =
        segment.mFromKey == segment.mToKey && segment.mFromKey == 0.0f ?
          ValuePosition::First :
          ValuePosition::Right;

      if (i == n - 1 ||
          segment.mToValue != property.mSegments[i + 1].mFromValue) {
        entry = entries.AppendElement();
        entry->mProperty = property.mProperty;
        entry->mValue = segment.mToValue;
        entry->mOffset = segment.mToKey;
        entry->mTimingFunction =
          segment.mToKey == 1.0f ? nullptr : &segment.mTimingFunction;
        entry->mPosition =
          segment.mFromKey == segment.mToKey && segment.mToKey == 1.0f ?
            ValuePosition::Last :
            ValuePosition::Left;
      }
    }
  }

  entries.Sort(OrderedKeyframeValueEntry::ForKeyframeGenerationComparator());

  for (size_t i = 0, n = entries.Length(); i < n; ) {
    OrderedKeyframeValueEntry* entry = &entries[i];
    OrderedKeyframeValueEntry* previousEntry = nullptr;

    // Create a JS object with the explicit ComputedKeyframe dictionary members.
    ComputedKeyframe keyframeDict;
    keyframeDict.mOffset.SetValue(entry->mOffset);
    keyframeDict.mComputedOffset.Construct(entry->mOffset);
    if (entry->mTimingFunction) {
      // If null, leave easing as its default "linear".
      keyframeDict.mEasing.Truncate();
      entry->mTimingFunction->AppendToString(keyframeDict.mEasing);
    }
    keyframeDict.mComposite.SetValue(CompositeOperation::Replace);

    JS::Rooted<JS::Value> keyframeJSValue(aCx);
    if (!ToJSValue(aCx, keyframeDict, &keyframeJSValue)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    JS::Rooted<JSObject*> keyframe(aCx, &keyframeJSValue.toObject());
    do {
      const char* name = nsCSSProps::PropertyIDLName(entry->mProperty);
      nsString stringValue;
      StyleAnimationValue::UncomputeValue(entry->mProperty,
                                          entry->mValue,
                                          stringValue);
      JS::Rooted<JS::Value> value(aCx);
      if (!ToJSValue(aCx, stringValue, &value) ||
          !JS_DefineProperty(aCx, keyframe, name, value, JSPROP_ENUMERATE)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return;
      }
      if (++i == n) {
        break;
      }
      previousEntry = entry;
      entry = &entries[i];
    } while (entry->SameKeyframe(*previousEntry));

    aResult.AppendElement(keyframe);
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
  // Animation::CanThrottle checks for not in effect animations
  // before calling this.
  MOZ_ASSERT(IsInEffect(), "Effect should be in effect");

  // Unthrottle if this animation is not current (i.e. it has passed the end).
  // In future we may be able to throttle this case too, but we should only get
  // occasional ticks while the animation is in this state so it doesn't matter
  // too much.
  if (!IsCurrent()) {
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
  // prior to the IsPropertyRunningOnCompositor check because we should
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

    AnimationCollection* collection = GetCollection();
    MOZ_ASSERT(collection,
      "CanThrottle should be called on an effect associated with an animation");
    layers::Layer* layer =
      FrameLayerBuilder::GetDedicatedLayer(frame, record.mLayerType);
    // Unthrottle if the layer needs to be brought up to date with the animation.
    if (!layer ||
        collection->mAnimationGeneration > layer->GetAnimationGeneration()) {
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
    if (!IsPropertyRunningOnCompositor(property.mProperty)) {
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

  AnimationCollection* collection = GetCollection();
  MOZ_ASSERT(collection,
    "CanThrottleTransformChanges should be involved with animation collection");
  TimeStamp styleRuleRefreshTime = collection->mStyleRuleRefreshTime;
  // If this animation can cause overflow, we can throttle some of the ticks.
  if (!styleRuleRefreshTime.IsNull() &&
      (now - styleRuleRefreshTime) < OverflowRegionRefreshInterval()) {
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

  nsIFrame* frame = mTarget->GetPrimaryFrame();
  if (!frame) {
    return nullptr;
  }

  if (mPseudoType == nsCSSPseudoElements::ePseudo_before) {
    frame = nsLayoutUtils::GetBeforeFrame(frame);
  } else if (mPseudoType == nsCSSPseudoElements::ePseudo_after) {
    frame = nsLayoutUtils::GetAfterFrame(frame);
  } else {
    MOZ_ASSERT(mPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement,
               "unknown mPseudoType");
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
  return mTarget->GetComposedDoc();
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

AnimationCollection *
KeyframeEffectReadOnly::GetCollection() const
{
  return mAnimation ? mAnimation->GetCollection() : nullptr;
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
  const nsIContent* aContent)
{
  if (aFrame->Combines3DTransformWithAncestors() ||
      aFrame->Extend3DContext()) {
    if (aContent) {
      nsCString message;
      message.AppendLiteral("Gecko bug: Async animation of 'preserve-3d' "
        "transforms is not supported.  See bug 779598");
      AnimationUtils::LogAsyncAnimationFailure(message, aContent);
    }
    return false;
  }
  // Note that testing BackfaceIsHidden() is not a sufficient test for
  // what we need for animating backface-visibility correctly if we
  // remove the above test for Extend3DContext(); that would require
  // looking at backface-visibility on descendants as well.
  if (aFrame->StyleDisplay()->BackfaceIsHidden()) {
    if (aContent) {
      nsCString message;
      message.AppendLiteral("Gecko bug: Async animation of "
        "'backface-visibility: hidden' transforms is not supported."
        "  See bug 1186204.");
      AnimationUtils::LogAsyncAnimationFailure(message, aContent);
    }
    return false;
  }
  if (aFrame->IsSVGTransformed()) {
    if (aContent) {
      nsCString message;
      message.AppendLiteral("Gecko bug: Async 'transform' animations of "
        "aFrames with SVG transforms is not supported.  See bug 779599");
      AnimationUtils::LogAsyncAnimationFailure(message, aContent);
    }
    return false;
  }

  return true;
}

bool
KeyframeEffectReadOnly::ShouldBlockCompositorAnimations(const nsIFrame*
                                                          aFrame) const
{
  // We currently only expect this method to be called when this effect
  // is attached to a playing Animation. If that ever changes we'll need
  // to update this to only return true when that is the case since paused,
  // filling, cancelled Animations etc. shouldn't stop other Animations from
  // running on the compositor.
  MOZ_ASSERT(mAnimation && mAnimation->IsPlaying());

  bool shouldLog = nsLayoutUtils::IsAnimationLoggingEnabled();

  for (const AnimationProperty& property : mProperties) {
    // Check for geometric properties
    if (IsGeometricProperty(property.mProperty)) {
      if (shouldLog) {
        nsCString message;
        message.AppendLiteral("Performance warning: Async animation of "
          "'transform' or 'opacity' not possible due to animation of geometric"
          "properties on the same element");
        AnimationUtils::LogAsyncAnimationFailure(message, aFrame->GetContent());
      }
      return true;
    }

    // Check for unsupported transform animations
    if (property.mProperty == eCSSProperty_transform) {
      if (!CanAnimateTransformOnCompositor(aFrame,
            shouldLog ? aFrame->GetContent() : nullptr)) {
        return true;
      }
    }
  }

  return false;
}

} // namespace dom
} // namespace mozilla
