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

// histbld.h
// Defines the interface for the CHistoryMenuBuilder class which fills in a menu
// with the current history items depending on the Fill Type passed into Fill.  This also
// keeps track of which url goes with each menu item and can be called with a menu item
// to go to a corresponding url

#ifndef _HISTBLD_H
#define _HISTBLD_H

#define MAX_NUM_HISTORY_MENU_ITEMS 15

#include "stdafx.h"

typedef enum {eFILLALL, eFILLFORWARD, eFILLBACKWARD} eFillEnum;

class CHistoryMenuBuilder{

private:
	CMapWordToPtr *m_pMenuMap;	// converts commands to their history entry

public:


	CHistoryMenuBuilder();
	~CHistoryMenuBuilder();

	void  Fill(HMENU hMenu, int nMaxMenuItemLength, eFillEnum eFill);
	BOOL  Execute(UINT nCommandID);
	char* GetURL(UINT nCommandID);


private:
	BOOL AddHistoryToMenu(HMENU hMenu, int nPosition, BOOL bCurrent, History_entry*  pEntry,int nMaxMenuItemLength);
	BOOL FillItems(HMENU hMenu, XP_List *pList, BOOL bForward, int nMaxMenuItemLength,
										   History_entry* pCurrentDoc);


};
#endif
