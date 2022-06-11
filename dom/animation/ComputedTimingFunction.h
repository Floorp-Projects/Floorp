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

namespace layers {
class TimingFunction;
}

class ComputedTimingFunction {
 public:
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

  static ComputedTimingFunction CubicBezier(double x1, double y1, double x2,
                                            double y2) {
    return ComputedTimingFunction(x1, y1, x2, y2);
  }
  static ComputedTimingFunction Steps(uint32_t aSteps, StyleStepPosition aPos) {
    MOZ_ASSERT(aSteps > 0, "The number of steps should be 1 or more");
    return ComputedTimingFunction(aSteps, aPos);
  }

  static Maybe<ComputedTimingFunction> FromLayersTimingFunction(
      const layers::TimingFunction& aTimingFunction);
  static layers::TimingFunction ToLayersTimingFunction(
      const Maybe<ComputedTimingFunction>& aComputedTimingFunction);

  explicit ComputedTimingFunction(const nsTimingFunction& aFunction);

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
    return mFunction.match(
        [&aOther](const KeywordFunction& aFunction) {
          return aOther.mTiming.tag ==
                     StyleComputedTimingFunction::Tag::Keyword &&
                 aFunction.mKeyword == aOther.mTiming.keyword._0;
        },
        [&aOther](const SMILKeySpline& aFunction) {
          return aOther.mTiming.tag ==
                     StyleComputedTimingFunction::Tag::CubicBezier &&
                 aFunction.X1() == aOther.mTiming.cubic_bezier.x1 &&
                 aFunction.Y1() == aOther.mTiming.cubic_bezier.y1 &&
                 aFunction.X2() == aOther.mTiming.cubic_bezier.x2 &&
                 aFunction.Y2() == aOther.mTiming.cubic_bezier.y2;
        },
        [&aOther](const StepFunc& aFunction) {
          return aOther.mTiming.tag ==
                     StyleComputedTimingFunction::Tag::Steps &&
                 aFunction.mSteps == uint32_t(aOther.mTiming.steps._0) &&
                 aFunction.mPos == aOther.mTiming.steps._1;
        },
        [&aOther](const StylePiecewiseLinearFunction& aFunction) {
          if (aOther.mTiming.tag !=
              StyleComputedTimingFunction::Tag::LinearFunction) {
            return false;
          }
          StylePiecewiseLinearFunction other;
          // TODO(dshin, bug 1773493): Having to go back and forth isn't ideal.
          Servo_CreatePiecewiseLinearFunction(
              &aOther.mTiming.linear_function._0, &other);
          return aFunction.entries == other.entries;
        });
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

  static Function ConstructFunction(const nsTimingFunction& aFunction);
  ComputedTimingFunction(double x1, double y1, double x2, double y2)
      : mFunction{AsVariant(SMILKeySpline{x1, y1, x2, y2})} {}
  ComputedTimingFunction(uint32_t aSteps, StyleStepPosition aPos)
      : mFunction{AsVariant(StepFunc{aSteps, aPos})} {}
  explicit ComputedTimingFunction(StylePiecewiseLinearFunction aFunction)
      : mFunction{AsVariant(std::move(aFunction))} {}

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
