/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TimingParams_h
#define mozilla_TimingParams_h

#include "X11UndefineNone.h"
#include "nsPrintfCString.h"
#include "nsStringFwd.h"
#include "nsPrintfCString.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/UnionTypes.h"  // For OwningUnrestrictedDoubleOrString
#include "mozilla/ComputedTimingFunction.h"
#include "mozilla/Maybe.h"
#include "mozilla/StickyTimeDuration.h"
#include "mozilla/TimeStamp.h"  // for TimeDuration

#include "mozilla/dom/AnimationEffectBinding.h"  // for FillMode
                                                 // and PlaybackDirection

#define PROGRESS_TIMELINE_DURATION_MILLISEC 100000

namespace mozilla {

namespace dom {
class UnrestrictedDoubleOrKeyframeEffectOptions;
class UnrestrictedDoubleOrKeyframeAnimationOptions;
}  // namespace dom

struct TimingParams {
  TimingParams() = default;

  TimingParams(float aDuration, float aDelay, float aIterationCount,
               dom::PlaybackDirection aDirection, dom::FillMode aFillMode)
      : mIterations(aIterationCount), mDirection(aDirection), mFill(aFillMode) {
    mDuration.emplace(StickyTimeDuration::FromMilliseconds(aDuration));
    mDelay = TimeDuration::FromMilliseconds(aDelay);
    Update();
  }

  TimingParams(const TimeDuration& aDuration, const TimeDuration& aDelay,
               const TimeDuration& aEndDelay, float aIterations,
               float aIterationStart, dom::PlaybackDirection aDirection,
               dom::FillMode aFillMode,
               Maybe<ComputedTimingFunction>&& aFunction)
      : mDelay(aDelay),
        mEndDelay(aEndDelay),
        mIterations(aIterations),
        mIterationStart(aIterationStart),
        mDirection(aDirection),
        mFill(aFillMode),
        mFunction(aFunction) {
    mDuration.emplace(aDuration);
    Update();
  }

  template <class OptionsType>
  static TimingParams FromOptionsType(const OptionsType& aOptions,
                                      ErrorResult& aRv);
  static TimingParams FromOptionsUnion(
      const dom::UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
      ErrorResult& aRv);
  static TimingParams FromOptionsUnion(
      const dom::UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
      ErrorResult& aRv);
  static TimingParams FromEffectTiming(const dom::EffectTiming& aEffectTiming,
                                       ErrorResult& aRv);
  // Returns a copy of |aSource| where each timing property in |aSource| that
  // is also specified in |aEffectTiming| is replaced with the value from
  // |aEffectTiming|.
  //
  // If any of the values in |aEffectTiming| are invalid, |aRv.Failed()| will be
  // true and an unmodified copy of |aSource| will be returned.
  static TimingParams MergeOptionalEffectTiming(
      const TimingParams& aSource,
      const dom::OptionalEffectTiming& aEffectTiming, ErrorResult& aRv);

  // Range-checks and validates an UnrestrictedDoubleOrString or
  // OwningUnrestrictedDoubleOrString object and converts to a
  // StickyTimeDuration value or Nothing() if aDuration is "auto".
  // Caller must check aRv.Failed().
  template <class DoubleOrString>
  static Maybe<StickyTimeDuration> ParseDuration(DoubleOrString& aDuration,
                                                 ErrorResult& aRv) {
    Maybe<StickyTimeDuration> result;
    if (aDuration.IsUnrestrictedDouble()) {
      double durationInMs = aDuration.GetAsUnrestrictedDouble();
      if (durationInMs >= 0) {
        result.emplace(StickyTimeDuration::FromMilliseconds(durationInMs));
      } else {
        nsPrintfCString err("Duration (%g) must be nonnegative", durationInMs);
        aRv.ThrowTypeError(err);
      }
    } else if (!aDuration.GetAsString().EqualsLiteral("auto")) {
      aRv.ThrowTypeError<dom::MSG_INVALID_DURATION_ERROR>(
          NS_ConvertUTF16toUTF8(aDuration.GetAsString()));
    }
    return result;
  }

  static void ValidateIterationStart(double aIterationStart, ErrorResult& aRv) {
    if (aIterationStart < 0) {
      nsPrintfCString err("Iteration start (%g) must not be negative",
                          aIterationStart);
      aRv.ThrowTypeError(err);
    }
  }

  static void ValidateIterations(double aIterations, ErrorResult& aRv) {
    if (IsNaN(aIterations)) {
      aRv.ThrowTypeError("Iterations must not be NaN");
      return;
    }

    if (aIterations < 0) {
      nsPrintfCString err("Iterations (%g) must not be negative", aIterations);
      aRv.ThrowTypeError(err);
    }
  }

  static Maybe<ComputedTimingFunction> ParseEasing(const nsACString& aEasing,
                                                   ErrorResult& aRv);

  static StickyTimeDuration CalcActiveDuration(
      const Maybe<StickyTimeDuration>& aDuration, double aIterations) {
    // If either the iteration duration or iteration count is zero,
    // Web Animations says that the active duration is zero. This is to
    // ensure that the result is defined when the other argument is Infinity.
    static const StickyTimeDuration zeroDuration;
    if (!aDuration || aDuration->IsZero() || aIterations == 0.0) {
      return zeroDuration;
    }

    return aDuration->MultDouble(aIterations);
  }
  // Return the duration of the active interval calculated by duration and
  // iteration count.
  StickyTimeDuration ActiveDuration() const {
    MOZ_ASSERT(CalcActiveDuration(mDuration, mIterations) == mActiveDuration,
               "Cached value of active duration should be up to date");
    return mActiveDuration;
  }

  StickyTimeDuration EndTime() const {
    MOZ_ASSERT(mEndTime == std::max(mDelay + ActiveDuration() + mEndDelay,
                                    StickyTimeDuration()),
               "Cached value of end time should be up to date");
    return mEndTime;
  }

  bool operator==(const TimingParams& aOther) const;
  bool operator!=(const TimingParams& aOther) const {
    return !(*this == aOther);
  }

  void SetDuration(Maybe<StickyTimeDuration>&& aDuration) {
    mDuration = std::move(aDuration);
    Update();
  }
  void SetDuration(const Maybe<StickyTimeDuration>& aDuration) {
    mDuration = aDuration;
    Update();
  }
  const Maybe<StickyTimeDuration>& Duration() const { return mDuration; }

  void SetDelay(const TimeDuration& aDelay) {
    mDelay = aDelay;
    Update();
  }
  const TimeDuration& Delay() const { return mDelay; }

  void SetEndDelay(const TimeDuration& aEndDelay) {
    mEndDelay = aEndDelay;
    Update();
  }
  const TimeDuration& EndDelay() const { return mEndDelay; }

  void SetIterations(double aIterations) {
    mIterations = aIterations;
    Update();
  }
  double Iterations() const { return mIterations; }

  void SetIterationStart(double aIterationStart) {
    mIterationStart = aIterationStart;
  }
  double IterationStart() const { return mIterationStart; }

  void SetDirection(dom::PlaybackDirection aDirection) {
    mDirection = aDirection;
  }
  dom::PlaybackDirection Direction() const { return mDirection; }

  void SetFill(dom::FillMode aFill) { mFill = aFill; }
  dom::FillMode Fill() const { return mFill; }

  void SetTimingFunction(Maybe<ComputedTimingFunction>&& aFunction) {
    mFunction = std::move(aFunction);
  }
  const Maybe<ComputedTimingFunction>& TimingFunction() const {
    return mFunction;
  }

  void Normalize();

 private:
  void Update() {
    mActiveDuration = CalcActiveDuration(mDuration, mIterations);

    mEndTime =
        std::max(mDelay + mActiveDuration + mEndDelay, StickyTimeDuration());
  }

  // mDuration.isNothing() represents the "auto" value
  Maybe<StickyTimeDuration> mDuration;
  TimeDuration mDelay;  // Initializes to zero
  TimeDuration mEndDelay;
  double mIterations = 1.0;  // Can be NaN, negative, +/-Infinity
  double mIterationStart = 0.0;
  dom::PlaybackDirection mDirection = dom::PlaybackDirection::Normal;
  dom::FillMode mFill = dom::FillMode::Auto;
  Maybe<ComputedTimingFunction> mFunction;
  StickyTimeDuration mActiveDuration = StickyTimeDuration();
  StickyTimeDuration mEndTime = StickyTimeDuration();
};

}  // namespace mozilla

#endif  // mozilla_TimingParams_h
