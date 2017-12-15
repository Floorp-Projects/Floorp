/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ComputedTimingFunction_h
#define mozilla_ComputedTimingFunction_h

#include "nsSMILKeySpline.h"  // nsSMILKeySpline
#include "nsStyleStruct.h"    // nsTimingFunction

namespace mozilla {

class ComputedTimingFunction
{
public:
  static ComputedTimingFunction
  CubicBezier(double x1, double y1, double x2, double y2)
  {
    return ComputedTimingFunction(x1, y1, x2, y2);
  }
  static ComputedTimingFunction
  Steps(nsTimingFunction::Type aType, uint32_t aSteps)
  {
    MOZ_ASSERT(aType == nsTimingFunction::Type::StepStart ||
               aType == nsTimingFunction::Type::StepEnd,
               "The type of timing function should be either step-start or "
               "step-end");
    MOZ_ASSERT(aSteps > 0, "The number of steps should be 1 or more");
    return ComputedTimingFunction(aType, aSteps);
  }
  static ComputedTimingFunction
  Frames(uint32_t aFrames)
  {
    MOZ_ASSERT(aFrames > 1, "The number of frames should be 2 or more");
    return ComputedTimingFunction(nsTimingFunction::Type::Frames, aFrames);
  }

  ComputedTimingFunction() = default;
  explicit ComputedTimingFunction(const nsTimingFunction& aFunction)
  {
    Init(aFunction);
  }
  void Init(const nsTimingFunction& aFunction);

  // BeforeFlag is used in step timing function.
  // https://drafts.csswg.org/css-timing/#before-flag
  enum class BeforeFlag {
    Unset,
    Set
  };
  double GetValue(double aPortion, BeforeFlag aBeforeFlag) const;
  const nsSMILKeySpline* GetFunction() const
  {
    NS_ASSERTION(HasSpline(), "Type mismatch");
    return &mTimingFunction;
  }
  nsTimingFunction::Type GetType() const { return mType; }
  bool HasSpline() const { return nsTimingFunction::IsSplineType(mType); }
  uint32_t GetSteps() const
  {
    MOZ_ASSERT(mType == nsTimingFunction::Type::StepStart ||
               mType == nsTimingFunction::Type::StepEnd);
    return mStepsOrFrames;
  }
  uint32_t GetFrames() const
  {
    MOZ_ASSERT(mType == nsTimingFunction::Type::Frames);
    return mStepsOrFrames;
  }
  bool operator==(const ComputedTimingFunction& aOther) const
  {
    return mType == aOther.mType &&
           (HasSpline() ?
            mTimingFunction == aOther.mTimingFunction :
            mStepsOrFrames == aOther.mStepsOrFrames);
  }
  bool operator!=(const ComputedTimingFunction& aOther) const
  {
    return !(*this == aOther);
  }
  bool operator==(const nsTimingFunction& aOther) const
  {
    return mType == aOther.mType &&
           (HasSpline()
            ? mTimingFunction.X1() == aOther.mFunc.mX1 &&
              mTimingFunction.Y1() == aOther.mFunc.mY1 &&
              mTimingFunction.X2() == aOther.mFunc.mX2 &&
              mTimingFunction.Y2() == aOther.mFunc.mY2
            : mStepsOrFrames == aOther.mStepsOrFrames);
  }
  bool operator!=(const nsTimingFunction& aOther) const
  {
    return !(*this == aOther);
  }
  int32_t Compare(const ComputedTimingFunction& aRhs) const;
  void AppendToString(nsAString& aResult) const;

  static double GetPortion(const Maybe<ComputedTimingFunction>& aFunction,
                           double aPortion,
                           BeforeFlag aBeforeFlag)
  {
    return aFunction ? aFunction->GetValue(aPortion, aBeforeFlag) : aPortion;
  }
  static int32_t Compare(const Maybe<ComputedTimingFunction>& aLhs,
                         const Maybe<ComputedTimingFunction>& aRhs);

private:
  ComputedTimingFunction(double x1, double y1, double x2, double y2)
    : mType(nsTimingFunction::Type::CubicBezier)
    , mTimingFunction(x1, y1, x2, y2) { }
  ComputedTimingFunction(nsTimingFunction::Type aType, uint32_t aStepsOrFrames)
    : mType(aType)
    , mStepsOrFrames(aStepsOrFrames) { }

  nsTimingFunction::Type mType = nsTimingFunction::Type::Linear;
  nsSMILKeySpline mTimingFunction;
  uint32_t mStepsOrFrames;
};

inline bool
operator==(const Maybe<ComputedTimingFunction>& aLHS,
           const nsTimingFunction& aRHS)
{
  if (aLHS.isNothing()) {
    return aRHS.mType == nsTimingFunction::Type::Linear;
  }
  return aLHS.value() == aRHS;
}

inline bool
operator!=(const Maybe<ComputedTimingFunction>& aLHS,
           const nsTimingFunction& aRHS)
{
  return !(aLHS == aRHS);
}

} // namespace mozilla

#endif // mozilla_ComputedTimingFunction_h
