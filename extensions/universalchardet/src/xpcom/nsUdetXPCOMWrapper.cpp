/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"

#include "nsUniversalDetector.h"
#include "nsUdetXPCOMWrapper.h"
#include "nsCharSetProber.h" // for DumpStatus

#include "nsUniversalCharDetDll.h"
//---- for XPCOM
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"

//---------------------------------------------------------------------
nsXPCOMDetector:: nsXPCOMDetector(uint32_t aLanguageFilter)
 : nsUniversalDetector(aLanguageFilter)
{
}
//---------------------------------------------------------------------
nsXPCOMDetector::~nsXPCOMDetector() 
{
}
//---------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsXPCOMDetector, nsICharsetDetector)

//---------------------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetector::Init(
              nsICharsetDetectionObserver* aObserver)
{
  NS_ASSERTION(mObserver == nullptr , "Init twice");
  if(nullptr == aObserver)
    return NS_ERROR_ILLEGAL_VALUE;

  mObserver = aObserver;
  return NS_OK;
}
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetector::DoIt(const char* aBuf,
              uint32_t aLen, bool* oDontFeedMe)
{
  NS_ASSERTION(mObserver != nullptr , "have not init yet");

  if((nullptr == aBuf) || (nullptr == oDontFeedMe))
    return NS_ERROR_ILLEGAL_VALUE;

  this->Reset();
  nsresult rv = this->HandleData(aBuf, aLen);
  if (NS_FAILED(rv))
    return rv;

  if (mDone)
  {
    if (mDetectedCharset)
      Report(mDetectedCharset);

    *oDontFeedMe = true;
  }
  *oDontFeedMe = false;
  return NS_OK;
}
//----------------------------------------------------------
NS_IMETHODIMP nsXPCOMDetector::Done()
{
  NS_ASSERTION(mObserver != nullptr , "have not init yet");
#ifdef DEBUG_chardet
  for (int32_t i = 0; i < NUM_OF_CHARSET_PROBERS; i++)
  {
    // If no data was received the array might stay filled with nulls
    // the way it was initialized in the constructor.
    if (mCharSetProbers[i])
      mCharSetProbers[i]->DumpStatus();
  }
#endif

  this->DataEnd();
  return NS_OK;
}
//----------------------------------------------------------
void nsXPCOMDetector::Report(const char* aCharset)
{
  NS_ASSERTION(mObserver != nullptr , "have not init yet");
#ifdef DEBUG_chardet
  printf("Universal Charset Detector report charset %s . \r\n", aCharset);
#endif
  mObserver->Notify(aCharset, eBestAnswer);
}


//---------------------------------------------------------------------
nsXPCOMStringDetector:: nsXPCOMStringDetector(uint32_t aLanguageFilter)
  : nsUniversalDetector(aLanguageFilter)
{
}
//---------------------------------------------------------------------
nsXPCOMStringDetector::~nsXPCOMStringDetector() 
{
}
//---------------------------------------------------------------------
NS_IMPL_ISUPPORTS(nsXPCOMStringDetector, nsIStringCharsetDetector)
//---------------------------------------------------------------------
void nsXPCOMStringDetector::Report(const char *aCharset) 
{
  mResult = aCharset;
#ifdef DEBUG_chardet
  printf("New Charset Prober report charset %s . \r\n", aCharset);
#endif
}
//---------------------------------------------------------------------
NS_IMETHODIMP nsXPCOMStringDetector::DoIt(const char* aBuf,
                     uint32_t aLen, const char** oCharset,
                     nsDetectionConfident &oConf)
{
  mResult = nullptr;
  this->Reset();
  nsresult rv = this->HandleData(aBuf, aLen); 
  if (NS_FAILED(rv))
    return rv;
  this->DataEnd();
  if (mResult)
  {
    *oCharset=mResult;
    oConf = eBestAnswer;
  }
  return NS_OK;
}
