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

#include "nsUnicodeToGB2312GL.h"
#include "nsUCvCnDll.h"


#define _GBKU_TABLE_		// use a shared table
#include "gbku.h"

//----------------------------------------------------------------------
// Class nsUnicodeToGB2312GL [implementation]

#define TRUE 1
#define FALSE 0

nsUnicodeToGB2312GL::nsUnicodeToGB2312GL()
{
  PRUint8 left, right;
  PRUnichar unicode;
  PRUint16 i;

  if ( !gUnicodeToGBKTableInitialized )
    {
      for ( i=0; i<MAX_GBK_LENGTH; i++ )
        {
          
          left =  ( i / 0x00BF + 0x0081);
          right = ( i % 0x00BF+ 0x0040);
          unicode = GBKToUnicodeTable[i];
      
          // to reduce size of UnicodeToGBKTable, we only do direct unicode to GB 
          // table mapping between unicode 0x4E00 and 0xA000. Others by searching
          // GBKToUnicodeTable. There is a trade off between memory usage and speed.
          if ( (unicode >= 0x4E00 ) && ( unicode <= 0xA000 ))
            {
              unicode -= 0x4E00; 
              UnicodeToGBKTable[unicode].leftbyte = left;
              UnicodeToGBKTable[unicode].rightbyte = right;
            }
        }
      gUnicodeToGBKTableInitialized = PR_TRUE; 
    }
}

NS_IMETHODIMP nsUnicodeToGB2312GL::ConvertNoBuff(const PRUnichar * aSrc, 
                                                 PRInt32 * aSrcLength, 
                                                 char * aDest, 
                                                 PRInt32 * aDestLength)
{
	PRInt32 i=0;
	PRInt32 iSrcLength = 0;
  DByte *pDestDBCode;
  DByte *pSrcDBCode; 
	PRInt32 iDestLength = 0;
  PRUnichar unicode;
  PRUint8 left, right;
  nsresult res = NS_OK;
	PRUnichar *pSrc = (PRUnichar *)aSrc;
	pDestDBCode = (DByte *)aDest;
    
  
  while( iSrcLength < *aSrcLength)
	{
		pDestDBCode = (DByte *)aDest;
    
    unicode = *pSrc;
		//if unicode's hi byte has something, it is not ASCII, must be a GB
		if( unicode & 0xff00 )
      {
        // To reduce the UnicodeToGBKTable size, we only use direct table mapping 
        // for unicode between 0x4E00 - 0xA000
        if ( (unicode >= 0x4E00 ) && ( unicode <= 0xA000 ) )
          {
            unicode -= 0x4E00;
            pDestDBCode->leftbyte =  UnicodeToGBKTable[unicode].leftbyte & 0x7f;                
            pDestDBCode->rightbyte = UnicodeToGBKTable[unicode].rightbyte & 0x7f;  
          }
        else  
          {
            // other ones we search in GBK to Unicode table
            for ( i=0; i<MAX_GBK_LENGTH; i++)
              {
                if ( unicode  == GBKToUnicodeTable[i] )
                  {
                    //this manipulation handles the little endian / big endian issues
                    left = (char) ( i / 0x00BF + 0x0081) & 0x7f  ;
                    right = (char) ( i % 0x00BF+ 0x0040) & 0x7f;
                    pDestDBCode->leftbyte = left;  
                    pDestDBCode->rightbyte = right;  
                  }
              }
          } 
        
        //	UnicodeToGBK( *pSrc, pDestDBCode);
        aDest += 2;	// increment 2 bytes
        pDestDBCode = (DByte *)aDest;
        iDestLength +=2; // Dest Length in units of chars, each GB char counts as two in string length
        iSrcLength++ ; // Each unicode char just count as one in PRUnichar string;  
		    pSrc++;	 // increment 2 bytes    
    }
		else
      { // This is ASCII, not expected, report error.
        res = NS_ERROR_UENC_NOMAPPING;
        break; 
      }

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


nsresult nsUnicodeToGB2312GL::CreateInstance(nsISupports ** aResult) 
{
  nsIUnicodeEncoder *p = new nsUnicodeToGB2312GL();
  if(p) {
    *aResult = p;
    return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
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



#define SET_REPRESENTABLE(info, c)  (info)[(c) >> 5] |= (1L << ((c) & 0x1f))

NS_IMETHODIMP nsUnicodeToGB2312GL::FillInfo(PRUint32 *aInfo)
{
	PRUint16 i,j, k;
  PRUnichar SrcUnicode;

  // aInfo should already been initialized as 2048 element array of 32 bit by caller
  NS_ASSERTION( sizeof(aInfo[0]) == 4, "aInfo size incorrect" );

  // valid GBK rows are in 0x81 to 0xFE
  for ( i=0x0081;i<0x00FF;i++) 
    {
      // HZ and GB2312 starts at row 0x21|0x80 = 0xA1
        if ( i < 0xA1 )
        continue;

      // valid GBK columns are in 0x41 to 0xFE
      for( j=0x0040;j<0x00FF;j++)
        {
          //HZ and GB2312 starts at col 0x21 | 0x80 = 0xA1
          if ( j < 0xA1 )
            continue;
          
          // k is index in GBKU.H table
          k = (i - 0x0081)*0x00BF +(j-0x0040);
          
          SrcUnicode = GBKToUnicodeTable[k];
          if (( SrcUnicode != 0xFFFF ) && (SrcUnicode != 0xFFFD) )
            {
              SET_REPRESENTABLE(aInfo, SrcUnicode);
            }            
        }
    }                   
  
  return NS_OK;
}
