/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationPlayer.h"
#include "mozilla/dom/AnimationPlayerBinding.h"

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
  NS_ABORT_IF_FALSE(0.0 <= aPortion && aPortion <= 1.0, "out of range");
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
      NS_ABORT_IF_FALSE(false, "bad type");
      // fall through
    case nsTimingFunction::StepEnd:
      return StepEnd(mSteps, aPortion);
  }
}

// In the Web Animations model, the time fraction can be outside the range
// [0.0, 1.0] but it shouldn't be Infinity.
const double ComputedTiming::kNullTimeFraction = PositiveInfinity<double>();

namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AnimationPlayer, mTimeline, mSource)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AnimationPlayer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AnimationPlayer, Release)

JSObject*
AnimationPlayer::WrapObject(JSContext* aCx)
{
  return dom::AnimationPlayerBinding::Wrap(aCx, this);
}

double
AnimationPlayer::StartTime() const
{
  Nullable<double> startTime = mTimeline->ToTimelineTime(mStartTime);
  return startTime.IsNull() ? 0.0 : startTime.Value();
}

double
AnimationPlayer::CurrentTime() const
{
  // In Web Animations, AnimationPlayers have a *current* time and Animations
  // have a *local* time. However, since we have a 1:1 correspondence between
  // AnimationPlayers and Animations, and since the startTime of *Animations*
  // (but not AnimationPlayers) is always 0, these are currently identical.
  Nullable<TimeDuration> currentTime = GetLocalTime();

  // The current time is currently only going to be null when don't have a
  // refresh driver (e.g. because we are in a display:none iframe).
  //
  // Web Animations says that in this case we should use a timeline time of
  // 0 (the "effective timeline time") and calculate the current time from that.
  // Doing that, however, requires storing the start time as an offset rather
  // than a timestamp so for now we just return 0.
  //
  // FIXME: Store player start time and pause start as offsets rather than
  // timestamps and return the appropriate current time when the timeline time
  // is null.
  if (currentTime.IsNull()) {
    return 0.0;
  }

  return currentTime.Value().ToMilliseconds();
}

void
AnimationPlayer::SetSource(Animation* aSource)
{
  if (mSource) {
    mSource->SetParentTime(Nullable<TimeDuration>());
  }
  mSource = aSource;
  if (mSource) {
    mSource->SetParentTime(GetLocalTime());
  }
}

void
AnimationPlayer::Tick()
{
  if (mSource) {
    mSource->SetParentTime(GetLocalTime());
  }
}

bool
AnimationPlayer::IsRunning() const
{
  if (IsPaused() || IsFinishedTransition()) {
    return false;
  }

  ComputedTiming computedTiming = GetComputedTiming(mTiming);
  return computedTiming.mPhase == ComputedTiming::AnimationPhase_Active;
}

bool
AnimationPlayer::IsCurrent() const
{
  if (IsFinishedTransition()) {
    return false;
  }

  ComputedTiming computedTiming = GetComputedTiming(mTiming);
  return computedTiming.mPhase == ComputedTiming::AnimationPhase_Before ||
         computedTiming.mPhase == ComputedTiming::AnimationPhase_Active;
}

ComputedTiming
AnimationPlayer::GetComputedTimingAt(const Nullable<TimeDuration>& aLocalTime,
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
  TimeDuration activeTime;
  // FIXME: The following check that the active duration is not equal to Forever
  // is a temporary workaround to avoid overflow and should be removed once
  // bug 1039924 is fixed.
  if (result.mActiveDuration != TimeDuration::Forever() &&
      localTime >= aTiming.mDelay + result.mActiveDuration) {
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
  TimeDuration iterationTime;
  if (aTiming.mIterationDuration != zeroDuration) {
    iterationTime = isEndOfFinalIteration
                    ? aTiming.mIterationDuration
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

TimeDuration
AnimationPlayer::ActiveDuration(const AnimationTiming& aTiming)
{
  if (aTiming.mIterationCount == mozilla::PositiveInfinity<float>()) {
    // An animation that repeats forever has an infinite active duration
    // unless its iteration duration is zero, in which case it has a zero
    // active duration.
    const TimeDuration zeroDuration;
    return aTiming.mIterationDuration == zeroDuration
           ? zeroDuration
           : TimeDuration::Forever();
  }
  return aTiming.mIterationDuration.MultDouble(aTiming.mIterationCount);
}

} // namespace dom
} // namespace mozilla
