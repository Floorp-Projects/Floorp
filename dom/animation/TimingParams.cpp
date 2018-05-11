/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TimingParams.h"

#include "mozilla/AnimationUtils.h"
#include "mozilla/dom/AnimatableBinding.h"
#include "mozilla/dom/KeyframeAnimationOptionsBinding.h"
#include "mozilla/dom/KeyframeEffectBinding.h"
#include "mozilla/ServoCSSParser.h"
#include "nsIDocument.h"

namespace mozilla {

template <class OptionsType>
static const dom::EffectTiming&
GetTimingProperties(const OptionsType& aOptions);

template <>
/* static */ const dom::EffectTiming&
GetTimingProperties(
  const dom::UnrestrictedDoubleOrKeyframeEffectOptions& aOptions)
{
  MOZ_ASSERT(aOptions.IsKeyframeEffectOptions());
  return aOptions.GetAsKeyframeEffectOptions();
}

template <>
/* static */ const dom::EffectTiming&
GetTimingProperties(
  const dom::UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions)
{
  MOZ_ASSERT(aOptions.IsKeyframeAnimationOptions());
  return aOptions.GetAsKeyframeAnimationOptions();
}

template <class OptionsType>
/* static */ TimingParams
TimingParams::FromOptionsType(const OptionsType& aOptions,
                              nsIDocument* aDocument,
                              ErrorResult& aRv)
{
  TimingParams result;

  if (aOptions.IsUnrestrictedDouble()) {
    double durationInMs = aOptions.GetAsUnrestrictedDouble();
    if (durationInMs >= 0) {
      result.mDuration.emplace(
        StickyTimeDuration::FromMilliseconds(durationInMs));
    } else {
      aRv.Throw(NS_ERROR_DOM_TYPE_ERR);
      return result;
    }
    result.Update();
  } else {
    const dom::EffectTiming& timing = GetTimingProperties(aOptions);
    result = FromEffectTiming(timing, aDocument, aRv);
  }

  return result;
}

/* static */ TimingParams
TimingParams::FromOptionsUnion(
  const dom::UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
  nsIDocument* aDocument,
  ErrorResult& aRv)
{
  return FromOptionsType(aOptions, aDocument, aRv);
}

/* static */ TimingParams
TimingParams::FromOptionsUnion(
  const dom::UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
  nsIDocument* aDocument,
  ErrorResult& aRv)
{
  return FromOptionsType(aOptions, aDocument, aRv);
}

/* static */ TimingParams
TimingParams::FromEffectTiming(const dom::EffectTiming& aEffectTiming,
                               nsIDocument* aDocument,
                               ErrorResult& aRv)
{
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
    TimingParams::ParseEasing(aEffectTiming.mEasing, aDocument, aRv);
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

/* static */ TimingParams
TimingParams::MergeOptionalEffectTiming(
  const TimingParams& aSource,
  const dom::OptionalEffectTiming& aEffectTiming,
  nsIDocument* aDocument,
  ErrorResult& aRv)
{
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
    easing =
      TimingParams::ParseEasing(aEffectTiming.mEasing.Value(), aDocument, aRv);
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

/* static */ Maybe<ComputedTimingFunction>
TimingParams::ParseEasing(const nsAString& aEasing,
                          nsIDocument* aDocument,
                          ErrorResult& aRv)
{
  MOZ_ASSERT(aDocument);

  nsTimingFunction timingFunction;
  RefPtr<URLExtraData> url = ServoCSSParser::GetURLExtraData(aDocument);
  if (!ServoCSSParser::ParseEasing(aEasing, url, timingFunction)) {
    aRv.ThrowTypeError<dom::MSG_INVALID_EASING_ERROR>(aEasing);
    return Nothing();
  }

  if (timingFunction.mType == nsTimingFunction::Type::Linear) {
    return Nothing();
  }

  return Some(ComputedTimingFunction(timingFunction));
}

bool
TimingParams::operator==(const TimingParams& aOther) const
{
  // We don't compare mActiveDuration and mEndTime because they are calculated
  // from other timing parameters.
  return mDuration == aOther.mDuration &&
         mDelay == aOther.mDelay &&
         mEndDelay == aOther.mEndDelay &&
         mIterations == aOther.mIterations &&
         mIterationStart == aOther.mIterationStart &&
         mDirection == aOther.mDirection &&
         mFill == aOther.mFill &&
         mFunction == aOther.mFunction;
}

} // namespace mozilla
