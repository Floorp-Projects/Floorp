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
//	UMultiFontTextHandler.h
// ===========================================================================
//
//	Authror: Frank Tang ftang@netscape.com
/*-----------------------------------------------------------------------------
	UMultiFontTextHandler
	Abstract class for the concrete class which draw/measre text base one multile font
-----------------------------------------------------------------------------*/
#pragma once
#include "UFontSwitcher.h"

class UMultiFontTextHandler {
public:
	UMultiFontTextHandler()	{};
	virtual void 	DrawString	(UFontSwitcher* fs, Str255 str) = 0;
	virtual short 	StringWidth	(UFontSwitcher* fs, Str255 str) = 0;
	virtual void 	DrawText	(UFontSwitcher* fs, char* text, int len) = 0;
	virtual short 	TextWidth	(UFontSwitcher* fs, char* text, int len) = 0;
	virtual void 	GetFontInfo	(UFontSwitcher* fs, FontInfo* fi) = 0;
		
};
