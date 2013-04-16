/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"

#include "nsUniversalDetector.h"

#include "nsMBCSGroupProber.h"
#include "nsSBCSGroupProber.h"
#include "nsEscCharsetProber.h"
#include "nsLatin1Prober.h"

nsUniversalDetector::nsUniversalDetector(uint32_t aLanguageFilter)
{
  mDone = false;
  mBestGuess = -1;   //illegal value as signal
  mInTag = false;
  mEscCharSetProber = nullptr;

  mStart = true;
  mDetectedCharset = nullptr;
  mGotData = false;
  mInputState = ePureAscii;
  mLastChar = '\0';
  mLanguageFilter = aLanguageFilter;

  uint32_t i;
  for (i = 0; i < NUM_OF_CHARSET_PROBERS; i++)
    mCharSetProbers[i] = nullptr;
}

nsUniversalDetector::~nsUniversalDetector() 
{
  for (int32_t i = 0; i < NUM_OF_CHARSET_PROBERS; i++)
    delete mCharSetProbers[i];

  delete mEscCharSetProber;
}

void 
nsUniversalDetector::Reset()
{
  mDone = false;
  mBestGuess = -1;   //illegal value as signal
  mInTag = false;

  mStart = true;
  mDetectedCharset = nullptr;
  mGotData = false;
  mInputState = ePureAscii;
  mLastChar = '\0';

  if (mEscCharSetProber)
    mEscCharSetProber->Reset();

  uint32_t i;
  for (i = 0; i < NUM_OF_CHARSET_PROBERS; i++)
    if (mCharSetProbers[i])
      mCharSetProbers[i]->Reset();
}

//---------------------------------------------------------------------
#define SHORTCUT_THRESHOLD      (float)0.95
#define MINIMUM_THRESHOLD      (float)0.20

nsresult nsUniversalDetector::HandleData(const char* aBuf, uint32_t aLen)
{
  if(mDone) 
    return NS_OK;

  if (aLen > 0)
    mGotData = true;

  //If the data starts with BOM, we know it is UTF
  if (mStart)
  {
    mStart = false;
    if (aLen > 2)
      switch (aBuf[0])
        {
        case '\xEF':
          if (('\xBB' == aBuf[1]) && ('\xBF' == aBuf[2]))
            // EF BB BF  UTF-8 encoded BOM
            mDetectedCharset = "UTF-8";
        break;
        case '\xFE':
          if ('\xFF' == aBuf[1])
            // FE FF  UTF-16, big endian BOM
            mDetectedCharset = "UTF-16BE";
        break;
        case '\xFF':
          if ('\xFE' == aBuf[1])
            // FF FE  UTF-16, little endian BOM
            mDetectedCharset = "UTF-16LE";
        break;
      }  // switch

      if (mDetectedCharset)
      {
        mDone = true;
        return NS_OK;
      }
  }
  
  uint32_t i;
  for (i = 0; i < aLen; i++)
  {
    //other than 0xa0, if every othe character is ascii, the page is ascii
    if (aBuf[i] & '\x80' && aBuf[i] != '\xA0')  //Since many Ascii only page contains NBSP 
    {
      //we got a non-ascii byte (high-byte)
      if (mInputState != eHighbyte)
      {
        //adjust state
        mInputState = eHighbyte;

        //kill mEscCharSetProber if it is active
        if (mEscCharSetProber) {
          delete mEscCharSetProber;
          mEscCharSetProber = nullptr;
        }

        //start multibyte and singlebyte charset prober
        if (nullptr == mCharSetProbers[0])
        {
          mCharSetProbers[0] = new nsMBCSGroupProber(mLanguageFilter);
          if (nullptr == mCharSetProbers[0])
            return NS_ERROR_OUT_OF_MEMORY;
        }
        if (nullptr == mCharSetProbers[1] &&
            (mLanguageFilter & NS_FILTER_NON_CJK))
        {
          mCharSetProbers[1] = new nsSBCSGroupProber;
          if (nullptr == mCharSetProbers[1])
            return NS_ERROR_OUT_OF_MEMORY;
        }
        if (nullptr == mCharSetProbers[2])
        {
          mCharSetProbers[2] = new nsLatin1Prober; 
          if (nullptr == mCharSetProbers[2])
            return NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
    else
    {
      //ok, just pure ascii so far
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
    if (nullptr == mEscCharSetProber) {
      mEscCharSetProber = new nsEscCharSetProber(mLanguageFilter);
      if (nullptr == mEscCharSetProber)
        return NS_ERROR_OUT_OF_MEMORY;
    }
    st = mEscCharSetProber->HandleData(aBuf, aLen);
    if (st == eFoundIt)
    {
      mDone = true;
      mDetectedCharset = mEscCharSetProber->GetCharSetName();
    }
    break;
  case eHighbyte:
    for (i = 0; i < NUM_OF_CHARSET_PROBERS; i++)
    {
      if (mCharSetProbers[i])
      {
        st = mCharSetProbers[i]->HandleData(aBuf, aLen);
        if (st == eFoundIt) 
        {
          mDone = true;
          mDetectedCharset = mCharSetProbers[i]->GetCharSetName();
          return NS_OK;
        }
      } 
    }
    break;

  default:  //pure ascii
    ;//do nothing here
  }
  return NS_OK;
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
    mDone = true;
    Report(mDetectedCharset);
    return;
  }
  
  switch (mInputState)
  {
  case eHighbyte:
    {
      float proberConfidence;
      float maxProberConfidence = (float)0.0;
      int32_t maxProber = 0;

      for (int32_t i = 0; i < NUM_OF_CHARSET_PROBERS; i++)
      {
        if (mCharSetProbers[i])
        {
          proberConfidence = mCharSetProbers[i]->GetConfidence();
          if (proberConfidence > maxProberConfidence)
          {
            maxProberConfidence = proberConfidence;
            maxProber = i;
          }
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
