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
  g_utGB2312Mapping
};

static uRange g_GB2312Ranges[] = {
  { 0x00, 0x7E },
  { 0xA1, 0xFE }
};


//----------------------------------------------------------------------
// Class nsGB2312ToUnicodeV2 [implementation]

nsGB2312ToUnicodeV2::nsGB2312ToUnicodeV2() 
: nsMultiTableDecoderSupport(2, 
                        (uRange *) &g_GB2312Ranges,
                        (uShiftTable**) &g_GB2312ShiftTableSet, 
                        (uMappingTable**) &g_GB2312MappingTableSet)
{
}

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
  // we are a single byte to Unicode converter, so...
  *aDestLength = aSrcLength;
  return NS_OK_UDEC_EXACTLENGTH;
}




//Overwriting the ConvertNoBuff() in nsUCvCnSupport.cpp.
//side effects: all the helper functions called by UCvCnSupport are deprecated

void GBToUnicode(DByte *pGBCode, PRUnichar * pUnicode)
{
	short int iGBToUnicodeIndex;

    if(pGBCode)	
	iGBToUnicodeIndex = ( (short int)(pGBCode->leftbyte & 0x7f) - 0x21)*(0x7e - 0x20 )+( (short int)(pGBCode->rightbyte &0x7f ) - 0x21);

	if( (iGBToUnicodeIndex >= 0 ) && ( iGBToUnicodeIndex < MAX_GB_LENGTH) )
	*pUnicode = GBToUnicodeTable[iGBToUnicodeIndex];

// e.g. 0xb0a1 --> Index = 0 --> left = 0x00, right = 0x30, value = 0x3000
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
			GBToUnicode(pSrcDBCode, pDestDBCode);
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

