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

  double GetValue(double aPortion, StyleEasingBeforeFlag aBeforeFlag) const;
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
                           double aPortion, StyleEasingBeforeFlag aBeforeFlag) {
    return aFunction ? aFunction->GetValue(aPortion, aBeforeFlag) : aPortion;
  }

 private:
  StyleComputedTimingFunction mFunction;
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
