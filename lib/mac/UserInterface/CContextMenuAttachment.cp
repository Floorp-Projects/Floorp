/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "CContextMenuAttachment.h"
#include "xp.h"

#include "CDrawingState.h"
#include "CApplicationEventAttachment.h"

//-----------------------------------
CContextMenuAttachment::CContextMenuAttachment(LStream* inStream)
:	LAttachment(inStream)
//-----------------------------------
{
	*inStream >> mMenuID >> mTextTraitsID;
	mHostView = dynamic_cast<LPane*>(LAttachable::GetDefaultAttachable());
	Assert_(mHostView);
	mHostCommander = dynamic_cast<LCommander*>(LAttachable::GetDefaultAttachable());
	if ( !mHostCommander )
		mHostCommander = LCommander::GetDefaultCommander();
	Assert_(mHostCommander);
}

//-----------------------------------
CContextMenuAttachment::CContextMenuAttachment ( ResIDT inMenuID, ResIDT inTextTraitsID )
:	LAttachment(), mMenuID(inMenuID), mTextTraitsID(inTextTraitsID)
//-----------------------------------
{
	mHostView = dynamic_cast<LPane*>(LAttachable::GetDefaultAttachable());
	Assert_(mHostView);
	mHostCommander = dynamic_cast<LCommander*>(LAttachable::GetDefaultAttachable());
	if ( !mHostCommander )
		mHostCommander = LCommander::GetDefaultCommander();
	Assert_(mHostCommander);
}

//-----------------------------------
CContextMenuAttachment::~CContextMenuAttachment()
//-----------------------------------
{
} // CContextMenuAttachment::CContextMenuAttachment

//-----------------------------------
EClickState CContextMenuAttachment::WaitMouseAction(
	const Point& 	inInitialPoint,
	Int32			inWhen,
	Int32			inDelay)
//-----------------------------------
{
	//mHostView->FocusDraw();

	if (CApplicationEventAttachment::CurrentEventHasModifiers(controlKey))
	{
		return eMouseTimeout;
	}

	if (inDelay == kDefaultDelay)			// GetDblTime() can be 48, 32 or 20 ticks
		inDelay = MAX(GetDblTime(), 30); 	// but 20 is too short to display the popup

	while (::StillDown())
	{
		Point theCurrentPoint;
		::GetMouse(&theCurrentPoint);
		if ((abs(theCurrentPoint.h - inInitialPoint.h) >= kMouseHysteresis) ||
			(abs(theCurrentPoint.v - inInitialPoint.v) >= kMouseHysteresis))
			return eMouseDragging;
		Int32 now = ::TickCount();
		if (abs( now - inWhen ) > inDelay)
			return eMouseTimeout;
	}
	return eMouseUpEarly;
} // CContextMenuAttachment::WaitMouseAction

//-----------------------------------
UInt16 CContextMenuAttachment::PruneMenu(LMenu* inMenu)
// Prune the menu.  Either enable each item, or remove it.  Returns the menu count.
//-----------------------------------
{

// Flag bits
#define kKeepItemDisabled		1<<0 /* Don't remove this item, show it disabled */
#define kSupercommanderCannotEnable	1<<1
#define kWantsToBeDefault	1<<2
// (NOTE: Bit 7 is not usable, because the icon ID must have this bit set!)
	MenuHandle menuH = inMenu->GetMacMenuH();
	Int16 index = 0; CommandT command;
	Boolean previousItemWasSeparator = true; // true for first item
	Boolean isSeparator = false;
	Int16 itemCount = ::CountMItems(menuH);
	UInt16 defaultItem = 0;
	UInt16 firstEnabledItem = 0;
	while (inMenu->FindNextCommand(index, command))
	{
		Boolean enabled = false;
		Str255 commandText; // may be changed by host commander!
		::GetMenuItemText(menuH, index, commandText);
		Int16 itemFlags; // icon ID used for flags
		::GetItemIcon(menuH, index, &itemFlags);
		// Get rid of the icon ID (which we use for the flag hack)
		if (itemFlags)
			::SetItemIcon(menuH, index, 0);
		if (command > 0)
		{
			Boolean usesMark = false;
			Char16 outMark;
			Boolean statusKnown = false;
			// If a command is not to be enabled by virtue of its super alone, check
			// the super first.  If the super enables it, it's outta there.
			if ((itemFlags & kSupercommanderCannotEnable) != 0)
			{
				LCommander* super = mHostCommander->GetSuperCommander();
				if (super)
				{
					Boolean superEnabled = false;
					mHostCommander->ProcessCommandStatus(
						command, superEnabled, usesMark, outMark, commandText);
					if (superEnabled)
					{
						enabled = false;
						statusKnown = true;
					}
				}
			}
			// Normal case.  Ask the commander, let it use super if appropriate:
			if (!statusKnown)
				mHostCommander->ProcessCommandStatus(
					command, enabled, usesMark, outMark, commandText);
		}
		if (enabled)
		{
			::EnableItem(menuH, index);
			::SetMenuItemText(menuH, index, commandText);
			// Allow a separator on the next line
			previousItemWasSeparator = false;
			// First item that says "I can be default" wins...
			if (defaultItem == 0 && (itemFlags & kWantsToBeDefault))
				defaultItem = index;
			if (firstEnabledItem == 0)
				firstEnabledItem = index;
		}
		else // disabled, including separators
		{
			isSeparator = commandText[1] == '-';
			if (!isSeparator || previousItemWasSeparator)
			{
				if (!isSeparator && (itemFlags & kKeepItemDisabled))
				{
					::DisableItem(menuH, index);
					previousItemWasSeparator = false;
				}
				else
				{
					inMenu->RemoveItem(index);
					// decrement index so that we refind the next item
					index--;
					itemCount--;
					if (!itemCount) break;
					// Uh Oh, all items after the last separator were removed!
					if (index == itemCount && previousItemWasSeparator)
					{
						inMenu->RemoveItem(index);
						index--;
						itemCount--;
						break;
					}
				}
			}
			if (isSeparator)
				previousItemWasSeparator = true;
		}
	} // while
	// The caller will only show the menu if we return a result > 0.  So handle the
	// case when there are items, but all items are disabled.
	if (defaultItem == 0 && itemCount > 0)
		defaultItem = firstEnabledItem ? firstEnabledItem : 1;
	return defaultItem;
} // CContextMenuAttachment::PruneMenu

//-----------------------------------
void CContextMenuAttachment::DoPopup(const SMouseDownEvent& inMouseDown, EClickState& outResult)
//-----------------------------------
{
	try
	{
		LMenu* menu = BuildMenu();
		MenuHandle menuH = menu->GetMacMenuH();
		
		CommandT command = 0;
		// Call PruneMenu to strip out disabled commands.  Do nothing if no commands
		// are available.
		UInt16 defaultItem = PruneMenu(menu);
		if (defaultItem > 0)
		{
			Int16 saveFont = ::LMGetSysFontFam();
			Int16 saveSize = ::LMGetSysFontSize();
			if (mTextTraitsID)
			{				
				TextTraitsH traitsH = UTextTraits::LoadTextTraits(mTextTraitsID);
				if (traitsH)
				{
					::LMSetSysFontFam((**traitsH).fontNumber);
					::LMSetSysFontSize((**traitsH).size);
					::LMSetLastSPExtra(-1L);
				}
			}
			// Handle the actual insertion into the hierarchical menubar
			::InsertMenu(menuH, hierMenu);
			// Bring up the menu.
			Point whereGlobal = inMouseDown.wherePort;
			mHostView->PortToGlobalPoint(whereGlobal);
			Int16 result = ::PopUpMenuSelect(
							menuH,
							whereGlobal.v - 5,
							whereGlobal.h - 5,
							defaultItem);
			mExecuteHost = false;
			outResult = eMouseHandledByAttachment;
			// Restore the system font
			::LMSetSysFontFam(saveFont);
			::LMSetSysFontSize(saveSize);
			::LMSetLastSPExtra(-1L);
			if (result)
				command = menu->CommandFromIndex(result);
		}
		delete menu;
		if (command)
			mHostCommander->ObeyCommand(command, (void*)&inMouseDown);
	}
	catch(...)
	{
	}
} // CContextMenuAttachment::DoPopup

//----------------------------------------------------------------------------------------
Boolean CContextMenuAttachment::Execute( MessageT inMessage, void *ioParam )
// Overridden to listen for multiple messages (context menu and context menu cursor)
//----------------------------------------------------------------------------------------
{
	Boolean	executeHost = true;
	
	if ((inMessage == msg_ContextMenu) || (inMessage == msg_ContextMenuCursor))
	{
		ExecuteSelf(inMessage, ioParam);
		executeHost = mExecuteHost;
	}
	return executeHost;
} // CContextMenuAttachment::Execute


//-----------------------------------
void CContextMenuAttachment::ExecuteSelf(
	MessageT	inMessage,
	void*		ioParam)
//-----------------------------------
{
	mExecuteHost = true;
	if (!mHostView || !mHostCommander)
		return;
	switch ( inMessage )
	{
		case msg_ContextMenu:
			mHostView->FocusDraw(); // so that coordinate computations are correct.
			SExecuteParams& params = *(SExecuteParams*)ioParam;		
			const SMouseDownEvent& mouseDown = *params.inMouseDown;
			params.outResult = WaitMouseAction(
										mouseDown.whereLocal,
										mouseDown.macEvent.when);
			if (params.outResult == eMouseTimeout)
				DoPopup(mouseDown, params.outResult);
			break;
		case msg_ContextMenuCursor:
			Assert_(ioParam != NULL);
			ChangeCursor ( (EventRecord*)ioParam );
			break;
	} // case of which message
} // CContextMenuAttachment::ExecuteSelf

//----------------------------------------------------------------------------------------
LMenu* CContextMenuAttachment::BuildMenu( )
// THROWS OSErr on error
// Reads in the menu with the id given in the PPob. This can be overridden to read the
// menu from some other place, such as from the back-end.
//----------------------------------------------------------------------------------------
{
	LMenu* menu = new LMenu(mMenuID);
	MenuHandle menuH = menu->GetMacMenuH();
	if (!menuH)
		throw (OSErr)ResError();
	::DetachResource((Handle)menuH);
	
	return menu;

} // CContextMenuAttachment::BuildMenu

//----------------------------------------------------------------------------------------
void CContextMenuAttachment::ChangeCursor( const EventRecord* inEvent )
// Makes the mouse the contextual menu cursor from OS8 if cmd key is down
//----------------------------------------------------------------------------------------
{
	if ( inEvent->modifiers & controlKey ) {
		CursHandle contextCurs = ::GetCursor ( kContextualCursorID );
		::SetCursor ( *contextCurs );
		mExecuteHost = false;
	}
	else
		::InitCursor();
} // ChangeCursor
