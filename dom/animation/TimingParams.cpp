/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TimingParams.h"

namespace mozilla {

template <class OptionsType>
static const dom::AnimationEffectTimingProperties&
GetTimingProperties(const OptionsType& aOptions);

template <>
/* static */ const dom::AnimationEffectTimingProperties&
GetTimingProperties(
  const dom::UnrestrictedDoubleOrKeyframeEffectOptions& aOptions)
{
  MOZ_ASSERT(aOptions.IsKeyframeEffectOptions());
  return aOptions.GetAsKeyframeEffectOptions();
}

template <>
/* static */ const dom::AnimationEffectTimingProperties&
GetTimingProperties(
  const dom::UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions)
{
  MOZ_ASSERT(aOptions.IsKeyframeAnimationOptions());
  return aOptions.GetAsKeyframeAnimationOptions();
}

template <class OptionsType>
static TimingParams
TimingParamsFromOptionsUnion(
  const OptionsType& aOptions,
  const Nullable<dom::ElementOrCSSPseudoElement>& aTarget,
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
    }
  } else {
    // If aTarget is a pseudo element, we pass its parent element because
    // TimingParams only needs its owner doc to parse easing and both pseudo
    // element and its parent element should have the same owner doc.
    // Bug 1246320: Avoid passing the element for parsing the timing function
    RefPtr<dom::Element> targetElement;
    if (!aTarget.IsNull()) {
      const dom::ElementOrCSSPseudoElement& target = aTarget.Value();
      MOZ_ASSERT(target.IsElement() || target.IsCSSPseudoElement(),
                 "Uninitialized target");
      if (target.IsElement()) {
        targetElement = &target.GetAsElement();
      } else {
        targetElement = target.GetAsCSSPseudoElement().ParentElement();
      }
    }
    const dom::AnimationEffectTimingProperties& timing =
      GetTimingProperties(aOptions);

    Maybe<StickyTimeDuration> duration =
      TimingParams::ParseDuration(timing.mDuration, aRv);
    if (aRv.Failed()) {
      return result;
    }
    TimingParams::ValidateIterationStart(timing.mIterationStart, aRv);
    if (aRv.Failed()) {
      return result;
    }

    result.mDuration = duration;
    result.mDelay = TimeDuration::FromMilliseconds(timing.mDelay);
    result.mEndDelay = TimeDuration::FromMilliseconds(timing.mEndDelay);
    result.mIterations = timing.mIterations;
    result.mIterationStart = timing.mIterationStart;
    result.mDirection = timing.mDirection;
    result.mFill = timing.mFill;
    result.mFunction =
      AnimationUtils::ParseEasing(timing.mEasing, targetElement->OwnerDoc());
  }
  return result;
}

/* static */ TimingParams
TimingParams::FromOptionsUnion(
  const dom::UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
  const Nullable<dom::ElementOrCSSPseudoElement>& aTarget,
  ErrorResult& aRv)
{
  return TimingParamsFromOptionsUnion(aOptions, aTarget, aRv);
}

/* static */ TimingParams
TimingParams::FromOptionsUnion(
  const dom::UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
  const Nullable<dom::ElementOrCSSPseudoElement>& aTarget,
  ErrorResult& aRv)
{
  return TimingParamsFromOptionsUnion(aOptions, aTarget, aRv);
}

bool
TimingParams::operator==(const TimingParams& aOther) const
{
  return mDuration == aOther.mDuration &&
         mDelay == aOther.mDelay &&
         mIterations == aOther.mIterations &&
         mIterationStart == aOther.mIterationStart &&
         mDirection == aOther.mDirection &&
         mFill == aOther.mFill &&
         mFunction == aOther.mFunction;
}

} // namespace mozilla
