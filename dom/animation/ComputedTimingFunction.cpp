/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ComputedTimingFunction.h"
#include "mozilla/ServoBindings.h"
#include "nsAlgorithm.h"  // For clamped()

namespace mozilla {

void ComputedTimingFunction::Init(const nsTimingFunction& aFunction) {
  const StyleComputedTimingFunction& timing = aFunction.mTiming;
  switch (timing.tag) {
    case StyleComputedTimingFunction::Tag::Keyword: {
      mType = static_cast<Type>(static_cast<uint8_t>(timing.keyword._0));

      static_assert(
          static_cast<uint8_t>(StyleTimingKeyword::Linear) == 0 &&
              static_cast<uint8_t>(StyleTimingKeyword::Ease) == 1 &&
              static_cast<uint8_t>(StyleTimingKeyword::EaseIn) == 2 &&
              static_cast<uint8_t>(StyleTimingKeyword::EaseOut) == 3 &&
              static_cast<uint8_t>(StyleTimingKeyword::EaseInOut) == 4,
          "transition timing function constants not as expected");

      static const float timingFunctionValues[5][4] = {
          {0.00f, 0.00f, 1.00f, 1.00f},  // linear
          {0.25f, 0.10f, 0.25f, 1.00f},  // ease
          {0.42f, 0.00f, 1.00f, 1.00f},  // ease-in
          {0.00f, 0.00f, 0.58f, 1.00f},  // ease-out
          {0.42f, 0.00f, 0.58f, 1.00f}   // ease-in-out
      };
      const float(&values)[4] = timingFunctionValues[uint8_t(mType)];
      mTimingFunction.Init(values[0], values[1], values[2], values[3]);
      break;
    }
    case StyleComputedTimingFunction::Tag::CubicBezier:
      mType = Type::CubicBezier;
      mTimingFunction.Init(timing.cubic_bezier.x1, timing.cubic_bezier.y1,
                           timing.cubic_bezier.x2, timing.cubic_bezier.y2);
      break;
    case StyleComputedTimingFunction::Tag::Steps:
      mType = Type::Step;
      mSteps.mSteps = static_cast<uint32_t>(timing.steps._0);
      mSteps.mPos = timing.steps._1;
      break;
  }
}

static inline double StepTiming(
    const ComputedTimingFunction::StepFunc& aStepFunc, double aPortion,
    ComputedTimingFunction::BeforeFlag aBeforeFlag) {
  // Use the algorithm defined in the spec:
  // https://drafts.csswg.org/css-easing-1/#step-timing-function-algo

  // Calculate current step.
  const int32_t currentStep = static_cast<int32_t>(
      clamped(floor(aPortion * aStepFunc.mSteps),
              (double)std::numeric_limits<int32_t>::min(),
              (double)std::numeric_limits<int32_t>::max()));
  CheckedInt32 checkedCurrentStep = currentStep;

  // Increment current step if it is jump-start or start.
  if (aStepFunc.mPos == StyleStepPosition::Start ||
      aStepFunc.mPos == StyleStepPosition::JumpStart ||
      aStepFunc.mPos == StyleStepPosition::JumpBoth) {
    ++checkedCurrentStep;
  }

  // If the "before flag" is set and we are at a transition point,
  // drop back a step
  if (aBeforeFlag == ComputedTimingFunction::BeforeFlag::Set &&
      fmod(aPortion * aStepFunc.mSteps, 1) == 0) {
    --checkedCurrentStep;
  }

  if (!checkedCurrentStep.isValid()) {
    // Unexpected behavior (e.g. overflow). Roll back to |currentStep|.
    checkedCurrentStep = currentStep;
  }

  // We should not produce a result outside [0, 1] unless we have an
  // input outside that range. This takes care of steps that would otherwise
  // occur at boundaries.
  if (aPortion >= 0.0 && checkedCurrentStep.value() < 0) {
    checkedCurrentStep = 0;
  }

  // |jumps| should always be in [1, INT_MAX].
  CheckedInt32 jumps = aStepFunc.mSteps;
  if (aStepFunc.mPos == StyleStepPosition::JumpBoth) {
    ++jumps;
  } else if (aStepFunc.mPos == StyleStepPosition::JumpNone) {
    --jumps;
  }

  if (!jumps.isValid()) {
    // Unexpected behavior (e.g. overflow). Roll back to |aStepFunc.mSteps|.
    jumps = aStepFunc.mSteps;
  }

  if (aPortion <= 1.0 && checkedCurrentStep.value() > jumps.value()) {
    checkedCurrentStep = jumps;
  }

  // Convert to the output progress value.
  MOZ_ASSERT(jumps.value() > 0, "`jumps` should be a positive integer");
  return double(checkedCurrentStep.value()) / double(jumps.value());
}

double ComputedTimingFunction::GetValue(
    double aPortion, ComputedTimingFunction::BeforeFlag aBeforeFlag) const {
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
        return 1.0 + (aPortion - 1.0) * (mTimingFunction.Y2() - 1) /
                         (mTimingFunction.X2() - 1);
      } else if (mTimingFunction.Y2() == 1 && mTimingFunction.X1() < 1.0) {
        return 1.0 + (aPortion - 1.0) * (mTimingFunction.Y1() - 1) /
                         (mTimingFunction.X1() - 1);
      }
      // If we can't calculate a sensible tangent, don't extrapolate at all.
      return 1.0;
    }

    return mTimingFunction.GetSplineValue(aPortion);
  }

  return StepTiming(mSteps, aPortion, aBeforeFlag);
}

int32_t ComputedTimingFunction::Compare(
    const ComputedTimingFunction& aRhs) const {
  if (mType != aRhs.mType) {
    return int32_t(mType) - int32_t(aRhs.mType);
  }

  if (mType == Type::CubicBezier) {
    int32_t order = mTimingFunction.Compare(aRhs.mTimingFunction);
    if (order != 0) {
      return order;
    }
  } else if (mType == Type::Step) {
    if (mSteps.mPos != aRhs.mSteps.mPos) {
      return int32_t(mSteps.mPos) - int32_t(aRhs.mSteps.mPos);
    } else if (mSteps.mSteps != aRhs.mSteps.mSteps) {
      return int32_t(mSteps.mSteps) - int32_t(aRhs.mSteps.mSteps);
    }
  }

  return 0;
}

void ComputedTimingFunction::AppendToString(nsACString& aResult) const {
  nsTimingFunction timing;
  switch (mType) {
    case Type::CubicBezier:
      timing.mTiming = StyleComputedTimingFunction::CubicBezier(
          mTimingFunction.X1(), mTimingFunction.Y1(), mTimingFunction.X2(),
          mTimingFunction.Y2());
      break;
    case Type::Step:
      timing.mTiming =
          StyleComputedTimingFunction::Steps(mSteps.mSteps, mSteps.mPos);
      break;
    case Type::Linear:
    case Type::Ease:
    case Type::EaseIn:
    case Type::EaseOut:
    case Type::EaseInOut:
      timing.mTiming = StyleComputedTimingFunction::Keyword(
          static_cast<StyleTimingKeyword>(mType));
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported timing type");
  }
  Servo_SerializeEasing(&timing, &aResult);
}

/* static */
int32_t ComputedTimingFunction::Compare(
    const Maybe<ComputedTimingFunction>& aLhs,
    const Maybe<ComputedTimingFunction>& aRhs) {
  // We can't use |operator<| for const Maybe<>& here because
  // 'ease' is prior to 'linear' which is represented by Nothing().
  // So we have to convert Nothing() as 'linear' and check it first.
  Type lhsType = aLhs.isNothing() ? Type::Linear : aLhs->GetType();
  Type rhsType = aRhs.isNothing() ? Type::Linear : aRhs->GetType();

  if (lhsType != rhsType) {
    return int32_t(lhsType) - int32_t(rhsType);
  }

  // Both of them are Nothing().
  if (lhsType == Type::Linear) {
    return 0;
  }

  // Other types.
  return aLhs->Compare(aRhs.value());
}

}  // namespace mozilla
