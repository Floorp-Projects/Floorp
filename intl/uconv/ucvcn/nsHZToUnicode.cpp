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
 * @created         08/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 *
 * Note: in this HZ-GB-2312 converter, we accept a string composed of 7-bit HZ 
 *       encoded Chinese chars,as it is defined in RFC1843 available at 
 *       http://www.cis.ohio-state.edu/htbin/rfc/rfc1843.html
 *       and RFC1842 available at http://www.cis.ohio-state.edu/htbin/rfc/rfc1842.html.
 *        
 *       In an effort to match the similar extended capability of Microsoft Internet Explorer
 *       5.0. We also accept the 8-bit GB encoded chars mixed in a HZ string.
 *       But this should not be a recommendedd practice for HTML authors.
 *
 *       The priority of converting are as follows: first convert 8-bit GB code; then,
 *       consume HZ ESC sequences such as '~{', '~}', '~~'; then, depending on the current
 *       state ( default to ASCII state ) of the string, each 7-bit char is converted as an 
 *       ASCII, or two 7-bit chars are converted into a Chinese character.
 */



#include "nsUCvCnDll.h"

#include "nsHZToUnicode.h"


#define _GBKU_TABLE_		// to use a shared GBKU table
#include "gbku.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

//----------------------------------------------------------------------
// Class nsHZToUnicode [implementation]


nsresult nsHZToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsHZToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Subclassing of nsTablesDecoderSupport class [implementation]

NS_IMETHODIMP nsHZToUnicode::GetMaxLength(const char * aSrc, 
                                              PRInt32 aSrcLength, 
                                              PRInt32 * aDestLength)
{
  // we are a single byte to Unicode converter, so...
  *aDestLength = aSrcLength;
  return NS_OK_UDEC_EXACTLENGTH;
}
 


//convert the 7-bit HZ into an 8-bit GB index of the GBK table and get its unicode
void nsHZToUnicode::HZToUnicode(DByte *pGBCode, PRUnichar * pUnicode)
{

    //we are re-using the GBK's GB to Unicode mapping table.
	PRInt16  iGBKToUnicodeIndex;

    if(pGBCode)	
	iGBKToUnicodeIndex = ( (short int)(pGBCode->leftbyte) | 0x80- 0x81)*0xbf +( (short int)(pGBCode->rightbyte) | 0x80 - 0x40);

	if( (iGBKToUnicodeIndex >= 0 ) && ( iGBKToUnicodeIndex < MAX_GBK_LENGTH) )
	{
		*pUnicode = GBKToUnicodeTable[iGBKToUnicodeIndex];
	}
	else
		*pUnicode = GB_UNDEFINED;
}

//convert the 8-bit GB index to its unicode by GBK table
void nsHZToUnicode::GBToUnicode(DByte *pGBCode, PRUnichar * pUnicode)
{

    //we are re-using the GBK's GB to Unicode mapping table.
	PRInt16  iGBKToUnicodeIndex;

    if(pGBCode)	
	iGBKToUnicodeIndex = ( (short int)pGBCode->leftbyte - 0x81)*0xbf +( (short int)pGBCode->rightbyte  - 0x40);

	if( (iGBKToUnicodeIndex >= 0 ) && ( iGBKToUnicodeIndex < MAX_GBK_LENGTH) )
	{
		*pUnicode = GBKToUnicodeTable[iGBKToUnicodeIndex];
	}
	else
		*pUnicode = GB_UNDEFINED;
}


//Overwriting the ConvertNoBuff() in nsUCvCnSupport.cpp.
NS_IMETHODIMP nsHZToUnicode::ConvertNoBuff(const char* aSrc,
											   PRInt32 * aSrcLength,
											   PRUnichar *aDest,
											   PRInt32 * aDestLength)
{

#define HZ_STATE_GB		1
#define HZ_STATE_ASCII	2
#define HZ_STATE_TILD	3
#define HZLEAD1 '~'
#define HZLEAD2 '{'
#define HZLEAD3 '}'

	static PRInt16 hz_state = HZ_STATE_ASCII;	// per HZ spec, default to ASCII state 
	short int i=0;
	short int iSrcLength = (short int)(*aSrcLength);
	DByte *pSrcDBCode = (DByte *)aSrc;
    PRUnichar *pDestDBCode = (PRUnichar *)aDest;
	int iDestlen = 0;
	char ch1, ch2;
	nsresult res = NS_OK;

    for (i=0;i<iSrcLength;i++)
	{
		pSrcDBCode = (DByte *)aSrc;
		pDestDBCode = aDest;

		if ( iDestlen >= (*aDestLength) )
		{
		    res = NS_OK_UDEC_MOREOUTPUT;
			break;
		}

        if ( *aSrc & 0x80 ) // if it is a 8-bit byte
		{
			// The source is a 8-bit GBCode
			GBToUnicode(pSrcDBCode, pDestDBCode);
			aSrc += 2;
			i++;
            iDestlen++;
            aDest++;
            *aSrcLength = i+1;
            continue;
		}
  
        // otherwise, it is a 7-bit byte 
        // The source will be an ASCII or a 7-bit HZ code depending on ch1
 
        ch1 = *aSrc;
        ch2	= *(aSrc+1);
        
        if (ch1 == HZLEAD1 )  // if it is lead by '~'
        {
          switch (ch2)
            {
            case HZLEAD2:	// we got a '~{'
              // we are switching to HZ state
              hz_state = HZ_STATE_GB;
              aSrc += 2;
              i++;
              break;
            case HZLEAD3:	// we got a '~}'
              // we are switching to ASCII state
              hz_state = HZ_STATE_ASCII;
              aSrc += 2;
              i++;
              break;
            case HZLEAD1:	// we got a '~~', process like an ASCII, but no state change
              aSrc++;
              *pDestDBCode = (PRUnichar) ( ((char)(*aSrc) )& 0x00ff);
              aSrc++;
              iDestlen++;
              aDest++;
              break;
            default:
              // undefined ESC sequence '~X' are ignored since this is a illegal combination 
              aSrc += 2;
              break;
            };

          continue;// go for next loop
        }

        // ch1 != '~'
        switch (hz_state)
          {
            case HZ_STATE_GB:
              // the following chars are HZ
              HZToUnicode(pSrcDBCode, pDestDBCode);
              aSrc += 2;
              i++;
              iDestlen++;
              aDest++;
              break;
            case HZ_STATE_ASCII:
            default:
              // default behavior also like an ASCII
              // when the source is an ASCII
              *pDestDBCode = (PRUnichar) ( ((char)(*aSrc) )& 0x00ff);
              aSrc++;
              iDestlen++;
              aDest++;
              break;
          }

          *aSrcLength = i+1;

    }// for loop

        *aDestLength = iDestlen;
	
        return NS_OK;
}


