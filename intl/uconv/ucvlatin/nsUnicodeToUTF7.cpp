/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsUnicodeToUTF7.h"
#include <string.h>

//----------------------------------------------------------------------
// Global functions and data [declaration]

#define ENC_DIRECT      0
#define ENC_BASE64      1

//----------------------------------------------------------------------
// Class nsBasicUTF7Encoder [implementation]

nsBasicUTF7Encoder::nsBasicUTF7Encoder(char aLastChar, char aEscChar) 
: nsEncoderSupport()
{
  mLastChar = aLastChar;
  mEscChar = aEscChar;
  Reset();
}

NS_IMETHODIMP nsBasicUTF7Encoder::FillInfo(PRUint32 *aInfo)
{
  memset(aInfo, 0xFF, (0x10000L >> 3));
  return NS_OK;
}

nsresult nsBasicUTF7Encoder::ShiftEncoding(PRInt32 aEncoding,
                                          char * aDest, 
                                          PRInt32 * aDestLength)
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
                            PRInt32 * aSrcLength, 
                            char * aDest, 
                            PRInt32 * aDestLength)
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
                             PRInt32 * aSrcLength, 
                             char * aDest, 
                             PRInt32 * aDestLength)
{
  nsresult res = NS_OK;
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  char * destEnd = aDest + *aDestLength;
  PRUnichar ch;
  PRUint32 value;

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

char nsBasicUTF7Encoder::ValueToChar(PRUint32 aValue) { 
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

PRBool nsBasicUTF7Encoder::DirectEncodable(PRUnichar aChar) {
  // spec says: printable US-ASCII chars
  if ((aChar >= 0x20) && (aChar <= 0x7e)) return PR_TRUE;
  else return PR_FALSE;
}

//----------------------------------------------------------------------
// Subclassing of nsEncoderSupport class [implementation]

NS_IMETHODIMP nsBasicUTF7Encoder::ConvertNoBuffNoErr(
                                  const PRUnichar * aSrc, 
                                  PRInt32 * aSrcLength, 
                                  char * aDest, 
                                  PRInt32 * aDestLength)
{
  nsresult res = NS_OK;
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  char * destEnd = aDest + *aDestLength;
  PRInt32 bcr,bcw;
  PRUnichar ch;
  PRInt32 enc;

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
                                               PRInt32 * aDestLength)
{
  return ShiftEncoding(ENC_DIRECT, aDest, aDestLength);
}

NS_IMETHODIMP nsBasicUTF7Encoder::GetMaxLength(const PRUnichar * aSrc, 
                                               PRInt32 aSrcLength,
                                               PRInt32 * aDestLength)
{
  // worst case
  *aDestLength = 5*aSrcLength;
  return NS_OK;
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


PRBool nsUnicodeToUTF7::DirectEncodable(PRUnichar aChar) {
  if ((aChar >= 'A') && (aChar <= 'Z')) return PR_TRUE;
  else if ((aChar >= 'a') && (aChar <= 'z')) return PR_TRUE;
  else if ((aChar >= '0') && (aChar <= '9')) return PR_TRUE;
  else if ((aChar >= 39) && (aChar <= 41)) return PR_TRUE;
  else if ((aChar >= 44) && (aChar <= 47)) return PR_TRUE;
  else if (aChar == 58) return PR_TRUE;
  else if (aChar == 63) return PR_TRUE;
  else if (aChar == ' ') return PR_TRUE;
  else if (aChar == 9) return PR_TRUE;
  else if (aChar == 13) return PR_TRUE;
  else if (aChar == 10) return PR_TRUE;
  else if (aChar == 60) return PR_TRUE;  // '<'
  else if (aChar == 33) return PR_TRUE;  // '!'
  else if (aChar == 34) return PR_TRUE;  // '"'
  else if (aChar == 62) return PR_TRUE;  // '>'
  else if (aChar == 61) return PR_TRUE;  // '='
  else if (aChar == 59) return PR_TRUE;  // ';'
  else if (aChar == 91) return PR_TRUE;  // '['
  else if (aChar == 93) return PR_TRUE;  // ']'
  else return PR_FALSE;
}
