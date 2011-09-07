/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//----------------------------------------------------------------------
// Global functions and data [declaration]
#include "nsUCSupport.h"
#include "nsUnicodeToUTF8.h"
#include <string.h>

NS_IMPL_ISUPPORTS1(nsUnicodeToUTF8, nsIUnicodeEncoder)

//----------------------------------------------------------------------
// nsUnicodeToUTF8 class [implementation]

NS_IMETHODIMP nsUnicodeToUTF8::GetMaxLength(const PRUnichar * aSrc, 
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
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
                                PRInt32 * aSrcLength, 
                                char * aDest, 
                                PRInt32 * aDestLength)
{
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  PRInt32 destLen = *aDestLength;
  PRUint32 n;

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
      *dest++ = (char)0xe0 | (mHighSurrogate >> 12);
      *dest++ = (char)0x80 | ((mHighSurrogate >> 6) & 0x003f);
      *dest++ = (char)0x80 | (mHighSurrogate & 0x003f);
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
    } else if (*src >= (PRUnichar)0xD800 && *src < (PRUnichar)0xDC00) {
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
        *dest++ = (char)0xe0 | (*src >> 12);
        *dest++ = (char)0x80 | ((*src >> 6) & 0x003f);
        *dest++ = (char)0x80 | (*src & 0x003f);
        destLen -= 3;
      } else {
        n = ((*src - (PRUnichar)0xd800) << 10) + (*(src+1) - (PRUnichar)0xdc00) + (PRUint32)0x10000;
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

NS_IMETHODIMP nsUnicodeToUTF8::Finish(char * aDest, PRInt32 * aDestLength)
{
  char * dest = aDest;

  if (mHighSurrogate) {
    if (*aDestLength < 3) {
      *aDestLength = 0;
      return NS_OK_UENC_MOREOUTPUT;
    }
    *dest++ = (char)0xe0 | (mHighSurrogate >> 12);
    *dest++ = (char)0x80 | ((mHighSurrogate >> 6) & 0x003f);
    *dest++ = (char)0x80 | (mHighSurrogate & 0x003f);
    mHighSurrogate = 0;
    *aDestLength = 3;
    return NS_OK;
  } 

  *aDestLength  = 0;
  return NS_OK;
}
