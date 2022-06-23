/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ComputedTimingFunction_h
#define mozilla_ComputedTimingFunction_h

#include "nsDebug.h"
#include "nsStringFwd.h"
#include "nsTimingFunction.h"

#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/SMILKeySpline.h"
#include "mozilla/Variant.h"

namespace mozilla {

class ComputedTimingFunction {
 public:
  explicit ComputedTimingFunction(const nsTimingFunction& aFunction);
  explicit ComputedTimingFunction(const StyleComputedTimingFunction& aFunction);

  static StyleComputedTimingFunction ToStyleComputedTimingFunction(
      const ComputedTimingFunction& aComputedTimingFunction);

  // BeforeFlag is used in step timing function.
  // https://drafts.csswg.org/css-easing/#before-flag
  enum class BeforeFlag { Unset, Set };
  double GetValue(double aPortion, BeforeFlag aBeforeFlag) const;
  bool operator==(const ComputedTimingFunction& aOther) const {
    return mFunction == aOther.mFunction;
  }
  bool operator!=(const ComputedTimingFunction& aOther) const {
    return !(*this == aOther);
  }
  bool operator==(const nsTimingFunction& aOther) const {
    return ToStyleComputedTimingFunction(*this) == aOther.mTiming;
  }
  bool operator!=(const nsTimingFunction& aOther) const {
    return !(*this == aOther);
  }
  void AppendToString(nsACString& aResult) const;

  static double GetPortion(const Maybe<ComputedTimingFunction>& aFunction,
                           double aPortion, BeforeFlag aBeforeFlag) {
    return aFunction ? aFunction->GetValue(aPortion, aBeforeFlag) : aPortion;
  }

 private:
  struct StepFunc {
    uint32_t mSteps = 1;
    StyleStepPosition mPos = StyleStepPosition::End;
    constexpr StepFunc() = default;
    constexpr StepFunc(uint32_t aSteps, StyleStepPosition aPos)
        : mSteps(aSteps), mPos(aPos){};
    bool operator==(const StepFunc& aOther) const {
      return mSteps == aOther.mSteps && mPos == aOther.mPos;
    }
  };

  struct KeywordFunction {
    KeywordFunction(mozilla::StyleTimingKeyword aKeyword,
                    SMILKeySpline aFunction)
        : mKeyword{aKeyword}, mFunction{aFunction} {}

    bool operator==(const KeywordFunction& aOther) const {
      return mKeyword == aOther.mKeyword && mFunction == aOther.mFunction;
    }
    mozilla::StyleTimingKeyword mKeyword;
    SMILKeySpline mFunction;
  };

  using Function = mozilla::Variant<KeywordFunction, SMILKeySpline, StepFunc,
                                    StylePiecewiseLinearFunction>;

  static Function ConstructFunction(
      const StyleComputedTimingFunction& aFunction);
  ComputedTimingFunction(double x1, double y1, double x2, double y2)
      : mFunction{AsVariant(SMILKeySpline{x1, y1, x2, y2})} {}
  ComputedTimingFunction(uint32_t aSteps, StyleStepPosition aPos)
      : mFunction{AsVariant(StepFunc{aSteps, aPos})} {}
  explicit ComputedTimingFunction(StylePiecewiseLinearFunction aFunction)
      : mFunction{AsVariant(std::move(aFunction))} {}
  static double StepTiming(const StepFunc& aStepFunc, double aPortion,
                           BeforeFlag aBeforeFlag);
  Function mFunction;
};

inline bool operator==(const Maybe<ComputedTimingFunction>& aLHS,
                       const nsTimingFunction& aRHS) {
  if (aLHS.isNothing()) {
    return aRHS.IsLinear();
  }
  return aLHS.value() == aRHS;
}

inline bool operator!=(const Maybe<ComputedTimingFunction>& aLHS,
                       const nsTimingFunction& aRHS) {
  return !(aLHS == aRHS);
}

}  // namespace mozilla

#endif  // mozilla_ComputedTimingFunction_h
