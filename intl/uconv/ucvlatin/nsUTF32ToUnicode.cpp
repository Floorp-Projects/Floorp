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

#include "nsUCSupport.h"
#include "nsUTF32ToUnicode.h"
#include "nsCharTraits.h"
#include <string.h>

//----------------------------------------------------------------------
// static functions and macro definition common to nsUTF32(BE|LE)ToUnicode

#ifdef IS_BIG_ENDIAN
#define LE_STRING_TO_UCS4(s)                                       \
        (PRUint8(*(s)) | (PRUint8(*((s) + 1)) << 8) |              \
         (PRUint8(*((s) + 2)) << 16) | (PRUint8(*((s) + 3)) << 24))
#else
#define LE_STRING_TO_UCS4(s) (*(PRUint32*) (s))
#endif

#ifdef IS_BIG_ENDIAN
#define BE_STRING_TO_UCS4(s) (*(PRUint32*) (s))
#else
#define BE_STRING_TO_UCS4(s)                                       \
        (PRUint8(*((s) + 3)) | (PRUint8(*((s) + 2)) << 8) |         \
         (PRUint8(*((s) + 1)) << 16) | (PRUint8(*(s)) << 24))
#endif
 
static nsresult ConvertCommon(const char * aSrc, 
                              PRInt32 * aSrcLength, 
                              PRUnichar * aDest, 
                              PRInt32 * aDestLength,
                              PRUint16 * aState,
                              PRUint8  * aBuffer,
                              PRBool aIsLE)
{
   
  NS_ENSURE_TRUE(*aState < 4, NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(*aDestLength > 0, NS_ERROR_INVALID_ARG);

  const char *src = aSrc;
  const char *srcEnd = aSrc + *aSrcLength;
   
  PRUnichar *dest = aDest;
  PRUnichar *destEnd = aDest + *aDestLength;

  if (*aState > *aSrcLength) 
  {
    memcpy(aBuffer + 4 - *aState, src, *aSrcLength);
    *aDestLength = 0;
    *aState -= *aSrcLength;
    return NS_OK_UDEC_MOREINPUT;
  }

  PRUint32 ucs4;

  // prev. run left a partial UTF-32 seq. 
  if (*aState > 0)
  {
    memcpy(aBuffer + 4 - *aState, src, *aState);
    ucs4 =  aIsLE ? LE_STRING_TO_UCS4(aBuffer) : BE_STRING_TO_UCS4(aBuffer); 
    if (ucs4 < 0x10000L)  // BMP
    {
      *dest++= IS_SURROGATE(ucs4) ? UCS2_REPLACEMENT_CHAR : PRUnichar(ucs4);
    }
    else if (ucs4 < 0x110000L)  // plane 1 through plane 16 
    {
      if (destEnd - dest < 2) 
      {
        *aSrcLength = 0;
        *aDestLength = 0;
        return NS_OK_UDEC_MOREOUTPUT;
      }
      *dest++= H_SURROGATE(ucs4);
      *dest++= L_SURROGATE(ucs4);
    }       
    // Codepoints in plane 17 and higher (> 0x10ffff)
    // are not representable in UTF-16 we use for the internal
    // character representation. This is not a problem
    // because Unicode/ISO 10646 will never assign characters
    // in plane 17 and higher. Therefore, we convert them
    // to Unicode replacement character (0xfffd).
    else                   
      *dest++ = UCS2_REPLACEMENT_CHAR;
    src += *aState;
    *aState = 0;
  }

  nsresult rv = NS_OK;  // conversion result

  for ( ; src < srcEnd && dest < destEnd; src += 4)
  {
    if (srcEnd - src < 4) 
    {
      // fill up aBuffer until src buffer gets exhausted.
      memcpy(aBuffer, src, srcEnd - src);
      *aState = 4 - (srcEnd - src); // set add. char to read in next run
      src = srcEnd;
      rv = NS_OK_UDEC_MOREINPUT;
      break;
    }

    ucs4 =  aIsLE ? LE_STRING_TO_UCS4(src) : BE_STRING_TO_UCS4(src); 
    if (ucs4 < 0x10000L)  // BMP
    {
      *dest++= IS_SURROGATE(ucs4) ? UCS2_REPLACEMENT_CHAR : PRUnichar(ucs4);
    }
    else if (ucs4 < 0x110000L)  // plane 1 through plane 16 
    {
      if (destEnd - dest < 2) 
        break;
      // ((ucs4 - 0x10000) >> 10) + 0xd800;
      *dest++= H_SURROGATE(ucs4);
      *dest++= L_SURROGATE(ucs4);
    }       
    else                       // plane 17 and higher
      *dest++ = UCS2_REPLACEMENT_CHAR;
  }

  //output not finished, output buffer too short
  if((NS_OK == rv) && (src < srcEnd) && (dest >= destEnd)) 
    rv = NS_OK_UDEC_MOREOUTPUT;

  *aSrcLength = src - aSrc;
  *aDestLength  = dest - aDest;

  return rv;
}


//----------------------------------------------------------------------
// Class nsUTF32ToUnicode [implementation]

nsUTF32ToUnicode::nsUTF32ToUnicode() : nsBasicDecoderSupport()
{
  Reset();
}

//----------------------------------------------------------------------
// Subclassing of nsDecoderSupport class [implementation]

NS_IMETHODIMP nsUTF32ToUnicode::GetMaxLength(const char * aSrc, 
                                            PRInt32 aSrcLength, 
                                            PRInt32 * aDestLength)
{
  // Non-BMP characters take two PRUnichars(a pair of surrogate codepoints)
  // so that we have to divide by 2 instead of 4 for the worst case.
  *aDestLength = aSrcLength / 2;
  return NS_OK;
}


//----------------------------------------------------------------------
// Subclassing of nsBasicDecoderSupport class [implementation]

NS_IMETHODIMP nsUTF32ToUnicode::Reset()
{
  // the number of additional bytes to read to complete UTF-32 4byte seq.
  mState = 0;  
  memset(mBufferInc, 0, 4);
  return NS_OK;

}


//----------------------------------------------------------------------
// Class nsUTF32BEToUnicode [implementation]

//----------------------------------------------------------------------
// Subclassing of nsUTF32ToUnicode class [implementation]

NS_IMETHODIMP nsUTF32BEToUnicode::Convert(const char * aSrc, 
                                          PRInt32 * aSrcLength, 
                                          PRUnichar * aDest, 
                                          PRInt32 * aDestLength)
{
  return ConvertCommon(aSrc, aSrcLength, aDest, aDestLength, &mState, 
                       mBufferInc, PR_FALSE);
}

//----------------------------------------------------------------------
// Class nsUTF32LEToUnicode [implementation]
  
//----------------------------------------------------------------------
// Subclassing of nsUTF32ToUnicode class [implementation]

NS_IMETHODIMP nsUTF32LEToUnicode::Convert(const char * aSrc, 
                                          PRInt32 * aSrcLength, 
                                          PRUnichar * aDest, 
                                          PRInt32 * aDestLength)
{
  return ConvertCommon(aSrc, aSrcLength, aDest, aDestLength, &mState, 
                       mBufferInc, PR_TRUE);
}

// XXX : What to do with 'unflushed' mBufferInc?? : Finish()
  
