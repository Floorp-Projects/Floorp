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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


#include <math.h>
#include <stdio.h>
//---- for XPCOM
#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsISupports.h"
#include "nsCharDetDll.h"
#include "pratom.h"
#include "nsPSMDetectors.h"

PRBool nsEUCSampler::Sample(const char* aIn, PRUint32 aLen)
{
    if(mState == 1)
        return PR_FALSE;
    const unsigned char* p = (const unsigned char*) aIn;
    if(aLen + mTotal > 0x80000000) 
       aLen = 0x80000000 - mTotal;

     PRUint32 i;
     for(i=0; (i<aLen) && (1 != mState) ;i++,p++)
     {
        switch(mState) {
           case 0:
             if( *p & 0x0080)  
             {
                if((0x00ff == *p) || ( 0x00a1 > *p)) {
                   mState = 1;
                } else {
                   mTotal++;
                   mFirstByteCnt[*p - 0x00a1]++;
                   mState = 2;
                }
             }
             break;
           case 1:
             break;
           case 2:
             if( *p & 0x0080)  
             {
                if((0x00ff == *p) || ( 0x00a1 > *p)) {
                   mState = 1;
                } else {
                   mTotal++;
                   mSecondByteCnt[*p - 0x00a1]++;
                   mState = 0;
                }
             } else {
                mState = 1;
             }
             break;
           default:
             mState = 1;
        }
     }
   return (1 != mState  );
}
float nsEUCSampler::GetScore(const float* aFirstByteFreq, float aFirstByteWeight,
                     const float* aSecondByteFreq, float aSecondByteWeight)
{
   return aFirstByteWeight * GetScore(aFirstByteFreq, mFirstByteFreq) +
          aSecondByteWeight * GetScore(aSecondByteFreq, mSecondByteFreq);
}

float nsEUCSampler::GetScore(const float* array1, const float* array2)
{
   float s;
   float sum=0.0;
   PRUint16 i;
   for(i=0;i<94;i++) {
     s = array1[i] - array2[i];
     sum += s * s;
   }
   return (float)sqrt((double)sum) / 94.0f;
}

void nsEUCSampler::CalFreq()
{
   PRUint32 i;
   for(i = 0 ; i < 94; i++) {
      mFirstByteFreq[i] = (float)mFirstByteCnt[i] / (float)mTotal;
      mSecondByteFreq[i] = (float)mSecondByteCnt[i] / (float)mTotal;
   }
}

//----------------------------------------------------------
NS_IMPL_ISUPPORTS1(nsXPCOMDetector, nsICharsetDetector);
NS_IMPL_ISUPPORTS1(nsXPCOMStringDetector, nsIStringCharsetDetector);
nsPSMDetector::nsPSMDetector(PRUint8 aItems, nsVerifier** aVerifierSet, nsEUCStatistics** aStatisticsSet)
{
  mClassRunSampler = (nsnull != aStatisticsSet);
  mStatisticsData = aStatisticsSet;
  mVerifier = aVerifierSet;

  mClassItems = aItems;
  Reset();
}
void nsPSMDetector::Reset()
{
  mRunSampler = mClassRunSampler;
  mDone= PR_FALSE;
  mItems = mClassItems;
  NS_ASSERTION(MAX_VERIFIERS >= mItems , "MAX_VERIFIERS is too small!");
  for(PRUint8 i = 0; i < mItems ; i++)
  {
     mState[i] = 0;
     mItemIdx[i] = i;
  }
#ifdef DETECTOR_DEBUG
  mDbgLen = mDbgTest = 0;
#endif   
}
//----------------------------------------------------------
void nsPSMDetector::DataEnd()
{
  if(mRunSampler)
     Sample(nsnull, 0, PR_TRUE);
}
//----------------------------------------------------------

// #define ftang_TRACE_STATE
// #define TRACE_VERIFIER nsCP1252Verifier

PRBool nsPSMDetector::HandleData(const char* aBuf, PRUint32 aLen)
{
  PRUint32 i,j;
  PRUint32 st;
  for(i=0; i < aLen; i++)
  {
     char b = aBuf[i];
     for(j = 0; j < mItems; )
     {
#ifdef ftang_TRACE_STATE
       if(  mVerifier[mItemIdx[j]] == & TRACE_VERIFIER )
       {
           printf("%d = %d\n", i + mDbgLen, mState[j]);
       }
#endif
#ifdef DETECTOR_DEBUG
        mDbgTest++;
#endif 
        st = GETNEXTSTATE( mVerifier[mItemIdx[j]], b, mState[j] );
        if(eItsMe == st) 
        {
#ifdef DETECTOR_DEBUG
            printf("It's %s- byte %d(%x) test %d\n", 
                    mVerifier[mItemIdx[j]]->charset,
                    i+mDbgLen,
                    i+mDbgLen,
                    mDbgTest
                  );
#endif
            Report( mVerifier[mItemIdx[j]]->charset);
            mDone = PR_TRUE;
            return mDone;
        } else if (eError == st) 
        {
#ifdef DETECTOR_DEBUG
            printf("It's NOT %s- byte %d(%x)\n", 
                    mVerifier[mItemIdx[j]]->charset,
                    i+mDbgLen,
                    i+mDbgLen);
#endif
            mItems--;
            if(j < mItems )
            {
                mItemIdx[j] = mItemIdx[mItems];
                mState[j] = mState[mItems];
            } 
        } else {
            mState[j++] = st;
        } 
     }
     if( mItems <= 1) 
     {
         if( 1 == mItems) {
#ifdef DETECTOR_DEBUG
             printf("It's %s- byte %d (%x) Test %d. The only left\n", 
                       mVerifier[mItemIdx[0]]->charset,
                       i+mDbgLen,
                       i+mDbgLen,
                       mDbgTest);
#endif
             Report( mVerifier[mItemIdx[0]]->charset);
         }
         mDone = PR_TRUE;
         return mDone;
     } else {
        // If the only charset left is UCS2LE/UCS2BE and another, report the other
        PRInt32 nonUCS2Num=0;
        PRInt32 nonUCS2Idx=0;
        for(j = 0; j < mItems; j++) {
             if(((&nsUCS2BEVerifier) != mVerifier[mItemIdx[j]]) &&
                ((&nsUCS2LEVerifier) != mVerifier[mItemIdx[j]])) {
                  nonUCS2Num++;
                  nonUCS2Idx = j;
             }
        }
        if(1 == nonUCS2Num) {
#ifdef DETECTOR_DEBUG
             printf("It's %s- byte %d (%x) Test %d. The only left except UCS2LE/BE\n", 
                       mVerifier[mItemIdx[nonUCS2Idx]]->charset,
                       i+mDbgLen,
                       i+mDbgLen,
                       mDbgTest);
#endif
            Report( mVerifier[mItemIdx[nonUCS2Idx]]->charset);
            mDone = PR_TRUE;
            return mDone;
        }
     }
  }
  if(mRunSampler)
     Sample(aBuf, aLen);

#ifdef DETECTOR_DEBUG
  mDbgLen += aLen;
#endif
  return PR_FALSE;
}

void nsPSMDetector::Sample(const char* aBuf, PRUint32 aLen, PRBool aLastChance)
{
     PRInt32 nonUCS2Num=0;
     PRInt32 j;
     PRInt32 eucNum=0;
     for(j = 0; j < mItems; j++) {
        if(nsnull != mStatisticsData[mItemIdx[j]]) 
             eucNum++;
        if(((&nsUCS2BEVerifier) != mVerifier[mItemIdx[j]]) &&
                ((&nsUCS2LEVerifier) != mVerifier[mItemIdx[j]])) {
                  nonUCS2Num++;
        }
     }
     mRunSampler = (eucNum > 1);
     if(mRunSampler) {
        mRunSampler = mSampler.Sample(aBuf, aLen);
        if(((aLastChance && mSampler.GetSomeData()) || 
            mSampler.EnoughData())
           && (eucNum == nonUCS2Num)) {
          mSampler.CalFreq();
#ifdef DETECTOR_DEBUG
          printf("We cannot figure out charset from the encoding, "
                 "All EUC based charset share the same encoding structure.\n"
                 "Detect based on statistics"); 
          if(aLastChance) {
             printf(" after we receive all the data.\n"); 
          } else {
             printf(" after we receive enough data.\n");
          }
#endif
          PRInt32 bestIdx = -1;
          PRInt32 eucCnt=0;
          float bestScore = 0.0f;
          for(j = 0; j < mItems; j++) {
             if((nsnull != mStatisticsData[mItemIdx[j]])  &&
                (&gBig5Statistics != mStatisticsData[mItemIdx[j]]))
             {
                float score = mSampler.GetScore(
                   mStatisticsData[mItemIdx[j]]->mFirstByteFreq,
                   mStatisticsData[mItemIdx[j]]->mFirstByteWeight,
                   mStatisticsData[mItemIdx[j]]->mSecoundByteFreq,
                   mStatisticsData[mItemIdx[j]]->mSecoundByteWeight );
#ifdef DETECTOR_DEBUG
                printf("Differences between %s and this data is %2.8f\n",
                       mVerifier[mItemIdx[j]]->charset,
                       score);
#endif
                if(( 0 == eucCnt++) || (bestScore > score )) {
                   bestScore = score;
                   bestIdx = j;
                } // if(( 0 == eucCnt++) || (bestScore > score )) 
            } // if(nsnull != ...)
         } // for
         if (bestIdx >= 0)
         {
#ifdef DETECTOR_DEBUG
           printf("Based on the statistic, we decide it is %s",
            mVerifier[mItemIdx[bestIdx]]->charset);
#endif
           Report( mVerifier[mItemIdx[bestIdx]]->charset);
           mDone = PR_TRUE;
         }
       } // if (eucNum == nonUCS2Num)
     } // if(mRunSampler)
}
//----------------------------------------------------------
nsXPCOMDetector::nsXPCOMDetector(PRUint8 aItems, nsVerifier **aVer, nsEUCStatistics** aStatisticsSet)
   : nsPSMDetector( aItems, aVer, aStatisticsSet)
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
  mObserver = nsnull;
}
//----------------------------------------------------------
nsXPCOMDetector::~nsXPCOMDetector()
{
  PR_AtomicDecrement(&g_InstanceCount);
}
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetector::Init(
  nsICharsetDetectionObserver* aObserver)
{
  NS_ASSERTION(mObserver == nsnull , "Init twice");
  if(nsnull == aObserver)
     return NS_ERROR_ILLEGAL_VALUE;

  mObserver = aObserver;
  return NS_OK;
}
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetector::DoIt(
  const char* aBuf, PRUint32 aLen, PRBool* oDontFeedMe)
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");

  if((nsnull == aBuf) || (nsnull == oDontFeedMe))
     return NS_ERROR_ILLEGAL_VALUE;

  this->HandleData(aBuf, aLen);
  *oDontFeedMe = mDone;
  return NS_OK;
}
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetector::Done()
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");
  this->DataEnd();
  return NS_OK;
}
//----------------------------------------------------------
void nsXPCOMDetector::Report(const char* charset)
{
  mObserver->Notify(charset, eSureAnswer);
}
//----------------------------------------------------------
nsXPCOMStringDetector::nsXPCOMStringDetector(PRUint8 aItems, nsVerifier** aVer, nsEUCStatistics** aStatisticsSet)
   : nsPSMDetector( aItems, aVer, aStatisticsSet)
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}
//----------------------------------------------------------
nsXPCOMStringDetector::~nsXPCOMStringDetector()
{
  PR_AtomicDecrement(&g_InstanceCount);
}
//----------------------------------------------------------
void nsXPCOMStringDetector::Report(const char* charset)
{
  mResult = charset;
}
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMStringDetector::DoIt(const char* aBuf, PRUint32 aLen, 
                   const char** oCharset, 
                   nsDetectionConfident &oConfident)
{
  mResult = nsnull;
  this->HandleData(aBuf, aLen);

  if( nsnull == mResult) {
     // If we have no result and detector is done - answer no match
     if(mDone) 
     {
        *oCharset = nsnull;
        oConfident = eNoAnswerMatch;
     } else {
        // if we have no answer force the Done method and find the answer
        // if we find one, return it as eBestAnswer
        this->DataEnd();
        *oCharset = mResult;
        oConfident = (mResult) ? eBestAnswer : eNoAnswerMatch ;
     }
  } else {
     // If we have answer, return as eSureAnswer
     *oCharset = mResult;
     oConfident = eSureAnswer;
  }
  this->Reset();
  return NS_OK;
}

