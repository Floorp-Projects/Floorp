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

#pragma once
#include "UMultiFontTextHandler.h"
/*-----------------------------------------------------------------------------
	UUnicodeTextHandler
	Switch font to draw Unicode (UCS2)
	It use Singleton (See Design Patterns by Erich Gamma )
-----------------------------------------------------------------------------*/

class UUnicodeTextHandler : public UMultiFontTextHandler{
public:
	static UUnicodeTextHandler*	Instance();
	UUnicodeTextHandler()	{};
	virtual void DrawUnicode(UFontSwitcher* fs, INTL_Unicode* text, int len);
	virtual short UnicodeWidth(UFontSwitcher* fs, INTL_Unicode* text, int len);

	virtual void DrawString	(UFontSwitcher* fs,  Str255 str) 
		{ 	DrawUnicode(fs, (INTL_Unicode*)str+2, ((str[0] - 1 )/ 2)); };
		
	virtual void DrawText	(UFontSwitcher* fs,  char* text, int len)
		{	DrawUnicode(fs, (INTL_Unicode*) text, (len / 2));	};
		
	virtual short StringWidth(UFontSwitcher* fs, Str255 str) 
		{ return UnicodeWidth(fs, (INTL_Unicode*)str+2, ((str[0] - 1 )/ 2)); };
		
	virtual short TextWidth(UFontSwitcher* fs, char* text, int len)
		{	return UnicodeWidth(fs, (INTL_Unicode*) text, (len / 2));	};	
	virtual void 	GetFontInfo	(UFontSwitcher* fs, FontInfo* fi);

private:
	static UUnicodeTextHandler*	fTheOnlyInstance;			
};
