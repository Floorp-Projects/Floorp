/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsShiftJISToUnicode_h__
#define nsShiftJISToUnicode_h__
#include "nsISupports.h"
#include "nsUCSupport.h"


class nsShiftJISToUnicode : public nsBasicDecoderSupport
{
public:

 nsShiftJISToUnicode() 
     { 
         mState=0; mData=0; 
     }
 virtual ~nsShiftJISToUnicode() {}

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
        mState = 0;
        return NS_OK;
     }

  virtual PRUnichar GetCharacterForUnMapped();

private:

private:
 int32_t  mState;
 int32_t mData;
};

class nsEUCJPToUnicodeV2 : public nsBasicDecoderSupport
{
public:

 nsEUCJPToUnicodeV2() 
     { 
          mState=0; mData=0; 
     }
 virtual ~nsEUCJPToUnicodeV2() {}

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
        mState = 0;
        return NS_OK;
     }

private:
 int32_t  mState;
 int32_t mData;
};
 
class nsISO2022JPToUnicodeV2 : public nsBasicDecoderSupport
{
public:

 nsISO2022JPToUnicodeV2() 
     { 
        mState = mState_ASCII;
        mLastLegalState = mState_ASCII;
        mData = 0;
        mRunLength = 0;
        G2charset = G2_unknown;
        mGB2312Decoder = nullptr;
        mEUCKRDecoder = nullptr;
        mISO88597Decoder = nullptr;
     }
 virtual ~nsISO2022JPToUnicodeV2()
     {
        NS_IF_RELEASE(mGB2312Decoder);
        NS_IF_RELEASE(mEUCKRDecoder);
        NS_IF_RELEASE(mISO88597Decoder);
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
        mState = mState_ASCII;
        mLastLegalState = mState_ASCII;
        mRunLength = 0;
        return NS_OK;
     }

private:
 enum {
   mState_ASCII,
   mState_ESC,
   mState_ESC_28,
   mState_ESC_24,
   mState_ESC_24_28,
   mState_JISX0201_1976Roman,
   mState_JISX0201_1976Kana,
   mState_JISX0208_1978,
   mState_GB2312_1980,
   mState_JISX0208_1983,
   mState_KSC5601_1987,
   mState_JISX0212_1990,
   mState_JISX0208_1978_2ndbyte,
   mState_GB2312_1980_2ndbyte,
   mState_JISX0208_1983_2ndbyte,
   mState_KSC5601_1987_2ndbyte,
   mState_JISX0212_1990_2ndbyte,
   mState_ESC_2e,
   mState_ESC_4e,
   mState_ERROR
 } mState, mLastLegalState;
 int32_t mData;
 int32_t mRunLength; // the length of a non-ASCII run
 enum {
   G2_unknown,
   G2_ISO88591,
   G2_ISO88597
 } G2charset;
 nsIUnicodeDecoder *mGB2312Decoder;
 nsIUnicodeDecoder *mEUCKRDecoder;
 nsIUnicodeDecoder *mISO88597Decoder;
};
#endif // nsShiftJISToUnicode_h__
