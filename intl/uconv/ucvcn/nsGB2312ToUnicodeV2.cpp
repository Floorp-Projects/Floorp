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


#include "nsGB2312ToUnicodeV2.h"
#include "nsUCvCnDll.h"


#define _GBKU_TABLE_		// to use a shared GBKU table
#include "gbku.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]



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




//Overwriting the ConvertNoBuff() in nsUCvCnSupport.cpp.

void nsGB2312ToUnicodeV2::GBKToUnicode(DByte *pGBCode, PRUnichar * pUnicode)
{
	short int iGBKToUnicodeIndex;

    if(pGBCode)	
	iGBKToUnicodeIndex = ( (short int)(pGBCode->leftbyte) - 0x81)*0xbf +( (short int)(pGBCode->rightbyte) - 0x40);

	if( (iGBKToUnicodeIndex >= 0 ) && ( iGBKToUnicodeIndex < MAX_GBK_LENGTH) )
	*pUnicode = GBKToUnicodeTable[iGBKToUnicodeIndex];

}





NS_IMETHODIMP nsGB2312ToUnicodeV2::ConvertNoBuff(const char* aSrc,
											   PRInt32 * aSrcLength,
											   PRUnichar *aDest,
											   PRInt32 * aDestLength)
{

	short int i=0;
	short int iSrcLength = (short int)(*aSrcLength);
	DByte *pSrcDBCode = (DByte *)aSrc;
    PRUnichar *pDestDBCode = (PRUnichar *)aDest;
	int iDestlen = 0;


    for (i=0;i<iSrcLength;i++)
	{
		pSrcDBCode = (DByte *)aSrc;
		pDestDBCode = aDest;

		if ( iDestlen >= (*aDestLength) )
		{
			break;
		}

		if ( *aSrc & 0x80 )
		{
			// The source is a GBCode
			GBKToUnicode(pSrcDBCode, pDestDBCode);
			aSrc += 2;
			i++;
		}
		else
		{
			// The source is an ASCII
		    *pDestDBCode = (PRUnichar) ( ((char)(*aSrc) )& 0x00ff);
			aSrc++;
		}

   	    iDestlen++;
	    aDest++;
		*aSrcLength = i+1;
	}

    *aDestLength = iDestlen;
	
	return NS_OK;
}

