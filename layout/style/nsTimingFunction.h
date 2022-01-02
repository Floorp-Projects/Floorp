/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTimingFunction_h
#define nsTimingFunction_h

#include "mozilla/ServoStyleConsts.h"

struct nsTimingFunction {
  mozilla::StyleComputedTimingFunction mTiming;

  explicit nsTimingFunction(
      mozilla::StyleTimingKeyword aKeyword = mozilla::StyleTimingKeyword::Ease)
      : mTiming(mozilla::StyleComputedTimingFunction::Keyword(aKeyword)) {}

  nsTimingFunction(float x1, float y1, float x2, float y2)
      : mTiming(mozilla::StyleComputedTimingFunction::CubicBezier(x1, y1, x2,
                                                                  y2)) {}

  bool IsLinear() const {
    return mTiming.IsKeyword() &&
           mTiming.AsKeyword() == mozilla::StyleTimingKeyword::Linear;
  }

  bool operator==(const nsTimingFunction& aOther) const {
    return mTiming == aOther.mTiming;
  }

  bool operator!=(const nsTimingFunction& aOther) const {
    return !(*this == aOther);
  }
};

#endif  // nsTimingFunction_h
