/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SMIL_SMILREPEATCOUNT_H_
#define DOM_SMIL_SMILREPEATCOUNT_H_

#include "nsDebug.h"
#include <math.h>

namespace mozilla {

//----------------------------------------------------------------------
// SMILRepeatCount
//
// A tri-state non-negative floating point number for representing the number of
// times an animation repeat, i.e. the SMIL repeatCount attribute.
//
// The three states are:
//  1. not-set
//  2. set (with non-negative, non-zero count value)
//  3. indefinite
//
class SMILRepeatCount {
 public:
  SMILRepeatCount() : mCount(kNotSet) {}
  explicit SMILRepeatCount(double aCount) : mCount(kNotSet) {
    SetCount(aCount);
  }

  operator double() const {
    MOZ_ASSERT(IsDefinite(),
               "Converting indefinite or unset repeat count to double");
    return mCount;
  }
  bool IsDefinite() const { return mCount != kNotSet && mCount != kIndefinite; }
  bool IsIndefinite() const { return mCount == kIndefinite; }
  bool IsSet() const { return mCount != kNotSet; }

  SMILRepeatCount& operator=(double aCount) {
    SetCount(aCount);
    return *this;
  }
  void SetCount(double aCount) {
    NS_ASSERTION(aCount > 0.0, "Negative or zero repeat count");
    mCount = aCount > 0.0 ? aCount : kNotSet;
  }
  void SetIndefinite() { mCount = kIndefinite; }
  void Unset() { mCount = kNotSet; }

 private:
  static const double kNotSet;
  static const double kIndefinite;

  double mCount;
};

}  // namespace mozilla

#endif  // DOM_SMIL_SMILREPEATCOUNT_H_
