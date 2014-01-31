/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CharDistribution.h"

#include "JISFreq.tab"
#include "Big5Freq.tab"
#include "EUCKRFreq.tab"
#include "EUCTWFreq.tab"
#include "GB2312Freq.tab"
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

EUCTWDistributionAnalysis::EUCTWDistributionAnalysis()
{
  mCharToFreqOrder = EUCTWCharToFreqOrder;
  mTableSize = mozilla::ArrayLength(EUCTWCharToFreqOrder);
  mTypicalDistributionRatio = EUCTW_TYPICAL_DISTRIBUTION_RATIO;
}

EUCKRDistributionAnalysis::EUCKRDistributionAnalysis()
{
  mCharToFreqOrder = EUCKRCharToFreqOrder;
  mTableSize = mozilla::ArrayLength(EUCKRCharToFreqOrder);
  mTypicalDistributionRatio = EUCKR_TYPICAL_DISTRIBUTION_RATIO;
}

GB2312DistributionAnalysis::GB2312DistributionAnalysis()
{
  mCharToFreqOrder = GB2312CharToFreqOrder;
  mTableSize = mozilla::ArrayLength(GB2312CharToFreqOrder);
  mTypicalDistributionRatio = GB2312_TYPICAL_DISTRIBUTION_RATIO;
}

Big5DistributionAnalysis::Big5DistributionAnalysis()
{
  mCharToFreqOrder = Big5CharToFreqOrder;
  mTableSize = mozilla::ArrayLength(Big5CharToFreqOrder);
  mTypicalDistributionRatio = BIG5_TYPICAL_DISTRIBUTION_RATIO;
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

