/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsUnicodeToGB2312V2.h"
#include "nsUCvCnDll.h"


#define _GBU_TABLE_		// make the table in the include file to be extern
#include "gbu.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]


static PRInt16 g_ASCIIShiftTable[] =  {
  0, u1ByteCharset,
  ShiftCell(0,0,0,0,0,0,0,0)
};

static PRInt16 g_GB2312ShiftTable[] =  {
  0, u2BytesGRCharset,
  ShiftCell(0,0,0,0,0,0,0,0)
};

static PRInt16 *g_GB2312ShiftTableSet [] = {
  g_ASCIIShiftTable,
  g_GB2312ShiftTable
};

static PRUint16 *g_GB2312MappingTableSet [] ={
  g_AsciiMapping,
  g_ufGB2312Mapping
};
//----------------------------------------------------------------------
// Class nsUnicodeToGB2312V2 [implementation]

nsUnicodeToGB2312V2::nsUnicodeToGB2312V2() 
: nsMultiTableEncoderSupport(2,
                        (uShiftTable**) &g_GB2312ShiftTableSet, 
                        (uMappingTable**) &g_GB2312MappingTableSet)
{
}


#define TRUE 1
#define FALSE 0

void UnicodeToGB(PRUnichar SrcUnicode, DByte *pGBCode)
{
	short int iRet = FALSE;
	short int i = 0;
	short int iGBToUnicodeIndex = 0;


	for ( i=0; i<MAX_GB_LENGTH; i++)
	{
		if ( SrcUnicode == GBToUnicodeTable[i] )
		{
			iGBToUnicodeIndex = i;
			iRet = TRUE;
			break;
		}
	}

	if ( iRet )
	{
		//convert from one dimensional index to (left, right) pair
		if(pGBCode)
		{
		pGBCode->leftbyte =  (char) ( iGBToUnicodeIndex / (0x007e - 0x0020) + 0x0021)  ;
		pGBCode->leftbyte |= 0x80;
		pGBCode->rightbyte = (char) ( iGBToUnicodeIndex % (0x007e - 0x0020)+ 0x0021);
		pGBCode->rightbyte |= 0x80;
		}
	}


}


NS_IMETHODIMP nsUnicodeToGB2312V2::ConvertNoBuff(const PRUnichar * aSrc, 
										PRInt32 * aSrcLength, 
										char * aDest, 
										PRInt32 * aDestLength)
{
	PRInt32 i=0;
	PRInt32 iSrcLength = *aSrcLength;
    DByte *pDestDBCode;
    DByte *pSrcDBCode; 
	PRInt32 iDestLength = 0;

	PRUnichar *pSrc = (PRUnichar *)aSrc;

	pDestDBCode = (DByte *)aDest;

    for (i=0;i< iSrcLength;i++)
	{
		pDestDBCode = (DByte *)aDest;

		if( (*pSrc) & 0xff00 )
		{
		// hi byte has something, it is not ASCII, must be a GB

			UnicodeToGB( *pSrc, pDestDBCode);
			aDest += 2;	// increment 2 bytes
			pDestDBCode = (DByte *)aDest;
			iDestLength +=2;
		}
		else
		{
		// this is an ASCII
            pSrcDBCode = (DByte *)pSrc;
			*aDest = pSrcDBCode->leftbyte;
			aDest++; // increment 1 byte
			iDestLength +=1;
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


nsresult nsUnicodeToGB2312V2::CreateInstance(nsISupports ** aResult) 
{
  nsIUnicodeEncoder *p = new nsUnicodeToGB2312V2();
  if(p) {
   *aResult = p;
   return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
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
