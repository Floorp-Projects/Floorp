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
  Maybe<ComputedTimingFunction> easing =
      TimingParams::ParseEasing(aEffectTiming.mEasing, aRv);
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
  result.mFunction = easing;

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

  Maybe<ComputedTimingFunction> easing;
  if (aEffectTiming.mEasing.WasPassed()) {
    easing = TimingParams::ParseEasing(aEffectTiming.mEasing.Value(), aRv);
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
Maybe<ComputedTimingFunction> TimingParams::ParseEasing(
    const nsACString& aEasing, ErrorResult& aRv) {
  nsTimingFunction timingFunction;
  if (!ServoCSSParser::ParseEasing(aEasing, timingFunction)) {
    aRv.ThrowTypeError<dom::MSG_INVALID_EASING_ERROR>(aEasing);
    return Nothing();
  }

  if (timingFunction.IsLinear()) {
    return Nothing();
  }

  return Some(ComputedTimingFunction(timingFunction));
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

void TimingParams::Normalize() {
  // FIXME: Bug 1775327, do normalization, instead of using these magic numbers.
  mDuration = Some(StickyTimeDuration::FromMilliseconds(
      PROGRESS_TIMELINE_DURATION_MILLISEC));
  mDelay = TimeDuration::FromMilliseconds(0);
  mIterations = std::numeric_limits<double>::infinity();
  mDirection = dom::PlaybackDirection::Alternate;
  mFill = dom::FillMode::Both;

  Update();
}

}  // namespace mozilla
