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
//	CHistoryMenu.cp
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include "Netscape_Constants.h"

#include "CHistoryMenu.h"
#include "CWindowMediator.h"
#include "CBrowserContext.h"
#include "CBrowserWindow.h"
#include "UMenuUtils.h"
#include "cstring.h"

#include "xp_list.h"

CHistoryMenu* CHistoryMenu::sHistoryMenu = NULL;

CHistoryMenu::CHistoryMenu(ResIDT inMenuID) :
	LMenu(inMenuID),
	mCurrentHistoryEntry(NULL)
{
	mLastNonDynamicItem = ::CountMenuItems(GetMacMenuH());
	sHistoryMenu = this;
}

CHistoryMenu::~CHistoryMenu()
{
	sHistoryMenu = NULL;
}

void CHistoryMenu::Update(void)
{
	CMediatedWindow* topWindow = (CWindowMediator::GetWindowMediator())->FetchTopWindow(WindowType_Any, regularLayerType);
	if (topWindow)
	{
		if (topWindow->GetWindowType() == WindowType_Browser)
		{
			CBrowserWindow* browserWindow = (CBrowserWindow*)topWindow;
			if (browserWindow->GetWindowContext())
			{
				History_entry* currentEntry = (browserWindow->GetWindowContext())->GetCurrentHistoryEntry();
				if (GetCurrentHistoryEntry() != currentEntry)
					SyncMenuToHistory(browserWindow->GetWindowContext());
			}
		}
	}
	else
		// If there are no windows, we default to browser menus, so clear
		// history items from menu
		UMenuUtils::PurgeMenuItems(GetMacMenuH(), GetLastNonDynamicItem());
}

#define Min(a,b)		(((a) < (b)) ? (a) : (b))

// Rebuild history menu
void CHistoryMenu::SyncMenuToHistory(CNSContext* inNSContext)
{
	Int16				menuSize;
	Int16				numHistoryMenuEntries;
	Int16				historyLength;
	
	menuSize = ::CountMItems(GetMacMenuH());
	historyLength = inNSContext->GetHistoryListCount();
	
	// this is total number of history menu items we want in menu
	// add one because we loop from 1 -> num instead of from 0 -> (num - 1)
	numHistoryMenuEntries = Min(historyLength, cMaxHistoryMenuItems) + 1;

	Int16	count = 1,
			// since we want most recent history entries, grab entries from end
			// of history list
			entryIndex = historyLength,
			// this is index of menu item we're going to insert
			currentMenuItemIndex = count + GetLastNonDynamicItem(),
			// index of of current history entry in 
			currentEntryIndex = inNSContext->GetIndexOfCurrentHistoryEntry();
	// Now loop over current history menu items, overwriting current menu items.
	// If the new history is longer than current one, append menu items
	while (count < numHistoryMenuEntries)
	{
		// copy title string because we need to 'unescape' string
		CAutoPtr<cstring> title;
		
		title.reset(inNSContext->GetHistoryEntryTitleByIndex(entryIndex));
		if (title.get())
		{
			if (!title->length())
			{
				title.reset(new cstring);
				inNSContext->GetHistoryURLByIndex(*title, entryIndex);
			}
		
			UMenuUtils::AdjustStringForMenuTitle(*title);
			LStr255 pstr(*title);
			// append menu item if we've reached end of menu
			if (currentMenuItemIndex > menuSize)
				UMenuUtils::AppendMenuItem(GetMacMenuH(), pstr, true);
			else
				::SetMenuItemText(GetMacMenuH(), currentMenuItemIndex, pstr);
		}

//		NOTE: These command keys are used in the "Window" menu now.
//		if (count < 11)		// Setting of command numbers
//			::SetItemCmd(GetMacMenuH(), currentMenuItemIndex, '0' + count - 1);
//		else
//			::SetItemCmd(GetMacMenuH(), currentMenuItemIndex, 0);

		if (entryIndex == currentEntryIndex)	// Checkmark next to current entry
			::SetItemMark(GetMacMenuH(), currentMenuItemIndex, '');
		else
			::SetItemMark(GetMacMenuH(), currentMenuItemIndex, 0);
		++count;
		--entryIndex;
		++currentMenuItemIndex;
	}
	// remove left over menu items just in case new history list is
	// smaller than previous one
	if (currentMenuItemIndex <= menuSize)
		UMenuUtils::PurgeMenuItems(GetMacMenuH(), currentMenuItemIndex - 1);
	SetCurrentHistoryEntry(inNSContext->GetCurrentHistoryEntry());
}

CommandT CHistoryMenu::GetFirstSyntheticCommandID(void)
{
	if (sHistoryMenu)
		return (-(((Int32)sHistoryMenu->GetMenuID()) << 16)
				- sHistoryMenu->GetLastNonDynamicItem()
				- 1);
	else
		return cmd_Nothing;
}

Boolean CHistoryMenu::IsHistoryMenuSyntheticCommandID(CommandT inCommandID)
{
	Boolean result = false;
	CommandT firstCommandID, lastCommandID;
	
	if (sHistoryMenu)
	{
		firstCommandID = -(((Int32)sHistoryMenu->GetMenuID()) << 16)
						 - sHistoryMenu->GetLastNonDynamicItem()
						 - 1;
		lastCommandID = firstCommandID
						- (::CountMenuItems(sHistoryMenu->GetMacMenuH())
						   - sHistoryMenu->GetLastNonDynamicItem())
						+ 1;
		// check that lastCommandID <= inCommandID <= firstCommandID
		// note: we're checking synthetic commands which are negative
		result = lastCommandID <= inCommandID && inCommandID <= firstCommandID;
	}
	
	return result;
}
