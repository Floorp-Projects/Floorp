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
//	CGuidePopupMenu.cp
// ===========================================================================

#include "CGuidePopupMenu.h"
		
#include <algorithm>
		
#include "prefapi.h"		
#include "CApplicationEventAttachment.h"
#include "uapp.h"
#include "CAutoPtrXP.h"

// ---------------------------------------------------------------------------
//		¥ [static member data]
// ---------------------------------------------------------------------------

CAutoPtr<LMenu>	CGuidePopupMenu::sMenu;
Boolean			CGuidePopupMenu::sOwnsMenu = true;
Boolean			CGuidePopupMenu::sMenuIsSetup = false;
vector<string>	CGuidePopupMenu::sURLs;		

// ---------------------------------------------------------------------------
//		¥ CGuidePopupMenu
// ---------------------------------------------------------------------------

CGuidePopupMenu::CGuidePopupMenu(
	LStream* inStream)

	:	super(inStream)
{
}

// ---------------------------------------------------------------------------
//		¥ ~CGuidePopupMenu
// ---------------------------------------------------------------------------

CGuidePopupMenu::~CGuidePopupMenu()
{
}
	
#pragma mark -
	
// ---------------------------------------------------------------------------
//		¥ FinishCreateSelf
// ---------------------------------------------------------------------------
						
void
CGuidePopupMenu::FinishCreateSelf()
{
	super::FinishCreateSelf();
	
	SetupMenu();
}
	
#pragma mark -
	
// ---------------------------------------------------------------------------
//		¥ SetupMenu
// ---------------------------------------------------------------------------

void
CGuidePopupMenu::SetupMenu()
{
	ThrowIfNil_(GetMenu());
	ThrowIfNil_(GetMenu()->GetMacMenuH());
	
	if (!sMenuIsSetup)
	{
		// Populate the URL vector with the toolbar places items using
		// the javascript preferences mechanism
		
		for (int i = 0, rc = PREF_NOERROR; rc == PREF_NOERROR; i++)
		{
			CAutoPtrXP<char>	label;
			CAutoPtrXP<char>	url;
			char*				tempLabel = 0;
			char*				tempURL = 0;		
			
			rc = PREF_CopyIndexConfigString("toolbar.places.item", i, "label", &tempLabel);
			label.reset(tempLabel);
			
			if (rc == PREF_NOERROR)
			{
				PREF_CopyIndexConfigString("toolbar.places.item", i, "url", &tempURL);
				url.reset(tempURL);
				
				CtoPstr(label.get());
				
				// Insert a "blank" item first...
				
				GetMenu()->InsertCommand("\p ", cmd_Nothing, i);
				
				// Then change it. We do this so that no interpretation of metacharacters will occur.
				
				::SetMenuItemText(GetMenu()->GetMacMenuH(), i + 1, (StringPtr)label.get());

				sURLs.push_back(url.get());							
			}
		}
		
		sMenuIsSetup = true;
	}
}
	
#pragma mark -
						
// ---------------------------------------------------------------------------
//		¥ GetMenu
// ---------------------------------------------------------------------------
	
LMenu*
CGuidePopupMenu::GetMenu() const
{
	return sMenu.get();
	
}	
	
// ---------------------------------------------------------------------------
//		¥ OwnsMenu
// ---------------------------------------------------------------------------
	
Boolean
CGuidePopupMenu::OwnsMenu() const
{
	return sOwnsMenu;
	
}	
					
// ---------------------------------------------------------------------------
//		¥ SetMenu
// ---------------------------------------------------------------------------
	
void
CGuidePopupMenu::SetMenu(LMenu* inMenu)
{
	ThrowIfNot_(GetMenu() == nil);
	ThrowIfNot_(inMenu == nil);
	
	sMenu.reset(nil);
}

// ---------------------------------------------------------------------------
//		¥ AdoptMenu
// ---------------------------------------------------------------------------

void
CGuidePopupMenu::AdoptMenu(
	LMenu* inMenuToAdopt)
{
	ThrowIfNot_(GetMenu() == nil);

	EliminatePreviousMenu();
	
	sMenu.reset(inMenuToAdopt);
	sOwnsMenu = true;
}

// ---------------------------------------------------------------------------
//		¥ MakeNewMenu
// ---------------------------------------------------------------------------
//	We are caching the menu in a static CAutoPtr. So we guard instantiations
//	and only allow one.

void
CGuidePopupMenu::MakeNewMenu()
{
	if (!GetMenu())
	{
		super::MakeNewMenu();
	}
}

// ---------------------------------------------------------------------------
//		¥ EliminatePreviousMenu
// ---------------------------------------------------------------------------
//	This is intentionally empty. The lifetime of the menu is controlled by
//	the static CAutoPtr.

void
CGuidePopupMenu::EliminatePreviousMenu()
{
}						

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ GetItem
// ---------------------------------------------------------------------------
		
short
CGuidePopupMenu::GetItem(const string& inURL) const
{
	short item = 0;
	
	vector<string>::const_iterator theItem = find(sURLs.begin(), sURLs.end(), inURL.c_str());
	
	if (theItem != sURLs.end())
	{
		item = (theItem - sURLs.begin()) + 1;
	}
	
	return item;
}
	
// ---------------------------------------------------------------------------
//		¥ GetURL
// ---------------------------------------------------------------------------
		
string
CGuidePopupMenu::GetURL(short item) const
{
	ThrowIfNot_(item >= 0 && item <= sURLs.size() - 1);
	
	return sURLs[item];
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ HandleNewValue
// ---------------------------------------------------------------------------

Boolean
CGuidePopupMenu::HandleNewValue(Int32 inNewValue)
{
	if (inNewValue)
	{
		CFrontApp& theApp = dynamic_cast<CFrontApp&>(CApplicationEventAttachment::GetApplication());
		
		theApp.DoGetURL(GetURL(inNewValue - 1).c_str());
	}
	
	return true;
}
