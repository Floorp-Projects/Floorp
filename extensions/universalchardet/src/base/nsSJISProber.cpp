/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// for S-JIS encoding, obeserve characteristic:
// 1, kana character (or hankaku?) often have hight frequency of appereance
// 2, kana character often exist in group
// 3, certain combination of kana is never used in japanese language

#include "nsSJISProber.h"

void  nsSJISProber::Reset(void)
{
  mCodingSM->Reset(); 
  mNumOfRoman = 0;
  mNumOfHankaku = 0;
  mNumOfKana = 0;
  mNumOfKanji = 0;
  mNumOfMisc = 0;
  mState = eDetecting;
  mContextAnalyser.Reset();
  mDistributionAnalyser.Reset();
}

nsProbingState nsSJISProber::HandleData(const char* aBuf, PRUint32 aLen)
{
  nsSMState codingState;

  for (PRUint32 i = 0; i < aLen; i++)
  {
    codingState = mCodingSM->NextState(aBuf[i]);
    if (codingState == eError)
    {
      mState = eNotMe;
      break;
    }
    if (codingState == eItsMe)
    {
      mState = eFoundIt;
      break;
    }
    if (codingState == eStart)
    {
      PRUint32 charLen = mCodingSM->GetCurrentCharLen();
      if (i == 0)
      {
        mLastChar[1] = aBuf[0];
        GetDistribution(mCodingSM->GetCurrentCharLen(), mLastChar);
        mContextAnalyser.HandleOneChar(mLastChar+2-charLen, charLen);
        mDistributionAnalyser.HandleOneChar(mLastChar, charLen);
      }
      else
      {
        GetDistribution(mCodingSM->GetCurrentCharLen(), aBuf+i-1);
        mContextAnalyser.HandleOneChar(aBuf+i+1-charLen, charLen);
        mDistributionAnalyser.HandleOneChar(aBuf+i-1, charLen);
      }
    }
  }

  mLastChar[0] = aBuf[aLen-1];

  if (mState == eDetecting)
    if (mContextAnalyser.GotEnoughData() && GetConfidence() > SHORTCUT_THRESHOLD)
      mState = eFoundIt;

  return mState;
}

void nsSJISProber::GetDistribution(PRUint32 aCharLen, const char* aStr)
{
  if (aCharLen >= 2)
  {
    if ((unsigned char)*aStr == (unsigned char)0x82 && 
        (unsigned char)*(aStr+1) >= (unsigned char)0x9f && 
        (unsigned char)*(aStr+1) <= (unsigned char)0xf1 ||
        (unsigned char)*aStr == (unsigned char)0x83 && 
        (unsigned char)*(aStr+1) >= (unsigned char)0x40 && 
        (unsigned char)*(aStr+1) <= (unsigned char)0x96)
      mNumOfKana++;
    else if ((unsigned char)*aStr >= (unsigned char)0x88)
      mNumOfKanji++;
    else
      mNumOfMisc++;
  }
  else
  {
    if ((unsigned char)*(aStr+1) >= (unsigned char)0xa1)
      mNumOfHankaku++;
    else
      mNumOfRoman++;
  }
}

float nsSJISProber::GetConfidence(void)
{
  float contxtCf = mContextAnalyser.GetConfidence();
  float distribCf = mDistributionAnalyser.GetConfidence();

  return (contxtCf > distribCf ? contxtCf : distribCf);
}

