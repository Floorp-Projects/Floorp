/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUTF7ToUnicode.h"

#define ENC_DIRECT      0
#define ENC_BASE64      1

//----------------------------------------------------------------------
// Class nsBasicUTF7Decoder [implementation]

nsBasicUTF7Decoder::nsBasicUTF7Decoder(char aLastChar, char aEscChar) 
: nsBufferDecoderSupport(1)
{
  mLastChar = aLastChar;
  mEscChar = aEscChar;
  Reset();
}

nsresult nsBasicUTF7Decoder::DecodeDirect(
                             const char * aSrc, 
                             PRInt32 * aSrcLength, 
                             PRUnichar * aDest, 
                             PRInt32 * aDestLength)
{
  const char * srcEnd = aSrc + *aSrcLength;
  const char * src = aSrc;
  PRUnichar * destEnd = aDest + *aDestLength;
  PRUnichar * dest = aDest;
  nsresult res = NS_OK;
  char ch;

  while (src < srcEnd) {
    ch = *src;

    // stop when we meet other chars or end of direct encoded seq.
    // if (!(DirectEncodable(ch)) || (ch == mEscChar)) {
    // but we are decoding; so we should be lax; pass everything until escchar
    if (ch == mEscChar) {
      res = NS_ERROR_UDEC_ILLEGALINPUT;
      break;
    }

    if (dest >= destEnd) {
      res = NS_OK_UDEC_MOREOUTPUT;
      break;
    } else {
      *dest++ = ch;
      src++;
    }
  }

  *aSrcLength = src - aSrc;
  *aDestLength = dest - aDest;
  return res;
}

nsresult nsBasicUTF7Decoder::DecodeBase64(
                             const char * aSrc, 
                             PRInt32 * aSrcLength, 
                             PRUnichar * aDest, 
                             PRInt32 * aDestLength)
{
  const char * srcEnd = aSrc + *aSrcLength;
  const char * src = aSrc;
  PRUnichar * destEnd = aDest + *aDestLength;
  PRUnichar * dest = aDest;
  nsresult res = NS_OK;
  char ch;
  PRUint32 value;

  while (src < srcEnd) {
    ch = *src;

    // stop when we meet other chars or end of direct encoded seq.
    value = CharToValue(ch);
    if (value > 0xff) {
      res = NS_ERROR_UDEC_ILLEGALINPUT;
      break;
    }

    switch (mEncStep) {
      case 0:
        mEncBits = value << 10;
        break;
      case 1:
        mEncBits += value << 4;
        break;
      case 2:
        if (dest >= destEnd) {
          res = NS_OK_UDEC_MOREOUTPUT;
          break;
        }
        mEncBits += value >> 2;
        *(dest++) = (PRUnichar) mEncBits;
        mEncBits = (value & 0x03) << 14;
        break;
      case 3:
        mEncBits += value << 8;
        break;
      case 4:
        mEncBits += value << 2;
        break;
      case 5:
        if (dest >= destEnd) {
          res = NS_OK_UDEC_MOREOUTPUT;
          break;
        }
        mEncBits += value >> 4;
        *(dest++) = (PRUnichar) mEncBits;
        mEncBits = (value & 0x0f) << 12;
        break;
      case 6:
        mEncBits += value << 6;
        break;
      case 7:
        if (dest >= destEnd) {
          res = NS_OK_UDEC_MOREOUTPUT;
          break;
        }
        mEncBits += value;
        *(dest++) = (PRUnichar) mEncBits;
        mEncBits = 0;
        break;
    }

    if (res != NS_OK) break;

    src++;
    (++mEncStep)%=8;
  }

  *aSrcLength = src - aSrc;
  *aDestLength = dest - aDest;
  return res;
}

PRUint32 nsBasicUTF7Decoder::CharToValue(char aChar) {
  if ((aChar>='A')&&(aChar<='Z'))
    return (PRUint8)(aChar-'A');
  else if ((aChar>='a')&&(aChar<='z'))
    return (PRUint8)(26+aChar-'a');
  else if ((aChar>='0')&&(aChar<='9'))
    return (PRUint8)(26+26+aChar-'0');
  else if (aChar=='+')
    return (PRUint8)(26+26+10);
  else if (aChar==mLastChar)
    return (PRUint8)(26+26+10+1);
  else
    return 0xffff;
}

//----------------------------------------------------------------------
// Subclassing of nsBufferDecoderSupport class [implementation]

NS_IMETHODIMP nsBasicUTF7Decoder::ConvertNoBuff(const char * aSrc, 
                                                PRInt32 * aSrcLength, 
                                                PRUnichar * aDest, 
                                                PRInt32 * aDestLength)
{
  const char * srcEnd = aSrc + *aSrcLength;
  const char * src = aSrc;
  PRUnichar * destEnd = aDest + *aDestLength;
  PRUnichar * dest = aDest;
  PRInt32 bcr,bcw;
  nsresult res = NS_OK;
  char ch;

  while (src < srcEnd) {
    ch = *src;

    // fist, attept to decode in the current mode
    bcr = srcEnd - src;
    bcw = destEnd - dest;
    if (mEncoding == ENC_DIRECT) 
      res = DecodeDirect(src, &bcr, dest, &bcw);
    else if ((mFreshBase64) && (*src == '-')) {
      *dest = mEscChar;
      bcr = 0;
      bcw = 1;
      res = NS_ERROR_UDEC_ILLEGALINPUT;
    } else {
      mFreshBase64 = false;
      res = DecodeBase64(src, &bcr, dest, &bcw);
    }
    src += bcr;
    dest += bcw;

    // if an illegal char was encountered, test if it is an escape seq.
    if (res == NS_ERROR_UDEC_ILLEGALINPUT) {
      if (mEncoding == ENC_DIRECT) {
        if (*src == mEscChar) {
          mEncoding = ENC_BASE64;
          mFreshBase64 = true;
          mEncBits = 0;
          mEncStep = 0;
          src++;
          res = NS_OK;
        } else break;
      } else {
        mEncoding = ENC_DIRECT;
        res = NS_OK;
        // absorbe end of escape sequence
        if (*src == '-') src++;
      }
    } else if (res != NS_OK) break;
  }

  *aSrcLength = src - aSrc;
  *aDestLength = dest - aDest;
  return res;
}

NS_IMETHODIMP nsBasicUTF7Decoder::Reset()
{
  mEncoding = ENC_DIRECT;
  mEncBits = 0;
  mEncStep = 0;
  return nsBufferDecoderSupport::Reset();
}

//----------------------------------------------------------------------
// Class nsUTF7ToUnicode [implementation]

nsUTF7ToUnicode::nsUTF7ToUnicode() 
: nsBasicUTF7Decoder('/', '+')
{
}
