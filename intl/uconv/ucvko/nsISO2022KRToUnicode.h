/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsISO2022KRToUnicode_h__
#define nsISO2022KRToUnicode_h__
#include "nsUCSupport.h"


 
class nsISO2022KRToUnicode : public nsBasicDecoderSupport
{
public:
  nsISO2022KRToUnicode()
  { 
    mState = mState_Init;
    mLastLegalState = mState_ASCII;
    mData = 0;
    mEUCKRDecoder = nullptr;
    mRunLength = 0;
  }

  virtual ~nsISO2022KRToUnicode()
  {
    NS_IF_RELEASE(mEUCKRDecoder);
  }

  NS_IMETHOD Convert(const char * aSrc, int32_t * aSrcLength,
     PRUnichar * aDest, int32_t * aDestLength) ;
  
  NS_IMETHOD GetMaxLength(const char * aSrc, int32_t aSrcLength,
     int32_t * aDestLength) 
  {
    *aDestLength = aSrcLength;
    return NS_OK;
  }

  NS_IMETHOD Reset()
  {
    mState = mState_Init;
    mLastLegalState = mState_ASCII;
    mRunLength = 0;
    return NS_OK;
  }

private:
  enum {
    mState_Init,
    mState_ASCII,
    mState_ESC,
    mState_ESC_24,
    mState_ESC_24_29,
    mState_KSX1001_1992,
    mState_KSX1001_1992_2ndbyte,
    mState_ERROR
  } mState, mLastLegalState;

  uint8_t mData;

  // Length of non-ASCII run
  uint32_t mRunLength;

  nsIUnicodeDecoder *mEUCKRDecoder;
};
#endif // nsISO2022KRToUnicode_h__
