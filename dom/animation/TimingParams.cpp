/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TimingParams.h"

namespace mozilla {

TimingParams::TimingParams(const dom::AnimationEffectTimingProperties& aRhs,
                           const dom::Element* aTarget)
  : mDelay(TimeDuration::FromMilliseconds(aRhs.mDelay))
  , mEndDelay(TimeDuration::FromMilliseconds(aRhs.mEndDelay))
  , mIterations(aRhs.mIterations)
  , mIterationStart(aRhs.mIterationStart)
  , mDirection(aRhs.mDirection)
  , mFill(aRhs.mFill)
{
  if (aRhs.mDuration.IsUnrestrictedDouble()) {
    mDuration.emplace(StickyTimeDuration::FromMilliseconds(
                        aRhs.mDuration.GetAsUnrestrictedDouble()));
  }
  mFunction = AnimationUtils::ParseEasing(aTarget, aRhs.mEasing);
}

TimingParams::TimingParams(double aDuration)
{
  mDuration.emplace(StickyTimeDuration::FromMilliseconds(aDuration));
}

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
  const Nullable<dom::ElementOrCSSPseudoElement>& aTarget)
{
  if (aOptions.IsUnrestrictedDouble()) {
    return TimingParams(aOptions.GetAsUnrestrictedDouble());
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
    return TimingParams(GetTimingProperties(aOptions), targetElement);
  }
}

/* static */ TimingParams
TimingParams::FromOptionsUnion(
  const dom::UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
  const Nullable<dom::ElementOrCSSPseudoElement>& aTarget)
{
  return TimingParamsFromOptionsUnion(aOptions, aTarget);
}

/* static */ TimingParams
TimingParams::FromOptionsUnion(
  const dom::UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
  const Nullable<dom::ElementOrCSSPseudoElement>& aTarget)
{
  return TimingParamsFromOptionsUnion(aOptions, aTarget);
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
