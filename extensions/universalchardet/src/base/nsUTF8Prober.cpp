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

#include "nsUTF8Prober.h"

void  nsUTF8Prober::Reset(void)
{
  mCodingSM->Reset(); 
  mNumOfMBChar = 0;
  mState = eDetecting;
}

nsProbingState nsUTF8Prober::HandleData(const char* aBuf, PRUint32 aLen)
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
    for (PRUint32 i = 0; i < mNumOfMBChar; i++)
      unlike *= ONE_CHAR_PROB;
    return (float)1.0 - unlike;
  }
  else
    return (float)0.99;
}

