/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TimingParams_h
#define mozilla_TimingParams_h

#include "nsStringFwd.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/UnionTypes.h" // For OwningUnrestrictedDoubleOrString
#include "mozilla/ComputedTimingFunction.h"
#include "mozilla/Maybe.h"
#include "mozilla/StickyTimeDuration.h"
#include "mozilla/TimeStamp.h" // for TimeDuration

// X11 has a #define for None
#ifdef None
#undef None
#endif
#include "mozilla/dom/AnimationEffectReadOnlyBinding.h" // for FillMode
                                                        // and PlaybackDirection

class nsIDocument;

namespace mozilla {

namespace dom {
class UnrestrictedDoubleOrKeyframeEffectOptions;
class UnrestrictedDoubleOrKeyframeAnimationOptions;
}

struct TimingParams
{
  TimingParams() = default;

  static TimingParams FromOptionsUnion(
    const dom::UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
    nsIDocument* aDocument, ErrorResult& aRv);
  static TimingParams FromOptionsUnion(
    const dom::UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
    nsIDocument* aDocument, ErrorResult& aRv);

  // Range-checks and validates an UnrestrictedDoubleOrString or
  // OwningUnrestrictedDoubleOrString object and converts to a
  // StickyTimeDuration value or Nothing() if aDuration is "auto".
  // Caller must check aRv.Failed().
  template <class DoubleOrString>
  static Maybe<StickyTimeDuration> ParseDuration(DoubleOrString& aDuration,
                                                 ErrorResult& aRv)
  {
    Maybe<StickyTimeDuration> result;
    if (aDuration.IsUnrestrictedDouble()) {
      double durationInMs = aDuration.GetAsUnrestrictedDouble();
      if (durationInMs >= 0) {
        result.emplace(StickyTimeDuration::FromMilliseconds(durationInMs));
      } else {
        aRv.ThrowTypeError<dom::MSG_ENFORCE_RANGE_OUT_OF_RANGE>(
          NS_LITERAL_STRING("duration"));
      }
    } else if (!aDuration.GetAsString().EqualsLiteral("auto")) {
      aRv.ThrowTypeError<dom::MSG_INVALID_DURATION_ERROR>();
    }
    return result;
  }

  static void ValidateIterationStart(double aIterationStart,
                                     ErrorResult& aRv)
  {
    if (aIterationStart < 0) {
      aRv.ThrowTypeError<dom::MSG_ENFORCE_RANGE_OUT_OF_RANGE>(
        NS_LITERAL_STRING("iterationStart"));
    }
  }

  static void ValidateIterations(double aIterations, ErrorResult& aRv)
  {
    if (IsNaN(aIterations) || aIterations < 0) {
      aRv.ThrowTypeError<dom::MSG_ENFORCE_RANGE_OUT_OF_RANGE>(
        NS_LITERAL_STRING("iterations"));
    }
  }

  static Maybe<ComputedTimingFunction> ParseEasing(const nsAString& aEasing,
                                                   nsIDocument* aDocument,
                                                   ErrorResult& aRv);

  // mDuration.isNothing() represents the "auto" value
  Maybe<StickyTimeDuration> mDuration;
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
