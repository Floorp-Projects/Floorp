/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUTF16ToUnicode.h"
#include "nsCharTraits.h"

enum {
  STATE_NORMAL = 0,
  STATE_HALF_CODE_POINT = 1,
  STATE_FIRST_CALL = 2,
  STATE_SECOND_BYTE = STATE_FIRST_CALL | STATE_HALF_CODE_POINT,
  STATE_ODD_SURROGATE_PAIR = 4
};

nsresult
nsUTF16ToUnicodeBase::UTF16ConvertToUnicode(const char * aSrc,
                                            int32_t * aSrcLength,
                                            PRUnichar * aDest,
                                            int32_t * aDestLength,
                                            bool aSwapBytes)
{
  const char* src = aSrc;
  const char* srcEnd = aSrc + *aSrcLength;
  PRUnichar* dest = aDest;
  PRUnichar* destEnd = aDest + *aDestLength;
  PRUnichar oddHighSurrogate;

  switch(mState) {
    case STATE_FIRST_CALL:
      NS_ASSERTION(*aSrcLength > 1, "buffer too short");
      src+=2;
      mState = STATE_NORMAL;
      break;

    case STATE_SECOND_BYTE:
      NS_ASSERTION(*aSrcLength > 0, "buffer too short");
      src++;
      mState = STATE_NORMAL;
      break;

    case STATE_ODD_SURROGATE_PAIR:
      if (*aDestLength < 2)
        goto error;
      else {
        *dest++ = mOddHighSurrogate;
        *dest++ = mOddLowSurrogate;
        mOddHighSurrogate = mOddLowSurrogate = 0;
        mState = STATE_NORMAL;
      }
      break;

    case STATE_NORMAL:
    case STATE_HALF_CODE_POINT:
    default:
      break;
  }

  oddHighSurrogate = mOddHighSurrogate;

  if (src == srcEnd) {
    *aDestLength = dest - aDest;
    return (mState != STATE_NORMAL || oddHighSurrogate) ?
           NS_OK_UDEC_MOREINPUT : NS_OK;
  }

  const char* srcEvenEnd;

  PRUnichar u;
  if (mState == STATE_HALF_CODE_POINT) {
    if (dest == destEnd)
      goto error;

    // the 1st byte of a 16-bit code unit was stored in |mOddByte| in the
    // previous run while the 2nd byte has to come from |*src|.
    mState = STATE_NORMAL;
#ifdef IS_BIG_ENDIAN
    u = (mOddByte << 8) | uint8_t(*src++); // safe, we know we have at least one byte.
#else
    u = (*src++ << 8) | mOddByte; // safe, we know we have at least one byte.
#endif
    srcEvenEnd = src + ((srcEnd - src) & ~1); // handle even number of bytes in main loop
    goto have_codepoint;
  } else {
    srcEvenEnd = src + ((srcEnd - src) & ~1); // handle even number of bytes in main loop
  }

  while (src != srcEvenEnd) {
    if (dest == destEnd)
      goto error;

#if !defined(__sparc__) && !defined(__arm__)
    u = *(const PRUnichar*)src;
#else
    memcpy(&u, src, 2);
#endif
    src += 2;

have_codepoint:
    if (aSwapBytes)
      u = u << 8 | u >> 8;

    if (!IS_SURROGATE(u)) {
      if (oddHighSurrogate) {
        if (mErrBehavior == kOnError_Signal) {
          goto error2;
        }
        *dest++ = UCS2_REPLACEMENT_CHAR;
        if (dest == destEnd)
          goto error;
        oddHighSurrogate = 0;
      }
      *dest++ = u;
    } else if (NS_IS_HIGH_SURROGATE(u)) {
      if (oddHighSurrogate) {
        if (mErrBehavior == kOnError_Signal) {
          goto error2;
        }
        *dest++ = UCS2_REPLACEMENT_CHAR;
        if (dest == destEnd)
          goto error;
      }
      oddHighSurrogate = u;
    }
    else /* if (NS_IS_LOW_SURROGATE(u)) */ {
      if (oddHighSurrogate && *aDestLength > 1) {
        if (dest + 1 >= destEnd) {
          mOddLowSurrogate = u;
          mOddHighSurrogate = oddHighSurrogate;
          mState = STATE_ODD_SURROGATE_PAIR;
          goto error;
        }
        *dest++ = oddHighSurrogate;
        *dest++ = u;
      } else {
        if (mErrBehavior == kOnError_Signal) {
          goto error2;
        }
        *dest++ = UCS2_REPLACEMENT_CHAR;
      }
      oddHighSurrogate = 0;
    }
  }
  if (src != srcEnd) {
    // store the lead byte of a 16-bit unit for the next run.
    mOddByte = *src++;
    mState = STATE_HALF_CODE_POINT;
  }

  mOddHighSurrogate = oddHighSurrogate;

  *aDestLength = dest - aDest;
  *aSrcLength =  src  - aSrc; 
  return (mState != STATE_NORMAL || oddHighSurrogate) ?
         NS_OK_UDEC_MOREINPUT : NS_OK;

error:
  *aDestLength = dest - aDest;
  *aSrcLength =  src  - aSrc; 
  return  NS_OK_UDEC_MOREOUTPUT;

error2:
  *aDestLength = dest - aDest;
  *aSrcLength = --src - aSrc; 
  return  NS_ERROR_ILLEGAL_INPUT;
}

NS_IMETHODIMP
nsUTF16ToUnicodeBase::Reset()
{
  mState = STATE_FIRST_CALL;
  mOddByte = 0;
  mOddHighSurrogate = 0;
  mOddLowSurrogate = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsUTF16ToUnicodeBase::GetMaxLength(const char * aSrc, int32_t aSrcLength, 
                                   int32_t * aDestLength)
{
  // the left-over data of the previous run have to be taken into account.
  *aDestLength = (aSrcLength + ((STATE_HALF_CODE_POINT & mState) ? 1 : 0)) / 2;
  if (mOddHighSurrogate)
    (*aDestLength)++;
  if (mOddLowSurrogate)
    (*aDestLength)++;
  return NS_OK;
}


NS_IMETHODIMP
nsUTF16BEToUnicode::Convert(const char * aSrc, int32_t * aSrcLength,
                            PRUnichar * aDest, int32_t * aDestLength)
{
  switch (mState) {
    case STATE_FIRST_CALL:
      if (*aSrcLength < 2) {
        if (*aSrcLength < 1) {
          *aDestLength = 0;
          return NS_OK;
        }
        if (uint8_t(*aSrc) != 0xFE) {
          mState = STATE_NORMAL;
          break;
        }
        *aDestLength = 0;
        mState = STATE_SECOND_BYTE;
        return NS_OK_UDEC_MOREINPUT;
      }
#ifdef IS_LITTLE_ENDIAN
      // on LE machines, BE BOM is 0xFFFE
      if (0xFFFE != *((PRUnichar*)aSrc)) {
        mState = STATE_NORMAL;
      }
#else
      if (0xFEFF != *((PRUnichar*)aSrc)) {
        mState = STATE_NORMAL;
      }
#endif
      break;

    case STATE_SECOND_BYTE:
      if (*aSrcLength < 1) {
        *aDestLength = 0;
        return NS_OK_UDEC_MOREINPUT;
      }
      if (uint8_t(*aSrc) != 0xFF) {
        mOddByte = 0xFE;
        mState = STATE_HALF_CODE_POINT;
      }
      break;
  }

  nsresult rv = UTF16ConvertToUnicode(aSrc, aSrcLength, aDest, aDestLength,
#ifdef IS_LITTLE_ENDIAN
                                      true
#else
                                      false
#endif
                                      );
  return rv;
}

NS_IMETHODIMP
nsUTF16LEToUnicode::Convert(const char * aSrc, int32_t * aSrcLength,
                            PRUnichar * aDest, int32_t * aDestLength)
{
  switch (mState) {
    case STATE_FIRST_CALL:
      if (*aSrcLength < 2) {
        if (*aSrcLength < 1) {
          *aDestLength = 0;
          return NS_OK;
        }
        if (uint8_t(*aSrc) != 0xFF) {
          mState = STATE_NORMAL;
          break;
        }
        *aDestLength = 0;
        mState = STATE_SECOND_BYTE;
        return NS_OK_UDEC_MOREINPUT;
      }
#ifdef IS_BIG_ENDIAN
      // on BE machines, LE BOM is 0xFFFE
      if (0xFFFE != *((PRUnichar*)aSrc)) {
        mState = STATE_NORMAL;
      }
#else
      if (0xFEFF != *((PRUnichar*)aSrc)) {
        mState = STATE_NORMAL;
      }
#endif
      break;

    case STATE_SECOND_BYTE:
      if (*aSrcLength < 1) {
        *aDestLength = 0;
        return NS_OK_UDEC_MOREINPUT;
      }
      if (uint8_t(*aSrc) != 0xFE) {
        mOddByte = 0xFF;
        mState = STATE_HALF_CODE_POINT;
      }
      break;
  }

  nsresult rv = UTF16ConvertToUnicode(aSrc, aSrcLength, aDest, aDestLength,
#ifdef IS_BIG_ENDIAN
                                      true
#else
                                      false
#endif
                                      );
  return rv;
}

NS_IMETHODIMP
nsUTF16ToUnicode::Reset()
{
  mEndian = kUnknown;
  mFoundBOM = false;
  return nsUTF16ToUnicodeBase::Reset();
}

NS_IMETHODIMP
nsUTF16ToUnicode::Convert(const char * aSrc, int32_t * aSrcLength,
                          PRUnichar * aDest, int32_t * aDestLength)
{
    if(STATE_FIRST_CALL == mState && *aSrcLength < 2)
    {
      nsresult res = (*aSrcLength == 0) ? NS_OK : NS_ERROR_ILLEGAL_INPUT;
      *aSrcLength=0;
      *aDestLength=0;
      return res;
    }
    if(STATE_FIRST_CALL == mState) // first time called
    {
      // check if BOM (0xFEFF) is at the beginning, remove it if found, and
      // set mEndian accordingly.
      if(0xFF == uint8_t(aSrc[0]) && 0xFE == uint8_t(aSrc[1])) {
        mEndian = kLittleEndian;
        mFoundBOM = true;
      }
      else if(0xFE == uint8_t(aSrc[0]) && 0xFF == uint8_t(aSrc[1])) {
        mEndian = kBigEndian;
        mFoundBOM = true;
      }
      // BOM is not found, but we can use a simple heuristic to determine
      // the endianness. Assume the first character is [U+0001, U+00FF].
      // Not always valid, but it's very likely to hold for html/xml/css. 
      else if(!aSrc[0] && aSrc[1]) {  // 0x00 0xhh (hh != 00)
        mState = STATE_NORMAL;
        mEndian = kBigEndian;
      }
      else if(aSrc[0] && !aSrc[1]) {  // 0xhh 0x00 (hh != 00)
        mState = STATE_NORMAL;
        mEndian = kLittleEndian;
      }
      else { // Neither BOM nor 'plausible' byte patterns at the beginning.
             // Just assume it's BE (following Unicode standard)
             // and let the garbage show up in the browser. (security concern?)
             // (bug 246194)
        mState = STATE_NORMAL;
        mEndian = kBigEndian;
      }
    }
    
    nsresult rv = UTF16ConvertToUnicode(aSrc, aSrcLength, aDest, aDestLength,
#ifdef IS_BIG_ENDIAN
                                        (mEndian == kLittleEndian)
#elif defined(IS_LITTLE_ENDIAN)
                                        (mEndian == kBigEndian)
#else
    #error "Unknown endianness"
#endif
                                        );

    // If BOM is not found and we're to return NS_OK, signal that BOM
    // is not found. Otherwise, return |rv| from |UTF16ConvertToUnicode|
    return (rv == NS_OK && !mFoundBOM) ? NS_OK_UDEC_NOBOMFOUND : rv;
}
