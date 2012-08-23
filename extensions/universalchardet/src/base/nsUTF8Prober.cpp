/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUTF8Prober.h"

void  nsUTF8Prober::Reset(void)
{
  mCodingSM->Reset(); 
  mNumOfMBChar = 0;
  mState = eDetecting;
}

nsProbingState nsUTF8Prober::HandleData(const char* aBuf, uint32_t aLen)
{
  nsSMState codingState;

  for (uint32_t i = 0; i < aLen; i++)
  {
    codingState = mCodingSM->NextState(aBuf[i]);
    if (codingState == eItsMe)
    {
      mState = eFoundIt;
      break;
    }
    if (codingState == eStart)
    {
      if (mCodingSM->GetCurrentCharLen() >= 2)
        mNumOfMBChar++;
    }
  }

  if (mState == eDetecting)
    if (GetConfidence() > SHORTCUT_THRESHOLD)
      mState = eFoundIt;
  return mState;
}

#define ONE_CHAR_PROB   (float)0.50

float nsUTF8Prober::GetConfidence(void)
{
  float unlike = (float)0.99;

  if (mNumOfMBChar < 6)
  {
    for (uint32_t i = 0; i < mNumOfMBChar; i++)
      unlike *= ONE_CHAR_PROB;
    return (float)1.0 - unlike;
  }
  else
    return (float)0.99;
}

