/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ComputedTimingFunction.h"
#include "nsStyleUtil.h"

namespace mozilla {

void
ComputedTimingFunction::Init(const nsTimingFunction &aFunction)
{
  mType = aFunction.mType;
  if (nsTimingFunction::IsSplineType(mType)) {
    mTimingFunction.Init(aFunction.mFunc.mX1, aFunction.mFunc.mY1,
                         aFunction.mFunc.mX2, aFunction.mFunc.mY2);
  } else {
    mSteps = aFunction.mSteps;
    mStepSyntax = aFunction.mStepSyntax;
  }
}

static inline double
StepEnd(uint32_t aSteps, double aPortion)
{
  MOZ_ASSERT(0.0 <= aPortion && aPortion <= 1.0, "out of range");
  uint32_t step = uint32_t(aPortion * aSteps); // floor
  return double(step) / double(aSteps);
}

double
ComputedTimingFunction::GetValue(double aPortion) const
{
  if (HasSpline()) {
    return mTimingFunction.GetSplineValue(aPortion);
  }
  if (mType == nsTimingFunction::Type::StepStart) {
    // There are diagrams in the spec that seem to suggest this check
    // and the bounds point should not be symmetric with StepEnd, but
    // should actually step up at rather than immediately after the
    // fraction points.  However, we rely on rounding negative values
    // up to zero, so we can't do that.  And it's not clear the spec
    // really meant it.
    return 1.0 - StepEnd(mSteps, 1.0 - aPortion);
  }
  MOZ_ASSERT(mType == nsTimingFunction::Type::StepEnd, "bad type");
  return StepEnd(mSteps, aPortion);
}

int32_t
ComputedTimingFunction::Compare(const ComputedTimingFunction& aRhs) const
{
  if (mType != aRhs.mType) {
    return int32_t(mType) - int32_t(aRhs.mType);
  }

  if (mType == nsTimingFunction::Type::CubicBezier) {
    int32_t order = mTimingFunction.Compare(aRhs.mTimingFunction);
    if (order != 0) {
      return order;
    }
  } else if (mType == nsTimingFunction::Type::StepStart ||
             mType == nsTimingFunction::Type::StepEnd) {
    if (mSteps != aRhs.mSteps) {
      return int32_t(mSteps) - int32_t(aRhs.mSteps);
    }
    if (mStepSyntax != aRhs.mStepSyntax) {
      return int32_t(mStepSyntax) - int32_t(aRhs.mStepSyntax);
    }
  }

  return 0;
}

void
ComputedTimingFunction::AppendToString(nsAString& aResult) const
{
  switch (mType) {
    case nsTimingFunction::Type::CubicBezier:
      nsStyleUtil::AppendCubicBezierTimingFunction(mTimingFunction.X1(),
                                                   mTimingFunction.Y1(),
                                                   mTimingFunction.X2(),
                                                   mTimingFunction.Y2(),
                                                   aResult);
      break;
    case nsTimingFunction::Type::StepStart:
    case nsTimingFunction::Type::StepEnd:
      nsStyleUtil::AppendStepsTimingFunction(mType, mSteps, mStepSyntax,
                                             aResult);
      break;
    default:
      nsStyleUtil::AppendCubicBezierKeywordTimingFunction(mType, aResult);
      break;
  }
}

} // namespace mozilla
