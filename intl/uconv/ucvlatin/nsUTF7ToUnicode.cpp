/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsUTF7ToUnicode.h"

#define ENC_DIRECT      0
#define ENC_BASE64      1

//----------------------------------------------------------------------
// Class nsBasicUTF7Decoder [implementation]

nsBasicUTF7Decoder::nsBasicUTF7Decoder(char aLastChar, char aEscChar) 
: nsBufferDecoderSupport()
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
      mFreshBase64 = PR_FALSE;
      res = DecodeBase64(src, &bcr, dest, &bcw);
    }
    src += bcr;
    dest += bcw;

    // if an illegal char was encountered, test if it is an escape seq.
    if (res == NS_ERROR_UDEC_ILLEGALINPUT) {
      if (mEncoding == ENC_DIRECT) {
        if (*src == mEscChar) {
          mEncoding = ENC_BASE64;
          mFreshBase64 = PR_TRUE;
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

NS_IMETHODIMP nsBasicUTF7Decoder::GetMaxLength(const char * aSrc, 
                                               PRInt32 aSrcLength, 
                                               PRInt32 * aDestLength)
{
  // worst case
  *aDestLength = aSrcLength;
  return NS_OK;
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
