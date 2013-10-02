/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//----------------------------------------------------------------------
// Global functions and data [declaration]
#include "nsUnicodeToUTF8.h"

NS_IMPL_ISUPPORTS1(nsUnicodeToUTF8, nsIUnicodeEncoder)

//----------------------------------------------------------------------
// nsUnicodeToUTF8 class [implementation]

NS_IMETHODIMP nsUnicodeToUTF8::GetMaxLength(const PRUnichar * aSrc, 
                                              int32_t aSrcLength,
                                              int32_t * aDestLength)
{
  // aSrc is interpreted as UTF16, 3 is normally enough.
  // But when previous buffer only contains part of the surrogate pair, we 
  // need to complete it here. If the first word in following buffer is not
  // in valid surrogate range, we need to convert the remaining of last buffer
  // to 3 bytes.
  *aDestLength = 3*aSrcLength + 3;
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToUTF8::Convert(const PRUnichar * aSrc, 
                                int32_t * aSrcLength, 
                                char * aDest, 
                                int32_t * aDestLength)
{
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  int32_t destLen = *aDestLength;
  uint32_t n;

  //complete remaining of last conversion
  if (mHighSurrogate) {
    if (src < srcEnd) {
      *aDestLength = 0;
      return NS_OK_UENC_MOREINPUT;
    }
    if (*aDestLength < 4) {
      *aSrcLength = 0;
      *aDestLength = 0;
      return NS_OK_UENC_MOREOUTPUT;
    }
    if (*src < (PRUnichar)0xdc00 || *src > (PRUnichar)0xdfff) { //not a pair
      *dest++ = (char)0xef; //replacement character
      *dest++ = (char)0xbf;
      *dest++ = (char)0xbd;
      destLen -= 3;
    } else { 
      n = ((mHighSurrogate - (PRUnichar)0xd800) << 10) + 
              (*src - (PRUnichar)0xdc00) + 0x10000;
      *dest++ = (char)0xf0 | (n >> 18);
      *dest++ = (char)0x80 | ((n >> 12) & 0x3f);
      *dest++ = (char)0x80 | ((n >> 6) & 0x3f);
      *dest++ = (char)0x80 | (n & 0x3f);
      ++src;
      destLen -= 4;
    }
    mHighSurrogate = 0;
  }

  while (src < srcEnd) {
    if ( *src <= 0x007f) {
      if (destLen < 1)
        goto error_more_output;
      *dest++ = (char)*src;
      --destLen;
    } else if (*src <= 0x07ff) {
      if (destLen < 2)
        goto error_more_output;
      *dest++ = (char)0xc0 | (*src >> 6);
      *dest++ = (char)0x80 | (*src & 0x003f);
      destLen -= 2;
    } else if (*src >= (PRUnichar)0xd800 && *src <= (PRUnichar)0xdfff) {
      if (*src >= (PRUnichar)0xdc00) { //not a pair
        if (destLen < 3)
          goto error_more_output;
        *dest++ = (char)0xef; //replacement character
        *dest++ = (char)0xbf;
        *dest++ = (char)0xbd;
        destLen -= 3;
        ++src;
        continue;
      }
      if ((src+1) >= srcEnd) {
        //we need another surrogate to complete this unicode char
        mHighSurrogate = *src;
        *aDestLength = dest - aDest;
        return NS_OK_UENC_MOREINPUT;
      }
      //handle surrogate
      if (destLen < 4)
        goto error_more_output;
      if (*(src+1) < (PRUnichar)0xdc00 || *(src+1) > 0xdfff) { //not a pair
        *dest++ = (char)0xef; //replacement character
        *dest++ = (char)0xbf;
        *dest++ = (char)0xbd;
        destLen -= 3;
      } else {
        n = ((*src - (PRUnichar)0xd800) << 10) + (*(src+1) - (PRUnichar)0xdc00) + (uint32_t)0x10000;
        *dest++ = (char)0xf0 | (n >> 18);
        *dest++ = (char)0x80 | ((n >> 12) & 0x3f);
        *dest++ = (char)0x80 | ((n >> 6) & 0x3f);
        *dest++ = (char)0x80 | (n & 0x3f);
        destLen -= 4;
        ++src;
      }
    } else { 
      if (destLen < 3)
        goto error_more_output;
      //treat rest of the character as BMP
      *dest++ = (char)0xe0 | (*src >> 12);
      *dest++ = (char)0x80 | ((*src >> 6) & 0x003f);
      *dest++ = (char)0x80 | (*src & 0x003f);
      destLen -= 3;
    }
    ++src;
  }

  *aDestLength = dest - aDest;
  return NS_OK;

error_more_output:
  *aSrcLength = src - aSrc;
  *aDestLength = dest - aDest;
  return NS_OK_UENC_MOREOUTPUT;
}

NS_IMETHODIMP nsUnicodeToUTF8::Finish(char * aDest, int32_t * aDestLength)
{
  char * dest = aDest;

  if (mHighSurrogate) {
    if (*aDestLength < 3) {
      *aDestLength = 0;
      return NS_OK_UENC_MOREOUTPUT;
    }
    *dest++ = (char)0xef; //replacement character
    *dest++ = (char)0xbf;
    *dest++ = (char)0xbd;
    mHighSurrogate = 0;
    *aDestLength = 3;
    return NS_OK;
  } 

  *aDestLength  = 0;
  return NS_OK;
}
