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


#include "nsEscCharsetProber.h"

nsEscCharSetProber::nsEscCharSetProber(void)
{
  mCodingSM[0] = new nsCodingStateMachine(&HZSMModel);
  mCodingSM[1] = new nsCodingStateMachine(&ISO2022CNSMModel);
  mCodingSM[2] = new nsCodingStateMachine(&ISO2022JPSMModel);
  mCodingSM[3] = new nsCodingStateMachine(&ISO2022KRSMModel);
  mActiveSM = NUM_OF_ESC_CHARSETS;
  mState = eDetecting;
  mDetectedCharset = nsnull;
};

nsEscCharSetProber::~nsEscCharSetProber(void)
{
  for (PRUint32 i = 0; i < NUM_OF_ESC_CHARSETS; i++)
    delete mCodingSM[i];
}

void nsEscCharSetProber::Reset(void)
{
  mState = eDetecting;
  for (PRUint32 i = 0; i < NUM_OF_ESC_CHARSETS; i++)
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
      //byte is feed to all active state machine 
      codingState = mCodingSM[j]->NextState(aBuf[i]);
      if (codingState == eError)
      {
        //got negative answer for this state machine, make it inactive
        mActiveSM--;
        if (mActiveSM == 0)
        {
          mState = eNotMe;
          return mState;
        }
        else if (j != (PRInt32)mActiveSM)
        {
          nsCodingStateMachine* t;
          t = mCodingSM[mActiveSM];
          mCodingSM[mActiveSM] = mCodingSM[j];
          mCodingSM[j] = t;
        }
      }
      else if (codingState == eItsMe)
      {
        mState = eFoundIt;
        mDetectedCharset = mCodingSM[j]->GetCodingStateMachine();
        return mState;
      }
    }
  }

  return mState;
}

