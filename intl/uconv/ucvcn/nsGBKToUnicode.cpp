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
// Class nsGB2312ToUnicode [implementation]


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



//Overwriting the ConvertNoBuff() in nsUCvCnSupport.cpp.
//side effects: all the helper functions called by UCvCnSupport are deprecated

void nsGBKToUnicode::GBKToUnicode(DByte *pGBCode, PRUnichar * pUnicode)
{
	short int iGBKToUnicodeIndex;

    if(pGBCode)	
	iGBKToUnicodeIndex = ( (short int)(pGBCode->leftbyte) - 0x81)*0xbf +( (short int)(pGBCode->rightbyte) - 0x40);

	if( (iGBKToUnicodeIndex >= 0 ) && ( iGBKToUnicodeIndex < MAX_GBK_LENGTH) )
	*pUnicode = GBKToUnicodeTable[iGBKToUnicodeIndex];

}



NS_IMETHODIMP nsGBKToUnicode::ConvertNoBuff(const char* aSrc,
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


