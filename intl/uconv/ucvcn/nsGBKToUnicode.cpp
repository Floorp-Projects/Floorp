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
      
      // The valid range for the 1st byte is [0x81,0xFE] 
      if ( (PRUint8) 0x80 < (PRUint8)*aSrc && (PRUint8)*aSrc < (PRUint8)0xff )
		{
          if(i+1 >= iSrcLength) 
          {
            rv = NS_OK_UDEC_MOREINPUT;
            break;
          }


          left = pSrcDBCode->leftbyte;  
          right = pSrcDBCode->rightbyte;
          // To make sure, the second byte has to be checked as well.
          // In GBK, the second byte range is [0x40,0x7E] and [0x80,0XFE]
          if ( right >= (PRUint8)0x40 && (right & 0x7f) != (PRUint8)0x7F) 
            {
              // Valid GBK code
              iGBKToUnicodeIndex = (left - 0x0081)*0x00BF + (right - 0x0040);  
              *pDestDBCode = GBKToUnicodeTable[iGBKToUnicodeIndex];
              aSrc += 2;
              i++;
            }
          else if ( left == (PRUint8)0xA0 )
            {
              // stand-alone (not followed by a valid second byte) 0xA0 !
              // treat it as valid a la Netscape 4.x
              *pDestDBCode = (PRUnichar) ( ((char )(*aSrc)) & 0x00ff);
              aSrc++;
            }
          
          else 
            {
              // Invalid GBK code point (second byte should be 0x40 or higher)
              *pDestDBCode = (PRUnichar)0xfffd;
              aSrc++;
            }
          
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


