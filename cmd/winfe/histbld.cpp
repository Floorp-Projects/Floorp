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

// histbld.cpp
// This implements the CHistoryMenuBuilder class

#include "stdafx.h"
#include "histbld.h"

// Function - CHistoryMenuBuilder
//
// Constructor creates the command to history entry map
// 
// Arguments -  none
//
// Returns - nothing
CHistoryMenuBuilder::CHistoryMenuBuilder()
{

	m_pMenuMap = new CMapWordToPtr;
}

// Function - ~CHistoryMenuBuilder
//
// Destructor destroys the command to history entry map
// 
// Arguments -  none
//
// Returns - nothing
CHistoryMenuBuilder::~CHistoryMenuBuilder()
{
	m_pMenuMap->RemoveAll();
	delete m_pMenuMap;
}

// Function - Fill
//
// Fills the menu with up to MAX_HISTORY_MENU_ITEMS entries.  The type of fills
// (everything, everything after the current item, or everything before the current
//  item) is determined by the user 
// 
// Arguments -  hMenu				   the menu we are adding to
//			    nMaxMenuItemLength     the length of the longest item that can be stored in the menu.
//									   Anything longer will be truncated.
//			    eFill				   the type of fill to perform
//
// Returns - nothing
void CHistoryMenuBuilder::Fill(HMENU hMenu, int nMaxMenuItemLength, eFillEnum eFill)
{

		m_pMenuMap->RemoveAll();

		CAbstractCX * pCX = FEU_GetLastActiveFrameContext();
		
		if (!pCX)
			return;


		// Get the session history list
		XP_List* pList = SHIST_GetList(pCX->GetContext());

		// Get the pointer to the current history entry
		History_entry*  pCurrentDoc = pCX->GetContext()->hist.cur_doc_ptr;

		XP_List *pCurrentNode = XP_ListFindObject(pList, pCurrentDoc);

		ASSERT(pList);
		if (!pList)
			return;

		pList = pList->prev;
		
		int nNumItems = 0;

		if(eFill == eFILLFORWARD)
		{
			//Start at the one after the current node and pass in TRUE to go forward
			FillItems(hMenu, pCurrentNode->next, TRUE, nMaxMenuItemLength, pCurrentDoc);
		}
		else if(eFill == eFILLBACKWARD)
		{
			//Start at the one before the current node and pass in FALSE to go backwards
			FillItems(hMenu, pCurrentNode->prev, FALSE, nMaxMenuItemLength, pCurrentDoc);
		}
		else //eFill == eALL
		{
			//Start at the last item and pass in FALSE to go backwards
			FillItems(hMenu, pList, FALSE, nMaxMenuItemLength, pCurrentDoc);
		}
		
}

// Function - Execute
//
// Given the command id, this function finds the corresponding history entry and changes
// the browser to that page
// 
// Arguments -  nCommandID		 the command we want to execute
//
// Returns - TRUE if it could execute the command, FALSE otherwise
BOOL CHistoryMenuBuilder::Execute(UINT nCommandID)
{
		void* nLastSelectedData;

		m_pMenuMap->Lookup(nCommandID, nLastSelectedData);

		CAbstractCX * pCX = FEU_GetLastActiveFrameContext();
		
		if (!pCX)
			return FALSE;

		// Get ready to load
		URL_Struct* pURL = SHIST_CreateURLStructFromHistoryEntry(pCX->GetContext(),
			(History_entry*)nLastSelectedData);

		if (!pURL)
			return FALSE;
	
		//      Load up.
		pCX->GetUrl(pURL, FO_CACHE_AND_PRESENT);

		return TRUE;
}

char * CHistoryMenuBuilder::GetURL(UINT nCommandID)
{

		void* pCommandData;

		m_pMenuMap->Lookup(nCommandID, pCommandData);

		return ((History_entry*)pCommandData)->address;
}

//////////////////////////////////////////////////////////////////////////
//						Helper Functions for CHistoryMenuBuilder
//////////////////////////////////////////////////////////////////////////

// Function - AddHistoryToMenu
//
// Adds a history item to the passed in menu.  Also attaches a numerica accelerator to the menu item
// if nPosition is less than 10.
// 
// Arguments -  hMenu				the menu we are adding to
//			    nPosition			the position in the menu (0 is at the top).  This is used for creating
//									the accelerator.  Items are always appended to the menu
//				bCurrent			Is this the current page that's showing.  If it is, we'll put a checkmark
//									by the menu item.
//				pEntry				the history entry we are adding to the menu
//				nMaxMenuItemLength	The maximum length string we can store in the menu. Strings longer than
//									this are truncated
//
// Returns - TRUE if it could add the item, FALSE otherwise
BOOL CHistoryMenuBuilder::AddHistoryToMenu(HMENU hMenu, int nPosition, BOOL bCurrent, History_entry*  pEntry,
										   int nMaxMenuItemLength)
{

	char*  lpszText;

	
	ASSERT(pEntry->title);
	if (!pEntry->title)
		 return FALSE;

	// Each of the menu items is numbered
	if (nPosition < 10)
		lpszText = PR_smprintf("&%d %s", nPosition, pEntry->title);
	else
		lpszText = PR_smprintf("  %s", pEntry->title);

	// We need to shorten the name to keep the menu width reasonable. TRUE for
	// the third argument means to modify the passed in string and not make a copy
	char*   lpszShortName = fe_MiddleCutString(lpszText, nMaxMenuItemLength, TRUE);

	// Append the menu item
	::AppendMenu(hMenu,MF_STRING | MF_ENABLED | bCurrent ? MF_CHECKED : MF_UNCHECKED, 
					  FIRST_HISTORY_MENU_ID + nPosition, lpszShortName);

	// Add the history entry to the map. We need to do this because Windows
	// doesn't allow any client data to be associated with menu items
	m_pMenuMap->SetAt(FIRST_HISTORY_MENU_ID + nPosition, pEntry);

	// Free the allocated memory
	XP_FREE(lpszText);

	return TRUE;

}

// Function - FillItems
//
// Adds all of the history entries in the passed in list to the menu up to MAX_NUM_HISTORY_MENU_ITEMS
// 
// Arguments -  hMenu				the menu we are adding to
//				pList				the list that has all of the history entries
//				bForward			if TRUE, iterate over the next link otherwise over the prev link
//				nMaxMenuItemLength	The maximum length string we can store in the menu. Strings longer than
//									this are truncated
//				pCurrentDoc			The page we are currently on
//
// Returns - TRUE if it could add the items, FALSE otherwise
BOOL CHistoryMenuBuilder::FillItems(HMENU hMenu, XP_List *pList, BOOL bForward, int nMaxMenuItemLength,
										   History_entry* pCurrentDoc)
{

	int nNumItems = 0;

	while(pList)
	{
		History_entry*  pEntry = (History_entry*)pList->object;


		ASSERT(pEntry);
		if (!pEntry)
			return FALSE;

		if(nNumItems >= MAX_NUM_HISTORY_MENU_ITEMS)
			return FALSE;

		AddHistoryToMenu(hMenu, nNumItems++, pCurrentDoc == pEntry, pEntry, nMaxMenuItemLength);

	
		if(bForward)
		{
			pList = pList->next;
		}
		else
		{
			pList = pList->prev;
		}
	}

	return TRUE;
}

