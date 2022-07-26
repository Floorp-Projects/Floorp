/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TimingParams.h"

#include "mozilla/AnimationUtils.h"
#include "mozilla/dom/AnimatableBinding.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/KeyframeAnimationOptionsBinding.h"
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "mozilla/ServoCSSParser.h"

namespace mozilla {

template <class OptionsType>
static const dom::EffectTiming& GetTimingProperties(
    const OptionsType& aOptions);

template <>
/* static */
const dom::EffectTiming& GetTimingProperties(
    const dom::UnrestrictedDoubleOrKeyframeEffectOptions& aOptions) {
  MOZ_ASSERT(aOptions.IsKeyframeEffectOptions());
  return aOptions.GetAsKeyframeEffectOptions();
}

template <>
/* static */
const dom::EffectTiming& GetTimingProperties(
    const dom::UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions) {
  MOZ_ASSERT(aOptions.IsKeyframeAnimationOptions());
  return aOptions.GetAsKeyframeAnimationOptions();
}

template <class OptionsType>
/* static */
TimingParams TimingParams::FromOptionsType(const OptionsType& aOptions,
                                           ErrorResult& aRv) {
  TimingParams result;

  if (aOptions.IsUnrestrictedDouble()) {
    double durationInMs = aOptions.GetAsUnrestrictedDouble();
    if (durationInMs >= 0) {
      result.mDuration.emplace(
          StickyTimeDuration::FromMilliseconds(durationInMs));
    } else {
      nsPrintfCString error("Duration value %g is less than 0", durationInMs);
      aRv.ThrowTypeError(error);
      return result;
    }
    result.Update();
  } else {
    const dom::EffectTiming& timing = GetTimingProperties(aOptions);
    result = FromEffectTiming(timing, aRv);
  }

  return result;
}

/* static */
TimingParams TimingParams::FromOptionsUnion(
    const dom::UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
    ErrorResult& aRv) {
  return FromOptionsType(aOptions, aRv);
}

/* static */
TimingParams TimingParams::FromOptionsUnion(
    const dom::UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
    ErrorResult& aRv) {
  return FromOptionsType(aOptions, aRv);
}

/* static */
TimingParams TimingParams::FromEffectTiming(
    const dom::EffectTiming& aEffectTiming, ErrorResult& aRv) {
  TimingParams result;

  Maybe<StickyTimeDuration> duration =
      TimingParams::ParseDuration(aEffectTiming.mDuration, aRv);
  if (aRv.Failed()) {
    return result;
  }
  TimingParams::ValidateIterationStart(aEffectTiming.mIterationStart, aRv);
  if (aRv.Failed()) {
    return result;
  }
  TimingParams::ValidateIterations(aEffectTiming.mIterations, aRv);
  if (aRv.Failed()) {
    return result;
  }
  Maybe<StyleComputedTimingFunction> easing =
      ParseEasing(aEffectTiming.mEasing, aRv);
  if (aRv.Failed()) {
    return result;
  }

  result.mDuration = duration;
  result.mDelay = TimeDuration::FromMilliseconds(aEffectTiming.mDelay);
  result.mEndDelay = TimeDuration::FromMilliseconds(aEffectTiming.mEndDelay);
  result.mIterations = aEffectTiming.mIterations;
  result.mIterationStart = aEffectTiming.mIterationStart;
  result.mDirection = aEffectTiming.mDirection;
  result.mFill = aEffectTiming.mFill;
  result.mFunction = std::move(easing);

  result.Update();

  return result;
}

/* static */
TimingParams TimingParams::MergeOptionalEffectTiming(
    const TimingParams& aSource, const dom::OptionalEffectTiming& aEffectTiming,
    ErrorResult& aRv) {
  MOZ_ASSERT(!aRv.Failed(), "Initially return value should be ok");

  TimingParams result = aSource;

  // Check for errors first

  Maybe<StickyTimeDuration> duration;
  if (aEffectTiming.mDuration.WasPassed()) {
    duration =
        TimingParams::ParseDuration(aEffectTiming.mDuration.Value(), aRv);
    if (aRv.Failed()) {
      return result;
    }
  }

  if (aEffectTiming.mIterationStart.WasPassed()) {
    TimingParams::ValidateIterationStart(aEffectTiming.mIterationStart.Value(),
                                         aRv);
    if (aRv.Failed()) {
      return result;
    }
  }

  if (aEffectTiming.mIterations.WasPassed()) {
    TimingParams::ValidateIterations(aEffectTiming.mIterations.Value(), aRv);
    if (aRv.Failed()) {
      return result;
    }
  }

  Maybe<StyleComputedTimingFunction> easing;
  if (aEffectTiming.mEasing.WasPassed()) {
    easing = ParseEasing(aEffectTiming.mEasing.Value(), aRv);
    if (aRv.Failed()) {
      return result;
    }
  }

  // Assign values

  if (aEffectTiming.mDuration.WasPassed()) {
    result.mDuration = duration;
  }
  if (aEffectTiming.mDelay.WasPassed()) {
    result.mDelay =
        TimeDuration::FromMilliseconds(aEffectTiming.mDelay.Value());
  }
  if (aEffectTiming.mEndDelay.WasPassed()) {
    result.mEndDelay =
        TimeDuration::FromMilliseconds(aEffectTiming.mEndDelay.Value());
  }
  if (aEffectTiming.mIterations.WasPassed()) {
    result.mIterations = aEffectTiming.mIterations.Value();
  }
  if (aEffectTiming.mIterationStart.WasPassed()) {
    result.mIterationStart = aEffectTiming.mIterationStart.Value();
  }
  if (aEffectTiming.mDirection.WasPassed()) {
    result.mDirection = aEffectTiming.mDirection.Value();
  }
  if (aEffectTiming.mFill.WasPassed()) {
    result.mFill = aEffectTiming.mFill.Value();
  }
  if (aEffectTiming.mEasing.WasPassed()) {
    result.mFunction = easing;
  }

  result.Update();

  return result;
}

/* static */
Maybe<StyleComputedTimingFunction> TimingParams::ParseEasing(
    const nsACString& aEasing, ErrorResult& aRv) {
  StyleComputedTimingFunction timingFunction;
  if (!ServoCSSParser::ParseEasing(aEasing, timingFunction)) {
    aRv.ThrowTypeError<dom::MSG_INVALID_EASING_ERROR>(aEasing);
    return Nothing();
  }

  if (timingFunction.IsLinearKeyword()) {
    return Nothing();
  }

  return Some(std::move(timingFunction));
}

bool TimingParams::operator==(const TimingParams& aOther) const {
  // We don't compare mActiveDuration and mEndTime because they are calculated
  // from other timing parameters.
  return mDuration == aOther.mDuration && mDelay == aOther.mDelay &&
         mEndDelay == aOther.mEndDelay && mIterations == aOther.mIterations &&
         mIterationStart == aOther.mIterationStart &&
         mDirection == aOther.mDirection && mFill == aOther.mFill &&
         mFunction == aOther.mFunction;
}

// FIXME: This is a tentative way to normalize the timing which is defined in
// [web-animations-2] [1]. I borrow this implementation and some concepts for
// the edge cases from Chromium [2] so we can match the behavior with them. The
// implementation here ignores the case of percentage of start delay, end delay,
// and duration because Gecko doesn't support them. We may have to update the
// calculation if the spec issue [3] gets any update.
//
// [1]
// https://drafts.csswg.org/web-animations-2/#time-based-animation-to-a-proportional-animation
// [2] https://chromium-review.googlesource.com/c/chromium/src/+/2992387
// [3] https://github.com/w3c/csswg-drafts/issues/4862
TimingParams TimingParams::Normalize(
    const TimeDuration& aTimelineDuration) const {
  MOZ_ASSERT(aTimelineDuration,
             "the timeline duration of scroll-timeline is always non-zero now");

  TimingParams normalizedTiming(*this);

  // Handle iteration duration value of "auto" first.
  // FIXME: Bug 1676794: Gecko doesn't support `animation-duration:auto` and we
  // don't support JS-generated scroll animations, so we don't fall into this
  // case for now. Need to check this again after we support ScrollTimeline
  // interface.
  if (!mDuration) {
    // If the iteration duration is auto, then:
    //   Set start delay and end delay to 0, as it is not possible to mix time
    //   and proportions.
    normalizedTiming.mDelay = TimeDuration();
    normalizedTiming.mEndDelay = TimeDuration();
    normalizedTiming.Update();
    return normalizedTiming;
  }

  if (mEndTime.IsZero()) {
    // mEndTime of zero causes division by zero so we handle it here.
    //
    // FIXME: The spec doesn't mention this case, so we might have to update
    // this based on the spec issue,
    // https://github.com/w3c/csswg-drafts/issues/7459.
    normalizedTiming.mDelay = TimeDuration();
    normalizedTiming.mEndDelay = TimeDuration();
    normalizedTiming.mDuration = Some(TimeDuration());
  } else if (mEndTime == TimeDuration::Forever()) {
    // The iteration count or duration may be infinite; however, start and
    // end delays are strictly finite. Thus, in the limit when end time
    // approaches infinity:
    //    start delay / end time = finite / infinite = 0
    //    end delay / end time = finite / infinite = 0
    //    iteration duration / end time = 1 / iteration count
    // This condition can be reached by switching to a scroll timeline on
    // an existing infinite duration animation.
    //
    // FIXME: The spec doesn't mention this case, so we might have to update
    // this based on the spec issue,
    // https://github.com/w3c/csswg-drafts/issues/7459.
    normalizedTiming.mDelay = TimeDuration();
    normalizedTiming.mEndDelay = TimeDuration();
    normalizedTiming.mDuration =
        Some(aTimelineDuration.MultDouble(1.0 / mIterations));
  } else {
    // Convert to percentages then multiply by the timeline duration.
    const double endTimeInSec = mEndTime.ToSeconds();
    normalizedTiming.mDelay =
        aTimelineDuration.MultDouble(mDelay.ToSeconds() / endTimeInSec);
    normalizedTiming.mEndDelay =
        aTimelineDuration.MultDouble(mEndDelay.ToSeconds() / endTimeInSec);
    normalizedTiming.mDuration = Some(StickyTimeDuration(
        aTimelineDuration.MultDouble(mDuration->ToSeconds() / endTimeInSec)));
  }

  normalizedTiming.Update();
  return normalizedTiming;
}

}  // namespace mozilla
