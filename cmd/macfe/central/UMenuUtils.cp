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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	UMenuUtils.cp
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include <UTextTraits.h>
#include <LString.h>

#include <string.h>

#include "UMenuUtils.h"
#include "cstring.h"

#include "net.h" // for NET_UnEscape

#ifndef __WINDOWS__
#include <WIndows.h>
#endif

#ifndef __LOWMEM__
#include <LowMem.h>
#endif

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Uint8 UMenuUtils::sEnabledDummy[] = "\px";
Uint8 UMenuUtils::sDisabledDummy[] = "\p(x";
Uint8 UMenuUtils::sDisabledSep[] = "\p(-";

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UMenuUtils::PurgeMenuItems(MenuHandle inMenu, Int16 inAfter)
{
	Int16 theItemCount = ::CountMItems(inMenu) - inAfter;
	while (theItemCount > 0)
	{
		::DeleteMenuItem(inMenu, inAfter + 1);
		theItemCount--;
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Int16 UMenuUtils::InsertMenuItem(
	MenuHandle 		inMenu,
	ConstStringPtr	inText,
	Int16 			inAfter,
	Boolean			inEnabled)
{
	Int16 theItemIndex;
	Int16 theItemCount;
	
	if (inAfter == 0)
		theItemIndex = 1;
	else
		{
		theItemCount = ::CountMItems(inMenu);
		if (inAfter >= theItemCount)
			theItemIndex = theItemCount + 1;
		else
			theItemIndex = inAfter + 1;
		}

	if (inEnabled)
		::InsertMenuItem(inMenu, sEnabledDummy, inAfter);
	else
		::InsertMenuItem(inMenu, sDisabledDummy, inAfter);
	
	::SetMenuItemText(inMenu, theItemIndex, inText);
	
	return theItemIndex;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Int16 UMenuUtils::AppendMenuItem(
	MenuHandle		inMenu,
	ConstStringPtr	inText,
	Boolean			inEnabled)
{
	Int16 theItemIndex = ::CountMItems(inMenu) + 1;
	if (inEnabled)
		::AppendMenu(inMenu, sEnabledDummy);
	else
		::AppendMenu(inMenu, sDisabledDummy);
		
	::SetMenuItemText(inMenu, theItemIndex, inText);
	
	return theItemIndex;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UMenuUtils::AppendSeparator(MenuHandle inMenu)
{
	::AppendMenu(inMenu, sDisabledSep);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Int32 UMenuUtils::PopupWithTraits(
	MenuHandle 	inMenu,
	Point		inGlobalWhere,
	Int16 		inTopItem,
	ResIDT		inTraitsID)
{
	TextTraitsH theTraits = UTextTraits::LoadTextTraits(inTraitsID);
	
	CGrafPtr theWindowMgrPort;
	::GetCWMgrPort(&theWindowMgrPort);
	
	Int16 saveFontFam = LMGetSysFontFam();
	Int16 saveFont = theWindowMgrPort->txFont;
	Int16 saveSize = theWindowMgrPort->txSize;
	Int16 saveFace = theWindowMgrPort->txFace;
	
	theWindowMgrPort->txFont = (*theTraits)->fontNumber;
	theWindowMgrPort->txSize = (*theTraits)->size;
	theWindowMgrPort->txFace = (*theTraits)->style;
	LMSetSysFontFam((*theTraits)->fontNumber);

	Int32 theResult = ::PopUpMenuSelect(inMenu, inGlobalWhere.v, inGlobalWhere.h, inTopItem);
	
	theWindowMgrPort->txFont = saveFont;
	theWindowMgrPort->txSize = saveSize;
	theWindowMgrPort->txFace = saveFace;
	LMSetSysFontFam(saveFontFam);

	return theResult;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	This is a copy of CreateMenuString that takes a char*

void UMenuUtils::AdjustStringForMenuTitle(cstring& inString)
{
	// NET_UnEscape returns a char* which, for now, is
	// just the string passed in
	NET_UnEscape(inString);
	if ( LString::CStringLength(inString) > 50 )	// Cut it down to reasonable size
	{
		inString[50] = '\0';	// truncate inString
		inString += "...";
	}
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ=
UInt16 UMenuUtils::GetMenuBarWidth()
{
	GDHandle	menuBarDevice;
	Rect		screenRect;

	menuBarDevice = GetMainDevice();
	screenRect = (**menuBarDevice).gdRect;
	
	return screenRect.right - screenRect.left;
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void UMenuUtils::ConvertToIconMenu(
	MenuHandle	inMenu,
	ResIDT 		inIconResID)
{
	Handle iconSuite;
	
	if (GetMenuBarWidth() >= 640) return;
	
	if (GetIconSuite(&iconSuite, inIconResID, kSelectorAllSmallData) == noErr)
	{
		//	Set the menu title to exactly 5 characters in length.
		SetMenuItemText(inMenu, 0, "\p12345");
		
		char *menuData = *(char **)inMenu + sizeof(MenuInfo) - sizeof(Str255);
		
		//	Set the first byte of the title equal to 1 to indicate it's "iconized."
		*(menuData + 1) = 1;
		
		//	Store the icon suite handle in the remaining 4 bytes of the title.
		*(Handle *)(menuData + 2) = iconSuite;
	}
}
