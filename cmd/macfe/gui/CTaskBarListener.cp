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

#include "CTaskBarListener.h"

#include "Netscape_Constants.h"
#include "CWindowMediator.h"
#include "macutil.h"
#include "UTearOffPalette.h"
#include "resgui.h"

enum
{
	eTaskBarFloatingWindowID = 1003
};

void CTaskBarListener::GetToggleTaskBarWindowMenuItemName(Str255 &name)
{
	CTearOffManager *theTearOffManger = CTearOffManager::GetDefaultManager();

	short stringID;
	/*
	StringHandle hString;
	if (theTearOffManger->IsFloatingWindow(eTaskBarFloatingWindowID))
	{
		stringID = cmd_ToggleTaskBar + 1;
	}
	else
	{
		stringID = cmd_ToggleTaskBar;
	}
	hString = GetString(stringID);
	CopyString(name, (*hString));
	*/
	
	if (theTearOffManger->IsFloatingWindow(eTaskBarFloatingWindowID))
	{
		stringID = 1;
	}
	else
	{
		stringID = 2;
	}
	
	GetIndString(name, cmd_ToggleTaskBar, stringID);
}

void CTaskBarListener::ToggleTaskBarWindow()
{
	CTearOffManager *theTearOffManger = CTearOffManager::GetDefaultManager();

	if (theTearOffManger->IsFloatingWindow(eTaskBarFloatingWindowID))
	{
		theTearOffManger->CloseFloatingWindow(eTaskBarFloatingWindowID);
	}
	else
	{
		Point	topLeft;
		topLeft.h = topLeft.v = 100;
		theTearOffManger->OpenFloatingWindow(	eTaskBarFloatingWindowID,
												topLeft, false, 0, "\p");
	}
}
