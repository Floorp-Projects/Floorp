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
//	UUnicodeTextHandler.cp
// ===========================================================================
//
//	Authror: Frank Tang ftang@netscape.com

/*-----------------------------------------------------------------------------
	UUnicodeTextHandler
	Switch font to draw Unicode (UCS2)
	It use Singleton (See Design Patterns by Erich Gamma )
-----------------------------------------------------------------------------*/
#include "UUnicodeTextHandler.h"

UUnicodeTextHandler* UUnicodeTextHandler::fTheOnlyInstance = NULL;
UUnicodeTextHandler* UUnicodeTextHandler::Instance()
{
	if(fTheOnlyInstance == NULL)
		fTheOnlyInstance = new UUnicodeTextHandler();
	return fTheOnlyInstance;
}
void UUnicodeTextHandler::DrawUnicode(UFontSwitcher* fs, INTL_Unicode* unicode, int len)
{
	INTL_CompoundStr* cs = INTL_CompoundStrFromUnicode(unicode, len);
	if(cs)
	{
		INTL_Encoding_ID encoding;
		unsigned char* outtext;
		INTL_CompoundStrIterator iter;
		for(iter = INTL_CompoundStrFirstStr((INTL_CompoundStrIterator)cs, &encoding , &outtext);
				iter != NULL;
					iter = INTL_CompoundStrNextStr(iter, &encoding, &outtext))
		{
			if((outtext) && (*outtext))
			{
				fs->EncodingTextFont(encoding);
				::DrawText(outtext, 0, XP_STRLEN((char*)outtext));
			}
		}
		INTL_CompoundStrDestroy(cs);
	}
}
short UUnicodeTextHandler::UnicodeWidth(UFontSwitcher* fs, INTL_Unicode* unicode, int len)
{
	short width = 0;
	INTL_CompoundStr* cs = INTL_CompoundStrFromUnicode(unicode, len);
	if(cs)
	{
		INTL_Encoding_ID encoding;
		unsigned char* outtext;
		INTL_CompoundStrIterator iter;
		for(iter = INTL_CompoundStrFirstStr((INTL_CompoundStrIterator)cs, &encoding , &outtext);
				iter != NULL;
					iter = INTL_CompoundStrNextStr(iter, &encoding, &outtext))
		{
			if((outtext) && (*outtext))
			{
				fs->EncodingTextFont(encoding);
				width += ::TextWidth(outtext, 0, XP_STRLEN((char*)outtext));
			}
		}
		INTL_CompoundStrDestroy(cs);
	}
	return width;
}
void UUnicodeTextHandler::GetFontInfo	(UFontSwitcher* fs, FontInfo* fi)
{
	FontInfo cur;
	int16 *list;
	int16 num;
	list = INTL_GetUnicodeCSIDList(&num);
	fi->ascent =	fi->descent =	fi->widMax =	fi->leading = 0;
	for(int i = 0; i < num; i++)
	{
		fs->EncodingTextFont(list[i]);
		::GetFontInfo(&cur);
		if(fi->ascent  < cur.ascent)	fi->ascent  = cur.ascent;
		if(fi->descent < cur.descent)	fi->descent = cur.descent;
		if(fi->widMax  < cur.widMax)	fi->widMax  = cur.widMax;
		if(fi->leading < cur.leading)	fi->leading = cur.leading;
	}
}
