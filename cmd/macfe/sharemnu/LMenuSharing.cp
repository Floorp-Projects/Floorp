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

#include "LMenuSharing.h" // ALWAYS put the .h file first.  This ensures that it compiles.

#include "MenuSharing.h" // This is the UserLand header.
#include "uapp.h"
#include "uerrmgr.h"
#include "PascalString.h"
#include "MercutioAPI.h"

#include "CTargetedUpdateMenuRegistry.h"

// Menu sharing callbacks
static pascal void ErrorDialog (Str255 s);
static pascal void EventFilter (EventRecord *ev);

static pascal void ErrorDialog (Str255 s)
{
	ErrorManager::PlainAlert(s, NULL, NULL, NULL);
} 

static pascal void EventFilter (EventRecord *ev)
{
	(CFrontApp::GetApplication())->DispatchEvent(*ev);
}

// ===========================================================================
// LMenuSharing.cp										 
// ===========================================================================
//	MenuSharing class. Based upon Frontier and draper@usis.com code

Int16 LMenuSharingAttachment::sInsertAfterMenuID = 0;
Int16 LMenuSharingAttachment::sNextPluginMenuID = 20000;

LMenuSharingAttachment::LMenuSharingAttachment(
	MessageT	inMessage,
	Boolean		inExecuteHost,Int16	resIDofLastMenu)
		: LAttachment(inMessage, inExecuteHost)
{
	mCanMenuShare = ::InitSharedMenus (ErrorDialog, EventFilter);	
	sInsertAfterMenuID = resIDofLastMenu + 1;
}

Int16 LMenuSharingAttachment::AllocatePluginMenuID(Boolean isSubmenu)
{
	// force the menus to be recreated.
	DisposeSharedMenus();
	if (isSubmenu) {
		// this needs to take a menu ID out of the space used by MenuSharing.
		return sInsertAfterMenuID++;
	} else {
		// these can come out of a different range, because they aren't hierarchical.
		return sNextPluginMenuID++;
	}
}

void LMenuSharingAttachment::ExecuteSelf( MessageT inMessage, void* ioParam)
{
	if (!mCanMenuShare)
			return; // Don't bother if we can't menu share...
	mExecuteHost = true;
	switch (inMessage)
	{
	case msg_Event:
		
		EventRecord *	ev = (EventRecord*)ioParam;
		ResIDT			menuId;
		Int16			menuItem;
		MenuHandle		menuH;
		CommandT		command;
		LMenuBar*		menubar;
		WindowPtr 		w;
		Int16			part;
		LCommander*		commander;
		
		CheckSharedMenus(sInsertAfterMenuID);
		
		if (ev->what== mouseDown)
		{
			part = FindWindow (ev->where, &w);
	 		if (part == inMenuBar) 
			{
				// (CFrontApp::GetApplication())->UpdateMenus();
				// myoung: Do this instead.
				CTargetedUpdateMenuRegistry::SetCommands(CFrontApp::GetCommandsToUpdateBeforeSelectingMenu());
				CTargetedUpdateMenuRegistry::UpdateMenus();
				
				menubar = LMenuBar::GetCurrentMenuBar();

				Int32 theMenuChoice;
				command = menubar->MenuCommandSelection(*ev, theMenuChoice);
			
				menubar->FindMenuItem(command,menuId,menuH,menuItem);
				mExecuteHost = false;
				if (command != cmd_Nothing)
				{
					if (LCommander::IsSyntheticCommand(command, menuId, menuItem))
					{
						if (SharedMenuHit (menuId, menuItem))
						{
							HiliteMenu(0);
							break;
						}
					}
					commander = LCommander::GetTarget();
					commander->ProcessCommand(command, nil);
					mExecuteHost = false;
				}
				HiliteMenu(0);
			}
			break;
		}
		else if (ev->what!=keyDown)	//On keyDown, just drop through the attachments
			break;
	case msg_KeyPress:

		ev = (EventRecord*)ioParam;
		char 		ch;

		mExecuteHost = true;
		command = cmd_Nothing;
		ch = (*ev).message & charCodeMask;
		menubar = LMenuBar::GetCurrentMenuBar();
		if (menubar->CouldBeKeyCommand(*ev))
		{
			// I moved this in here so it is only called when the command key is down.
			// It was making editing unusable.  Also, we only need the menu stuff, not
			// the side effects (like button updates), so call the base class method.
			// --- 97/01/21 jrm
			
			// (CFrontApp::GetApplication())->LApplication::UpdateMenus();
			// myoung: Don't do this here.

			//	Use code similar to CFrontApp::EventKeyDown which calls MDEF_MenuKey
			//	instead of calling LMenuBar::FindKeyCommand which calls MenuKey.
			//	DDM 06-JUN-96
			Int32 theMenuChoice = MDEF_MenuKey(ev->message, ev->modifiers, ::GetMenu(666));
			
			if (HiWord(theMenuChoice) != 0)
			{
				command = LMenuBar::GetCurrentMenuBar()->FindCommand(HiWord(theMenuChoice), LoWord(theMenuChoice));
				
				if (LCommander::IsSyntheticCommand(command,menuId, menuItem))
				{
					if (SharedMenuHit (menuId, menuItem))// someone is running a script from a commandKey
					{
						HiliteMenu(0);
						mExecuteHost = false;						
						break;
					}
				}
			}
			if (ch == '.')
			{
				if (SharedScriptRunning ()) /*cmd-period terminates the script*/
								CancelSharedScript (); /*cancel the shared menu script*/
			}
		}
		break;	
	default:
		break;
	}
}

	
	
