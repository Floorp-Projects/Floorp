/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 */

#include "CharDistribution.h"

#include "JISFreq.tab"
#include "Big5Freq.tab"
#include "EUCKRFreq.tab"
#include "EUCTWFreq.tab"
#include "GB2312Freq.tab"

#define SURE_YES ((float)0.99)
#define SURE_NO  ((float)0.01)

//return confidence base on received data
float CharDistributionAnalysis::GetConfidence()
{ 
  float r;

  if (mTotalChars > 0)
  {
    r = mFreqChars / (float)(mTotalChars - mFreqChars) / mTypicalDistributionRatio;
    //normalize confidence, (and we don't want to be 100% sure)
    if (r >= (float)1.00)
      r = SURE_YES;
  }
  else  //if we didn't receive any character in our consideration range, return negative answer
    r = SURE_NO;
  
  return r;
}

EUCTWDistributionAnalysis::EUCTWDistributionAnalysis()
{
  mCharToFreqOrder = EUCTWCharToFreqOrder;
  mTableSize = EUCTW_TABLE_SIZE;
  mTypicalDistributionRatio = EUCTW_TYPICAL_DISTRIBUTION_RATIO;
};

EUCKRDistributionAnalysis::EUCKRDistributionAnalysis()
{
  mCharToFreqOrder = EUCKRCharToFreqOrder;
  mTableSize = EUCKR_TABLE_SIZE;
  mTypicalDistributionRatio = EUCKR_TYPICAL_DISTRIBUTION_RATIO;
};

GB2312DistributionAnalysis::GB2312DistributionAnalysis()
{
  mCharToFreqOrder = GB2312CharToFreqOrder;
  mTableSize = GB2312_TABLE_SIZE;
  mTypicalDistributionRatio = GB2312_TYPICAL_DISTRIBUTION_RATIO;
};

Big5DistributionAnalysis::Big5DistributionAnalysis()
{
  mCharToFreqOrder = Big5CharToFreqOrder;
  mTableSize = BIG5_TABLE_SIZE;
  mTypicalDistributionRatio = BIG5_TYPICAL_DISTRIBUTION_RATIO;
};

SJISDistributionAnalysis::SJISDistributionAnalysis()
{
  mCharToFreqOrder = JISCharToFreqOrder;
  mTableSize = JIS_TABLE_SIZE;
  mTypicalDistributionRatio = JIS_TYPICAL_DISTRIBUTION_RATIO;
};

EUCJPDistributionAnalysis::EUCJPDistributionAnalysis()
{
  mCharToFreqOrder = JISCharToFreqOrder;
  mTableSize = JIS_TABLE_SIZE;
  mTypicalDistributionRatio = JIS_TYPICAL_DISTRIBUTION_RATIO;
};

