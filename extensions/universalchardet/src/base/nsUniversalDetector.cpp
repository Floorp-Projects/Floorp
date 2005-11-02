/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * CONFIDENTIAL AND PROPRIETARY SOURCE CODE
 * OF NETSCAPE COMMUNICATIONS CORPORATION
 *
 * Copyright (c) 2000 Netscape Communications Corporation.
 * All Rights Reserved.
 *
 * Use of this Source Code is subject to the terms of the applicable
 * license agreement from Netscape Communications Corporation.
 *
 * The copyright notice(s) in this Source Code does not indicate actual
 * or intended publication of this Source Code.
 */

#include "nscore.h"
#include <stdio.h>

#include "nsUniversalDetector.h"

#include "nsUniversalCharDetDll.h"
//---- for XPCOM
#include "nsIFactory.h"
#include "nsISupports.h"
#include "pratom.h"
#include "prmem.h"
#include "nsCOMPtr.h"

#include "nsMBCSGroupProber.h"
#include "nsSBCSGroupProber.h"
#include "nsEscCharsetProber.h"

static NS_DEFINE_CID(kUniversalDetectorCID, NS_UNIVERSAL_DETECTOR_CID);
static NS_DEFINE_CID(kUniversalStringDetectorCID, NS_UNIVERSAL_STRING_DETECTOR_CID);

nsUniversalDetector::nsUniversalDetector()
{
  mDone = PR_FALSE;
	mBestGuess = -1;   //illegal value as signal
	mAvailable = PR_FALSE;
	mInTag = PR_FALSE;
  mEscCharSetProber = nsnull;

  mStart = PR_TRUE;
  mDetectedCharset = nsnull;
  mGotData = PR_FALSE;
  mInputState = ePureAscii;
  mLastChar = '\0';
}

nsUniversalDetector::~nsUniversalDetector() 
{
  if (eHighbyte == mInputState)
     for (PRInt32 i = 0; i < NUM_OF_CHARSET_PROBERS; i++)
      delete mCharSetProbers[i];
  if (mEscCharSetProber)
    delete mEscCharSetProber;
}

//---------------------------------------------------------------------
#define SHORTCUT_THRESHOLD      (float)0.95
#define MINIMUM_THRESHOLD      (float)0.20

void nsUniversalDetector::HandleData(const char* aBuf, PRUint32 aLen)
{
  if(mDone) 
    return;

  if (aLen > 0)
    mGotData = PR_TRUE;

  //If the data start with BOM, we know it is UCS2
  if (mStart)
  {
    mStart = PR_FALSE;
    if (aLen > 1)
      if (aBuf[0] == '\376' && aBuf[1] == '\377')
        mDetectedCharset = "UTF-16BE";
      else if (aBuf[0] == '\377' && aBuf[1] == '\376')
        mDetectedCharset = "UTF-16LE";

      if (mDetectedCharset)
      {
        mDone = PR_TRUE;
        return;
      }
  }
  
  PRUint32 i;
  for (i = 0; i < aLen; i++)
  {
    if (aBuf[i] & 0x80 && aBuf[i] != (char)0xA0)  //Since many Ascii only page contains NBSP 
    {
      if (mInputState != eHighbyte)
      {
        mInputState = eHighbyte;
        if (mEscCharSetProber)
          delete mEscCharSetProber;

        mCharSetProbers[0] = new nsMBCSGroupProber;
        mCharSetProbers[1] = new nsSBCSGroupProber;
      }
    }
    else
    {
      if ( ePureAscii == mInputState &&
        (aBuf[i] == '\033' || (aBuf[i] == '{' && mLastChar == '~')) )
      {
        //found escape character or HZ "~{"
        mInputState = eEscAscii;
      }
      mLastChar = aBuf[i];
    }
  }

  nsProbingState st;
  switch (mInputState)
  {
  case eEscAscii:
    if (nsnull == mEscCharSetProber)
      mEscCharSetProber = new nsEscCharSetProber;
    st = mEscCharSetProber->HandleData(aBuf, aLen);
    if (st == eFoundIt)
    {
      mDone = PR_TRUE;
      mDetectedCharset = mEscCharSetProber->GetCharSetName();
    }
    break;
  case eHighbyte:
    for (i = 0; i < NUM_OF_CHARSET_PROBERS; i++)
    {
      st = mCharSetProbers[i]->HandleData(aBuf, aLen);
      if (st == eFoundIt) 
      {
        mDone = PR_TRUE;
        mDetectedCharset = mCharSetProbers[i]->GetCharSetName();
    #ifdef DEBUG_chardet
           printf(" %d Prober found charset %s in HandleData. \r\n", i, mCharSetProbers[i]->GetCharSetName());
    #endif
        return;
      } 
    }
    break;

  default:  //pure ascii
    ;//do nothing here
  }
  return ;  
}


//---------------------------------------------------------------------
void nsUniversalDetector::DataEnd()
{
  if (!mGotData)
  {
    // we haven't got any data yet, return immediately 
    // caller program sometimes call DataEnd before anything has been sent to detector
    return;
  }

  if (mDetectedCharset)
  {
    mDone = PR_TRUE;
    Report(mDetectedCharset);
#ifdef DEBUG_chardet
   printf("New Charset Prober found charset %s in HandleData. \r\n", mDetectedCharset);
#endif
    return;
  }
  
  switch (mInputState)
  {
  case eHighbyte:
    {
      float proberConfidence;
      float maxProberConfidence = (float)0.0;
      PRInt32 maxProber;

      for (PRInt32 i = 0; i < NUM_OF_CHARSET_PROBERS; i++)
      {
        proberConfidence = mCharSetProbers[i]->GetConfidence();
    #ifdef DEBUG_chardet
           printf("%d Prober has confidence %f in charset %s in DataEnd. \r\n", i, proberConfidence, mCharSetProbers[i]->GetCharSetName());
    #endif

        if (proberConfidence > maxProberConfidence)
        {
          maxProberConfidence = proberConfidence;
          maxProber = i;
        }
      }
      //do not report anything because we are not confident of it, that's in fact a negative answer
      if (maxProberConfidence > MINIMUM_THRESHOLD)
        Report(mCharSetProbers[maxProber]->GetCharSetName());
    }
    break;
  case eEscAscii:
    break;
  default:
    ;
  }
  return;
}

//---------------------------------------------------------------------
nsUniversalXPCOMDetector:: nsUniversalXPCOMDetector()
	     : nsUniversalDetector()
{
    NS_INIT_REFCNT();
    mObserver = nsnull;
}
//---------------------------------------------------------------------
nsUniversalXPCOMDetector::~nsUniversalXPCOMDetector() 
{
    NS_IF_RELEASE(mObserver);
}
//---------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(nsUniversalXPCOMDetector, nsICharsetDetector)

//---------------------------------------------------------------------
NS_IMETHODIMP nsUniversalXPCOMDetector::Init(
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
NS_IMETHODIMP nsUniversalXPCOMDetector::DoIt(
  const char* aBuf, PRUint32 aLen, PRBool* oDontFeedMe)
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");

  if((nsnull == aBuf) || (nsnull == oDontFeedMe))
     return NS_ERROR_ILLEGAL_VALUE;

  this->HandleData(aBuf, aLen);
	
  if (mDone)
  {

    if (mDetectedCharset)
    {
	    Report(mDetectedCharset);
    }

	 *oDontFeedMe = PR_TRUE;
  }
  *oDontFeedMe = PR_FALSE;
  return NS_OK;
}
//----------------------------------------------------------
NS_IMETHODIMP nsUniversalXPCOMDetector::Done()
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");
  this->DataEnd();
  return NS_OK;
}
//----------------------------------------------------------
void nsUniversalXPCOMDetector::Report(const char* aCharset)
{
  NS_ASSERTION(mObserver != nsnull , "have not init yet");
#ifdef DEBUG_chardet
       printf("New Charset Prober report charset %s . \r\n", aCharset);
#endif
  mObserver->Notify(aCharset, eBestAnswer);
}


//---------------------------------------------------------------------
nsUniversalXPCOMStringDetector:: nsUniversalXPCOMStringDetector()
	     : nsUniversalDetector()
{
    NS_INIT_REFCNT();
}
//---------------------------------------------------------------------
nsUniversalXPCOMStringDetector::~nsUniversalXPCOMStringDetector() 
{
}
//---------------------------------------------------------------------
//MY_NS_IMPL_ISUPPORTS(nsUniversalXPCOMStringDetector, 
//  NS_GET_IID(nsIStringCharsetDetector), nsIStringCharsetDetector)
NS_IMPL_ISUPPORTS1(nsUniversalXPCOMStringDetector, nsIStringCharsetDetector)
//---------------------------------------------------------------------
void nsUniversalXPCOMStringDetector::Report(const char *aCharset) 
{
   mResult = aCharset;
#ifdef DEBUG_chardet
       printf("New Charset Prober report charset %s . \r\n", aCharset);
#endif
}
//---------------------------------------------------------------------
NS_IMETHODIMP nsUniversalXPCOMStringDetector::DoIt(const char* aBuf, PRUint32 aLen, 
                     const char** oCharset, nsDetectionConfident &oConf)
{
   mResult = nsnull;
   this->HandleData(aBuf, aLen); 
   this->DataEnd();
   if (mResult)
   {
       *oCharset=mResult;
       oConf = eBestAnswer;
   }
   return NS_OK;
}
       

