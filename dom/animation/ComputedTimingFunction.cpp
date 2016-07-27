/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ComputedTimingFunction.h"
#include "nsAlgorithm.h" // For clamped()
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
  }
}

static inline double
StepTiming(uint32_t aSteps,
           double aPortion,
           ComputedTimingFunction::BeforeFlag aBeforeFlag,
           nsTimingFunction::Type aType)
{
  MOZ_ASSERT(0.0 <= aPortion && aPortion <= 1.0, "out of range");
  MOZ_ASSERT(aType == nsTimingFunction::Type::StepStart ||
             aType == nsTimingFunction::Type::StepEnd, "invalid type");

  if (aPortion == 1.0) {
    return 1.0;
  }

  // Calculate current step using step-end behavior
  uint32_t step = uint32_t(aPortion * aSteps); // floor

  // step-start is one step ahead
  if (aType == nsTimingFunction::Type::StepStart) {
    step++;
  }

  // If the "before flag" is set and we are at a transition point,
  // drop back a step (but only if we are not already at the zero point--
  // we do this clamping here since |step| is an unsigned integer)
  if (step != 0 &&
      aBeforeFlag == ComputedTimingFunction::BeforeFlag::Set &&
      fmod(aPortion * aSteps, 1) == 0) {
    step--;
  }

  // Convert to a progress value
  return double(step) / double(aSteps);
}

double
ComputedTimingFunction::GetValue(
    double aPortion,
    ComputedTimingFunction::BeforeFlag aBeforeFlag) const
{
  if (HasSpline()) {
    // Check for a linear curve.
    // (GetSplineValue(), below, also checks this but doesn't work when
    // aPortion is outside the range [0.0, 1.0]).
    if (mTimingFunction.X1() == mTimingFunction.Y1() &&
        mTimingFunction.X2() == mTimingFunction.Y2()) {
      return aPortion;
    }

    // Ensure that we return 0 or 1 on both edges.
    if (aPortion == 0.0) {
      return 0.0;
    }
    if (aPortion == 1.0) {
      return 1.0;
    }

    // For negative values, try to extrapolate with tangent (p1 - p0) or,
    // if p1 is coincident with p0, with (p2 - p0).
    if (aPortion < 0.0) {
      if (mTimingFunction.X1() > 0.0) {
        return aPortion * mTimingFunction.Y1() / mTimingFunction.X1();
      } else if (mTimingFunction.Y1() == 0 && mTimingFunction.X2() > 0.0) {
        return aPortion * mTimingFunction.Y2() / mTimingFunction.X2();
      }
      // If we can't calculate a sensible tangent, don't extrapolate at all.
      return 0.0;
    }

    // For values greater than 1, try to extrapolate with tangent (p2 - p3) or,
    // if p2 is coincident with p3, with (p1 - p3).
    if (aPortion > 1.0) {
      if (mTimingFunction.X2() < 1.0) {
        return 1.0 + (aPortion - 1.0) *
          (mTimingFunction.Y2() - 1) / (mTimingFunction.X2() - 1);
      } else if (mTimingFunction.Y2() == 1 && mTimingFunction.X1() < 1.0) {
        return 1.0 + (aPortion - 1.0) *
          (mTimingFunction.Y1() - 1) / (mTimingFunction.X1() - 1);
      }
      // If we can't calculate a sensible tangent, don't extrapolate at all.
      return 1.0;
    }

    return mTimingFunction.GetSplineValue(aPortion);
  }

  // Since we use endpoint-exclusive timing, the output of a steps(start) timing
  // function when aPortion = 0.0 is the top of the first step. When aPortion is
  // negative, however, we should use the bottom of the first step. We handle
  // negative values of aPortion specially here since once we clamp aPortion
  // to [0,1] below we will no longer be able to distinguish to the two cases.
  if (aPortion < 0.0) {
    return 0.0;
  }

  // Clamp in case of steps(end) and steps(start) for values greater than 1.
  aPortion = clamped(aPortion, 0.0, 1.0);

  return StepTiming(mSteps, aPortion, aBeforeFlag, mType);
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
      nsStyleUtil::AppendStepsTimingFunction(mType, mSteps, aResult);
      break;
    default:
      nsStyleUtil::AppendCubicBezierKeywordTimingFunction(mType, aResult);
      break;
  }
}

/* static */ int32_t
ComputedTimingFunction::Compare(const Maybe<ComputedTimingFunction>& aLhs,
                                const Maybe<ComputedTimingFunction>& aRhs)
{
  // We can't use |operator<| for const Maybe<>& here because
  // 'ease' is prior to 'linear' which is represented by Nothing().
  // So we have to convert Nothing() as 'linear' and check it first.
  nsTimingFunction::Type lhsType = aLhs.isNothing() ?
    nsTimingFunction::Type::Linear : aLhs->GetType();
  nsTimingFunction::Type rhsType = aRhs.isNothing() ?
    nsTimingFunction::Type::Linear : aRhs->GetType();

  if (lhsType != rhsType) {
    return int32_t(lhsType) - int32_t(rhsType);
  }

  // Both of them are Nothing().
  if (lhsType == nsTimingFunction::Type::Linear) {
    return 0;
  }

  // Other types.
  return aLhs->Compare(aRhs.value());
}

} // namespace mozilla
