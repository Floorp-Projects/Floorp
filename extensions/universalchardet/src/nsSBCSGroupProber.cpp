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

#include <stdio.h>
#include "prmem.h"

#include "nsSBCharSetProber.h"
#include "nsSBCSGroupProber.h"


nsSBCSGroupProber::nsSBCSGroupProber()
{
  mProbers[0] = new nsSingleByteCharSetProber(&Win1251Model);
  mProbers[1] = new nsSingleByteCharSetProber(&Koi8rModel);
  mProbers[2] = new nsSingleByteCharSetProber(&Latin5Model);
  mProbers[3] = new nsSingleByteCharSetProber(&MacCyrillicModel);
  mProbers[4] = new nsSingleByteCharSetProber(&Ibm866Model);
  mProbers[5] = new nsSingleByteCharSetProber(&Ibm855Model);
  mProbers[6] = new nsSingleByteCharSetProber(&Win1253Model);
  mProbers[7] = new nsSingleByteCharSetProber(&Latin7Model);
  mProbers[8] = new nsSingleByteCharSetProber(&Win1251BulgarianModel);
  mProbers[9] = new nsSingleByteCharSetProber(&Latin5BulgarianModel);
  mProbers[10] = new nsSingleByteCharSetProber(&Win1250HungarianModel);
  mProbers[11] = new nsSingleByteCharSetProber(&Latin2HungarianModel);

  Reset();
}

nsSBCSGroupProber::~nsSBCSGroupProber()
{
  for (PRUint32 i = 0; i < NUM_OF_SBCS_PROBERS; i++)
  {
    delete mProbers[i];
  }
}


const char* nsSBCSGroupProber::GetCharSetName()
{
  //if we have no answer yet
  if (mBestGuess == -1)
  {
    GetConfidence();
    //no charset seems positive
    if (mBestGuess == -1)
      //we will use default.
      mBestGuess = 0;
  }
  return mProbers[mBestGuess]->GetCharSetName();
}

void  nsSBCSGroupProber::Reset(void)
{
  for (PRUint32 i = 0; i < NUM_OF_SBCS_PROBERS; i++)
  {
    mProbers[i]->Reset();
    mIsActive[i] = PR_TRUE;
  }
  mBestGuess = -1;
  mState = eDetecting;
}

//This filter apply to all scripts that does not use latin letters (english letter)
PRBool nsSBCSGroupProber::FilterWithoutEnglishLetters(const char* aBuf, PRUint32 aLen, char** newBuf, PRUint32& newLen)
{
  //do filtering to reduce load to probers
  char *newptr;
  char *prevPtr, *curPtr;
  
  PRBool meetMSB = PR_FALSE;   
  newptr = *newBuf = (char*)PR_MALLOC(aLen);
  if (!newptr)
    return PR_FALSE;

  for (curPtr = prevPtr = (char*)aBuf; curPtr < aBuf+aLen; curPtr++)
  {
    if (*curPtr & 0x80)
      meetMSB = PR_TRUE;
    else if (*curPtr < 'A' || (*curPtr > 'Z' && *curPtr < 'a') || *curPtr > 'z') 
    {
      //current char is a symbol, most likely a punctuation. we treat it as segment delimiter
      if (meetMSB && curPtr > prevPtr) 
      //this segment contains more than single symbol, and it has upper ascii, we need to keep it
      {
        while (prevPtr < curPtr) *newptr++ = *prevPtr++;  
        prevPtr++;
        *newptr++ = ' ';
        meetMSB = PR_FALSE;
      }
      else //ignore current segment. (either because it is just a symbol or just a english word
        prevPtr = curPtr+1;
    }
  }

  newLen = newptr - *newBuf;

  return PR_TRUE;
}

//This filter apply to all scripts that does use latin letters (english letter)
PRBool nsSBCSGroupProber::FilterWithEnglishLetters(const char* aBuf, PRUint32 aLen, char** newBuf, PRUint32& newLen)
{
  //do filtering to reduce load to probers
  char *newptr;
  char *prevPtr, *curPtr;
  PRBool isInTag = PR_FALSE;

  newptr = *newBuf = (char*)PR_MALLOC(aLen);
  if (!newptr)
    return PR_FALSE;

  for (curPtr = prevPtr = (char*)aBuf; curPtr < aBuf+aLen; curPtr++)
  {
		if (*curPtr == '>')
			isInTag = PR_FALSE;
    else if (*curPtr == '<')
      isInTag = PR_TRUE;

    if (!(*curPtr & 0x80) &&
        (*curPtr < 'A' || (*curPtr > 'Z' && *curPtr < 'a') || *curPtr > 'z') )
    {
      if (curPtr > prevPtr && !isInTag) //current segment contains more than just a symbol 
                                        // and it is not inside a tag, keep it
      {
        while (prevPtr < curPtr) *newptr++ = *prevPtr++;  
        prevPtr++;
        *newptr++ = ' ';
      }
      else
        prevPtr = curPtr+1;
    }
  }

  newLen = newptr - *newBuf;

  return PR_TRUE;
}

nsProbingState nsSBCSGroupProber::HandleData(const char* aBuf, PRUint32 aLen)
{
  nsProbingState st;
  PRUint32 i;
  char *newBuf1, *newBuf2;
  PRUint32 newLen1, newLen2;

  //apply filter to original buffer, and we got new buffer back
  //depend on what script it is, we will feed them the new buffer 
  //we got after applying proper filter
  FilterWithoutEnglishLetters(aBuf, aLen, &newBuf1, newLen1);
  FilterWithEnglishLetters(aBuf, aLen, &newBuf2, newLen2);

  for (i = 0; i < NUM_OF_SBCS_PROBERS; i++)
  {
     if (!mIsActive[i])
       continue;
     if (mProbers[i]->KeepEnglishLetters())
       //for scripts that use english letters, feed them with buffer got from FilterWithEnglishLetters
       st = mProbers[i]->HandleData(newBuf2, newLen2);
     else 
       //for scripts that does not use english letters, feed them with buffer got from FilterWithoutEnglishLetters
       st = mProbers[i]->HandleData(newBuf1, newLen1);
     if (st == eFoundIt)
     {
       mBestGuess = i;
       mState = eFoundIt;
#ifdef DEBUG_chardet
       printf("MBCS Prober found charset %d in HandleData. \r\n", i);
#endif
       break;
     }
     else if (st == eNotMe)
     {
       mIsActive[i] = PR_FALSE;
       mActiveNum--;
       if (mActiveNum <= 0)
       {
         mState = eNotMe;
         break;
       }
     }
  }

  PR_FREEIF(newBuf1);
  PR_FREEIF(newBuf2);

  return mState;
}

float nsSBCSGroupProber::GetConfidence(void)
{
  PRUint32 i;
  float bestConf = 0.0, cf;

  switch (mState)
  {
  case eFoundIt:
    return (float)0.99; //sure yes
  case eNotMe:
    return (float)0.01;  //sure no
  default:
    for (i = 0; i < NUM_OF_SBCS_PROBERS; i++)
    {
      if (!mIsActive[i])
        continue;
      cf = mProbers[i]->GetConfidence();
      if (bestConf < cf)
      {
        bestConf = cf;
        mBestGuess = i;
      }
    }
  }
#ifdef DEBUG_chardet
       printf("SBCS Group Prober confidence is %f in charset %d . \r\n", bestConf, mBestGuess);
       for (i = 0; i < NUM_OF_SBCS_PROBERS; i++)
       {
          if (!mIsActive[i])
            printf("[%s] is inactive\r\n", mProbers[i]->GetCharSetName(), i);
          else
          {
            cf = mProbers[i]->GetConfidence();
            printf("[%s] detector has confidence %f\r\n", mProbers[i]->GetCharSetName(), cf);
          }
       }
#endif
  return bestConf;
}

