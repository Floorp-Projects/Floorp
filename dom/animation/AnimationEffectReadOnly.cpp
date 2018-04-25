/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AnimationEffectReadOnly.h"
#include "mozilla/dom/AnimationEffectReadOnlyBinding.h"

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/KeyframeEffectReadOnly.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/FloatingPoint.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(AnimationEffectReadOnly)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AnimationEffectReadOnly)
  if (tmp->mTiming) {
    tmp->mTiming->Unlink();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument, mTiming, mAnimation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AnimationEffectReadOnly)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument, mTiming, mAnimation)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(AnimationEffectReadOnly)

NS_IMPL_CYCLE_COLLECTING_ADDREF(AnimationEffectReadOnly)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AnimationEffectReadOnly)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AnimationEffectReadOnly)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

AnimationEffectReadOnly::AnimationEffectReadOnly(
  nsIDocument* aDocument, AnimationEffectTimingReadOnly* aTiming)
  : mDocument(aDocument)
  , mTiming(aTiming)
{
  MOZ_ASSERT(aTiming);
}

// https://drafts.csswg.org/web-animations/#current
bool
AnimationEffectReadOnly::IsCurrent() const
{
  if (!mAnimation || mAnimation->PlayState() == AnimationPlayState::Finished) {
    return false;
  }

  ComputedTiming computedTiming = GetComputedTiming();
  return computedTiming.mPhase == ComputedTiming::AnimationPhase::Before ||
         computedTiming.mPhase == ComputedTiming::AnimationPhase::Active;
}

// https://drafts.csswg.org/web-animations/#in-effect
bool
AnimationEffectReadOnly::IsInEffect() const
{
  ComputedTiming computedTiming = GetComputedTiming();
  return !computedTiming.mProgress.IsNull();
}

already_AddRefed<AnimationEffectTimingReadOnly>
AnimationEffectReadOnly::Timing()
{
  RefPtr<AnimationEffectTimingReadOnly> temp(mTiming);
  return temp.forget();
}

void
AnimationEffectReadOnly::SetSpecifiedTiming(const TimingParams& aTiming)
{
  if (mTiming->AsTimingParams() == aTiming) {
    return;
  }
  mTiming->SetTimingParams(aTiming);
  if (mAnimation) {
    mAnimation->NotifyEffectTimingUpdated();
    if (AsKeyframeEffect()) {
      AsKeyframeEffect()->RequestRestyle(EffectCompositor::RestyleType::Layer);
    }
  }
  // For keyframe effects, NotifyEffectTimingUpdated above will eventually cause
  // KeyframeEffectReadOnly::NotifyAnimationTimingUpdated to be called so it can
  // update its registration with the target element as necessary.
}

ComputedTiming
AnimationEffectReadOnly::GetComputedTimingAt(
    const Nullable<TimeDuration>& aLocalTime,
    const TimingParams& aTiming,
    double aPlaybackRate)
{
  static const StickyTimeDuration zeroDuration;

  // Always return the same object to benefit from return-value optimization.
  ComputedTiming result;

  if (aTiming.Duration()) {
    MOZ_ASSERT(aTiming.Duration().ref() >= zeroDuration,
               "Iteration duration should be positive");
    result.mDuration = aTiming.Duration().ref();
  }

  MOZ_ASSERT(aTiming.Iterations() >= 0.0 && !IsNaN(aTiming.Iterations()),
             "mIterations should be nonnegative & finite, as ensured by "
             "ValidateIterations or CSSParser");
  result.mIterations = aTiming.Iterations();

  MOZ_ASSERT(aTiming.IterationStart() >= 0.0,
             "mIterationStart should be nonnegative, as ensured by "
             "ValidateIterationStart");
  result.mIterationStart = aTiming.IterationStart();

  result.mActiveDuration = aTiming.ActiveDuration();
  result.mEndTime = aTiming.EndTime();
  result.mFill = aTiming.Fill() == dom::FillMode::Auto ?
                 dom::FillMode::None :
                 aTiming.Fill();

  // The default constructor for ComputedTiming sets all other members to
  // values consistent with an animation that has not been sampled.
  if (aLocalTime.IsNull()) {
    return result;
  }
  const TimeDuration& localTime = aLocalTime.Value();

  StickyTimeDuration beforeActiveBoundary =
    std::max(std::min(StickyTimeDuration(aTiming.Delay()), result.mEndTime),
             zeroDuration);

  StickyTimeDuration activeAfterBoundary =
    std::max(std::min(StickyTimeDuration(aTiming.Delay() +
                                         result.mActiveDuration),
                      result.mEndTime),
             zeroDuration);

  if (localTime > activeAfterBoundary ||
      (aPlaybackRate >= 0 && localTime == activeAfterBoundary)) {
    result.mPhase = ComputedTiming::AnimationPhase::After;
    if (!result.FillsForwards()) {
      // The animation isn't active or filling at this time.
      return result;
    }
    result.mActiveTime =
      std::max(std::min(StickyTimeDuration(localTime - aTiming.Delay()),
                        result.mActiveDuration),
               zeroDuration);
  } else if (localTime < beforeActiveBoundary ||
             (aPlaybackRate < 0 && localTime == beforeActiveBoundary)) {
    result.mPhase = ComputedTiming::AnimationPhase::Before;
    if (!result.FillsBackwards()) {
      // The animation isn't active or filling at this time.
      return result;
    }
    result.mActiveTime
      = std::max(StickyTimeDuration(localTime - aTiming.Delay()),
                 zeroDuration);
  } else {
    MOZ_ASSERT(result.mActiveDuration,
               "How can we be in the middle of a zero-duration interval?");
    result.mPhase = ComputedTiming::AnimationPhase::Active;
    result.mActiveTime = localTime - aTiming.Delay();
  }

  // Convert active time to a multiple of iterations.
  // https://drafts.csswg.org/web-animations/#overall-progress
  double overallProgress;
  if (!result.mDuration) {
    overallProgress = result.mPhase == ComputedTiming::AnimationPhase::Before
                      ? 0.0
                      : result.mIterations;
  } else {
    overallProgress = result.mActiveTime / result.mDuration;
  }

  // Factor in iteration start offset.
  if (IsFinite(overallProgress)) {
    overallProgress += result.mIterationStart;
  }

  // Determine the 0-based index of the current iteration.
  // https://drafts.csswg.org/web-animations/#current-iteration
  result.mCurrentIteration =
    (result.mIterations >= UINT64_MAX
     && result.mPhase == ComputedTiming::AnimationPhase::After)
    || overallProgress >= UINT64_MAX
    ? UINT64_MAX // In GetComputedTimingDictionary(),
                 // we will convert this into Infinity
    : static_cast<uint64_t>(overallProgress);

  // Convert the overall progress to a fraction of a single iteration--the
  // simply iteration progress.
  // https://drafts.csswg.org/web-animations/#simple-iteration-progress
  double progress = IsFinite(overallProgress)
                    ? fmod(overallProgress, 1.0)
                    : fmod(result.mIterationStart, 1.0);

  // When we are at the end of the active interval and the end of an iteration
  // we need to report the end of the final iteration and not the start of the
  // next iteration. We *don't* want to do this, however, when we have
  // a zero-iteration animation.
  if (progress == 0.0 &&
      (result.mPhase == ComputedTiming::AnimationPhase::After ||
       result.mPhase == ComputedTiming::AnimationPhase::Active) &&
      result.mActiveTime == result.mActiveDuration &&
      result.mIterations != 0.0) {
    // The only way we can reach the end of the active interval and have
    // a progress of zero and a current iteration of zero, is if we have a zero
    // iteration count -- something we should have detected above.
    MOZ_ASSERT(result.mCurrentIteration != 0,
               "Should not have zero current iteration");
    progress = 1.0;
    if (result.mCurrentIteration != UINT64_MAX) {
      result.mCurrentIteration--;
    }
  }

  // Factor in the direction.
  bool thisIterationReverse = false;
  switch (aTiming.Direction()) {
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
      MOZ_ASSERT_UNREACHABLE("Unknown PlaybackDirection type");
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
  if (aTiming.TimingFunction()) {
    progress = aTiming.TimingFunction()->GetValue(progress, result.mBeforeFlag);
  }

  MOZ_ASSERT(IsFinite(progress), "Progress value should be finite");
  result.mProgress.SetValue(progress);
  return result;
}

ComputedTiming
AnimationEffectReadOnly::GetComputedTiming(const TimingParams* aTiming) const
{
  double playbackRate = mAnimation ? mAnimation->PlaybackRate() : 1;
  return GetComputedTimingAt(GetLocalTime(),
                             aTiming ? *aTiming : SpecifiedTiming(),
                             playbackRate);
}

// Helper functions for generating a ComputedTimingProperties dictionary
static void
GetComputedTimingDictionary(const ComputedTiming& aComputedTiming,
                            const Nullable<TimeDuration>& aLocalTime,
                            const TimingParams& aTiming,
                            ComputedTimingProperties& aRetVal)
{
  // AnimationEffectTimingProperties
  aRetVal.mDelay = aTiming.Delay().ToMilliseconds();
  aRetVal.mEndDelay = aTiming.EndDelay().ToMilliseconds();
  aRetVal.mFill = aComputedTiming.mFill;
  aRetVal.mIterationStart = aComputedTiming.mIterationStart;
  aRetVal.mIterations = aComputedTiming.mIterations;
  aRetVal.mDuration.SetAsUnrestrictedDouble() =
    aComputedTiming.mDuration.ToMilliseconds();
  aRetVal.mDirection = aTiming.Direction();
  if (aTiming.TimingFunction()) {
    aRetVal.mEasing.Truncate();
    aTiming.TimingFunction()->AppendToString(aRetVal.mEasing);
  }

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

void
AnimationEffectReadOnly::GetComputedTimingAsDict(
  ComputedTimingProperties& aRetVal) const
{
  double playbackRate = mAnimation ? mAnimation->PlaybackRate() : 1;
  const Nullable<TimeDuration> currentTime = GetLocalTime();
  GetComputedTimingDictionary(GetComputedTimingAt(currentTime,
                                                  SpecifiedTiming(),
                                                  playbackRate),
                              currentTime,
                              SpecifiedTiming(),
                              aRetVal);
}

AnimationEffectReadOnly::~AnimationEffectReadOnly()
{
  // mTiming is cycle collected, so we have to do null check first even though
  // mTiming shouldn't be null during the lifetime of KeyframeEffect.
  if (mTiming) {
    mTiming->Unlink();
  }
}

Nullable<TimeDuration>
AnimationEffectReadOnly::GetLocalTime() const
{
  // Since the *animation* start time is currently always zero, the local
  // time is equal to the parent time.
  Nullable<TimeDuration> result;
  if (mAnimation) {
    result = mAnimation->GetCurrentTime();
  }
  return result;
}

} // namespace dom
} // namespace mozilla
