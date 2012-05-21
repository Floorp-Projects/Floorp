/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUCConstructors.h"
#include "nsUCS2BEToUnicode.h"
#include "nsUCvLatinDll.h"
#include "nsCharTraits.h"
#include <string.h>
#include "prtypes.h"

#define STATE_NORMAL             0
#define STATE_HALF_CODE_POINT    1
#define STATE_FIRST_CALL         2
#define STATE_FOUND_BOM          3
#define STATE_ODD_SURROGATE_PAIR 4

static nsresult
UTF16ConvertToUnicode(PRUint8& aState, PRUint8& aOddByte,
                      PRUnichar& aOddHighSurrogate, PRUnichar& aOddLowSurrogate,
                      const char * aSrc,
                      PRInt32 * aSrcLength, PRUnichar * aDest,
                      PRInt32 * aDestLength,
                      bool aSwapBytes)
{
  const char* src = aSrc;
  const char* srcEnd = aSrc + *aSrcLength;
  PRUnichar* dest = aDest;
  PRUnichar* destEnd = aDest + *aDestLength;

  switch(aState) {
    case STATE_FOUND_BOM:
      NS_ASSERTION(*aSrcLength > 1, "buffer too short");
      src+=2;
      aState = STATE_NORMAL;
      break;

    case STATE_FIRST_CALL: // first time called
      NS_ASSERTION(*aSrcLength > 1, "buffer too short");
      // Eliminate BOM (0xFEFF). Note that different endian case is taken care
      // of in |Convert| of LE and BE converters. Here, we only have to
      // deal with the same endian case. That is, 0xFFFE (byte-swapped BOM) is
      // illegal.
      if(0xFEFF == *((PRUnichar*)src)) {
        src+=2;
      } else if(0xFFFE == *((PRUnichar*)src)) {
        *aSrcLength=0;
        *aDestLength=0;
        return NS_ERROR_ILLEGAL_INPUT;
      }  
      aState = STATE_NORMAL;
      break;

    case STATE_ODD_SURROGATE_PAIR:
      if (*aDestLength < 2)
        *dest++ = UCS2_REPLACEMENT_CHAR;
      else {
        *dest++ = aOddHighSurrogate;
        *dest++ = aOddLowSurrogate;
        aOddHighSurrogate = aOddLowSurrogate = 0;
        aState = STATE_NORMAL;
      }
      break;

    case STATE_NORMAL:
    case STATE_HALF_CODE_POINT:
    default:
      break;
  }

  if (src == srcEnd) {
    *aDestLength = dest - aDest;
    return NS_OK;
  }

  PRUnichar oddHighSurrogate = aOddHighSurrogate;

  const char* srcEvenEnd;

  PRUnichar u;
  if (aState == STATE_HALF_CODE_POINT) {
    // the 1st byte of a 16-bit code unit was stored in |aOddByte| in the
    // previous run while the 2nd byte has to come from |*src|.
    aState = STATE_NORMAL;
#ifdef IS_BIG_ENDIAN
    u = (aOddByte << 8) | *src++; // safe, we know we have at least one byte.
#else
    u = (*src++ << 8) | aOddByte; // safe, we know we have at least one byte.
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
        *dest++ = UCS2_REPLACEMENT_CHAR;
        if (dest == destEnd)
          goto error;
        oddHighSurrogate = 0;
      }
      *dest++ = u;
    } else if (NS_IS_HIGH_SURROGATE(u)) {
      if (oddHighSurrogate) {
        *dest++ = UCS2_REPLACEMENT_CHAR;
        if (dest == destEnd)
          goto error;
      }
      oddHighSurrogate = u;
    }
    else /* if (NS_IS_LOW_SURROGATE(u)) */ {
      if (oddHighSurrogate && *aDestLength > 1) {
        if (dest + 1 >= destEnd) {
          aOddLowSurrogate = u;
          aOddHighSurrogate = oddHighSurrogate;
          aState = STATE_ODD_SURROGATE_PAIR;
          goto error;
        }
        *dest++ = oddHighSurrogate;
        *dest++ = u;
      } else {
        *dest++ = UCS2_REPLACEMENT_CHAR;
      }
      oddHighSurrogate = 0;
    }
  }
  if (src != srcEnd) {
    // store the lead byte of a 16-bit unit for the next run.
    aOddByte = *src++;
    aState = STATE_HALF_CODE_POINT;
  }

  aOddHighSurrogate = oddHighSurrogate;

  *aDestLength = dest - aDest;
  *aSrcLength =  src  - aSrc; 
  return NS_OK;

error:
  *aDestLength = dest - aDest;
  *aSrcLength =  src  - aSrc; 
  return  NS_OK_UDEC_MOREOUTPUT;
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
nsUTF16ToUnicodeBase::GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
                                   PRInt32 * aDestLength)
{
  // the left-over data of the previous run have to be taken into account.
  *aDestLength = (aSrcLength + ((STATE_HALF_CODE_POINT == mState) ? 1 : 0)) / 2;
  if (mOddHighSurrogate)
    (*aDestLength)++;
  if (mOddLowSurrogate)
    (*aDestLength)++;
  return NS_OK;
}


NS_IMETHODIMP
nsUTF16BEToUnicode::Convert(const char * aSrc, PRInt32 * aSrcLength,
                            PRUnichar * aDest, PRInt32 * aDestLength)
{
    if(STATE_FIRST_CALL == mState && *aSrcLength < 2)
    {
      nsresult res = (*aSrcLength == 0) ? NS_OK : NS_ERROR_ILLEGAL_INPUT;
      *aSrcLength=0;
      *aDestLength=0;
      return res;
    }
#ifdef IS_LITTLE_ENDIAN
    // Remove the BOM if we're little-endian. The 'same endian' case with the
    // leading BOM will be taken care of by |UTF16ConvertToUnicode|.
    if(STATE_FIRST_CALL == mState) // Called for the first time.
    {
      mState = STATE_NORMAL;
      if(0xFFFE == *((PRUnichar*)aSrc)) {
        // eliminate BOM (on LE machines, BE BOM is 0xFFFE)
        mState = STATE_FOUND_BOM;
      } else if(0xFEFF == *((PRUnichar*)aSrc)) {
        *aSrcLength=0;
        *aDestLength=0;
        return NS_ERROR_ILLEGAL_INPUT;
      }
    }
#endif

  nsresult rv = UTF16ConvertToUnicode(mState, mOddByte, mOddHighSurrogate,
                                      mOddLowSurrogate,
                                      aSrc, aSrcLength, aDest, aDestLength,
#ifdef IS_LITTLE_ENDIAN
                                      true
#else
                                      false
#endif
                                      );
  return rv;
}

NS_IMETHODIMP
nsUTF16LEToUnicode::Convert(const char * aSrc, PRInt32 * aSrcLength,
                            PRUnichar * aDest, PRInt32 * aDestLength)
{
    if(STATE_FIRST_CALL == mState && *aSrcLength < 2)
    {
      nsresult res = (*aSrcLength == 0) ? NS_OK : NS_ERROR_ILLEGAL_INPUT;
      *aSrcLength=0;
      *aDestLength=0;
      return res;
    }
#ifdef IS_BIG_ENDIAN
    // Remove the BOM if we're big-endian. The 'same endian' case with the
    // leading BOM will be taken care of by |UTF16ConvertToUnicode|.
    if(STATE_FIRST_CALL == mState) // first time called
    {
      mState = STATE_NORMAL;
      if(0xFFFE == *((PRUnichar*)aSrc)) {
        // eliminate BOM (on BE machines, LE BOM is 0xFFFE)
        mState = STATE_FOUND_BOM;
      } else if(0xFEFF == *((PRUnichar*)aSrc)) {
        *aSrcLength=0;
        *aDestLength=0;
        return NS_ERROR_ILLEGAL_INPUT;
      }
    }
#endif
    
  nsresult rv = UTF16ConvertToUnicode(mState, mOddByte, mOddHighSurrogate,
                                      mOddLowSurrogate,
                                      aSrc, aSrcLength, aDest, aDestLength,
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
nsUTF16ToUnicode::Convert(const char * aSrc, PRInt32 * aSrcLength,
                          PRUnichar * aDest, PRInt32 * aDestLength)
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
      mState = STATE_NORMAL;
      // check if BOM (0xFEFF) is at the beginning, remove it if found, and
      // set mEndian accordingly.
      if(0xFF == PRUint8(aSrc[0]) && 0xFE == PRUint8(aSrc[1])) {
        mState = STATE_FOUND_BOM;
        mEndian = kLittleEndian;
        mFoundBOM = true;
      }
      else if(0xFE == PRUint8(aSrc[0]) && 0xFF == PRUint8(aSrc[1])) {
        mState = STATE_FOUND_BOM;
        mEndian = kBigEndian;
        mFoundBOM = true;
      }
      // BOM is not found, but we can use a simple heuristic to determine
      // the endianness. Assume the first character is [U+0001, U+00FF].
      // Not always valid, but it's very likely to hold for html/xml/css. 
      else if(!aSrc[0] && aSrc[1]) {  // 0x00 0xhh (hh != 00)
        mEndian = kBigEndian;
      }
      else if(aSrc[0] && !aSrc[1]) {  // 0xhh 0x00 (hh != 00)
        mEndian = kLittleEndian;
      }
      else { // Neither BOM nor 'plausible' byte patterns at the beginning.
             // Just assume it's BE (following Unicode standard)
             // and let the garbage show up in the browser. (security concern?)
             // (bug 246194)
        mEndian = kBigEndian;
      }
    }
    
    nsresult rv = UTF16ConvertToUnicode(mState, mOddByte, mOddHighSurrogate,
                                        mOddLowSurrogate,
                                        aSrc, aSrcLength, aDest, aDestLength,
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
