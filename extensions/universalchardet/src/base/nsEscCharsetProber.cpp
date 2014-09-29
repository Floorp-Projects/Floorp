/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsEscCharsetProber.h"
#include "nsUniversalDetector.h"

nsEscCharSetProber::nsEscCharSetProber()
{
  mCodingSM = new nsCodingStateMachine(&ISO2022JPSMModel);
  mState = eDetecting;
  mDetectedCharset = nullptr;
}

nsEscCharSetProber::~nsEscCharSetProber(void)
{
}

void nsEscCharSetProber::Reset(void)
{
  mState = eDetecting;
  mCodingSM->Reset();
  mDetectedCharset = nullptr;
}

nsProbingState nsEscCharSetProber::HandleData(const char* aBuf, uint32_t aLen)
{
  nsSMState codingState;
  uint32_t i;

  for ( i = 0; i < aLen && mState == eDetecting; i++)
  {
    codingState = mCodingSM->NextState(aBuf[i]);
    if (codingState == eItsMe)
    {
      mState = eFoundIt;
      mDetectedCharset = mCodingSM->GetCodingStateMachine();
      return mState;
    }
  }

  return mState;
}

