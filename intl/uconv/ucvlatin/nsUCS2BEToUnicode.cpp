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

#include "nsUCConstructors.h"
#include "nsUCS2BEToUnicode.h"
#include "nsUCvLatinDll.h"
#include <string.h>
#include "prtypes.h"

// XXX : illegal surrogate code points are just passed through !!
static nsresult
UTF16ConvertToUnicode(PRUint8& aState, PRUint8& aData, const char * aSrc,
                      PRInt32 * aSrcLength, PRUnichar * aDest,
                      PRInt32 * aDestLength)
{
  const char* src = aSrc;
  const char* srcEnd = aSrc + *aSrcLength;
  PRUnichar* dest = aDest;
  PRUnichar* destEnd = aDest + *aDestLength;

  if(2 == aState) // first time called
  {
    NS_ASSERTION(*aSrcLength >= 2, "Too few bytes in input");

    // Eliminate BOM (0xFEFF). Note that different endian case is taken care of
    // in |Convert| of LE and BE converters. Here, we only have to
    // deal with the same endian case. That is, 0xFFFE (byte-swapped BOM) is
    // illegal.
    if(0xFEFF == *((PRUnichar*)src)) {
      src+=2;
    } else if(0xFFFE == *((PRUnichar*)src)) {
      *aSrcLength=0;
      *aDestLength=0;
      return NS_ERROR_ILLEGAL_INPUT;
    }  
    aState=0;
  }

  PRInt32 copybytes;

  if((1 == aState) && (src < srcEnd))
  {
    if(dest >= destEnd)
      goto error;

    char tmpbuf[2];

    // the 1st byte of a 16-bit code unit was stored in |aData| in the previous
    // run while the 2nd byte has to come from |*src|. We just have to copy
    // 'byte-by-byte'. Byte-swapping, if necessary, will be done in |Convert| of
    // LE and BE converters.
    PRUnichar * up = (PRUnichar*) &tmpbuf[0];
    tmpbuf[0]= aData;
    tmpbuf[1]= *src++;
    *dest++ = *up;
  }
  
  copybytes = (destEnd-dest)*2;
  // if |srcEnd-src| is odd, we copy one fewer bytes.
  if(copybytes > (~1 & (srcEnd - src)))
      copybytes = ~1 & (srcEnd - src);
  memcpy(dest,src,copybytes);
  src +=copybytes;
  dest +=(copybytes/2);
  if(srcEnd == src)  { // srcLength was even.
     aState = 0;
  } else if(1 == (srcEnd - src) ) { // srcLength was odd. 
     aState = 1;
     aData  = *src++;  // store the lead byte of a 16-bit unit for the next run.
  } else  {
     goto error;
  }
  
  *aDestLength = dest - aDest;
  *aSrcLength =  src  - aSrc; 
  return NS_OK;

error:
  *aDestLength = dest - aDest;
  *aSrcLength =  src  - aSrc; 
  return  NS_OK_UDEC_MOREOUTPUT;
}

static void
SwapBytes(PRUnichar *aDest, PRInt32 aLen)
{
  for (PRUnichar *p = aDest; aLen > 0; ++p, --aLen)
     *p = ((*p & 0xff) << 8) | ((*p >> 8) & 0xff);
}

NS_IMETHODIMP
nsUTF16ToUnicodeBase::Reset()
{
  mState = 2;
  mData = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsUTF16ToUnicodeBase::GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
                                   PRInt32 * aDestLength)
{
  // the left-over byte of the previous run has to be taken into account.
  *aDestLength = (aSrcLength + ((1 == mState) ? 1 : 0)) / 2;
  return NS_OK;
}


NS_IMETHODIMP
nsUTF16BEToUnicode::Convert(const char * aSrc, PRInt32 * aSrcLength,
                            PRUnichar * aDest, PRInt32 * aDestLength)
{
#ifdef IS_LITTLE_ENDIAN
    // Remove the BOM if we're little-endian. The 'same endian' case with the
    // leading BOM will be taken care of by |UTF16ConvertToUnicode|.
    if(2 == mState) // Called for the first time.
    {
      NS_ASSERTION(*aSrcLength >= 2, "Too few bytes in input");
      if(0xFFFE == *((PRUnichar*)aSrc)) {
        // eliminate BOM (on LE machines, BE BOM is 0xFFFE)
        aSrc+=2;
        *aSrcLength-=2;
      } else if(0xFEFF == *((PRUnichar*)aSrc)) {
        *aSrcLength=0;
        *aDestLength=0;
        return NS_ERROR_ILLEGAL_INPUT;
      }  
      mState=0;
    }
#endif

  nsresult rv = UTF16ConvertToUnicode(mState, mData, aSrc, aSrcLength,
                                      aDest, aDestLength);

#ifdef IS_LITTLE_ENDIAN
  SwapBytes(aDest, *aDestLength);
#endif
  return rv;
}

NS_IMETHODIMP
nsUTF16LEToUnicode::Convert(const char * aSrc, PRInt32 * aSrcLength,
                            PRUnichar * aDest, PRInt32 * aDestLength)
{
#ifdef IS_BIG_ENDIAN
    // Remove the BOM if we're big-endian. The 'same endian' case with the
    // leading BOM will be taken care of by |UTF16ConvertToUnicode|.
    if(2 == mState) // first time called
    {
      NS_ASSERTION(*aSrcLength >= 2, "Too few bytes in input");
      if(0xFFFE == *((PRUnichar*)aSrc)) {
        // eliminate BOM (on BE machines, LE BOM is 0xFFFE)
        aSrc+=2;
        *aSrcLength-=2;
      } else if(0xFEFF == *((PRUnichar*)aSrc)) {
        *aSrcLength=0;
        *aDestLength=0;
        return NS_ERROR_ILLEGAL_INPUT;
      }  
      mState=0;
    }
#endif
    
  nsresult rv = UTF16ConvertToUnicode(mState, mData, aSrc, aSrcLength, aDest,
                                      aDestLength);

#ifdef IS_BIG_ENDIAN
  SwapBytes(aDest, *aDestLength);
#endif
  return rv;
}

NS_IMETHODIMP
nsUTF16ToUnicode::Reset()
{
  mEndian = kUnknown;
  mFoundBOM = PR_FALSE;
  return nsUTF16ToUnicodeBase::Reset();
}

NS_IMETHODIMP
nsUTF16ToUnicode::Convert(const char * aSrc, PRInt32 * aSrcLength,
                          PRUnichar * aDest, PRInt32 * aDestLength)
{
    if(2 == mState) // first time called
    {
      NS_ASSERTION(*aSrcLength >= 2, "Too few bytes in input");

      // check if BOM (0xFEFF) is at the beginning, remove it if found, and
      // set mEndian accordingly.
      if(0xFF == PRUint8(aSrc[0]) && 0xFE == PRUint8(aSrc[1])) {
        aSrc += 2;
        *aSrcLength -= 2;
        mState = 0;
        mEndian = kLittleEndian;
        mFoundBOM = PR_TRUE;
      }
      else if(0xFE == PRUint8(aSrc[0]) && 0xFF == PRUint8(aSrc[1])) {
        aSrc += 2;
        *aSrcLength -= 2;
        mState = 0;
        mEndian = kBigEndian;
        mFoundBOM = PR_TRUE;
      }
      // BOM is not found, but we can use a simple heuristic to determine
      // the endianness. Assume the first character is [U+0001, U+00FF].
      // Not always valid, but it's very likely to hold for html/xml/css. 
      else if(!aSrc[0] && aSrc[1]) {  // 0x00 0xhh (hh != 00)
        mState = 0;                   
        mEndian = kBigEndian;
      }
      else if(aSrc[0] && !aSrc[1]) {  // 0xhh 0x00 (hh != 00)
        mState = 0;
        mEndian = kLittleEndian;
      }
      else { // Neither BOM nor 'plausible' byte patterns at the beginning.
             // Just assume it's BE (following Unicode standard)
             // and let the garbage show up in the browser. (security concern?)
             // (bug 246194)
        mState = 0;   
        mEndian = kBigEndian;
      }
    }
    
    nsresult rv = UTF16ConvertToUnicode(mState, mData, aSrc, aSrcLength, aDest,
                                        aDestLength);

#ifdef IS_BIG_ENDIAN
    if (mEndian == kLittleEndian)
#elif defined(IS_LITTLE_ENDIAN)
    if (mEndian == kBigEndian)
#else
    #error "Unknown endianness"
#endif
      SwapBytes(aDest, *aDestLength);

    // If BOM is not found and we're to return NS_OK, signal that BOM
    // is not found. Otherwise, return |rv| from |UTF16ConvertToUnicode|
    return (rv == NS_OK && !mFoundBOM) ? NS_OK_UDEC_NOBOMFOUND : rv;
}
