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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CMailFolderButtonPopup.cp

/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "CMailFolderButtonPopup.h"
#include "StSetBroadcasting.h"
#include "MailNewsgroupWindow_Defines.h"
#include "CDrawingState.h"
#include "CMessageFolder.h"


#pragma mark -

/*======================================================================================
	Call UpdateMailFolderMixins() to initialize the menu.
======================================================================================*/

void CMailFolderButtonPopup::FinishCreateSelf(void) {

	CPatternButtonPopup::FinishCreateSelf();
	
	CMailFolderMixin::UpdateMailFolderMixinsNow(this);
}


/*======================================================================================
	Instead of broadcasting the actual value, broadcast the new folder string.
======================================================================================*/

void CMailFolderButtonPopup::BroadcastValueMessage(void) {

	if ( mValueMessage != msg_Nothing )
	{
		BroadcastMessage(mValueMessage, MGetSelectedFolder().GetFolderInfo());
	}
}


/*======================================================================================
	When the user selects a new item, we don't really want to change the currently
	selected item in the menu, just broadcast the item that the user selected and
	reset the currently selected item.
======================================================================================*/

Boolean CMailFolderButtonPopup::TrackHotSpot(Int16 inHotSpot, Point inPoint, Int16 inModifiers) {

	// Store the current value
	
	Int32 oldValue = mValue;
	OSErr thrownErr = noErr;
	Boolean result;
	
	Try_ {
		result = CPatternButtonPopup::TrackHotSpot(inHotSpot, inPoint, inModifiers);
	}
	Catch_(inErr) {
		thrownErr = inErr;
	}
	EndCatch_
	
	// Reset the old value
	
	StSetBroadcasting setBroadcasting(this, false);	// Don't broadcast anything here
	SetValue(oldValue);
	FailOSErr_(thrownErr);
	
	return result;
}


/*======================================================================================
	Get a handle to the Mac menu associated with this object.
======================================================================================*/

MenuHandle CMailFolderButtonPopup::MGetSystemMenuHandle(void)
{
	MenuHandle menuH = nil;
	
	if (GetMenu())
	{
		menuH = GetMenu()->GetMacMenuH();
	}
	
	return menuH;
}


/*======================================================================================
	Refresh the menu display.
======================================================================================*/

void CMailFolderButtonPopup::MRefreshMenu(void) {

	// Nothing, no user menu displayed unless clicked
}

#pragma mark -

//-----------------------------------
CMailFolderPatternTextPopup::CMailFolderPatternTextPopup(LStream *inStream)
:	CPatternButtonPopupText(inStream)
//-----------------------------------
{
	mUseFolderIcons = eUseFolderIcons;
	// This is for the relocation menu, so add newsgroups.
	CMailFolderMixin::mDesiredFolderFlags =
			(FolderChoices)(int(mDesiredFolderFlags) | int(eWantNews));
}

/*======================================================================================
	Call UpdateMailFolderMixins() to initialize the menu.
======================================================================================*/

void CMailFolderPatternTextPopup::FinishCreateSelf(void)
{
	Inherited::FinishCreateSelf();
	CMailFolderMixin::UpdateMailFolderMixinsNow(this);
}


/*======================================================================================
	Instead of broadcasting the actual value, broadcast the new folder string.
======================================================================================*/

void CMailFolderPatternTextPopup::BroadcastValueMessage(void) {

	if ( mValueMessage != msg_Nothing ) {
		BroadcastMessage(mValueMessage, MGetSelectedFolder().GetFolderInfo());
	}
}


/*======================================================================================
	Unlike CMailFolderButtonPopup, when the user selects a new item, we DO really want
	to change the currently selected item in the menu, not just broadcast the item that
	the user selected and reset the currently selected item.
======================================================================================*/

Boolean CMailFolderPatternTextPopup::TrackHotSpot(Int16 inHotSpot, Point inPoint, Int16 inModifiers)
{
	return Inherited::TrackHotSpot(inHotSpot, inPoint, inModifiers);
}

void CMailFolderPatternTextPopup::HandlePopupMenuSelect(	Point		inPopupLoc,
												Int16		inCurrentItem,
												Int16		&outMenuID,
												Int16		&outMenuItem)
{
	Int16 saveFont = ::LMGetSysFontFam();
	Int16 saveSize = ::LMGetSysFontSize();
	
	StMercutioMDEFTextState theMercutioMDEFTextState;
	
	Inherited::HandlePopupMenuSelect(inPopupLoc, inCurrentItem, outMenuID, outMenuItem);

	// Restore the system font
	
	::LMSetSysFontFam(saveFont);
	::LMSetSysFontSize(saveSize);
	::LMSetLastSPExtra(-1L);

}

/*======================================================================================
	Get a handle to the Mac menu associated with this object.
======================================================================================*/

MenuHandle CMailFolderPatternTextPopup::MGetSystemMenuHandle(void) {
	if ( GetMenu() )
		return GetMenu()->GetMacMenuH();
	
	return NULL;
}


/*======================================================================================
	Refresh the menu display.
======================================================================================*/

void CMailFolderPatternTextPopup::MRefreshMenu(void) {

	// Nothing, no user menu displayed unless clicked
}


#pragma mark -

//-----------------------------------
CMailFolderGAPopup::CMailFolderGAPopup(LStream *inStream)
:	LGAPopup(inStream)
//-----------------------------------
{
	mUseFolderIcons = eUseFolderIcons;
	// This is for the relocation menu, so add newsgroups.
	CMailFolderMixin::mDesiredFolderFlags =
			(FolderChoices)(int(mDesiredFolderFlags) | int(eWantNews));
}

/*======================================================================================
	Call UpdateMailFolderMixins() to initialize the menu.
======================================================================================*/

void CMailFolderGAPopup::FinishCreateSelf(void)
{
	LGAPopup::FinishCreateSelf();
	CMailFolderMixin::UpdateMailFolderMixinsNow(this);
}


/*======================================================================================
	Instead of broadcasting the actual value, broadcast the new folder string.
======================================================================================*/

void CMailFolderGAPopup::BroadcastValueMessage(void) {

	if ( mValueMessage != msg_Nothing ) {
		BroadcastMessage(mValueMessage, MGetSelectedFolder().GetFolderInfo());
	}
}


/*======================================================================================
	Unlike CMailFolderButtonPopup, when the user selects a new item, we DO really want
	to change the currently selected item in the menu, not just broadcast the item that
	the user selected and reset the currently selected item.
======================================================================================*/

Boolean CMailFolderGAPopup::TrackHotSpot(Int16 inHotSpot, Point inPoint, Int16 inModifiers)
{
	return LGAPopup::TrackHotSpot(inHotSpot, inPoint, inModifiers);
}

void CMailFolderGAPopup::HandlePopupMenuSelect(	Point		inPopupLoc,
												Int16		inCurrentItem,
												Int16		&outMenuID,
												Int16		&outMenuItem)
{
	Int16 saveFont = ::LMGetSysFontFam();
	Int16 saveSize = ::LMGetSysFontSize();
	
	StMercutioMDEFTextState theMercutioMDEFTextState;
	
	LGAPopup::HandlePopupMenuSelect(inPopupLoc, inCurrentItem, outMenuID, outMenuItem);

	// Restore the system font
	
	::LMSetSysFontFam(saveFont);
	::LMSetSysFontSize(saveSize);
	::LMSetLastSPExtra(-1L);

}

/*======================================================================================
	Get a handle to the Mac menu associated with this object.
======================================================================================*/

MenuHandle CMailFolderGAPopup::MGetSystemMenuHandle(void) {
	return GetMacMenuH();
}


/*======================================================================================
	Refresh the menu display.
======================================================================================*/

void CMailFolderGAPopup::MRefreshMenu(void) {

	// Nothing, no user menu displayed unless clicked
}


#pragma mark -

//-----------------------------------
CFolderScopeGAPopup::CFolderScopeGAPopup(LStream* inStream)
//-----------------------------------
:	CMailFolderGAPopup(inStream)
{
	// This is for the scope menu in the message search window.
	mUseFolderIcons = eUseFolderIcons;
	CMailFolderMixin::mDesiredFolderFlags =
			(FolderChoices)(int(mDesiredFolderFlags) | int(eWantNews) | int(eWantHosts));
}

#pragma mark -

// Class static members

CMailFolderSubmenu *CMailFolderSubmenu::sMoveMessageMenu = nil;
CMailFolderSubmenu *CMailFolderSubmenu::sCopyMessageMenu = nil;

// Helper functions

static void CreateMailFolderSubmenu(Int16 inMENUid, CMailFolderSubmenu **outMenu) {

	if ( *outMenu == nil ) {
		Try_ {
			*outMenu = new CMailFolderSubmenu(inMENUid);
			if ( *outMenu ) {
				CMailFolderMixin::UpdateMailFolderMixinsNow(*outMenu);
			}
		}
		Catch_(inErr) {
			if ( *outMenu ) {
				delete *outMenu;
				*outMenu = nil;
			}
		}
		EndCatch_
	}
}

static void InstallMailFolderSubmenu(CommandT inCommand, CMailFolderSubmenu *inMenu) {

	if ( inMenu && !inMenu->IsInstalled() ) {
		ResIDT menuID;
		MenuHandle menuHandle;
		Int16 menuItem;
		
		LMenuBar::GetCurrentMenuBar()->FindMenuItem(inCommand, menuID,
													menuHandle, menuItem);

		if ( menuHandle != nil ) {
			LMenuBar::GetCurrentMenuBar()->InstallMenu(inMenu, hierMenu);
			
			// Install the submenu (hierarchical menu)
			
			::SetItemCmd(menuHandle, menuItem, hMenuCmd);
			::SetItemMark(menuHandle, menuItem, inMenu->GetMenuID());
		}
	}
}

static void RemoveMailFolderSubmenu(CMailFolderSubmenu *inMenu) {

	if ( inMenu && inMenu->IsInstalled() ) {
		LMenuBar::GetCurrentMenuBar()->RemoveMenu(inMenu);
	}
}

/*======================================================================================
	Install the mail folder submenus (file, copy) into the current menu bar. If the
	menu bar already contains the submenus, do nothing. This method should be called
	when a window using the menus becomes active.
	
	This method can be called at any time.
======================================================================================*/

void CMailFolderSubmenu::InstallMailFolderSubmenus(void) {

	// Create and initialize the menus if they don't exist yet
	CreateMenus();
	
	// Install the menus
	InstallMailFolderSubmenu(cmd_MoveMailMessages, sMoveMessageMenu);
	InstallMailFolderSubmenu(cmd_CopyMailMessages, sCopyMessageMenu);
}


/*======================================================================================
	Remove the mail folder submenus (file, copy) from the current menu bar. If the
	menu bar doesn't contain the submenus, do nothing. This method should be called
	when a window using the menus becomes inactive or a new menu bar is installed.
	
	This method can be called at any time.
======================================================================================*/

void CMailFolderSubmenu::RemoveMailFolderSubmenus(void) {

	// Remove the menus
	RemoveMailFolderSubmenu(sMoveMessageMenu);
	RemoveMailFolderSubmenu(sCopyMessageMenu);
}

//-----------------------------------
void CMailFolderSubmenu::SetSelectedFolder(const MSG_FolderInfo* inInfo)
//	Set the currently selected folder item in the hierarchical menus. If inInfo is nil
//	or empty, all items are unselected.
//	
//	This method can be called at any time.
//-----------------------------------
{
	// Create and initialize the menus if they don't exist yet
	CreateMenus();
	
	if ( sMoveMessageMenu != nil )
		sMoveMessageMenu->MSetSelectedFolder(inInfo);
	if ( sCopyMessageMenu != nil )
		sCopyMessageMenu->MSetSelectedFolder(inInfo);
}

//-----------------------------------
Boolean CMailFolderSubmenu::IsMailFolderCommand(CommandT *ioMenuCommand, const char** outName)
//	Determine if the specified synthetic menu command passed to ObeyCommand() is from
//	a mail folder menu. If it is, reassign ioMenuCommand to represent the actual mail
//	folder command ID and set outName to the name of the selected mail folder and return
//	true. Otherwise, return false and do nothing. outName can be nil, in which case no
//	name is assigned.
//	
//	This method can be called at any time.
//-----------------------------------
{	
	ResIDT menuID;
	Int16 menuItem;
	if ( LCommander::IsSyntheticCommand(*ioMenuCommand, menuID, menuItem) )
	{	
		if ( menuID == menuID_MoveMessage )
		{
			if ( sMoveMessageMenu != nil )
			{
				*ioMenuCommand = cmd_MoveMailMessages;
				if ( outName != nil )
					*outName = sMoveMessageMenu->MGetFolderName(menuItem);
				return true;
			}
		}
		else if ( menuID == menuID_CopyMessage )
		{
			if ( sCopyMessageMenu != nil )
			{
				*ioMenuCommand = cmd_CopyMailMessages;
				if ( outName != nil )
					*outName = sCopyMessageMenu->MGetFolderName(menuItem);
				return true;
			}
		}
	}	
	return false;
}

/*======================================================================================
	Create the hierarchical menus if they don't already exist.
	
	This method can be called at any time.
======================================================================================*/

void CMailFolderSubmenu::CreateMenus(void) {

	// Create and initialize the menus if they don't exist yet
	CreateMailFolderSubmenu(CMailFolderSubmenu::menuID_MoveMessage, &sMoveMessageMenu);
	CreateMailFolderSubmenu(CMailFolderSubmenu::menuID_CopyMessage, &sCopyMessageMenu);
	
}

#pragma mark -

/*======================================================================================
======================================================================================*/
CFolderMoveGAPopup::CFolderMoveGAPopup(LStream *inStream) : 
	CMailFolderGAPopup( inStream )
{
}

CFolderMoveGAPopup::~CFolderMoveGAPopup()
{
}

// reset the default menu item to the original value				
Boolean 	
CFolderMoveGAPopup::TrackHotSpot(Int16 inHotSpot, Point inPoint, Int16 inModifiers)
{
	Int32 	oldValue = GetValue();
	bool	result = false;
		
	result = LGAPopup::TrackHotSpot(inHotSpot, inPoint, inModifiers);
	if (oldValue != GetValue())
	{
		BroadcastMessage(mValueMessage, MGetSelectedFolder().GetFolderInfo());
	}
	else
	{	
		// we want to prevent a broadcast because we have already
		// sent one out when we set the value of the control
		StSetBroadcasting broacast(this, false);	
		SetValue(oldValue);	
	}			
	return result;
}



