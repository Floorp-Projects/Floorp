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
 * A character set converter from Unicode to GBK.
 * 
 *
 * @created         08/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 * Revision History
 * 04/Oct/1999. Yueheng Xu: used table UnicodeToGBKTable[0x5200] to make 
 *              Unicode to GB mapping fast 
 */

#include "nsUnicodeToGBK.h"
#include "nsUCvCnDll.h"

#define _UNICODE_TO_GBK_ENCODER_  // this is the place we allocate memory for UnicodeToGBKTable[0xFFFF]
#define _GBKU_TABLE_		// to use a shared GBKU table
#include "gbku.h"

//----------------------------------------------------------------------
// Class nsUnicodeToGBK [implementation]

#define TRUE 1
#define FALSE 0

nsUnicodeToGBK::nsUnicodeToGBK()
  {
    PRUint8 left, right;
    PRUnichar unicode;
    PRUnichar i;

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


NS_IMETHODIMP nsUnicodeToGBK::ConvertNoBuff(const PRUnichar * aSrc, 
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

    while (iSrcLength < *aSrcLength )
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
             pDestDBCode->leftbyte =  UnicodeToGBKTable[unicode].leftbyte ;                
             pDestDBCode->rightbyte = UnicodeToGBKTable[unicode].rightbyte ;  
           }
         else  
           {
             // other ones we search in GBK to Unicode table
             for ( i=0; i<MAX_GBK_LENGTH; i++)
               {
                 if ( unicode  == GBKToUnicodeTable[i] )
                   {
                     //this manipulation handles the little endian / big endian issues
                     left = (PRUint8) ( i / 0x00BF + 0x0081);
                     right = (PRUint8) ( i % 0x00BF+ 0x0040);
                     pDestDBCode->leftbyte = left;  
                     pDestDBCode->rightbyte = right;  
                   }
               }
           } 
         
         
         //	UnicodeToGBK( *pSrc, pDestDBCode);
         aDest += 2;	// increment 2 bytes
         pDestDBCode = (DByte *)aDest;
         iDestLength +=2;
       }
       else
         {
           // this is an ASCII
           *aDest = (char)*pSrc;
           aDest++; // increment 1 byte
           iDestLength +=1;
         }

       iSrcLength++ ; // Each unicode char just count as one in PRUnichar string;  	  
       pSrc++;	 // increment 2 bytes
       
       if ( iDestLength >= (*aDestLength) && (iSrcLength < *aSrcLength) )
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

NS_IMETHODIMP nsUnicodeToGBK::GetMaxLength(const PRUnichar * aSrc, 
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  *aDestLength = 2 * aSrcLength;
  return NS_OK;
}


#define SET_REPRESENTABLE(info, c)  (info)[(c) >> 5] |= (1L << ((c) & 0x1f))

NS_IMETHODIMP nsUnicodeToGBK::FillInfo(PRUint32 *aInfo)
{

  PRUint16 i,j, k;
  PRUnichar SrcUnicode;

  // aInfo should already been initialized as 2048 element array of 32 bit by caller
  NS_ASSERTION( sizeof(aInfo[0]) == 4, "aInfo size incorrect" );

  // valid GBK rows are in 0x81 to 0xFE
  for ( i=0x0081; i<=0x00FF; i++) 
    {
      // valid GBK columns are in 0x40 to 0xFE
      for( j=0x0040; j<0x00FF; j++)
        {
          // k is index in GBKU.H table
          k = (i - 0x0081)*0x00BF +(j-0x0040);    
          SrcUnicode = GBKToUnicodeTable[k];
          if (( SrcUnicode != 0xFFFF ) && (SrcUnicode != 0xFFFD) )
            {
              SET_REPRESENTABLE(aInfo, SrcUnicode);
            }               
        }
    }                   

  //GBK font lib also have single byte ASCII characters, set them here
  for ( SrcUnicode = 0x0000; SrcUnicode <= 0x00FF; SrcUnicode++)
  {
    SET_REPRESENTABLE(aInfo, SrcUnicode);
  }   
  return NS_OK;
}








