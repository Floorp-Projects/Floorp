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
 * The Initial Developer of the Original Code is Intel
 * Corporation. Portions created by Intel are
 * Copyright (C) 1999 Intel Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): Yueheng Xu <yuheng.xu@intel.com>
 *
 * This file is a newer implementation of GB2312 converters developed by
 * Intel Corporation intended to replace the existing mozilla GB2312 converters
 * developed by Netscape Communications in mozilla. 
 *
 */
// =======================================================================
// Original Author: Yueheng Xu
// email: yueheng.xu@intel.com
// phone: (503)264-2248
// Intel Corporation, Oregon, USA
// Last Update: September 7, 1999
// Revision History: 
// 09/07/1999 - initial version. Based on exisiting nsGB2312ToUnicode.cpp. Removed
//              dependecy on those shift/map tables. Now we only depend on gbku.h.
// 09/28/1999 - changed to use the same table and code as GBKToUnicode converter
// ======================================================================================

#include "nsGB2312ToUnicodeV2.h"
#include "nsUCvCnDll.h"

#define _GBKU_TABLE_		// to use a shared GBKU table
#include "gbku.h"

//----------------------------------------------------------------------
// Class nsGB2312ToUnicodeV2 [implementation]

nsresult nsGB2312ToUnicodeV2::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsGB2312ToUnicodeV2();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Subclassing of nsTablesDecoderSupport class [implementation]

NS_IMETHODIMP nsGB2312ToUnicodeV2::GetMaxLength(const char * aSrc, 
                                              PRInt32 aSrcLength, 
                                              PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK;
}

NS_IMETHODIMP nsGB2312ToUnicodeV2::ConvertNoBuff(const char* aSrc,
                                                 PRInt32 * aSrcLength,
                                                 PRUnichar *aDest,
                                                 PRInt32 * aDestLength)
{

  PRInt32 i=0;
  PRInt32 iSrcLength = (*aSrcLength);
  DByte *pSrcDBCode = (DByte *)aSrc;
  PRUnichar *pDestDBCode = (PRUnichar *)aDest;
  PRInt32 iDestlen = 0;
  PRUint8 left;
  PRUint8 right;
  PRUint16 iGBKToUnicodeIndex = 0;
  nsresult rv = NS_OK;

  for (i=0;i<iSrcLength;i++)
    {
      pSrcDBCode = (DByte *)aSrc;
      pDestDBCode = aDest;
      
      if ( iDestlen >= (*aDestLength) )
      {
         rv = NS_OK_UDEC_MOREOUTPUT;
         break;
      }
		
      if ( *aSrc & 0x80 )
        {
          if(i+1 >= iSrcLength)
          {
            rv = NS_OK_UDEC_MOREINPUT;
            break;
          }
          
		  // The source is a GBCode
     
          left = pSrcDBCode->leftbyte; 
          right = pSrcDBCode->rightbyte;

          iGBKToUnicodeIndex = (left - 0x0081)*0x00BF + (right - 0x0040);  
          *pDestDBCode = GBKToUnicodeTable[iGBKToUnicodeIndex];
          
          aSrc += 2;
          i++;
		}
      else
		{
          // The source is an ASCII
          *pDestDBCode = (PRUnichar) ( ((char )(*aSrc)) & 0x00ff);
          aSrc++;
		}

      iDestlen++;
      aDest++;
      *aSrcLength = i+1;
	}

  *aDestLength = iDestlen;

  return rv;
  
}
