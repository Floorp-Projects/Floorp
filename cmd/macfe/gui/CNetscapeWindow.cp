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

#ifdef MOZ_OFFLINE
#include "CPatternProgressBar.h"
#include "MailNewsgroupWindow_Defines.h"
#include "LGAIconSuiteControl.h"
#endif //MOZ_OFFLINE

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
void CNetscapeWindow::AdjustStagger(NetscapeWindowT inType)
// Moved here from CBrowserWindow
//-----------------------------------
{
	if (HasAttribute(windAttr_Regular) && HasAttribute(windAttr_Resizable))
	{		
		Rect structureBounds = UWindows::GetWindowStructureRect(mMacWindowP);
		
		GDHandle windowDevice = UWindows::FindDominantDevice(structureBounds);
		GDHandle mainDevice = ::GetMainDevice();
		
		Rect staggerBounds;
		
		if (windowDevice && (windowDevice != mainDevice))
			staggerBounds = (*windowDevice)->gdRect;
		else
		{
			staggerBounds = (*mainDevice)->gdRect;
			staggerBounds.top += LMGetMBarHeight();
		}
		::InsetRect(&staggerBounds, 4, 4);	//	Same as zoom limit
		
		Rect contentBounds = UWindows::GetWindowContentRect(mMacWindowP);
		
		Rect windowBorder;
		windowBorder.top = contentBounds.top - structureBounds.top;
		windowBorder.left = contentBounds.left - structureBounds.left;
		windowBorder.bottom = structureBounds.bottom - contentBounds.bottom;
		windowBorder.right = structureBounds.right - contentBounds.right;
		
		Point maxTopLeft;
		maxTopLeft.v = staggerBounds.bottom
			- (windowBorder.top + mMinMaxSize.top + windowBorder.bottom);
		maxTopLeft.h = staggerBounds.right
			- (windowBorder.left + mMinMaxSize.left + windowBorder.right);
		
		UInt16 vStaggerOffset = windowBorder.top;
		
		if (vStaggerOffset > 12)
			vStaggerOffset -= 4;	//	Tweak it up
		
		const int maxStaggerPositionsCount = 10;
		Point staggerPositions[maxStaggerPositionsCount];
		UInt16 usedStaggerPositions[maxStaggerPositionsCount];
		int staggerPositionsCount;
		
		for (	staggerPositionsCount = 0;
				staggerPositionsCount < maxStaggerPositionsCount;
				++staggerPositionsCount)
		{
			staggerPositions[staggerPositionsCount].v
				= staggerBounds.top + (vStaggerOffset * staggerPositionsCount);
			staggerPositions[staggerPositionsCount].h
				= staggerBounds.left + (4 * staggerPositionsCount);
			
			if ((staggerPositions[staggerPositionsCount].v > maxTopLeft.v)
				|| (staggerPositions[staggerPositionsCount].h > maxTopLeft.h))
				break;
			
			usedStaggerPositions[staggerPositionsCount] = 0;
		}
		unsigned int windowCount = 0;
		CMediatedWindow *foundWindow = NULL;
		CWindowIterator windowIterator(inType);
		
		for (windowIterator.Next(foundWindow); foundWindow; windowIterator.Next(foundWindow))
		{
			CNetscapeWindow *netscapeWindow = dynamic_cast<CNetscapeWindow*>(foundWindow);
			
			if (netscapeWindow && (netscapeWindow != this)
				&& netscapeWindow->HasAttribute(windAttr_Regular))
			{
				++windowCount;
				Rect bounds = UWindows::GetWindowStructureRect(netscapeWindow->mMacWindowP);
				
				for (int index = 0; index < staggerPositionsCount; ++index)
				{
					Boolean matchTop = (bounds.top == staggerPositions[index].v);
					Boolean matchLeft = (bounds.left == staggerPositions[index].h);
					
					if ((matchTop) && (matchLeft))
						usedStaggerPositions[index] += 1;
					
					if ((matchTop) || (matchLeft))
						break;
				}
			}
		}
		Point structureSize;
		structureSize.v = structureBounds.bottom - structureBounds.top;
		structureSize.h = structureBounds.right - structureBounds.left;
		
		if (windowCount)
		{
			Boolean foundStaggerPosition = false;
			
			for (UInt16 minCount = 0; (minCount < 100) && !foundStaggerPosition; ++minCount)
			{
				for (int index = 0; index < staggerPositionsCount; ++index)
				{
					if (usedStaggerPositions[index] == minCount)
					{
						structureBounds.top = staggerPositions[index].v;
						structureBounds.left = staggerPositions[index].h;
						foundStaggerPosition = true;
						break;
					}
				}
			}
			if (!foundStaggerPosition)
			{
				structureBounds.top = staggerBounds.top;
				structureBounds.left = staggerBounds.left;
			}
			structureBounds.bottom = structureBounds.top + structureSize.v;
			structureBounds.right = structureBounds.left + structureSize.h;
		}
		if ((structureBounds.top > maxTopLeft.v) || (structureBounds.left > maxTopLeft.h))
		{
			structureBounds.top = staggerBounds.top;
			structureBounds.left = staggerBounds.left;
			structureBounds.bottom = structureBounds.top + structureSize.v;
			structureBounds.right = structureBounds.left + structureSize.h;
		}
		if (structureBounds.bottom > staggerBounds.bottom)
			structureBounds.bottom = staggerBounds.bottom;
		
		if (structureBounds.right > staggerBounds.right)
			structureBounds.right = staggerBounds.right;
		
		contentBounds.top = structureBounds.top + windowBorder.top;
		contentBounds.left = structureBounds.left + windowBorder.left;
		contentBounds.bottom = structureBounds.bottom - windowBorder.bottom;
		contentBounds.right = structureBounds.right - windowBorder.right;
		
		DoSetBounds(contentBounds);
	}
} // CNetscapeWindow::AdjustStagger()

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
		if (nscpWindow)		// Bug #113623
		{
			CNSContext* cnsContext = nscpWindow->GetWindowContext();
			if (cnsContext)
			{
				MWContext* mwContext = *cnsContext;
				if (mwContext)
					FE_Progress(mwContext, CStr255(inMessage));
			}

#ifdef MOZ_OFFLINE
			else
			{
				// Mail Compose window (for instance?) has no context but has a status bar 
				CPatternProgressCaption* progressCaption
					= dynamic_cast<CPatternProgressCaption*>
						(nscpWindow->FindPaneByID(kMailNewsStatusPaneID));
				if (progressCaption)
					progressCaption->SetDescriptor(inMessage);
			}
			LGAIconSuiteControl* offlineButton
				= dynamic_cast<LGAIconSuiteControl*>
					(nscpWindow->FindPaneByID(kOfflineButtonPaneID));
			if (offlineButton)
			{
				ResIDT iconID = NET_IsOffline() ? 15200 : 15201;
				offlineButton->SetIconResourceID(iconID);
				offlineButton->Refresh();
			}

#endif //MOZ_OFFLINE
		}
	}
} // CNetscapeWindow::DisplayStatusMessageInAllWindows

//----------------------------------------------------------------------------------------
void CNetscapeWindow::StopAllContexts()
//----------------------------------------------------------------------------------------
{
	CNSContext* context = GetWindowContext();
	if (context)
		XP_InterruptContext(*context);
}

//----------------------------------------------------------------------------------------
Boolean CNetscapeWindow::IsAnyContextBusy()
//----------------------------------------------------------------------------------------
{
	CNSContext* context = GetWindowContext();
	if (context)
		return XP_IsContextBusy(*context);
	return false;
}

//-----------------------------------
Boolean CNetscapeWindow::ObeyCommand(CommandT inCommand, void *ioParam)
//-----------------------------------
{
	Boolean	cmdHandled = false;
	
	switch (inCommand)
	{
		case cmd_Preferences:
			DoDefaultPrefs();
			return true;
		case cmd_Stop:
			// The stop bucks here.
			StopAllContexts();
			return true;
		default:
			cmdHandled = Inherited::ObeyCommand(inCommand, ioParam);
			break;
	}
		
	return cmdHandled;
}

//----------------------------------------------------------------------------------------
void CNetscapeWindow::FindCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
//----------------------------------------------------------------------------------------
{
	outUsesMark = false;
	
	switch (inCommand)
	{
		case cmd_Stop:
			outEnabled = IsAnyContextBusy();
			break;
		
		default:
			Inherited::FindCommandStatus(
				inCommand, outEnabled, outUsesMark, outMark, outName);
	}
} // CNetscapeWindow::FindCommandStatus
