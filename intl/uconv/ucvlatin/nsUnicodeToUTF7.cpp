/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeToUTF7.h"
#include <string.h>

//----------------------------------------------------------------------
// Global functions and data [declaration]

#define ENC_DIRECT      0
#define ENC_BASE64      1

//----------------------------------------------------------------------
// Class nsBasicUTF7Encoder [implementation]

nsBasicUTF7Encoder::nsBasicUTF7Encoder(char aLastChar, char aEscChar) 
: nsEncoderSupport(5)
{
  mLastChar = aLastChar;
  mEscChar = aEscChar;
  Reset();
}

nsresult nsBasicUTF7Encoder::ShiftEncoding(int32_t aEncoding,
                                          char * aDest, 
                                          int32_t * aDestLength)
{
  if (aEncoding == mEncoding) {
    *aDestLength = 0;
    return NS_OK;
  } 

  nsresult res = NS_OK;
  char * dest = aDest;
  char * destEnd = aDest + *aDestLength;

  if (mEncStep != 0) {
    if (dest >= destEnd) return NS_OK_UENC_MOREOUTPUT;
    *(dest++)=ValueToChar(mEncBits);
    mEncStep = 0;
    mEncBits = 0;
  }

  if (dest >= destEnd) {
    res = NS_OK_UENC_MOREOUTPUT;
  } else {
    switch (aEncoding) {
      case 0:
        *(dest++) = '-';
        mEncStep = 0;
        mEncBits = 0;
        break;
      case 1:
        *(dest++) = mEscChar;
        break;
    }
    mEncoding = aEncoding;
  }

  *aDestLength  = dest - aDest;
  return res;
}

nsresult nsBasicUTF7Encoder::EncodeDirect(
                            const PRUnichar * aSrc, 
                            int32_t * aSrcLength, 
                            char * aDest, 
                            int32_t * aDestLength)
{
  nsresult res = NS_OK;
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  char * destEnd = aDest + *aDestLength;
  PRUnichar ch;

  while (src < srcEnd) {
    ch = *src;

    // stop when we reach Unicode chars
    if (!DirectEncodable(ch)) break;

    if (ch == mEscChar) {
      // special case for the escape char
      if (destEnd - dest < 1) {
        res = NS_OK_UENC_MOREOUTPUT;
        break;
      } else {
        *dest++ = (char)ch;
        *dest++ = (char)'-';
        src++;
      }
    } else {
      //classic direct encoding
      if (dest >= destEnd) {
        res = NS_OK_UENC_MOREOUTPUT;
        break;
      } else {
        *dest++ = (char)ch;
        src++;
      }
    }
  }

  *aSrcLength = src - aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

nsresult nsBasicUTF7Encoder::EncodeBase64(
                             const PRUnichar * aSrc, 
                             int32_t * aSrcLength, 
                             char * aDest, 
                             int32_t * aDestLength)
{
  nsresult res = NS_OK;
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  char * destEnd = aDest + *aDestLength;
  PRUnichar ch;
  uint32_t value;

  while (src < srcEnd) {
    ch = *src;

    // stop when we reach printable US-ASCII chars
    if (DirectEncodable(ch)) break;

    switch (mEncStep) {
      case 0:
        if (destEnd - dest < 2) {
          res = NS_OK_UENC_MOREOUTPUT;
          break;
        }
        value=ch>>10;
        *(dest++)=ValueToChar(value);
        value=(ch>>4)&0x3f;
        *(dest++)=ValueToChar(value);
        mEncBits=(ch&0x0f)<<2;
        break;
      case 1:
        if (destEnd - dest < 3) {
          res = NS_OK_UENC_MOREOUTPUT;
          break;
        }
        value=mEncBits+(ch>>14);
        *(dest++)=ValueToChar(value);
        value=(ch>>8)&0x3f;
        *(dest++)=ValueToChar(value);
        value=(ch>>2)&0x3f;
        *(dest++)=ValueToChar(value);
        mEncBits=(ch&0x03)<<4;
        break;
      case 2:
        if (destEnd - dest < 3) {
          res = NS_OK_UENC_MOREOUTPUT;
          break;
        }
        value=mEncBits+(ch>>12);
        *(dest++)=ValueToChar(value);
        value=(ch>>6)&0x3f;
        *(dest++)=ValueToChar(value);
        value=ch&0x3f;
        *(dest++)=ValueToChar(value);
        mEncBits=0;
        break;
    }

    if (res != NS_OK) break;

    src++;
    (++mEncStep)%=3;
  }

  *aSrcLength = src - aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

char nsBasicUTF7Encoder::ValueToChar(uint32_t aValue) { 
  if (aValue < 26) 
    return (char)('A'+aValue);
  else if (aValue < 26 + 26) 
    return (char)('a' + aValue - 26);
  else if (aValue < 26 + 26 + 10)
    return (char)('0' + aValue - 26 - 26);
  else if (aValue == 26 + 26 + 10)
    return '+';
  else if (aValue == 26 + 26 + 10 + 1)
    return mLastChar;
  else
    return -1;
}

bool nsBasicUTF7Encoder::DirectEncodable(PRUnichar aChar) {
  // spec says: printable US-ASCII chars
  if ((aChar >= 0x20) && (aChar <= 0x7e)) return true;
  else return false;
}

//----------------------------------------------------------------------
// Subclassing of nsEncoderSupport class [implementation]

NS_IMETHODIMP nsBasicUTF7Encoder::ConvertNoBuffNoErr(
                                  const PRUnichar * aSrc, 
                                  int32_t * aSrcLength, 
                                  char * aDest, 
                                  int32_t * aDestLength)
{
  nsresult res = NS_OK;
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  char * destEnd = aDest + *aDestLength;
  int32_t bcr,bcw;
  PRUnichar ch;
  int32_t enc;

  while (src < srcEnd) {
    // find the encoding for the next char
    ch = *src;
    if (DirectEncodable(ch)) 
      enc = ENC_DIRECT;
    else
      enc = ENC_BASE64;

    // if necessary, shift into the required encoding
    bcw = destEnd - dest;
    res = ShiftEncoding(enc, dest, &bcw);
    dest += bcw;
    if (res != NS_OK) break;

    // now encode (as much as you can)
    bcr = srcEnd - src;
    bcw = destEnd - dest;
    if (enc == ENC_DIRECT) 
      res = EncodeDirect(src, &bcr, dest, &bcw);
    else 
      res = EncodeBase64(src, &bcr, dest, &bcw);
    src += bcr;
    dest += bcw;

    if (res != NS_OK) break;
  }

  *aSrcLength = src - aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

NS_IMETHODIMP nsBasicUTF7Encoder::FinishNoBuff(char * aDest, 
                                               int32_t * aDestLength)
{
  return ShiftEncoding(ENC_DIRECT, aDest, aDestLength);
}

NS_IMETHODIMP nsBasicUTF7Encoder::Reset()
{
  mEncoding = ENC_DIRECT;
  mEncBits = 0;
  mEncStep = 0;
  return nsEncoderSupport::Reset();
}

//----------------------------------------------------------------------
// Class nsUnicodeToUTF7 [implementation]

nsUnicodeToUTF7::nsUnicodeToUTF7() 
: nsBasicUTF7Encoder('/', '+')
{
}


bool nsUnicodeToUTF7::DirectEncodable(PRUnichar aChar) {
  if ((aChar >= 'A') && (aChar <= 'Z')) return true;
  else if ((aChar >= 'a') && (aChar <= 'z')) return true;
  else if ((aChar >= '0') && (aChar <= '9')) return true;
  else if ((aChar >= 39) && (aChar <= 41)) return true;
  else if ((aChar >= 44) && (aChar <= 47)) return true;
  else if (aChar == 58) return true;
  else if (aChar == 63) return true;
  else if (aChar == ' ') return true;
  else if (aChar == 9) return true;
  else if (aChar == 13) return true;
  else if (aChar == 10) return true;
  else if (aChar == 60) return true;  // '<'
  else if (aChar == 33) return true;  // '!'
  else if (aChar == 34) return true;  // '"'
  else if (aChar == 62) return true;  // '>'
  else if (aChar == 61) return true;  // '='
  else if (aChar == 59) return true;  // ';'
  else if (aChar == 91) return true;  // '['
  else if (aChar == 93) return true;  // ']'
  else return false;
}
