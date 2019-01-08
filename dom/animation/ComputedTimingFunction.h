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

namespace mozilla {

class ComputedTimingFunction {
 public:
  enum class Type : uint8_t {
    Ease = uint8_t(StyleTimingKeyword::Ease),            // ease
    Linear = uint8_t(StyleTimingKeyword::Linear),        // linear
    EaseIn = uint8_t(StyleTimingKeyword::EaseIn),        // ease-in
    EaseOut = uint8_t(StyleTimingKeyword::EaseOut),      // ease-out
    EaseInOut = uint8_t(StyleTimingKeyword::EaseInOut),  // ease-in-out
    CubicBezier,                                         // cubic-bezier()
    Step,  // step-start | step-end | steps()
  };

  struct StepFunc {
    uint32_t mSteps;
    StyleStepPosition mPos;
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

  ComputedTimingFunction() = default;
  explicit ComputedTimingFunction(const nsTimingFunction& aFunction) {
    Init(aFunction);
  }
  void Init(const nsTimingFunction& aFunction);

  // BeforeFlag is used in step timing function.
  // https://drafts.csswg.org/css-easing/#before-flag
  enum class BeforeFlag { Unset, Set };
  double GetValue(double aPortion, BeforeFlag aBeforeFlag) const;
  const SMILKeySpline* GetFunction() const {
    NS_ASSERTION(HasSpline(), "Type mismatch");
    return &mTimingFunction;
  }
  Type GetType() const { return mType; }
  bool HasSpline() const { return mType != Type::Step; }
  const StepFunc& GetSteps() const {
    MOZ_ASSERT(mType == Type::Step);
    return mSteps;
  }
  bool operator==(const ComputedTimingFunction& aOther) const {
    return mType == aOther.mType &&
           (HasSpline() ? mTimingFunction == aOther.mTimingFunction
                        : mSteps == aOther.mSteps);
  }
  bool operator!=(const ComputedTimingFunction& aOther) const {
    return !(*this == aOther);
  }
  bool operator==(const nsTimingFunction& aOther) const {
    switch (aOther.mTiming.tag) {
      case StyleComputedTimingFunction::Tag::Keyword:
        return uint8_t(mType) == uint8_t(aOther.mTiming.keyword._0);
      case StyleComputedTimingFunction::Tag::CubicBezier:
        return mTimingFunction.X1() == aOther.mTiming.cubic_bezier.x1 &&
               mTimingFunction.Y1() == aOther.mTiming.cubic_bezier.y1 &&
               mTimingFunction.X2() == aOther.mTiming.cubic_bezier.x2 &&
               mTimingFunction.Y2() == aOther.mTiming.cubic_bezier.y2;
      case StyleComputedTimingFunction::Tag::Steps:
        return mSteps.mSteps == uint32_t(aOther.mTiming.steps._0) &&
               mSteps.mPos == aOther.mTiming.steps._1;
      default:
        return false;
    }
  }
  bool operator!=(const nsTimingFunction& aOther) const {
    return !(*this == aOther);
  }
  int32_t Compare(const ComputedTimingFunction& aRhs) const;
  void AppendToString(nsAString& aResult) const;

  static double GetPortion(const Maybe<ComputedTimingFunction>& aFunction,
                           double aPortion, BeforeFlag aBeforeFlag) {
    return aFunction ? aFunction->GetValue(aPortion, aBeforeFlag) : aPortion;
  }
  static int32_t Compare(const Maybe<ComputedTimingFunction>& aLhs,
                         const Maybe<ComputedTimingFunction>& aRhs);

 private:
  ComputedTimingFunction(double x1, double y1, double x2, double y2)
      : mType(Type::CubicBezier), mTimingFunction(x1, y1, x2, y2) {}
  ComputedTimingFunction(uint32_t aSteps, StyleStepPosition aPos)
      : mType(Type::Step), mSteps{aSteps, aPos} {}

  Type mType;
  SMILKeySpline mTimingFunction;
  StepFunc mSteps;
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
