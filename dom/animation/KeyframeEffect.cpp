/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/KeyframeEffect.h"
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "mozilla/FloatingPoint.h"
#include "AnimationCommon.h"
#include "nsCSSPropertySet.h"

namespace mozilla {

void
ComputedTimingFunction::Init(const nsTimingFunction &aFunction)
{
  mType = aFunction.mType;
  if (mType == nsTimingFunction::Function) {
    mTimingFunction.Init(aFunction.mFunc.mX1, aFunction.mFunc.mY1,
                         aFunction.mFunc.mX2, aFunction.mFunc.mY2);
  } else {
    mSteps = aFunction.mSteps;
  }
}

static inline double
StepEnd(uint32_t aSteps, double aPortion)
{
  MOZ_ASSERT(0.0 <= aPortion && aPortion <= 1.0, "out of range");
  uint32_t step = uint32_t(aPortion * aSteps); // floor
  return double(step) / double(aSteps);
}

double
ComputedTimingFunction::GetValue(double aPortion) const
{
  switch (mType) {
    case nsTimingFunction::Function:
      return mTimingFunction.GetSplineValue(aPortion);
    case nsTimingFunction::StepStart:
      // There are diagrams in the spec that seem to suggest this check
      // and the bounds point should not be symmetric with StepEnd, but
      // should actually step up at rather than immediately after the
      // fraction points.  However, we rely on rounding negative values
      // up to zero, so we can't do that.  And it's not clear the spec
      // really meant it.
      return 1.0 - StepEnd(mSteps, 1.0 - aPortion);
    default:
      MOZ_ASSERT(false, "bad type");
      // fall through
    case nsTimingFunction::StepEnd:
      return StepEnd(mSteps, aPortion);
  }
}

// In the Web Animations model, the time fraction can be outside the range
// [0.0, 1.0] but it shouldn't be Infinity.
const double ComputedTiming::kNullTimeFraction = PositiveInfinity<double>();

namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(KeyframeEffectReadOnly,
                                   AnimationEffectReadOnly,
                                   mTarget)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(KeyframeEffectReadOnly,
                                               AnimationEffectReadOnly)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(KeyframeEffectReadOnly)
NS_INTERFACE_MAP_END_INHERITING(AnimationEffectReadOnly)

NS_IMPL_ADDREF_INHERITED(KeyframeEffectReadOnly, AnimationEffectReadOnly)
NS_IMPL_RELEASE_INHERITED(KeyframeEffectReadOnly, AnimationEffectReadOnly)

JSObject*
KeyframeEffectReadOnly::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto)
{
  return KeyframeEffectReadOnlyBinding::Wrap(aCx, this, aGivenProto);
}

void
KeyframeEffectReadOnly::SetParentTime(Nullable<TimeDuration> aParentTime)
{
  mParentTime = aParentTime;
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
    result.mPhase = ComputedTiming::AnimationPhase_After;
    if (!aTiming.FillsForwards()) {
      // The animation isn't active or filling at this time.
      result.mTimeFraction = ComputedTiming::kNullTimeFraction;
      return result;
    }
    activeTime = result.mActiveDuration;
    // Note that infinity == floor(infinity) so this will also be true when we
    // have finished an infinitely repeating animation of zero duration.
    isEndOfFinalIteration =
      aTiming.mIterationCount != 0.0 &&
      aTiming.mIterationCount == floor(aTiming.mIterationCount);
  } else if (localTime < aTiming.mDelay) {
    result.mPhase = ComputedTiming::AnimationPhase_Before;
    if (!aTiming.FillsBackwards()) {
      // The animation isn't active or filling at this time.
      result.mTimeFraction = ComputedTiming::kNullTimeFraction;
      return result;
    }
    // activeTime is zero
  } else {
    MOZ_ASSERT(result.mActiveDuration != zeroDuration,
               "How can we be in the middle of a zero-duration interval?");
    result.mPhase = ComputedTiming::AnimationPhase_Active;
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
      ? UINT64_MAX // FIXME: When we return this via the API we'll need
                   // to make sure it ends up being infinity.
      : static_cast<uint64_t>(aTiming.mIterationCount) - 1;
  } else if (activeTime == zeroDuration) {
    // If the active time is zero we're either in the first iteration
    // (including filling backwards) or we have finished an animation with an
    // iteration duration of zero that is filling forwards (but we're not at
    // the exact end of an iteration since we deal with that above).
    result.mCurrentIteration =
      result.mPhase == ComputedTiming::AnimationPhase_After
      ? static_cast<uint64_t>(aTiming.mIterationCount) // floor
      : 0;
  } else {
    result.mCurrentIteration =
      static_cast<uint64_t>(activeTime / aTiming.mIterationDuration); // floor
  }

  // Normalize the iteration time into a fraction of the iteration duration.
  if (result.mPhase == ComputedTiming::AnimationPhase_Before) {
    result.mTimeFraction = 0.0;
  } else if (result.mPhase == ComputedTiming::AnimationPhase_After) {
    result.mTimeFraction = isEndOfFinalIteration
                         ? 1.0
                         : fmod(aTiming.mIterationCount, 1.0f);
  } else {
    // We are in the active phase so the iteration duration can't be zero.
    MOZ_ASSERT(aTiming.mIterationDuration != zeroDuration,
               "In the active phase of a zero-duration animation?");
    result.mTimeFraction =
      aTiming.mIterationDuration == TimeDuration::Forever()
      ? 0.0
      : iterationTime / aTiming.mIterationDuration;
  }

  bool thisIterationReverse = false;
  switch (aTiming.mDirection) {
    case NS_STYLE_ANIMATION_DIRECTION_NORMAL:
      thisIterationReverse = false;
      break;
    case NS_STYLE_ANIMATION_DIRECTION_REVERSE:
      thisIterationReverse = true;
      break;
    case NS_STYLE_ANIMATION_DIRECTION_ALTERNATE:
      thisIterationReverse = (result.mCurrentIteration & 1) == 1;
      break;
    case NS_STYLE_ANIMATION_DIRECTION_ALTERNATE_REVERSE:
      thisIterationReverse = (result.mCurrentIteration & 1) == 0;
      break;
  }
  if (thisIterationReverse) {
    result.mTimeFraction = 1.0 - result.mTimeFraction;
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

// http://w3c.github.io/web-animations/#in-play
bool
KeyframeEffectReadOnly::IsInPlay(const Animation& aAnimation) const
{
  if (IsFinishedTransition() ||
      aAnimation.PlayState() == AnimationPlayState::Finished) {
    return false;
  }

  return GetComputedTiming().mPhase == ComputedTiming::AnimationPhase_Active;
}

// http://w3c.github.io/web-animations/#current
bool
KeyframeEffectReadOnly::IsCurrent(const Animation& aAnimation) const
{
  if (IsFinishedTransition() ||
      aAnimation.PlayState() == AnimationPlayState::Finished) {
    return false;
  }

  ComputedTiming computedTiming = GetComputedTiming();
  return computedTiming.mPhase == ComputedTiming::AnimationPhase_Before ||
         computedTiming.mPhase == ComputedTiming::AnimationPhase_Active;
}

bool
KeyframeEffectReadOnly::IsInEffect() const
{
  if (IsFinishedTransition()) {
    return false;
  }

  ComputedTiming computedTiming = GetComputedTiming();
  return computedTiming.mTimeFraction != ComputedTiming::kNullTimeFraction;
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
KeyframeEffectReadOnly::ComposeStyle(
                          nsRefPtr<css::AnimValuesStyleRule>& aStyleRule,
                          nsCSSPropertySet& aSetProperties)
{
  ComputedTiming computedTiming = GetComputedTiming();

  // If the time fraction is null, we don't have fill data for the current
  // time so we shouldn't animate.
  if (computedTiming.mTimeFraction == ComputedTiming::kNullTimeFraction) {
    return;
  }

  MOZ_ASSERT(0.0 <= computedTiming.mTimeFraction &&
             computedTiming.mTimeFraction <= 1.0,
             "timing fraction should be in [0-1]");

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
    while (segment->mToKey < computedTiming.mTimeFraction) {
      MOZ_ASSERT(segment->mFromKey < segment->mToKey, "incorrect keys");
      ++segment;
      if (segment == segmentEnd) {
        MOZ_ASSERT_UNREACHABLE("incorrect time fraction");
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
      aStyleRule = new css::AnimValuesStyleRule();
    }

    double positionInSegment =
      (computedTiming.mTimeFraction - segment->mFromKey) /
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

} // namespace dom
} // namespace mozilla
