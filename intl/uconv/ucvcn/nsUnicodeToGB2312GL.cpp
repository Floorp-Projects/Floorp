/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *          Yueheng Xu, yueheng.xu@intel.com
 */

#include "nsICharRepresentable.h"
#include "nsUnicodeToGB2312GL.h"
#include "nsUCvCnDll.h"

#include "gbku.h"

//----------------------------------------------------------------------
// Class nsUnicodeToGB2312GL [implementation]

nsUnicodeToGB2312GL::nsUnicodeToGB2312GL()
{
  mUtil.InitToGBKTable();
}

NS_IMETHODIMP nsUnicodeToGB2312GL::ConvertNoBuff(const PRUnichar * aSrc, 
                                                 PRInt32 * aSrcLength, 
                                                 char * aDest, 
                                                 PRInt32 * aDestLength)
{
  PRInt32 iSrcLength = 0;
  PRInt32 iDestLength = 0;
  nsresult res = NS_OK;
  while( iSrcLength < *aSrcLength)
  {
    char byte1, byte2;
    if(mUtil.UnicodeToGBKChar(*aSrc, PR_TRUE, &byte1,&byte2))
    {
      if(iDestLength+2 > *aDestLength)
      {
        res = NS_OK_UENC_MOREOUTPUT;
        break;
      }
      aDest[0]=byte1;
      aDest[1]=byte2;
      aDest += 2;	// increment 2 bytes
      iDestLength +=2; // Dest Length in units of chars, each GB char counts as two in string length
    } else {
      // cannot map
      res = NS_ERROR_UENC_NOMAPPING;
      break; 
    }
    iSrcLength++ ; // Each unicode char just count as one in PRUnichar string;  
    aSrc++; 
    // if dest buffer not big enough, handles it here. 
    if ( (iDestLength >= *aDestLength) && ( iSrcLength < *aSrcLength) )
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

NS_IMETHODIMP nsUnicodeToGB2312GL::GetMaxLength(const PRUnichar * aSrc, 
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  *aDestLength = 2 * aSrcLength;
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToGB2312GL::FillInfo(PRUint32 *aInfo)
{
  mUtil.FillGB2312Info(aInfo);
  return NS_OK;
}
