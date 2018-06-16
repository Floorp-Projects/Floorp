/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nscore.h"
#include "nsCyrillicProb.h"
#include <stdio.h>

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsICharsetDetector.h"
#include "nsICharsetDetectionObserver.h"
#include "nsIStringCharsetDetector.h"
#include "nsCyrillicDetector.h"

//----------------------------------------------------------------------
// Interface nsISupports [implementation]
NS_IMPL_ISUPPORTS(nsCyrXPCOMDetector, nsICharsetDetector)
NS_IMPL_ISUPPORTS(nsCyrXPCOMStringDetector, nsIStringCharsetDetector)

void nsCyrillicDetector::HandleData(const char* aBuf, uint32_t aLen)
{
   uint8_t cls;
   const char* b;
   uint32_t i;
   if(mDone)
      return;
   for(i=0, b=aBuf;i<aLen;i++,b++)
   {
     for(unsigned j=0;j<mItems;j++)
     {
        if( 0x80 & *b)
           cls = mCyrillicClass[j][(*b) & 0x7F];
        else
           cls = 0;
        NS_ASSERTION( cls <= 32 , "illegal character class");
        mProb[j] += gCyrillicProb[mLastCls[j]][cls];
        mLastCls[j] = cls;
     }
   }
   // We now only based on the first block we receive
   DataEnd();
}

//---------------------------------------------------------------------
#define THRESHOLD_RATIO 1.5f
void nsCyrillicDetector::DataEnd()
{
   uint32_t max=0;
   uint8_t  maxIdx=0;
   uint8_t j;
   if(mDone)
      return;
   for(j=0;j<mItems;j++) {
      if(mProb[j] > max)
      {
           max = mProb[j];
           maxIdx= j;
      }
   }

   if( 0 == max ) // if we didn't get any 8 bits data
     return;

#ifdef DEBUG
   for(j=0;j<mItems;j++)
      printf("Charset %s->\t%d\n", mCharsets[j], mProb[j]);
#endif
   this->Report(mCharsets[maxIdx]);
   mDone = true;
}

//---------------------------------------------------------------------
nsCyrXPCOMDetector:: nsCyrXPCOMDetector(uint8_t aItems,
                      const uint8_t ** aCyrillicClass,
                      const char **aCharsets)
	     : nsCyrillicDetector(aItems, aCyrillicClass, aCharsets)
{
    mObserver = nullptr;
}

//---------------------------------------------------------------------
nsCyrXPCOMDetector::~nsCyrXPCOMDetector()
{
}

//---------------------------------------------------------------------
NS_IMETHODIMP nsCyrXPCOMDetector::Init(
  nsICharsetDetectionObserver* aObserver)
{
  NS_ASSERTION(mObserver == nullptr , "Init twice");
  if(nullptr == aObserver)
     return NS_ERROR_ILLEGAL_VALUE;

  mObserver = aObserver;
  return NS_OK;
}

//----------------------------------------------------------
NS_IMETHODIMP nsCyrXPCOMDetector::DoIt(
  const char* aBuf, uint32_t aLen, bool* oDontFeedMe)
{
  NS_ASSERTION(mObserver != nullptr , "have not init yet");

  if((nullptr == aBuf) || (nullptr == oDontFeedMe))
     return NS_ERROR_ILLEGAL_VALUE;

  this->HandleData(aBuf, aLen);
  *oDontFeedMe = false;
  return NS_OK;
}

//----------------------------------------------------------
NS_IMETHODIMP nsCyrXPCOMDetector::Done()
{
  NS_ASSERTION(mObserver != nullptr , "have not init yet");
  this->DataEnd();
  return NS_OK;
}

//----------------------------------------------------------
void nsCyrXPCOMDetector::Report(const char* aCharset)
{
  NS_ASSERTION(mObserver != nullptr , "have not init yet");
  mObserver->Notify(aCharset, eBestAnswer);
}

//---------------------------------------------------------------------
nsCyrXPCOMStringDetector:: nsCyrXPCOMStringDetector(uint8_t aItems,
                      const uint8_t ** aCyrillicClass,
                      const char **aCharsets)
	     : nsCyrillicDetector(aItems, aCyrillicClass, aCharsets)
	     , mResult(nullptr)
{
}

//---------------------------------------------------------------------
nsCyrXPCOMStringDetector::~nsCyrXPCOMStringDetector()
{
}

//---------------------------------------------------------------------
void nsCyrXPCOMStringDetector::Report(const char *aCharset)
{
   mResult = aCharset;
}

//---------------------------------------------------------------------
NS_IMETHODIMP nsCyrXPCOMStringDetector::DoIt(const char* aBuf, uint32_t aLen,
                     const char** oCharset, nsDetectionConfident &oConf)
{
   mResult = nullptr;
   mDone = false;
   this->HandleData(aBuf, aLen);
   this->DataEnd();
   *oCharset=mResult;
   oConf = eBestAnswer;
   return NS_OK;
}


