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
 *
 * This Unicode to HZ converter is a contribution from Intel Corporation
 * to the mozilla project.
 */
 /**
 * A character set converter from Unicode to HZ.
 * 
 *
 * @created         08/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 * Revision History
 * 04/Oct/1999. Yueheng Xu: Fixed line continuation problem when line 
 *                          ended by '~';
 *              Used table UnicodeToGBK[] to speed up the mapping.
 */

#include "nsUnicodeToHZ.h"
#include "nsUCvCnDll.h"

#define _GBKU_TABLE_		// to use a shared GBKU table
#include "gbku.h"

//----------------------------------------------------------------------
// Class nsUnicodeToGBK [implementation]

#define TRUE 1
#define FALSE 0

void  nsUnicodeToHZ::UnicodeToGBK(PRUnichar SrcUnicode, DByte *pGBCode)
{
  PRUnichar unicode;
  PRUint8 left, right;

  unicode = SrcUnicode;
  
  //if unicode's hi byte has something, it is not ASCII, must be a GB
  NS_ASSERTION( unicode & 0xff00, "expect a GB" );
    
  // To reduce the UnicodeToGBKTable size, we only use direct table mapping 
  // for unicode between 0x4E00 - 0xA000
  if ( (unicode >= 0x4E00 ) && ( unicode <= 0xA000 ) )
    {
      unicode -= 0x4E00; 
      left =  UnicodeToGBKTable[unicode].leftbyte ;                
      right = UnicodeToGBKTable[unicode].rightbyte ;  
      pGBCode->leftbyte = left;  
      pGBCode->rightbyte = right;  
    }
  else  
    {
      // other ones we search in GBK to Unicode table
      for ( PRUint16 i=0; i<MAX_GBK_LENGTH; i++)
        {
          if ( unicode  == GBKToUnicodeTable[i] )
            {
              left = (PRUint8) ( i / 0x00BF + 0x0081) | 0x80  ;
              right = (PRUint8) ( i % 0x00BF+ 0x0040) | 0x80;
              pGBCode->leftbyte = left;  
              pGBCode->rightbyte = right;  
            }
        }
    }   
}


void nsUnicodeToHZ::UnicodeToHZ(PRUnichar SrcUnicode, DByte *pGBCode)
{
  PRUnichar unicode;
  PRUint8 left, right;

  unicode = SrcUnicode;
  
  //if unicode's hi byte has something, it is not ASCII, must be a GB
  NS_ASSERTION( unicode & 0xff00, "expect a GB" );
    
  // To reduce the UnicodeToGBKTable size, we only use direct table mapping 
  // for unicode between 0x4E00 - 0xA000
  if ( (unicode >= 0x4E00 ) && ( unicode <= 0xA000 ) )
    {
      unicode -= 0x4E00; 
      left =  UnicodeToGBKTable[unicode].leftbyte  ;                
      right = UnicodeToGBKTable[unicode].rightbyte ;  
      pGBCode->leftbyte =  left & 0x7F ;                
      pGBCode->rightbyte = right & 0x7F ;  
    }
  else  
    {
      // other ones we search in GBK to Unicode table
      for ( PRUint16 i=0; i<MAX_GBK_LENGTH; i++)
        {
          if ( unicode  == GBKToUnicodeTable[i] )
            {
              left = (PRUint8) ( i / 0x00BF + 0x0081);
              right = (PRUint8) ( i % 0x00BF+ 0x0040);
              pGBCode->leftbyte = left;  
              pGBCode->rightbyte = right;  
            }
        }
    }   
}

nsUnicodeToHZ::nsUnicodeToHZ()
{
   PRUint8 left, right;
   PRUnichar unicode;
   PRUnichar i;

   for ( i=0; i<MAX_GBK_LENGTH; i++ )
     {
   
       left = (PRUint8) ( i / 0x00BF + 0x0081);
       right = (PRUint8)( i % 0x00BF+ 0x0040);
       unicode = GBKToUnicodeTable[i];

       // to reduce size of UnicodeToGBKTable, we only do direct unicode to GB 
       // table mapping between unicode 0x4E00 and 0xA000. Others by searching
       // GBKToUnicodeTable. There is a trade off between memory usage and speed.
       if ( (unicode >= 0x4E00 ) && ( unicode <= 0xA000 ))
         {
           unicode -= 0x4E00; // we start using table at 0x4E00 
           UnicodeToGBKTable[unicode].leftbyte = left;
           UnicodeToGBKTable[unicode].rightbyte = right; 
         }
     } 
}

  

NS_IMETHODIMP nsUnicodeToHZ::ConvertNoBuff(const PRUnichar * aSrc, 
										PRInt32 * aSrcLength, 
										char * aDest, 
										PRInt32 * aDestLength)
{
#define HZ_STATE_GB		1
#define HZ_STATE_ASCII	2
#define HZ_STATE_TILD	3
#define HZLEAD1 '~'
#define HZLEAD2 '{'
#define HZLEAD3 '}'
#define UNICODE_TILD	0x007E

  static PRUint16 hz_state = HZ_STATE_ASCII;	// per HZ spec, default to HZ mode
  PRInt32 i=0;
  PRInt32 iSrcLength = *aSrcLength;
  DByte *pDestDBCode; 
  PRInt32 iDestLength = 0;

  PRUnichar *pSrc = (PRUnichar *)aSrc;

  pDestDBCode = (DByte *)aDest;

  for (i=0;i< iSrcLength;i++)
	{
      pDestDBCode = (DByte *)aDest;

      if( (*pSrc) & 0xff00 )
		{
          // hi byte has something, it is not ASCII, process as a GB
          
          if ( hz_state != HZ_STATE_GB )
			{
              // we are adding a '~{' ESC sequence to star a HZ string
              hz_state = HZ_STATE_GB;
              pDestDBCode->leftbyte  = '~';
              pDestDBCode->rightbyte = '{';
              aDest += 2;	// increment 2 bytes
              pDestDBCode = (DByte *)aDest;
              iDestLength +=2;
			}

          UnicodeToHZ( *pSrc, pDestDBCode);
          aDest += 2;	// increment 2 bytes
          pDestDBCode = (DByte *)aDest;
          iDestLength +=2;
		}
      else
		{
          // this is an ASCII

          // if we are in HZ mode, end it by adding a '~}' ESC sequence
          if ( hz_state == HZ_STATE_GB )
			{
              hz_state = HZ_STATE_ASCII;
              pDestDBCode->leftbyte  = '~';
              pDestDBCode->rightbyte = '}';
              aDest += 2;	// increment 2 bytes
              pDestDBCode = (DByte *)aDest;
              iDestLength +=2;
			}
          
          // if this is a regular char '~' , convert it to two '~'
          if ( *pSrc == UNICODE_TILD )
			{
              pDestDBCode->leftbyte  = '~';
              pDestDBCode->rightbyte = '~';
              aDest += 2;	// increment 2 bytes
              pDestDBCode = (DByte *)aDest;
              iDestLength +=2;
			}
          else 
			{
              // other regular ASCII chars convert by normal ways
			
              // Is this works for both little endian and big endian machines ?
              *aDest = (char) ( (PRUnichar)(*pSrc) );
              
              aDest++; // increment 1 byte
              iDestLength +=1;
			}
		}
      pSrc++;	 // increment 2 bytes

      if ( iDestLength >= (*aDestLength) )
		{
          break;
		}
	}

  *aDestLength = iDestLength;
  *aSrcLength = i;
  
  return NS_OK;
}

#define SET_REPRESENTABLE(info, c)  (info)[(c) >> 5] |= (1L << ((c) & 0x1f))

NS_IMETHODIMP nsUnicodeToHZ::FillInfo(PRUint32 *aInfo)
{
	PRUint16 i,j, k;
    PRUnichar SrcUnicode;

    // aInfo should already been initialized as 2048 element array of 32 bit by caller
    NS_ASSERTION( sizeof(aInfo[0]) == 4, "aInfo size incorrect" );

    // valid GBK rows are in 0x81 to 0xFE
    for (i=0x81; i<0xFE; i++) 
      {
        // HZ and GB2312 starts at row 0x21|0x80 = 0xA1
        if ( i <  0xA1 )
          continue;

        // valid GBK columns are in 0x41 to 0xFE
        for( j=0x41; j<0xFE; j++)
          {
            //HZ and GB2312 starts at col 0x21 | 0x80 = 0xA1
            if ( j < 0xA1 )
              continue;
 
            // k is index in GBKU.H table
            k = (i - 0x81)*(0xFE - 0x80)+(j-0x41);

            SrcUnicode = GBKToUnicodeTable[i];
            if (( SrcUnicode != 0xFFFF ) && (SrcUnicode != 0xFFFD) )
              {
                SET_REPRESENTABLE(aInfo, SrcUnicode);
              }            
            
          }
      }                   
    return NS_OK;
}

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToHZ::GetMaxLength(const PRUnichar * aSrc, 
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  *aDestLength = 6 * aSrcLength;
  return NS_OK;
}
