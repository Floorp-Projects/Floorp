/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/**
 * A character set converter from GBK to Unicode.
 * 
 *
 * @created         07/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 */

#include "nsGBKToUnicode.h"
#include "nsUCvCnDll.h"
#include "gbku.h"

//----------------------------------------------------------------------
// Class nsGBKToUnicode [implementation]

nsresult nsGBKToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsGBKToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Subclassing of nsTablesDecoderSupport class [implementation]

NS_IMETHODIMP nsGBKToUnicode::GetMaxLength(const char * aSrc, 
                                              PRInt32 aSrcLength, 
                                              PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK;
}

NS_IMETHODIMP nsGBKToUnicode::ConvertNoBuff(const char* aSrc,
                                            PRInt32 * aSrcLength,
                                            PRUnichar *aDest,
                                            PRInt32 * aDestLength)
{
  PRInt32 i=0;
  PRInt32 iSrcLength = (*aSrcLength);
  DByte *pSrcDBCode = (DByte *)aSrc;
  PRUnichar *pDestDBCode = (PRUnichar *)aDest;
  PRInt32 iDestlen = 0;
  PRUint8 left, right;
  PRUint16 iGBKToUnicodeIndex = 0;
  nsresult rv=NS_OK;
  
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


