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

//===========================================================
// CRecentEditMenuAttachment.cp
//===========================================================

#include "CRecentEditMenuAttachment.h"
#include "CEditView.h"
#include "CBrowserContext.h"				//	operator MWContext*()
#include "resgui.h"
#include "macutil.h"						//	CMediatedWindow
#include "UMenuUtils.h"
#include "edt.h"
#include "CEditorWindow.h"



LMenu *CRecentEditMenuAttachment::sMenu = NULL;

CRecentEditMenuAttachment::CRecentEditMenuAttachment()
{
	UpdateMenu();
}

MWContext *CRecentEditMenuAttachment::GetTopWindowContext()
{
	// Ok, ok. I know this is skanky,
	// but there is no common way to get the context from a window: it depends on the window type.
	// So, we look around for a CEditView somewhere in the top window.
	// A CEditView we understand (and get an MWContext from).
	
	CMediatedWindow* topWin = NULL;		// find the top window to use the plugin in
	CWindowIterator iter(WindowType_Any);
	iter.Next(topWin);
	
	if (topWin == NULL
	|| ! (topWin->GetWindowType() == WindowType_Editor || topWin->GetWindowType() == WindowType_Compose) )
		return NULL;
	
	CEditView *editView = (CEditView *)(topWin->FindPaneByID(CEditView::pane_ID));
		
	if (editView == NULL || editView->GetNSContext() == NULL)
		return NULL;
		
	return editView->GetNSContext()->operator MWContext*();
}


// Processes
void CRecentEditMenuAttachment::ExecuteSelf( MessageT inMessage, void* ioParam )
{
	switch ( inMessage )	
	{
		case msg_CommandStatus:
		{
			SCommandStatus*	status = (SCommandStatus*)ioParam;
			
			if ( status->command >= RECENT_EDIT_MENU_BASE && status->command <= RECENT_EDIT_MENU_BASE_LAST )
			{
				*(status->enabled) = true;
				mExecuteHost = false;
				return;
			}
		}
		break;
		
		default:
		{
			if ( inMessage >= RECENT_EDIT_MENU_BASE && inMessage <= RECENT_EDIT_MENU_BASE_LAST )
			{
				MWContext *cntxt2 = GetTopWindowContext();
				if ( cntxt2 )
				{
					char *aURLtoOpen = NULL;
        			if ( EDT_GetEditHistory( cntxt2, inMessage - RECENT_EDIT_MENU_BASE - 1, &aURLtoOpen, NULL) )
					{
						URL_Struct* theURL = NET_CreateURLStruct( aURLtoOpen, NET_NORMAL_RELOAD );
						if ( theURL )
							CEditorWindow::MakeEditWindow( NULL, theURL );

						mExecuteHost = false;
						return;
 					}
 				}
			}
		}
		break;			
	}
	
	mExecuteHost = true;	// Let application handle it
}


LMenu *CRecentEditMenuAttachment::GetMenu()
{
	if (!sMenu)
		sMenu = new LMenu( menu_ID );
	
	return sMenu;
}


// build the font menu from the system
void CRecentEditMenuAttachment::UpdateMenu()
{
	if (!GetMenu() || !LMenuBar::GetCurrentMenuBar())
		return;
	
	int i;
	
	// ¥ delete all the menu items after the separator line
	MenuHandle menu = sMenu->GetMacMenuH();
	if ( menu )
	{
		for ( i = ::CountMItems( menu ); i > 0; i-- )
			sMenu->RemoveItem( i );
	}
	
	Try_
	{
		ThrowIfNil_( menu );
	
		// Add recently edited URLs to menu
		
		int i;
		char *urlp = NULL, *titlep = NULL;
		
		for ( i = 0; i < MAX_EDIT_HISTORY_LOCATIONS 
					&& EDT_GetEditHistory( GetTopWindowContext(), i, &urlp, &titlep ); i++ )
		{
			NET_UnEscape( urlp );
			
			// convert string to pascal-string for menu
			CStr255 menuStr(urlp);
			if ( menuStr.IsEmpty() )
				menuStr = titlep;
			
			if ( !menuStr.IsEmpty() )
			{
				// Insert a "blank" item first...
				::InsertMenuItem( GetMenu()->GetMacMenuH(), "\p ", i );
	
				// Then change it. We do this so that no interpretation of metacharacters will occur.
				::SetMenuItemText( GetMenu()->GetMacMenuH(), i + 1, menuStr );
				
				// SetCommand for menu item
				sMenu->SetCommand( i, RECENT_EDIT_MENU_BASE + i );
				sMenu->SetUsed( true );
			}
			else
				break;
		}
	}
	Catch_( inErr )
	{
	}
	EndCatch_
}


void CRecentEditMenuAttachment::RemoveMenus()
{
	if (sMenu)
	{
		LMenuBar *currentMenuBar = LMenuBar::GetCurrentMenuBar();
		if (currentMenuBar)
			currentMenuBar->RemoveMenu(sMenu);
	}
}


void CRecentEditMenuAttachment::InstallMenus()
{
	if (sMenu)
	{
		LMenuBar *currentMenuBar = LMenuBar::GetCurrentMenuBar();
		if (currentMenuBar)
		{
			StValueChanger<EDebugAction> okayToFail(gDebugThrow, debugAction_Nothing);
			currentMenuBar->InstallMenu(sMenu, hierMenu);
			
			ResIDT resID;
			MenuHandle menuh;
			Int16 whichItem;
			currentMenuBar->FindMenuItem( cmd_ID_toSearchFor, resID, menuh, whichItem );
			if ( menuh )
			{
				// make it hierarchical
				::SetItemCmd( menuh, whichItem, hMenuCmd );
				::SetItemMark( menuh, whichItem, menu_ID );
			}
		}
	}
}
