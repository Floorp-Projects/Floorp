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
//	CHistoryMenu.h
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include <LMenu.h>
#include <LListener.h>

// Mozilla

const Int16 cMaxHistoryMenuItems = 32;

class CNSContext;
typedef struct _History_entry History_entry;

class CHistoryMenu :	public LMenu
{
public:
						CHistoryMenu(ResIDT inMenuID);
	virtual				~CHistoryMenu();

	void				Update(void);

	static CommandT		GetFirstSyntheticCommandID(void);
	
	static Boolean		IsHistoryMenuSyntheticCommandID(CommandT inCommandID);

protected:
	void				SyncMenuToHistory(CNSContext* inNSContext);

	History_entry*		GetCurrentHistoryEntry(void);
	void				SetCurrentHistoryEntry(History_entry* inHistoryEntry);

	Int16				GetLastNonDynamicItem(void);

public:
	static CHistoryMenu*	sHistoryMenu;

protected:
	Int16				mLastNonDynamicItem;
	History_entry*		mCurrentHistoryEntry;
};

inline History_entry* CHistoryMenu::GetCurrentHistoryEntry()
	{	return mCurrentHistoryEntry;	};

inline void CHistoryMenu::SetCurrentHistoryEntry(History_entry* inHistoryEntry)
	{	mCurrentHistoryEntry = inHistoryEntry;	};

inline 	Int16 CHistoryMenu::GetLastNonDynamicItem()
	{	return mLastNonDynamicItem;		};
