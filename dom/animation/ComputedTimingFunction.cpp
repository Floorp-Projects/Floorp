/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ComputedTimingFunction.h"
#include "mozilla/Assertions.h"
#include "mozilla/ServoBindings.h"
#include "nsAlgorithm.h"  // For clamped()
#include "mozilla/layers/LayersMessages.h"

namespace mozilla {

ComputedTimingFunction::Function ComputedTimingFunction::ConstructFunction(
    const StyleComputedTimingFunction& aFunction) {
  switch (aFunction.tag) {
    case StyleComputedTimingFunction::Tag::Keyword: {
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
      const float(&values)[4] =
          timingFunctionValues[uint8_t(aFunction.keyword._0)];
      return AsVariant(KeywordFunction{
          aFunction.keyword._0,
          SMILKeySpline{values[0], values[1], values[2], values[3]}});
    }
    case StyleComputedTimingFunction::Tag::CubicBezier:
      return AsVariant(
          SMILKeySpline{aFunction.cubic_bezier.x1, aFunction.cubic_bezier.y1,
                        aFunction.cubic_bezier.x2, aFunction.cubic_bezier.y2});
    case StyleComputedTimingFunction::Tag::Steps:
      return AsVariant(StepFunc{static_cast<uint32_t>(aFunction.steps._0),
                                aFunction.steps._1});
    case StyleComputedTimingFunction::Tag::LinearFunction: {
      StylePiecewiseLinearFunction result;
      Servo_CreatePiecewiseLinearFunction(&aFunction.linear_function._0,
                                          &result);
      return AsVariant(result);
    }
  }
  MOZ_ASSERT_UNREACHABLE("Unknown timing function.");
  return ConstructFunction(mozilla::StyleComputedTimingFunction::Keyword(
      StyleTimingKeyword::Linear));
}

ComputedTimingFunction::ComputedTimingFunction(
    const StyleComputedTimingFunction& aFunction)
    : mFunction{ConstructFunction(aFunction)} {}

ComputedTimingFunction::ComputedTimingFunction(
    const nsTimingFunction& aFunction)
    : ComputedTimingFunction{aFunction.mTiming} {}

double ComputedTimingFunction::StepTiming(
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

static inline double GetSplineValue(double aPortion,
                                    const SMILKeySpline& aSpline) {
  // Check for a linear curve.
  // (GetSplineValue(), below, also checks this but doesn't work when
  // aPortion is outside the range [0.0, 1.0]).
  if (aSpline.X1() == aSpline.Y1() && aSpline.X2() == aSpline.Y2()) {
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
    if (aSpline.X1() > 0.0) {
      return aPortion * aSpline.Y1() / aSpline.X1();
    }
    if (aSpline.Y1() == 0 && aSpline.X2() > 0.0) {
      return aPortion * aSpline.Y2() / aSpline.X2();
    }
    // If we can't calculate a sensible tangent, don't extrapolate at all.
    return 0.0;
  }

  // For values greater than 1, try to extrapolate with tangent (p2 - p3) or,
  // if p2 is coincident with p3, with (p1 - p3).
  if (aPortion > 1.0) {
    if (aSpline.X2() < 1.0) {
      return 1.0 + (aPortion - 1.0) * (aSpline.Y2() - 1) / (aSpline.X2() - 1);
    }
    if (aSpline.Y2() == 1 && aSpline.X1() < 1.0) {
      return 1.0 + (aPortion - 1.0) * (aSpline.Y1() - 1) / (aSpline.X1() - 1);
    }
    // If we can't calculate a sensible tangent, don't extrapolate at all.
    return 1.0;
  }

  return aSpline.GetSplineValue(aPortion);
}

double ComputedTimingFunction::GetValue(
    double aPortion, ComputedTimingFunction::BeforeFlag aBeforeFlag) const {
  return mFunction.match(
      [aPortion](const KeywordFunction& aFunction) {
        return GetSplineValue(aPortion, aFunction.mFunction);
      },
      [aPortion](const SMILKeySpline& aFunction) {
        return GetSplineValue(aPortion, aFunction);
      },
      [aPortion, aBeforeFlag](const StepFunc& aFunction) {
        return StepTiming(aFunction, aPortion, aBeforeFlag);
      },
      [aPortion](const StylePiecewiseLinearFunction& aFunction) {
        return static_cast<double>(Servo_PiecewiseLinearFunctionAt(
            &aFunction, static_cast<float>(aPortion)));
      });
}

void ComputedTimingFunction::AppendToString(nsACString& aResult) const {
  nsTimingFunction timing;
  // This does not preserve the original input either - that is,
  // linear(0 0% 50%, 1 50% 100%) -> linear(0 0%, 0 50%, 1 50%, 1 100%)
  timing.mTiming = {ToStyleComputedTimingFunction(*this)};
  Servo_SerializeEasing(&timing, &aResult);
}

StyleComputedTimingFunction
ComputedTimingFunction::ToStyleComputedTimingFunction(
    const ComputedTimingFunction& aComputedTimingFunction) {
  return aComputedTimingFunction.mFunction.match(
      [](const KeywordFunction& aFunction) {
        return StyleComputedTimingFunction::Keyword(aFunction.mKeyword);
      },
      [](const SMILKeySpline& aFunction) {
        return StyleComputedTimingFunction::CubicBezier(
            static_cast<float>(aFunction.X1()),
            static_cast<float>(aFunction.Y1()),
            static_cast<float>(aFunction.X2()),
            static_cast<float>(aFunction.Y2()));
      },
      [](const StepFunc& aFunction) {
        return StyleComputedTimingFunction::Steps(
            static_cast<int>(aFunction.mSteps), aFunction.mPos);
      },
      [](const StylePiecewiseLinearFunction& aFunction) {
        Vector<StyleComputedLinearStop> stops;
        bool reserved = stops.initCapacity(aFunction.entries.Length());
        MOZ_RELEASE_ASSERT(reserved, "Failed to reserve memory");
        for (const auto& e : aFunction.entries.AsSpan()) {
          stops.infallibleAppend(StyleComputedLinearStop{
              e.y, StyleOptional<StylePercentage>::Some(StylePercentage{e.x}),
              StyleOptional<StylePercentage>::None()});
        }
        return StyleComputedTimingFunction::LinearFunction(
            StyleOwnedSlice<StyleComputedLinearStop>{std::move(stops)});
      });
}

}  // namespace mozilla
