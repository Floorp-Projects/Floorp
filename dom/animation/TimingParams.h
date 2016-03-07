/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TimingParams_h
#define mozilla_TimingParams_h

#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/UnionTypes.h" // For OwningUnrestrictedDoubleOrString
#include "mozilla/ComputedTimingFunction.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h" // for TimeDuration

// X11 has a #define for None
#ifdef None
#undef None
#endif
#include "mozilla/dom/AnimationEffectReadOnlyBinding.h"  // for FillMode
                                                         // and PlaybackDirection

namespace mozilla {

namespace dom {
struct AnimationEffectTimingProperties;
class Element;
class UnrestrictedDoubleOrKeyframeEffectOptions;
class UnrestrictedDoubleOrKeyframeAnimationOptions;
class ElementOrCSSPseudoElement;
}

struct TimingParams
{
  TimingParams() = default;
  TimingParams(const dom::AnimationEffectTimingProperties& aTimingProperties,
               const dom::Element* aTarget);
  explicit TimingParams(double aDuration);

  static TimingParams FromOptionsUnion(
    const dom::UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
    const Nullable<dom::ElementOrCSSPseudoElement>& aTarget);
  static TimingParams FromOptionsUnion(
    const dom::UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
    const Nullable<dom::ElementOrCSSPseudoElement>& aTarget);

  // The unitialized state of mDuration represents "auto".
  // Bug 1237173: We will replace this with Maybe<TimeDuration>.
  dom::OwningUnrestrictedDoubleOrString mDuration;
  TimeDuration mDelay;      // Initializes to zero
  TimeDuration mEndDelay;
  double mIterations = 1.0; // Can be NaN, negative, +/-Infinity
  double mIterationStart = 0.0;
  dom::PlaybackDirection mDirection = dom::PlaybackDirection::Normal;
  dom::FillMode mFill = dom::FillMode::Auto;
  Maybe<ComputedTimingFunction> mFunction;

  bool operator==(const TimingParams& aOther) const;
  bool operator!=(const TimingParams& aOther) const
  {
    return !(*this == aOther);
  }
};

} // namespace mozilla

#endif // mozilla_TimingParams_h
