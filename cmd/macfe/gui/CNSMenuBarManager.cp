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
//	CNSMenuBarManager.cp
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include "CNSMenuBarManager.h"
#include "CWindowMediator.h"
#include "CWindowMenu.h"
#include "CHistoryMenu.h"
#include "CBookmarksAttachment.h"
#include "CToolsAttachment.h"
#include "CFontMenuAttachment.h"
#include "CRecentEditMenuAttachment.h"
#include "Netscape_Constants.h"
#ifdef MOZ_MAIL_NEWS
#include "CMailFolderButtonPopup.h"
#endif
#include "CBrowserWindow.h" // need for MapWindowTypeToMBARResID - mjc
#include "menusharing.h"

CNSMenuBarManager* CNSMenuBarManager::sManager = NULL;

CNSMenuBarManager::CNSMenuBarManager(
	CWindowMediator* inWindowMediator) :
mCurrentMenuBarID(MBAR_Initial)
{
	inWindowMediator->AddListener(this);
	sManager = this;
}

CNSMenuBarManager::~CNSMenuBarManager()
{
	sManager = NULL;
}

void CNSMenuBarManager::ListenToMessage(MessageT inMessage, void* ioParam)
{
	switch (inMessage)
	{
		case msg_WindowActivated:
		case msg_WindowMenuBarModeChanged:
			{
				CMediatedWindow* inWindow = (CMediatedWindow*)ioParam;
				if (inWindow->HasAttribute(windAttr_Floating)) return;
				ResIDT newMenuBarID = MapWindowTypeToMBARResID(inWindow->GetWindowType(), inWindow);
				if (newMenuBarID > 0 && newMenuBarID != mCurrentMenuBarID)
					SwitchMenuBar(newMenuBarID);
			}
			break;
			
		case msg_WindowDisposed:
			{
				// Switch to initial menubar under these conditions
				if (mCurrentMenuBarID != MBAR_Initial)
				{
					CWindowMediator* windowMediator = CWindowMediator::GetWindowMediator();
					CMediatedWindow* inWindow = (CMediatedWindow*)ioParam;
					// count the regular, visible, mediated windows of any type
					int count = windowMediator->CountOpenWindows(WindowType_Any, regularLayerType, false);
					// two cases to switch:
					// 1. there are no regular mediated windows (so we must be closing a mediated floater or modal)
					// 2. we're closing the last regular mediated window
					if (count == 0 || (count == 1 && inWindow->IsVisible() && inWindow->HasAttribute(windAttr_Regular)))
						SwitchMenuBar(MBAR_Initial);
				}
			}
			break;
	}
}

ResIDT CNSMenuBarManager::MapWindowTypeToMBARResID(Uint32 inWindowType, CMediatedWindow* inWindow)
{
	ResIDT result = -1;

	switch(inWindowType)
	{
		case WindowType_Browser:
			if (inWindow != nil)
			{   // ask the browser window what kind of menubar it wants - mjc
				CBrowserWindow* theBrWn = dynamic_cast<CBrowserWindow*>(inWindow);
				switch(theBrWn->GetMenubarMode())
				{
					case BrWn_Menubar_None: 
					// ¥ FIXME return a value which tells the manager to hide the menubar.
					// fall through for now
					case BrWn_Menubar_Minimal:
						result = cMinimalMenubar;
						break;
					case BrWn_Menubar_Default:
						result = cBrowserMenubar;
						break;
				}
			}
			else result = cBrowserMenubar;
			break;
		case WindowType_Editor:
			result = cEditorMenubar;
			break;
		case WindowType_MailNews:
		case WindowType_MailThread:
		case WindowType_Message:
			result = cMailNewsMenubar;
			break;
		case WindowType_NavCenter:
			result = cBookmarksMenubar;
			break;
		case WindowType_Address:
			result = cAddressBookMenubar;
			break;
		case WindowType_Compose:
			result = cComposeMenubar;
			break;
	}
	return result;
}

void CNSMenuBarManager::SwitchMenuBar(ResIDT inMenuBarID)
{
	// remove go (history) and window menus from menubar so that it doesn't get deleted
	CHistoryMenu* historyMenu = CHistoryMenu::sHistoryMenu;
	CWindowMenu* windowMenu = CWindowMenu::sWindowMenu;
	LMenuBar* currentMenuBar = LMenuBar::GetCurrentMenuBar();
	currentMenuBar->RemoveMenu(windowMenu);

#ifdef EDITOR
	LMenu *fontMenu = CFontMenuAttachment::GetMenu();
	currentMenuBar->RemoveMenu(fontMenu);
#endif // EDITOR

	CBookmarksAttachment::RemoveMenus();
#ifdef EDITOR
	CToolsAttachment::RemoveMenus();
	CRecentEditMenuAttachment::RemoveMenus();
#endif // EDITOR
#ifdef MOZ_MAIL_NEWS
	CMailFolderSubmenu::RemoveMailFolderSubmenus();
#endif // MOZ_MAIL_NEWS
	if (mCurrentMenuBarID == MBAR_Initial)
		currentMenuBar->RemoveMenu(historyMenu);
	delete (currentMenuBar);
	DisposeSharedMenus();
	// create new menubar
	new LMenuBar( inMenuBarID );
	// now put go and window menus into new menubar
	currentMenuBar = LMenuBar::GetCurrentMenuBar();
	if (inMenuBarID == MBAR_Initial)
		currentMenuBar->InstallMenu(historyMenu, InstallMenu_AtEnd);
#ifdef EDITOR
	// put the tools menu in the editor menubar only
	if ((inMenuBarID == cEditorMenubar) || (inMenuBarID == cComposeMenubar))
	{
		CFontMenuAttachment::InstallMenus();
		CToolsAttachment::InstallMenus();
		CRecentEditMenuAttachment::InstallMenus();
	}
#endif // EDITOR
	if ((inMenuBarID != cBookmarksMenubar) && (inMenuBarID != cMinimalMenubar)) // don't put bookmarks menu in minimal menubar - mjc
		CBookmarksAttachment::InstallMenus();
#ifdef MOZ_MAIL_NEWS
	if ( inMenuBarID == cMailNewsMenubar ) {
		CMailFolderSubmenu::InstallMailFolderSubmenus();
	}
#endif // MOZ_MAIL_NEWS
	if (inMenuBarID != cMinimalMenubar) // don't put window menu in minimal menubar - mjc
		currentMenuBar->InstallMenu(windowMenu, InstallMenu_AtEnd);
	// cache current menubar MBAR ID
	mCurrentMenuBarID = inMenuBarID;
}
