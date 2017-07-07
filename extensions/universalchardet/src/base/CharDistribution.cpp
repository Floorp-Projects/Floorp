/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CharDistribution.h"

#include "JISFreq.tab"
#include "mozilla/ArrayUtils.h"

#define SURE_YES 0.99f
#define SURE_NO  0.01f

//return confidence base on received data
float CharDistributionAnalysis::GetConfidence(void)
{
  //if we didn't receive any character in our consideration range, or the
  // number of frequent characters is below the minimum threshold, return
  // negative answer
  if (mTotalChars <= 0 || mFreqChars <= mDataThreshold)
    return SURE_NO;

  if (mTotalChars != mFreqChars) {
    float r = mFreqChars / ((mTotalChars - mFreqChars) * mTypicalDistributionRatio);

    if (r < SURE_YES)
      return r;
  }
  //normalize confidence, (we don't want to be 100% sure)
  return SURE_YES;
}

SJISDistributionAnalysis::SJISDistributionAnalysis()
{
  mCharToFreqOrder = JISCharToFreqOrder;
  mTableSize = mozilla::ArrayLength(JISCharToFreqOrder);
  mTypicalDistributionRatio = JIS_TYPICAL_DISTRIBUTION_RATIO;
}

EUCJPDistributionAnalysis::EUCJPDistributionAnalysis()
{
  mCharToFreqOrder = JISCharToFreqOrder;
  mTableSize = mozilla::ArrayLength(JISCharToFreqOrder);
  mTypicalDistributionRatio = JIS_TYPICAL_DISTRIBUTION_RATIO;
}

