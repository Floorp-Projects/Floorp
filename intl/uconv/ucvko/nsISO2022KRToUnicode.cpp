/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsISO2022KRToUnicode.h"
#include "nsUCSupport.h"
#include "nsICharsetConverterManager.h"
#include "nsServiceManagerUtils.h"

NS_IMETHODIMP nsISO2022KRToUnicode::Convert(const char * aSrc, int32_t * aSrcLen, PRUnichar * aDest, int32_t * aDestLen)
{
  static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

  const unsigned char* srcEnd = (unsigned char*)aSrc + *aSrcLen;
  const unsigned char* src =(unsigned char*) aSrc;
  PRUnichar* destEnd = aDest + *aDestLen;
  PRUnichar* dest = aDest;
  while((src < srcEnd))
  {
    // if LF/CR, return to US-ASCII unconditionally.
    if ( *src == 0x0a || *src == 0x0d )
      mState = mState_Init;

    switch(mState)
    {
      case mState_Init:
        if(0x1b == *src) {
          mLastLegalState = mState_ASCII;
          mState = mState_ESC;
          break;
        }
        mState = mState_ASCII;
        // fall through

      case mState_ASCII:
        if(0x0e == *src) { // Shift-Out 
          mState = mState_KSX1001_1992;
          mRunLength = 0;
        } 
        else if(*src & 0x80) {
          if (CHECK_OVERRUN(dest, destEnd, 1))
            goto error1;
          *dest++ = 0xFFFD;
        } 
        else {
          if (CHECK_OVERRUN(dest, destEnd, 1))
            goto error1;
          *dest++ = (PRUnichar) *src;
        }
        break;
          
      case mState_ESC:
        if('$' == *src) {
          mState = mState_ESC_24;
        } 
        else  {
          if (CHECK_OVERRUN(dest, destEnd, 2))
            goto error1;
          *dest++ = (PRUnichar) 0x1b;
          *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
          mState =  mLastLegalState;
        }
        break;

      case mState_ESC_24: // ESC $
        if(')' == *src) {
          mState = mState_ESC_24_29;
        } 
        else  {
          if (CHECK_OVERRUN(dest, destEnd, 3))
            goto error1;
          *dest++ = (PRUnichar) 0x1b;
          *dest++ = (PRUnichar) '$';
          *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
          mState = mLastLegalState;
        }
        break;

      case mState_ESC_24_29: // ESC $ )
        mState = mLastLegalState;
        if('C' == *src) {
          mState = mState_ASCII;
          mRunLength = 0;
        } 
        else  {
          if (CHECK_OVERRUN(dest, destEnd, 4))
            goto error1;
          *dest++ = (PRUnichar) 0x1b;
          *dest++ = (PRUnichar) '$';
          *dest++ = (PRUnichar) ')';
          *dest++ = (0x80 & *src) ? 0xFFFD : (PRUnichar) *src;
          mState = mLastLegalState;
        }
        break;

      case mState_KSX1001_1992:
        if (0x20 < (uint8_t) *src  && (uint8_t) *src < 0x7f) {
          mData = (uint8_t) *src;
          mState = mState_KSX1001_1992_2ndbyte;
        } 
        else if (0x0f == *src) { // Shift-In (SI)
          mState = mState_ASCII;
          if (mRunLength == 0) {
            if (CHECK_OVERRUN(dest, destEnd, 1))
              goto error1;
            *dest++ = 0xFFFD;
          }
          mRunLength = 0;
        } 
        else if ((uint8_t) *src == 0x20 || (uint8_t) *src == 0x09) {
          // Allow space and tab between SO and SI (i.e. in Hangul segment)
          if (CHECK_OVERRUN(dest, destEnd, 1))
            goto error1;
          mState = mState_KSX1001_1992;
          *dest++ = (PRUnichar) *src;
          ++mRunLength;
        } 
        else {         // Everything else is invalid.
          if (CHECK_OVERRUN(dest, destEnd, 1))
            goto error1;
          *dest++ = 0xFFFD;
        }
        break;

      case mState_KSX1001_1992_2ndbyte:
        if ( 0x20 < (uint8_t) *src && (uint8_t) *src < 0x7f  ) {
          if (!mEUCKRDecoder) {
            // creating a delegate converter (EUC-KR)
            nsresult rv;
            nsCOMPtr<nsICharsetConverterManager> ccm = 
                  do_GetService(kCharsetConverterManagerCID, &rv);
            if (NS_SUCCEEDED(rv)) {
              rv = ccm->GetUnicodeDecoderRaw("EUC-KR", &mEUCKRDecoder);
            }
          }

          if (!mEUCKRDecoder) {// failed creating a delegate converter
           *dest++ = 0xFFFD;
          } 
          else {              
            if (CHECK_OVERRUN(dest, destEnd, 1))
              goto error1;
            unsigned char ksx[2];
            PRUnichar uni;
            int32_t ksxLen = 2, uniLen = 1;
            // mData is the original 1st byte.
            // *src is the present 2nd byte.
            // Put 2 bytes (one character) to ksx[] with EUC-KR encoding.
            ksx[0] = mData | 0x80;
            ksx[1] = *src | 0x80;
            // Convert EUC-KR to unicode.
            mEUCKRDecoder->Convert((const char *)ksx, &ksxLen, &uni, &uniLen);
            *dest++ = uni;
            ++mRunLength;
          }
          mState = mState_KSX1001_1992;
        } 
        else {        // Invalid 
          if ( 0x0f == *src ) {   // Shift-In (SI)
            mState = mState_ASCII;
          } 
          else {
            mState = mState_KSX1001_1992;
          }
          if (CHECK_OVERRUN(dest, destEnd, 1))
            goto error1;
          *dest++ = 0xFFFD;
        }
        break;

      case mState_ERROR:
        mState = mLastLegalState;
        if (CHECK_OVERRUN(dest, destEnd, 1))
          goto error1;
        *dest++ = 0xFFFD;
        break;

    } // switch
    src++;
  }
  *aDestLen = dest - aDest;
  return NS_OK;

error1:
  *aDestLen = dest-aDest;
  *aSrcLen = src-(unsigned char*)aSrc;
  return NS_OK_UDEC_MOREOUTPUT;
}

