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
//	CDefaultFontFontSwitcher.cp
// ===========================================================================
//
//	Authror: Frank Tang ftang@netscape.com
#include "CDefaultFontFontSwitcher.h"
#include "uintl.h"

INTL_Encoding_ID CDefaultFontFontSwitcher::gEncodingOfDefaultFont = 0;

CDefaultFontFontSwitcher::CDefaultFontFontSwitcher(UFontSwitcher* delegate)
{
	GrafPtr port;
	::GetPort(&port);
	mDefaultFont = port->txFont;
	if(0 == gEncodingOfDefaultFont)
	{
		gEncodingOfDefaultFont = ScriptToEncoding(::GetScriptManagerVariable( smSysScript ) );
	}
	mDelegate = delegate;
	
}
void CDefaultFontFontSwitcher::EncodingTextFont(INTL_Encoding_ID encoding)
{
	if(gEncodingOfDefaultFont == encoding)
			::TextFont(mDefaultFont);
	else
		mDelegate->EncodingTextFont(encoding);
}