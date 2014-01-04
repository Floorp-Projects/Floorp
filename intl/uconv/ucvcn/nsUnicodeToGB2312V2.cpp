/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeToGB2312V2.h"
#include "gbku.h"

//----------------------------------------------------------------------
// Class nsUnicodeToGB2312V2 [implementation]
nsUnicodeToGB2312V2::nsUnicodeToGB2312V2() :
  nsEncoderSupport(2)
{
}

NS_IMETHODIMP nsUnicodeToGB2312V2::ConvertNoBuff(const char16_t * aSrc, 
                                                 int32_t * aSrcLength, 
                                                 char * aDest, 
                                                 int32_t * aDestLength)
{
  int32_t iSrcLength = 0;
  int32_t iDestLength = 0;
  nsresult res = NS_OK;
  
  while (iSrcLength < *aSrcLength)
  {
    //if unicode's hi byte has something, it is not ASCII, must be a GB
    if(IS_ASCII(*aSrc))
    {
      // this is an ASCII
      *aDest = CAST_UNICHAR_TO_CHAR(*aSrc);
      aDest++; // increment 1 byte
      iDestLength +=1;
    } else {
      char byte1, byte2;
      if(mUtil.UnicodeToGBKChar(*aSrc, false, &byte1, &byte2))
      {
        if(iDestLength+2 > *aDestLength) 
        {
          res = NS_OK_UENC_MOREOUTPUT;
          break;
        }
        aDest[0]=byte1;
        aDest[1]=byte2;
        aDest += 2;  // increment 2 bytes
        iDestLength +=2; // each GB char count as two in char* string
      } else {
        // cannot convert
        res= NS_ERROR_UENC_NOMAPPING;
        iSrcLength++;   // include length of the unmapped character
        break;
      }
    }
    iSrcLength++ ;   // each unicode char just count as one in char16_t* string
    aSrc++;  
    if ( iDestLength >= (*aDestLength) && (iSrcLength < *aSrcLength ))
    {
      res = NS_OK_UENC_MOREOUTPUT;
      break;
    }
  }
  *aDestLength = iDestLength;
  *aSrcLength = iSrcLength;
  return res;
}
