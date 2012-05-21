/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsEscCharsetProber.h"
#include "nsUniversalDetector.h"

nsEscCharSetProber::nsEscCharSetProber(PRUint32 aLanguageFilter)
{
  for (PRUint32 i = 0; i < NUM_OF_ESC_CHARSETS; i++)
    mCodingSM[i] = nsnull;
  if (aLanguageFilter & NS_FILTER_CHINESE_SIMPLIFIED) 
  {
    mCodingSM[0] = new nsCodingStateMachine(&HZSMModel);
    mCodingSM[1] = new nsCodingStateMachine(&ISO2022CNSMModel);
  }
  if (aLanguageFilter & NS_FILTER_JAPANESE)
    mCodingSM[2] = new nsCodingStateMachine(&ISO2022JPSMModel);
  if (aLanguageFilter & NS_FILTER_KOREAN)
    mCodingSM[3] = new nsCodingStateMachine(&ISO2022KRSMModel);
  mActiveSM = NUM_OF_ESC_CHARSETS;
  mState = eDetecting;
  mDetectedCharset = nsnull;
}

nsEscCharSetProber::~nsEscCharSetProber(void)
{
  for (PRUint32 i = 0; i < NUM_OF_ESC_CHARSETS; i++)
    delete mCodingSM[i];
}

void nsEscCharSetProber::Reset(void)
{
  mState = eDetecting;
  for (PRUint32 i = 0; i < NUM_OF_ESC_CHARSETS; i++)
    if (mCodingSM[i])
      mCodingSM[i]->Reset();
  mActiveSM = NUM_OF_ESC_CHARSETS;
  mDetectedCharset = nsnull;
}

nsProbingState nsEscCharSetProber::HandleData(const char* aBuf, PRUint32 aLen)
{
  nsSMState codingState;
  PRInt32 j;
  PRUint32 i;

  for ( i = 0; i < aLen && mState == eDetecting; i++)
  {
    for (j = mActiveSM-1; j>= 0; j--)
    {
      if (mCodingSM[j])
      {
        codingState = mCodingSM[j]->NextState(aBuf[i]);
        if (codingState == eItsMe)
        {
          mState = eFoundIt;
          mDetectedCharset = mCodingSM[j]->GetCodingStateMachine();
          return mState;
        }
      }
    }
  }

  return mState;
}

