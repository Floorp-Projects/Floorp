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

ComputedTimingFunction::ComputedTimingFunction(
    const StyleComputedTimingFunction& aFunction)
    : mFunction{aFunction} {}

ComputedTimingFunction::ComputedTimingFunction(
    const nsTimingFunction& aFunction)
    : ComputedTimingFunction{aFunction.mTiming} {}

double ComputedTimingFunction::GetValue(
    double aPortion, StyleEasingBeforeFlag aBeforeFlag) const {
  return Servo_EasingFunctionAt(&mFunction, aPortion, aBeforeFlag);
}

void ComputedTimingFunction::AppendToString(nsACString& aResult) const {
  nsTimingFunction timing;
  // This does not preserve the original input either - that is,
  // linear(0 0% 50%, 1 50% 100%) -> linear(0 0%, 0 50%, 1 50%, 1 100%)
  timing.mTiming = mFunction;
  Servo_SerializeEasing(&timing, &aResult);
}
StyleComputedTimingFunction
ComputedTimingFunction::ToStyleComputedTimingFunction(
    const ComputedTimingFunction& aComputedTimingFunction) {
  return aComputedTimingFunction.mFunction;
}

}  // namespace mozilla
