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

//	UMenuUtils.h

#pragma once

#include <PP_Prefix.h>

#ifndef __MENUS__
#include <Menus.h>
#endif

class cstring;

class UMenuUtils
{
	public:
		
		static void		PurgeMenuItems(MenuHandle inMenu, Int16 inAfter = 0);

		static Int16	InsertMenuItem(
							MenuHandle 		inMenu,
							ConstStringPtr	inText,
							Int16 			inAfter,
							Boolean			inEnabled = true);
							
		static Int16	AppendMenuItem(
							MenuHandle		inMenu,
							ConstStringPtr	inText,
							Boolean			inEnabled = true);
							
		static void		AppendSeparator(MenuHandle inMenu);

		static Int32 	PopupWithTraits(
							MenuHandle		inMenu,
							Point 			inGlobalWhere,
							Int16 			inTopItem = 1,
							ResIDT 			inTraitsID = 0);

		static void		AdjustStringForMenuTitle(cstring& inString);

		static void		ConvertToIconMenu(MenuHandle inMenu, ResIDT inIconResID);
	
		static UInt16 	GetMenuBarWidth();
		
	protected:
		static Uint8 sEnabledDummy[];
		static Uint8 sDisabledDummy[];
		static Uint8 sDisabledSep[];
};

