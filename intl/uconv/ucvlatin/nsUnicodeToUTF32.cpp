/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2: 
 */
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
 *   Jungshik Shin <jshin@mailaps.org>
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

#include <string.h>
#include "nsUCSupport.h"
#include "nsUnicodeToUTF32.h"

#ifdef IS_BIG_ENDIAN
#define UCS4_TO_LE_STRING(u, s)           \
  PR_BEGIN_MACRO                          \
    s[3] = PRUint8(((u) >> 24) & 0xffL);  \
    s[2] = PRUint8(((u) >> 16) & 0xffL);  \
    s[1] = PRUint8(((u) >> 8) & 0xffL);   \
    s[0] = PRUint8((u) & 0xffL);          \
  PR_END_MACRO
#else 
#define UCS4_TO_LE_STRING(u, s)           \
  PR_BEGIN_MACRO                          \
    *((PRUint32*)(s)) = (u);              \
  PR_END_MACRO
#endif

#ifdef IS_BIG_ENDIAN
#define UCS4_TO_BE_STRING(u, s)           \
  PR_BEGIN_MACRO                          \
    *((PRUint32*)(s)) = (u);              \
  PR_END_MACRO
#else
#define UCS4_TO_BE_STRING(u, s)           \
  PR_BEGIN_MACRO                          \
    s[0] = PRUint8(((u) >> 24) & 0xffL);  \
    s[1] = PRUint8(((u) >> 16) & 0xffL);  \
    s[2] = PRUint8(((u) >> 8) & 0xffL);   \
    s[3] = PRUint8((u) & 0xffL);          \
  PR_END_MACRO
#endif

//----------------------------------------------------------------------
// Static functions common to nsUnicodeToUTF32LE and nsUnicodeToUTF32BE
 
static nsresult ConvertCommon(const PRUnichar * aSrc, 
                              PRInt32 * aSrcLength, 
                              char * aDest, 
                              PRInt32 * aDestLength,
                              PRUnichar * aHighSurrogate,
                              PRUnichar * aBOM,
                              PRBool aIsLE)
{
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  const char * destEnd = aDest + *aDestLength; 
  PRUint32 ucs4;

  // Handle BOM if necessary 
  if (0 != *aBOM)
  {
    if (*aDestLength < 4) {
      *aSrcLength = *aDestLength = 0;
      return NS_OK_UENC_MOREOUTPUT;
    }

    *(PRUint32*)dest = *aBOM;
    *aBOM = 0;
    dest += 4;
  }

  // left-over high surroage code point from the prev. run.
  if (*aHighSurrogate) 
  {
    if (! *aSrcLength)
    {
      *aDestLength = 0;
      return NS_OK_UENC_MOREINPUT;
    }
    if (*aDestLength < 4) 
    {
      *aSrcLength = 0;
      *aDestLength = 0;
      return NS_OK_UENC_MOREOUTPUT;
    }
    if ((*src & 0xfc00) != 0xdc00) // Not a low surrogate codepoint. Unpaird.
      ucs4 = PRUint32(*aHighSurrogate);
    else 
      ucs4 = (((*aHighSurrogate & 0x3ffL) << 10) | (*src & 0x3ffL)) + 0x10000;

    ++src;
    if (aIsLE)
      UCS4_TO_LE_STRING(ucs4, dest);
    else
      UCS4_TO_BE_STRING(ucs4, dest);
    dest += 4;
    *aHighSurrogate = 0;
  }

  while (src < srcEnd) {
    // regular codepoint or an unpaired low surrogate
    if ((src[0] & 0xfc00) != 0xd800) 
    {
      if (destEnd - dest < 4)
        goto error_more_output;
      ucs4 = PRUint32(src[0]);  
    }
    else  // high surrogate
    {
      if ((src+1) >= srcEnd) {
        //we need another surrogate to complete this unicode char
        *aHighSurrogate = src[0];
        *aDestLength = dest - aDest;
        return NS_OK_UENC_MOREINPUT;
      }
      //handle surrogate
      if (destEnd - dest < 4)
        goto error_more_output;
      if ((src[1] & 0xfc00) != 0xdc00)  // unpaired 
        ucs4 = PRUint32(src[0]);  
      else 
      {  // convert surrogate pair to UCS4
        ucs4 = (((src[0] & 0x3ffL) << 10) | (src[1] & 0x3ffL)) + 0x10000;
        *aHighSurrogate = 0;
        ++src;
      }
    }
    if (aIsLE)
      UCS4_TO_LE_STRING(ucs4, dest);
    else
      UCS4_TO_BE_STRING(ucs4, dest);
    dest += 4;
    ++src;
  }

  *aDestLength = dest - aDest;
  return NS_OK;

error_more_output:
  *aSrcLength = src - aSrc;
  *aDestLength = dest - aDest;
  return NS_OK_UENC_MOREOUTPUT;

}

static nsresult FinishCommon(char * aDest, 
                             PRInt32 * aDestLength, 
                             PRUnichar * aHighSurrogate,
                             PRBool aIsLE)
{
  char * dest = aDest;

  if (*aHighSurrogate) {
    if (*aDestLength < 4) {
      *aDestLength = 0;
      return NS_OK_UENC_MOREOUTPUT;
    }
    PRUint32 high = PRUint32(*aHighSurrogate);
    if (aIsLE)
      UCS4_TO_LE_STRING(high, dest);
    else
      UCS4_TO_BE_STRING(high, dest);
    *aHighSurrogate = 0;
    *aDestLength = 4;
    return NS_OK;
  } 

  *aDestLength  = 0;
  return NS_OK;
}



//----------------------------------------------------------------------
// Class nsUnicodeToUTF32 [implementation]

NS_IMPL_ISUPPORTS1(nsUnicodeToUTF32Base, nsIUnicodeEncoder)


//----------------------------------------------------------------------
// Subclassing of nsIUnicodeEncoder class [implementation]

NS_IMETHODIMP nsUnicodeToUTF32Base::GetMaxLength(const PRUnichar * aSrc, 
                                                 PRInt32 aSrcLength, 
                                                 PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength * 4;
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToUTF32Base::FillInfo(PRUint32 *aInfo)
{
  memset(aInfo, 0xFF, (0x10000L >> 3));
  return NS_OK;
}


//----------------------------------------------------------------------
// Class nsUnicodeToUTF32BE [implementation]

//----------------------------------------------------------------------
// Subclassing of nsUnicodeToUTF32 class [implementation]
  

NS_IMETHODIMP nsUnicodeToUTF32BE::Convert(const PRUnichar * aSrc, 
                                          PRInt32 * aSrcLength, 
                                          char * aDest, 
                                          PRInt32 * aDestLength)
{
  return ConvertCommon(aSrc, aSrcLength, aDest, aDestLength, 
                       &mHighSurrogate, &mBOM, PR_FALSE);
}

NS_IMETHODIMP nsUnicodeToUTF32BE::Finish(char * aDest, 
                                         PRInt32 * aDestLength)
{
  return FinishCommon(aDest, aDestLength, &mHighSurrogate, PR_FALSE);
}


//----------------------------------------------------------------------
// Class nsUnicodeToUTF32LE [implementation]
  
//----------------------------------------------------------------------
// Subclassing of nsUnicodeToUTF32 class [implementation]


NS_IMETHODIMP nsUnicodeToUTF32LE::Convert(const PRUnichar * aSrc, 
                                          PRInt32 * aSrcLength, 
                                          char * aDest, 
                                          PRInt32 * aDestLength)
{
  return ConvertCommon(aSrc, aSrcLength, aDest, aDestLength, 
                       &mHighSurrogate, &mBOM, PR_TRUE);
}

NS_IMETHODIMP nsUnicodeToUTF32LE::Finish(char * aDest, 
                                         PRInt32 * aDestLength)
{
  return FinishCommon(aDest, aDestLength, &mHighSurrogate, PR_TRUE);
}

