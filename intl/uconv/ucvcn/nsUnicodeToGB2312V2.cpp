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
 *    Yueheng Xu, yueheng.xu@intel.com
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

#include "nsUnicodeToGB2312V2.h"
#include "nsICharRepresentable.h"
#include "nsUCvCnDll.h"
#include "gbku.h"

//----------------------------------------------------------------------
// Class nsUnicodeToGB2312V2 [implementation]
nsUnicodeToGB2312V2::nsUnicodeToGB2312V2()
{
  mUtil.InitToGBKTable();
}

NS_IMETHODIMP nsUnicodeToGB2312V2::ConvertNoBuff(const PRUnichar * aSrc, 
                                                 PRInt32 * aSrcLength, 
                                                 char * aDest, 
                                                 PRInt32 * aDestLength)
{
  PRInt32 iSrcLength = 0;
  PRInt32 iDestLength = 0;
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
      if(mUtil.UnicodeToGBKChar(*aSrc, PR_FALSE, &byte1, &byte2))
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
    iSrcLength++ ;   // each unicode char just count as one in PRUnichar* string
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

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToGB2312V2::GetMaxLength(const PRUnichar * aSrc, 
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  *aDestLength = 2 * aSrcLength;
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToGB2312V2::FillInfo(PRUint32 *aInfo)
{
  mUtil.FillGB2312Info(aInfo);
  //GB2312 font lib also have single byte ASCII characters, set them here
  for ( PRUint16 u = 0x0000; u <= 0x007F; u++)
    SET_REPRESENTABLE(aInfo, u);
  return NS_OK;
}
