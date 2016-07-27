/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerAnimationUtils.h"
#include "mozilla/ComputedTimingFunction.h" // For ComputedTimingFunction
#include "mozilla/layers/LayersMessages.h" // For TimingFunction etc.

namespace mozilla {
namespace layers {

/* static */ Maybe<ComputedTimingFunction>
AnimationUtils::TimingFunctionToComputedTimingFunction(
  const TimingFunction& aTimingFunction)
{
  switch (aTimingFunction.type()) {
    case TimingFunction::Tnull_t:
      return Nothing();
    case TimingFunction::TCubicBezierFunction: {
      ComputedTimingFunction result;
      CubicBezierFunction cbf = aTimingFunction.get_CubicBezierFunction();
      result.Init(nsTimingFunction(cbf.x1(), cbf.y1(), cbf.x2(), cbf.y2()));
      return Some(result);
    }
    case TimingFunction::TStepFunction: {
      StepFunction sf = aTimingFunction.get_StepFunction();
      nsTimingFunction::Type type = sf.type() == 1 ?
        nsTimingFunction::Type::StepStart :
        nsTimingFunction::Type::StepEnd;
      ComputedTimingFunction result;
      result.Init(nsTimingFunction(type, sf.steps()));
      return Some(result);
    }
    default:
      MOZ_ASSERT_UNREACHABLE(
        "Function must be null, bezier or step");
      break;
  }
  return Nothing();
}

} // namespace layers
} // namespace mozilla
