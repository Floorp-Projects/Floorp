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
//	CNetscapeWindow.cp
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include "CNetscapeWindow.h"

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include "prefapi.h"
#include "resgui.h"

#include "CDragBar.h"
#include "CDragBarContainer.h"
#include "CNSContext.h"
#include "net.h"
#include "client.h"
#include "PascalString.h"

//-----------------------------------
CNetscapeWindow::CNetscapeWindow(LStream *inStream, DataIDT inWindowType)
//-----------------------------------
:	CMediatedWindow(inStream, inWindowType)
{
	// We need to read the preferences to really know whether
	// or not a tool bar should be visible. This can be taken
	// care of in the FinishCreateSelf method of derived classes.
	// For now, just assume they are all visible
	for (int i = 0; i < kMaxToolbars; i++)
		mToolbarShown[i] = true;
	SetAttribute(windAttr_DelaySelect);
} // CNetscapeWindow::CNetscapeWindow
									
//-----------------------------------
CNetscapeWindow::~CNetscapeWindow()
//-----------------------------------
{
}
	
//-----------------------------------
Boolean CNetscapeWindow::ObeyCommand(CommandT inCommand, void *ioParam)
//-----------------------------------
{
	Boolean		cmdHandled = true;
	
	switch (inCommand)
	{
		case cmd_Preferences:
			DoDefaultPrefs();
			break;
		default:
			cmdHandled = Inherited::ObeyCommand(inCommand, ioParam);
			break;
	}
		
	return cmdHandled;
}

//-----------------------------------
void CNetscapeWindow::ShowOneDragBar(PaneIDT dragBarID, Boolean isShown)
// shows or hides a single toolbar
// This used to be in CBrowserWindow. Moved up to CNetscapeWindow on 5/23/97 by andrewb
//-----------------------------------
{
	CDragBarContainer* container = dynamic_cast<CDragBarContainer*>(FindPaneByID(CDragBarContainer::class_ID));
	CDragBar* theBar = dynamic_cast<CDragBar*>(FindPaneByID(dragBarID));
	
	if (container && theBar) {
		if (isShown)
			container->ShowBar(theBar);
		else
			container->HideBar(theBar);
	}
} // CNetscapeWindow::ShowOneDragBar

//-----------------------------------
void CNetscapeWindow::ToggleDragBar(PaneIDT dragBarID, int whichBar, const char* prefName)
// toggles a toolbar and updates the specified preference
// This used to be in CBrowserWindow. Moved up to CNetscapeWindow on 5/23/97 by andrewb
//-----------------------------------
{
	Boolean showIt = ! mToolbarShown[whichBar];
	ShowOneDragBar(dragBarID, showIt);
	
	if (prefName) {
		PREF_SetBoolPref(prefName, showIt);
	}
	
	// force menu items to update show "Show" and "Hide" string changes are reflected
	LCommander::SetUpdateCommandStatus(true);
		
	mToolbarShown[whichBar] = showIt;
} // CNetscapeWindow::ToggleDragBar

//-----------------------------------
URL_Struct* CNetscapeWindow::CreateURLForProxyDrag(char* outTitle)
//-----------------------------------
{
	// Default implementation
	URL_Struct* result = nil;
	CNSContext* context = GetWindowContext();
	if (context)
	{		
		History_entry* theCurrentHistoryEntry = context->GetCurrentHistoryEntry();
		if (theCurrentHistoryEntry)
		{
			result = NET_CreateURLStruct(theCurrentHistoryEntry->address, NET_NORMAL_RELOAD);
			if (outTitle)
			{
				// The longest string that outTitle can contain is 255 characters but it
				// will eventually be truncated to 31 characters before it's used so why
				// not just truncate it here.  Fixes bug #86628 where a page has an
				// obscenely long (something like 1000 characters) title
				strncpy(outTitle, theCurrentHistoryEntry->title, 255);
				outTitle[255] = 0;	// Make damn sure it's truncated
				NET_UnEscape(outTitle);
			}
		}
	}
	return result;
} // CNetscapeWindow::CreateURLForProxyDrag

//-----------------------------------
/*static*/void CNetscapeWindow::DisplayStatusMessageInAllWindows(ConstStringPtr inMessage)
//-----------------------------------
{
	CMediatedWindow* aWindow = NULL;
	CWindowIterator iter(WindowType_Any);
	while (iter.Next(aWindow))
	{
		CNetscapeWindow* nscpWindow = dynamic_cast<CNetscapeWindow*>(aWindow);
		if (nscpWindow)
		{
			MWContext* mwContext = *nscpWindow->GetWindowContext();
			if (mwContext)
				FE_Progress(mwContext, CStr255(inMessage));
		}
	}
} // CNetscapeWindow::DisplayStatusMessageInAllWindows
