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
#include "nsVerifier.h"
//---- for verifiers
#include "nsSJISVerifier.h"
#include "nsEUCJPVerifier.h"
#include "nsCP1252Verifier.h"
#include "nsUTF8Verifier.h"
#include "nsISO2022JPVerifier.h"
#include "nsISO2022KRVerifier.h"
#include "nsISO2022CNVerifier.h"
#include "nsHZVerifier.h"
#include "nsUCS2BEVerifier.h"
#include "nsUCS2LEVerifier.h"
#include "nsBIG5Verifier.h"
#include "nsGB2312Verifier.h"
#include "nsEUCTWVerifier.h"
#include "nsEUCKRVerifier.h"
//---- end verifiers
#include <stdio.h>
//---- for XPCOM
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsCharDetDll.h"
#include "pratom.h"
// temp fix for XPCOM should be remove after alechf fix the xpcom one
#define MY_NS_IMPL_QUERY_INTERFACE(_class,_classiiddef,_interface)       \
NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr) \
{                                                                        \
  if (NULL == aInstancePtr) {                                            \
    return NS_ERROR_NULL_POINTER;                                        \
  }                                                                      \
                                                                         \
  *aInstancePtr = NULL;                                                  \
                                                                         \
  static NS_DEFINE_IID(kClassIID, _classiiddef);                         \
  if (aIID.Equals(kClassIID)) {                                          \
    *aInstancePtr = (void*) ((_interface*)this);                         \
    NS_ADDREF_THIS();                                                    \
    return NS_OK;                                                        \
  }                                                                      \
  if (aIID.Equals(NS_GET_IID(nsISupports))) {                            \
    *aInstancePtr = (void*) ((nsISupports*)this);                        \
    NS_ADDREF_THIS();                                                    \
    return NS_OK;                                                        \
  }                                                                      \
  return NS_NOINTERFACE;                                                 \
}
#define MY_NS_IMPL_ISUPPORTS(_class,_classiiddef,_interface) \
  NS_IMPL_ADDREF(_class)                       \
  NS_IMPL_RELEASE(_class)                      \
  MY_NS_IMPL_QUERY_INTERFACE(_class,_classiiddef,_interface)
//---- end XPCOM
//---- for XPCOM based detector interfaces
#include "nsICharsetDetector.h"
#include "nsICharsetDetectionObserver.h"
#include "nsIStringCharsetDetector.h"
#include "nsPSMDetectors.h"

NS_DEFINE_CID(kJAPSMDetectorCID,  NS_JA_PSMDETECTOR_CID);
NS_DEFINE_CID(kJAStringPSMDetectorCID,  NS_JA_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kKOPSMDetectorCID,  NS_KO_PSMDETECTOR_CID);
NS_DEFINE_CID(kKOStringPSMDetectorCID,  NS_KO_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHCNPSMDetectorCID,  NS_ZHCN_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHCNStringPSMDetectorCID,  NS_ZHCN_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHTWPSMDetectorCID,  NS_ZHTW_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHTWStringPSMDetectorCID,  NS_ZHTW_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHPSMDetectorCID,  NS_ZH_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHStringPSMDetectorCID,  NS_ZH_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kCJKPSMDetectorCID,  NS_CJK_PSMDETECTOR_CID);
NS_DEFINE_CID(kCJKStringPSMDetectorCID,  NS_CJK_STRING_PSMDETECTOR_CID);
//---- end XPCOM based detector interfaces


#define DETECTOR_DEBUG
typedef struct {
  float mFirstByteFreq[94];
  float mFirstByteStdDev;
  float mFirstByteMean;
  float mFirstByteWeight;
  float mSecoundByteFreq[94];
  float mSecoundByteStdDev;
  float mSecoundByteMean;
  float mSecoundByteWeight;
} nsEUCStatistics;

static nsEUCStatistics gBig5Statistics = 
#include "Big5Statistics.h"
// end of UECTWStatistics.h include

static nsEUCStatistics gEUCTWStatistics = 
#include "EUCTWStatistics.h"
// end of UECTWStatistics.h include

static nsEUCStatistics gGB2312Statistics = 
#include "GB2312Statistics.h"
// end of GB2312Statistics.h include

static nsEUCStatistics gEUCJPStatistics = 
#include "EUCJPStatistics.h"
// end of EUCJPStatistics.h include

static nsEUCStatistics gEUCKRStatistics = 
#include "EUCKRStatistics.h"
// end of EUCKRStatistics.h include

class nsEUCSampler {
  public:
    nsEUCSampler() {
      mTotal =0;
      mThreshold = 200;
	  mState = 0;
      PRInt32 i;
      for(i=0;i<94;i++)
          mFirstByteCnt[i] = mSecondByteCnt[i]=0;
    }
    PRBool EnoughData()  { return mTotal > mThreshold; }
    PRBool GetSomeData() { return mTotal > 1; }
    PRBool Sample(const char* aIn, PRUint32 aLen);
    void   CalFreq();
    float   GetScore(const float* aFirstByteFreq, float aFirstByteWeight,
                     const float* aSecondByteFreq, float aSecondByteWeight);
    float   GetScore(const float* array1, const float* array2);
  private:
    PRUint32 mTotal;
    PRUint32 mThreshold;
    PRInt8 mState;
    PRUint32 mFirstByteCnt[94];
    PRUint32 mSecondByteCnt[94];
    float mFirstByteFreq[94];
    float mSecondByteFreq[94];
   
};
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
/*
 In the current design, we know the following combination of verifiers 
 are not good-

 1. Two or more of the following verifier in one detector:
      nsEUCJPVerifer 
      nsGB2312Verifier
      nsEUCKRVerifer 
      nsEUCTWVerifier
      nsBIG5Verifer 

 */

//==========================================================

#define MAX_VERIFIERS 16
class nsPSMDetector {
public :
   nsPSMDetector(PRUint8 aItems, nsVerifier** aVerifierSet, nsEUCStatistics** aStatisticsSet);
   virtual ~nsPSMDetector() {};

   virtual PRBool HandleData(const char* aBuf, PRUint32 aLen);
   virtual void   DataEnd();
 
protected:
   virtual void Report(const char* charset) = 0;

   PRUint8 mItems;
   PRUint8 mClassItems;
   PRUint8 mState[MAX_VERIFIERS];
   PRUint8 mItemIdx[MAX_VERIFIERS];
   nsVerifier** mVerifier;
   nsEUCStatistics** mStatisticsData;
   PRBool mDone;

   PRBool mRunSampler;
   PRBool mClassRunSampler;
protected:
   void Reset();
   void Sample(const char* aBuf, PRUint32 aLen, PRBool aLastChance=PR_FALSE);
private:
#ifdef DETECTOR_DEBUG
   PRUint32 mDbgTest;
   PRUint32 mDbgLen;
#endif
   nsEUCSampler mSampler;

};
//----------------------------------------------------------
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
          PRInt32 bestIdx;
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
#ifdef DETECTOR_DEBUG
         printf("Based on the statistic, we decide it is %s",
          mVerifier[mItemIdx[bestIdx]]->charset);
#endif
         Report( mVerifier[mItemIdx[bestIdx]]->charset);
         mDone = PR_TRUE;
       } // if (eucNum == nonUCS2Num)
     } // if(mRunSampler)
}
//==========================================================
/*
   This class won't detect x-euc-tw for now. It can  only 
   tell a Big5 document is not x-euc-tw , but cannot tell
   a x-euc-tw docuement is not Big5 unless we hit characters
   defined in CNS 11643 plane 2.
   
   May need improvement ....
 */

#define ZHTW_DETECTOR_NUM_VERIFIERS 7
static nsVerifier *gZhTwVerifierSet[ZHTW_DETECTOR_NUM_VERIFIERS] = {
      &nsUTF8Verifier,
      &nsBIG5Verifier,
      &nsISO2022CNVerifier,
      &nsEUCTWVerifier,
      &nsCP1252Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier
};
static nsEUCStatistics *gZhTwStatisticsSet[ZHTW_DETECTOR_NUM_VERIFIERS] = {
      nsnull,
      &gBig5Statistics,
      nsnull,
      &gEUCTWStatistics,
      nsnull,
      nsnull,
      nsnull
};
//==========================================================
#define KO_DETECTOR_NUM_VERIFIERS 6
static nsVerifier *gKoVerifierSet[KO_DETECTOR_NUM_VERIFIERS] = {
      &nsUTF8Verifier,
      &nsEUCKRVerifier,
      &nsISO2022KRVerifier,
      &nsCP1252Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier
};
//==========================================================
#define ZHCN_DETECTOR_NUM_VERIFIERS 7
static nsVerifier *gZhCnVerifierSet[ZHCN_DETECTOR_NUM_VERIFIERS] = {
      &nsUTF8Verifier,
      &nsGB2312Verifier,
      &nsISO2022CNVerifier,
      &nsHZVerifier,
      &nsCP1252Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier
};
//==========================================================
#define JA_DETECTOR_NUM_VERIFIERS 7
static nsVerifier *gJaVerifierSet[JA_DETECTOR_NUM_VERIFIERS] = {
      &nsUTF8Verifier,
      &nsSJISVerifier,
      &nsEUCJPVerifier,
      &nsISO2022JPVerifier,
      &nsCP1252Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier
};
//==========================================================
#define ZH_DETECTOR_NUM_VERIFIERS 9
static nsVerifier *gZhVerifierSet[ZH_DETECTOR_NUM_VERIFIERS] = {
      &nsUTF8Verifier,
      &nsGB2312Verifier,
      &nsBIG5Verifier,
      &nsISO2022CNVerifier,
      &nsHZVerifier,
      &nsEUCTWVerifier,
      &nsCP1252Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier
};
static nsEUCStatistics *gZhStatisticsSet[ZH_DETECTOR_NUM_VERIFIERS] = {
      nsnull,
      &gGB2312Statistics,
      &gBig5Statistics,
      nsnull,
      nsnull,
      &gEUCTWStatistics,
      nsnull,
      nsnull,
      nsnull
};
//==========================================================
#define CJK_DETECTOR_NUM_VERIFIERS 14
static nsVerifier *gCJKVerifierSet[CJK_DETECTOR_NUM_VERIFIERS] = {
      &nsUTF8Verifier,
      &nsSJISVerifier,
      &nsEUCJPVerifier,
      &nsISO2022JPVerifier,
      &nsEUCKRVerifier,
      &nsISO2022KRVerifier,
      &nsBIG5Verifier,
      &nsEUCTWVerifier,
      &nsGB2312Verifier,
      &nsISO2022CNVerifier,
      &nsHZVerifier,
      &nsCP1252Verifier,
      &nsUCS2BEVerifier,
      &nsUCS2LEVerifier
};
static nsEUCStatistics *gCJKStatisticsSet[CJK_DETECTOR_NUM_VERIFIERS] = {
      nsnull,
      nsnull,
      &gEUCJPStatistics,
      nsnull,
      &gEUCKRStatistics,
      nsnull,
      &gBig5Statistics,
      &gEUCTWStatistics,
      &gGB2312Statistics,
      nsnull,
      nsnull,
      nsnull,
      nsnull,
      nsnull
};
//==========================================================
class nsXPCOMDetector : 
      private nsPSMDetector,
      public nsICharsetDetector // Implement the interface 
{
  NS_DECL_ISUPPORTS
public:
    nsXPCOMDetector(PRUint8 aItems, nsVerifier** aVer, nsEUCStatistics** aStatisticsSet);
    virtual ~nsXPCOMDetector();
  NS_IMETHOD Init(nsICharsetDetectionObserver* aObserver);
  NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, PRBool* oDontFeedMe);
  NS_IMETHOD Done();

protected:
  virtual void Report(const char* charset);

private:
  nsICharsetDetectionObserver* mObserver;
};
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
  NS_IF_RELEASE(mObserver);
  PR_AtomicDecrement(&g_InstanceCount);
}
//----------------------------------------------------------
MY_NS_IMPL_ISUPPORTS(nsXPCOMDetector,NS_GET_IID(nsICharsetDetector), nsICharsetDetector)
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetector::Init(
  nsICharsetDetectionObserver* aObserver)
{
  NS_ASSERTION(mObserver == nsnull , "Init twice");
  if(nsnull == aObserver)
     return NS_ERROR_ILLEGAL_VALUE;

  NS_IF_ADDREF(aObserver);
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
//==========================================================
class nsXPCOMStringDetector : 
      private nsPSMDetector,
      public nsIStringCharsetDetector // Implement the interface 
{
  NS_DECL_ISUPPORTS
public:
    nsXPCOMStringDetector(PRUint8 aItems, nsVerifier** aVer, nsEUCStatistics** aStatisticsSet);
    virtual ~nsXPCOMStringDetector();
    NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, 
                   const char** oCharset, 
                   nsDetectionConfident &oConfident);
protected:
  virtual void Report(const char* charset);
private:
  const char* mResult;
};
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
MY_NS_IMPL_ISUPPORTS(nsXPCOMStringDetector,NS_GET_IID(nsIStringCharsetDetector), nsIStringCharsetDetector)
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
//==========================================================

class nsXPCOMDetectorFactory : public nsIFactory {
   NS_DECL_ISUPPORTS

public:
   nsXPCOMDetectorFactory(const nsCID& aCID) {
     NS_INIT_REFCNT();
     mCID = aCID;
     PR_AtomicIncrement(&g_InstanceCount);
   }
   virtual ~nsXPCOMDetectorFactory() {
     PR_AtomicDecrement(&g_InstanceCount);
   }

   NS_IMETHOD CreateInstance(nsISupports* aDelegate, const nsIID& aIID, void** aResult);
   NS_IMETHOD LockFactory(PRBool aLock);
private:
   nsCID mCID;

};

//--------------------------------------------------------------
NS_DEFINE_IID( kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS( nsXPCOMDetectorFactory , kIFactoryIID);

NS_IMETHODIMP nsXPCOMDetectorFactory::CreateInstance(
    nsISupports* aDelegate, const nsIID &aIID, void** aResult)
{
  if(NULL == aResult)
        return NS_ERROR_NULL_POINTER;
  if(NULL != aDelegate)
        return NS_ERROR_NO_AGGREGATION;

  *aResult = NULL;

  nsXPCOMDetector *inst1 = nsnull;
  nsXPCOMStringDetector *inst2 = nsnull;
 
  if (mCID.Equals(kJAPSMDetectorCID)) {
      inst1 = new nsXPCOMDetector(JA_DETECTOR_NUM_VERIFIERS, gJaVerifierSet, nsnull);
  } else if (mCID.Equals(kKOPSMDetectorCID)) {
      inst1 = new nsXPCOMDetector(KO_DETECTOR_NUM_VERIFIERS, gKoVerifierSet, nsnull);
  } else if (mCID.Equals(kZHCNPSMDetectorCID)) {
      inst1 = new nsXPCOMDetector(ZHCN_DETECTOR_NUM_VERIFIERS, gZhCnVerifierSet, nsnull);
  } else if (mCID.Equals(kZHTWPSMDetectorCID)) {
      inst1 = new nsXPCOMDetector(ZHTW_DETECTOR_NUM_VERIFIERS, gZhTwVerifierSet, gZhTwStatisticsSet);
  } else if (mCID.Equals(kZHPSMDetectorCID)) {
      inst1 = new nsXPCOMDetector(ZH_DETECTOR_NUM_VERIFIERS, gZhVerifierSet, gZhStatisticsSet);
  } else if (mCID.Equals(kCJKPSMDetectorCID)) {
      inst1 = new nsXPCOMDetector(CJK_DETECTOR_NUM_VERIFIERS, gCJKVerifierSet, gCJKStatisticsSet);
  } else if (mCID.Equals(kJAStringPSMDetectorCID)) {
      inst2 = new nsXPCOMStringDetector(JA_DETECTOR_NUM_VERIFIERS - 3, gJaVerifierSet, nsnull);
  } else if (mCID.Equals(kKOStringPSMDetectorCID)) {
      inst2 = new nsXPCOMStringDetector(KO_DETECTOR_NUM_VERIFIERS - 3, gKoVerifierSet, nsnull);
  } else if (mCID.Equals(kZHCNStringPSMDetectorCID)) {
      inst2 = new nsXPCOMStringDetector(ZHCN_DETECTOR_NUM_VERIFIERS - 3, gZhCnVerifierSet, nsnull);
  } else if (mCID.Equals(kZHTWStringPSMDetectorCID)) {
      inst2 = new nsXPCOMStringDetector(ZHTW_DETECTOR_NUM_VERIFIERS - 3, gZhTwVerifierSet, gZhTwStatisticsSet);
  } else if (mCID.Equals(kZHStringPSMDetectorCID)) {
      inst2 = new nsXPCOMStringDetector(ZH_DETECTOR_NUM_VERIFIERS - 3, gZhVerifierSet, gZhStatisticsSet);
  } else if (mCID.Equals(kCJKStringPSMDetectorCID)) {
      inst2 = new nsXPCOMStringDetector(CJK_DETECTOR_NUM_VERIFIERS - 3, gCJKVerifierSet, gCJKStatisticsSet);
  }
  if((NULL == inst1) && (NULL == inst2)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res = NS_OK;

  if(inst1)
     res =inst1->QueryInterface(aIID, aResult);
  else
     res =inst2->QueryInterface(aIID, aResult);

  if(NS_FAILED(res)) {
     if(inst1)
       delete inst1;
     if(inst2)
       delete inst2;
  }

  return res;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetectorFactory::LockFactory(PRBool aLock)
{
  if(aLock)
     PR_AtomicIncrement( &g_LockCount );
  else
     PR_AtomicDecrement( &g_LockCount );
  return NS_OK;
}
//--------------------------------------------------------------
nsIFactory* NEW_PSMDETECTOR_FACTORY(const nsCID& aClass) {
  return new nsXPCOMDetectorFactory(aClass);
};


       
