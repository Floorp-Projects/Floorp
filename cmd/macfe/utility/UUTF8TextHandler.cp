/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

// ===========================================================================
//	UUTF8TextHandler.cp
// ===========================================================================
//
//	Authror: Frank Tang ftang@netscape.com
#include "UUTF8TextHandler.h"
#include "UUnicodeTextHandler.h"

UUTF8TextHandler* UUTF8TextHandler::fTheOnlyInstance = NULL;
UUTF8TextHandler* UUTF8TextHandler::Instance()
{
	if(fTheOnlyInstance == NULL)
		fTheOnlyInstance = new UUTF8TextHandler();
	return fTheOnlyInstance;
}

#define MXS(c,x,s)			(((c) - (x)) <<  (s))
#define M80S(c,s)			MXS(c,0x80,s)	
#define UTF8_1_to_UCS2(in)	((uint16) (in)[0])
#define UTF8_2_to_UCS2(in)	((uint16) (MXS((in)[0],0xC0, 6) | M80S((in)[1], 0)))
#define UTF8_3_to_UCS2(in)	((uint16) (MXS((in)[0],0xE0,12) | M80S((in)[1], 6) | M80S((in)[2], 0) ))
uint32 UUTF8TextHandler::UTF8ToUCS2(unsigned char* text, int length, INTL_Unicode *ucs2, uint32 ubuflen)
{
	unsigned char* p = text;
	unsigned char* last = text + length;
	uint32 ul;
	for(p = text, ul = 0; (p < last) && (ul < ubuflen) ; )
	{
		if( (*p) < 0x80)
		{	
			ucs2[ul++] = UTF8_1_to_UCS2(p);
			p += 1;
		}
		else if( (*p) < 0xc0 )
		{	
			Assert_( (*p) );	// Invalid UTF8 First Byte
			ucs2[ul++] = '?';
			p += 1;
		}
		else if( (*p) < 0xe0)
		{
			ucs2[ul++] = UTF8_2_to_UCS2(p);
			p += 2;
		}
		else if( (*p) < 0xf0)
		{
			ucs2[ul++] = UTF8_3_to_UCS2(p);
			p += 3;
		}
		else if( (*p) < 0xf8)
		{
			ucs2[ul++] = '?';
			p += 4;
		}
		else if( (*p) < 0xfc)
		{
			ucs2[ul++] = '?';
			p += 5;
		}
		else if( (*p) < 0xfe)
		{
			ucs2[ul++] = '?';
			p += 6;
		}
		else
		{
			Assert_( (*p) );	// Invalid UTF8 First Byte
			ucs2[ul++] = '?';
			p += 1;
		}
	}
	return ul;
}
void UUTF8TextHandler::DrawText(UFontSwitcher* fs, char* text, int len)
{
	if(text[0] != 0)
	{
		uint32 ucs2len;
		INTL_Unicode ucs2buf[512];
		ucs2len = UTF8ToUCS2((unsigned char*)text, len, ucs2buf, 512);
		// redirect to the UUnicodeTextHandler
		UUnicodeTextHandler::Instance()->DrawUnicode(fs, ucs2buf, ucs2len);
	}
}
short UUTF8TextHandler::TextWidth(UFontSwitcher* fs, char* text, int len)
{
	if(text[0] != 0)
	{
		uint32 ucs2len;
		INTL_Unicode ucs2buf[512];
		// redirect to the UUnicodeTextHandler
		ucs2len  = UTF8ToUCS2((unsigned char*)text, len, ucs2buf, 512 );
		return UUnicodeTextHandler::Instance()->UnicodeWidth(fs, ucs2buf, ucs2len);
	}
	return 0;
}
void UUTF8TextHandler::GetFontInfo	(UFontSwitcher* fs, FontInfo* fi)
{
	// redirect to the UUnicodeTextHandler
	UUnicodeTextHandler::Instance()->GetFontInfo(fs, fi);
}
