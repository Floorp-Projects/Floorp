/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
 /**
 * A character set converter from Unicode to HZ.
 * 
 *
 * @created         08/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 */

#include "nsUnicodeToHZ.h"
#include "nsUCvCnDll.h"


#define _GBKU_TABLE_		// to use a shared GBKU table
#include "gbku.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

//----------------------------------------------------------------------
// Class nsUnicodeToGBK [implementation]


#define TRUE 1
#define FALSE 0

void  nsUnicodeToHZ::UnicodeToGBK(PRUnichar SrcUnicode, DByte *pGBCode)
{
	short int iRet = FALSE;
	short int i = 0;
	short int iGBKToUnicodeIndex = 0;


	for ( i=0; i<MAX_GBK_LENGTH; i++)
	{
		if ( SrcUnicode == GBKToUnicodeTable[i] )
		{
			iGBKToUnicodeIndex = i;
			iRet = TRUE;
			break;
		}
	}

	if ( iRet )
	{
		//convert from one dimensional index to (left, right) pair
		if(pGBCode)
		{
		pGBCode->leftbyte =  (char) ( iGBKToUnicodeIndex / 0x00BF + 0x0081)  ;
		pGBCode->leftbyte |= 0x80;
		pGBCode->rightbyte = (char) ( iGBKToUnicodeIndex % 0x00BF+ 0x0040);
		pGBCode->rightbyte |= 0x80;
		}
	}


}


#define TRUE 1
#define FALSE 0

void nsUnicodeToHZ::UnicodeToHZ(PRUnichar SrcUnicode, DByte *pGBCode)
{
	short int iRet = FALSE;
	short int i = 0;
	short int iGBKToUnicodeIndex = 0;


	for ( i=0; i<MAX_GBK_LENGTH; i++)
	{
		if ( SrcUnicode == GBKToUnicodeTable[i] )
		{
			iGBKToUnicodeIndex = i;
			iRet = TRUE;
			break;
		}
	}

	if ( iRet )
	{
		//convert from one dimensional index to (left, right) pair
		if(pGBCode)
		{
		pGBCode->leftbyte =  (char) ( iGBKToUnicodeIndex / 0x00BF + 0x0081);
		pGBCode->rightbyte = (char) ( iGBKToUnicodeIndex % 0x00BF+ 0x0040);
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

	static	int hz_state = HZ_STATE_ASCII;	// per HZ spec, default to HZ mode
	PRInt32 i=0;
	PRInt32 iSrcLength = *aSrcLength;
    DByte *pDestDBCode;
    //    DByte *pSrcDBCode; 
	PRInt32 iDestLength = 0;

	PRUnichar *pSrc = (PRUnichar *)aSrc;

	pDestDBCode = (DByte *)aDest;

    for (i=0;i< iSrcLength;i++)
	{
		pDestDBCode = (DByte *)aDest;

		if( (*pSrc) & 0xff00 )
		{
		// hi byte has something, it is not ASCII, must be a GB

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
	short int i,j;
    PRUnichar SrcUnicode;
    PRUint16 k;


    for ( k=0; k++;k<65536)
    {
        aInfo[k] = 0x0000;
    }

    // valid GBK rows are in 0x81 to 0xFE
    for ( i=0x81;i++;i<0xFE) 
    {
      // HZ and GB2312 starts at row 0x21|0x80
      if ( i < ( 0x21 | 0x80))
      continue;

      // valid GBK columns are in 0x41 to 0xFE
      for( j=0x41; j++; j<0xFE)
      {
        //HZ and GB2312 starts at col 0x21 | 0x80
        if ( j < (0x21 | 0x80))
        continue;
 
        // k is index in GBKU.H table
         k = (i - 0x81)*(0xFE - 0x80)+(j-0x41);

         // sanity check
         if ( (k>=0) && ( k < MAX_GBK_LENGTH))
         {	
        	SrcUnicode = GBKToUnicodeTable[i];
            if (( SrcUnicode != 0xFFFF ) && (SrcUnicode != 0xFFFD) )
		    {
			    SET_REPRESENTABLE(aInfo, SrcUnicode);
		    }            
         }   
      }
   }                   



   return NS_OK;
}

nsresult nsUnicodeToHZ::CreateInstance(nsISupports ** aResult) 
{
  nsIUnicodeEncoder *p = new nsUnicodeToHZ();
  if(p) {
   *aResult = p;
   return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
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
