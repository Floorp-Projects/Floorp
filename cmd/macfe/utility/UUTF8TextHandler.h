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
//	UUTF8TextHandler.h
// ===========================================================================
//
//	Authror: Frank Tang ftang@netscape.com
#pragma once
#include "UMultiFontTextHandler.h"
/*-----------------------------------------------------------------------------
	UUTF8TextHandler
	Switch font to draw UTF8
	It use Singleton (See Design Patterns by Erich Gamma )
-----------------------------------------------------------------------------*/
class UUTF8TextHandler : public UMultiFontTextHandler {
public:
	static UUTF8TextHandler*	Instance();
	UUTF8TextHandler()	{};
	virtual void 	DrawText	(UFontSwitcher* fs, char* text, int len);
	virtual short 	TextWidth	(UFontSwitcher* fs, char* text, int len);
	
	virtual void 	DrawString	(UFontSwitcher* fs, Str255 str) 
			{ 		DrawText(fs, (char*)(str + 1), str[0]); };
		
	virtual short 	StringWidth(UFontSwitcher* fs, Str255 str) 
			{ return TextWidth(fs, (char*)(str + 1), str[0]); };
	virtual void 	GetFontInfo	(UFontSwitcher* fs, FontInfo* fi);
		
	uint32 UTF8ToUCS2(unsigned char* text, int length, INTL_Unicode *ucs2, uint32 ubuflen);
private:
	static UUTF8TextHandler*	fTheOnlyInstance;			
};
