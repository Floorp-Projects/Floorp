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

#include "CToolsAttachment.h"

#include "CEditView.h"
#include "CBrowserContext.h"		// operator MWContext*()
#include "resgui.h"					// TOOLS_MENU_BASE_LAST
#include "macutil.h"				// CMediatedWindow
#include "UMenuUtils.h"
#include "edt.h"
//#include "CSpellChecker.h"
#include <LArray.h>


#define PERM_TOOLS_ITEMS 2
#define PERM_TOOLS_END_ITEMS 1

const CommandT cmd_ToolsHierItem = TOOLS_MENU_BASE_LAST;

LMenu *CToolsAttachment::sMenu = NULL;
Boolean CToolsAttachment::sInvalidMenu = true;
LArray CToolsAttachment::sMenusList;

//===========================================================
// CToolsAttachment
//===========================================================
CToolsAttachment::CToolsAttachment()
{
}


MWContext *CToolsAttachment::GetTopWindowContext()
{
	// Ok, ok. I know this is skanky,
	// but there is no common way to get the context from a window: it depends on the window type.
	// So, we look around for a CEditView somewhere in the top window.
	// A CEditView we understand (and get an MWContext from).
	
	CMediatedWindow* topWin = NULL;		// find the top window to use the plugin in
	CWindowIterator iter(WindowType_Any);
//	do {
		iter.Next(topWin);
//	} while (topWin && topWin->GetWindowType() != WindowType_Editor && topWin->GetWindowType() != WindowType_Compose);
				
	if (topWin == NULL)
		return NULL;
	
	if ( topWin->GetWindowType() != WindowType_Editor && topWin->GetWindowType() != WindowType_Compose )
		return NULL;
	
	CEditView *editView = (CEditView *)(topWin->FindPaneByID(CEditView::pane_ID));
		
	if (editView == NULL || editView->GetNSContext() == NULL)
		return NULL;
		
	return editView->GetNSContext()->operator MWContext*();
}


// Processes:
void CToolsAttachment::ExecuteSelf( MessageT inMessage, void* ioParam )
{
	mExecuteHost = FALSE;
	switch ( inMessage )	
	{
//		case cmd_CheckSpelling:		// spell checker
//			return;
		
		case cmd_EditorPluginStop:
			MWContext *cntxt = GetTopWindowContext();
			if (cntxt)
				EDT_StopPlugin(cntxt);
			return;
		
		case msg_CommandStatus:
		{
			SCommandStatus*		status = (SCommandStatus*)ioParam;
				
			switch ( status->command )
			{
				case cmd_EditorPluginStop:
					MWContext *cntxt = GetTopWindowContext();
					*(status->enabled) = cntxt && EDT_IsPluginActive(cntxt);
					*(status->usesMark) = FALSE;
					return;
					
				default:
					if ( status->command >= TOOLS_MENU_BASE && status->command <= TOOLS_MENU_BASE_LAST )
					{
					 	*(status->enabled) = TRUE;
					 	*(status->usesMark) = FALSE;
						return;
					}
				break;
			}
		}
		break;
		
		default:
		{
			if ( inMessage >= TOOLS_MENU_BASE && inMessage <= TOOLS_MENU_BASE_LAST )
			{
				int32 index = inMessage - TOOLS_MENU_BASE;
				
				for (int32 CategoryIndex = 0; CategoryIndex < EDT_NumberOfPluginCategories(); CategoryIndex++)
					for (int32 PluginIndex = 0; PluginIndex < EDT_NumberOfPlugins(CategoryIndex); PluginIndex++)
						if (index-- == 0)
						{		// count down until we find which one...
							MWContext *cntxt = GetTopWindowContext();
							if (cntxt)
								EDT_PerformPlugin(cntxt, CategoryIndex, PluginIndex, 0, 0);		// what is the result for?

							return;
						}
			}
		}
		break;			
	}
	mExecuteHost = TRUE;	// Let application handle it
}


LMenu *CToolsAttachment::GetMenu()
{
	if (!sMenu)
		sMenu = new LMenu(cToolsMenuID);
	
	return sMenu;
}

void CToolsAttachment::UpdateMenu()
{
	if (!sInvalidMenu || !GetMenu() || !LMenuBar::GetCurrentMenuBar())
		return;
	
	int i;
	
	// ¥ delete all the dynamically created menus
	// ¥Êdelete all the hierarchical menus we have added from the menubar
	for ( i = 1; i <= sMenusList.GetCount(); i++ )
	{
		LMenu* m;
		sMenusList.FetchItemAt( i, &m );
		if ( m )
			LMenuBar::GetCurrentMenuBar()->RemoveMenu( m );
		delete m;
	}

	// ¥ delete all the menu items after the line in Tools menu
	MenuHandle menu = sMenu->GetMacMenuH();
	if ( menu )
	{
		int howMany = ::CountMItems( menu );
		for ( i = howMany - PERM_TOOLS_END_ITEMS; i > PERM_TOOLS_ITEMS; i-- )
			sMenu->RemoveItem( i );
	}
	sMenusList.RemoveItemsAt( sMenusList.GetCount(), 1 );

	int	whichItem = PERM_TOOLS_ITEMS;
	int	commandNum = TOOLS_MENU_BASE;
	int nextMenuID = cEditorPluginsFirstHierMenuID;
	
	Try_
	{
		ThrowIfNil_( sMenu );
		MenuHandle		mHand = sMenu->GetMacMenuH();
	
		ThrowIfNil_( mHand );
	
		for (int32 CategoryIndex = 0; CategoryIndex < EDT_NumberOfPluginCategories(); CategoryIndex++) {
		
			CStr255 headerName( EDT_GetPluginCategoryName( CategoryIndex ) );
			CreateMenuString( headerName );						// make sure it isn't too long

			whichItem = UMenuUtils::InsertMenuItem(mHand, headerName, whichItem); // returns actual insert loc
			sMenu->SetCommand(whichItem, cmd_ToolsHierItem);
				
			// ¥ Are there actually any menu items to put on this Hierarchical menu?
			if (EDT_NumberOfPlugins(CategoryIndex)) {
			
				// ¥ do we have any hierarchical menus left? 
				if (nextMenuID <= cEditorPluginsLastHierMenuID) {
				
					LMenu* subMenu = (LMenuBar::GetCurrentMenuBar())->FetchMenu( nextMenuID );
					if ( !subMenu )
					{
						StringHandle menuStringH = GetString( NEW_RESID );
						Assert_(menuStringH);
						if (menuStringH)
						{
							StHandleLocker locker((Handle)menuStringH);
							subMenu = new LMenu( nextMenuID,
												(unsigned char *)*menuStringH );
							LMenuBar::GetCurrentMenuBar()->InstallMenu( subMenu, hierMenu );
						}
					}
					else
						SysBeep( 1 );

					nextMenuID++;
					
					if ( subMenu )
					{
						sMenusList.InsertItemsAt( 1, sMenusList.GetCount(),  &subMenu );
						// ¥Êmake item hierarchical
						::SetItemCmd( mHand, whichItem, hMenuCmd );
						::SetItemMark( mHand, whichItem, subMenu->GetMenuID() );
						FillMenu(
								CategoryIndex,
								subMenu,
								commandNum,
								0 );
					}
					
				} else {
					
					// ¥ There are no hierarchical menus left,
					// so we will just add these onto the bottom of the main tools menu.
					// We have already put the (disabled) category name in the main tools menu
					
					FillMenu(
							CategoryIndex,
							sMenu,
							commandNum,
							whichItem );
							
					whichItem += EDT_NumberOfPlugins(CategoryIndex);
				}
				
			}
			
		}
		
		// this is a hack. The menu item "Stop Active Plug-in" gets pushed around and loses its command. So, reset it.
		sMenu->SetCommand(++whichItem, cmd_EditorPluginStop);

	}
	Catch_( inErr )
	{
	}
	EndCatch_


	sInvalidMenu = true;
}

void CToolsAttachment::FillMenu(
	int32 CategoryIndex,
	LMenu* newMenu,
	int& commandNum,		// next menu to create
	int whichItem )			// id of the first item to insert
{
	Try_
	{
		ThrowIfNil_( newMenu );
		MenuHandle		mHand = newMenu->GetMacMenuH();
	
		ThrowIfNil_( mHand );
	
		for (int32 PluginIndex = 0; PluginIndex < EDT_NumberOfPlugins(CategoryIndex); PluginIndex++) {

		 	// ¥ should really convert this to sMenu chars
			CStr255 pluginName( EDT_GetPluginName( CategoryIndex, PluginIndex) );
			CreateMenuString( pluginName );

			whichItem = UMenuUtils::InsertMenuItem(mHand, pluginName, whichItem); // returns actual insert loc
			newMenu->SetCommand(whichItem, commandNum++);
			
		}
	}
	Catch_( inErr )
	{
	}
	EndCatch_
}

void CToolsAttachment::RemoveMenus()
{
	if (sMenu)
	{
		LMenuBar *currentMenuBar = LMenuBar::GetCurrentMenuBar();
		
		if (currentMenuBar)
		{
			currentMenuBar->RemoveMenu(sMenu);
			
			for (ArrayIndexT index = 1; index <= sMenusList.GetCount(); ++index)
			{
				LMenu *menu;
				sMenusList.FetchItemAt(index, &menu);
				
				if (menu)
					currentMenuBar->RemoveMenu(menu);
			}
		}
	}
}

void CToolsAttachment::InstallMenus()
{
	if (sMenu)
	{
		LMenuBar *currentMenuBar = LMenuBar::GetCurrentMenuBar();
		
		if (currentMenuBar)
		{
			for (ArrayIndexT index = sMenusList.GetCount(); index > 0; --index)
			{
				LMenu *menu;
				sMenusList.FetchItemAt(index, &menu);
				
				if (menu)
				{
					StValueChanger<EDebugAction> okayToFail(gDebugThrow, debugAction_Nothing);
					currentMenuBar->InstallMenu(menu, hierMenu);
				}
			}
			StValueChanger<EDebugAction> okayToFail(gDebugThrow, debugAction_Nothing);
			currentMenuBar->InstallMenu(sMenu, InstallMenu_AtEnd);
		}
	}
}
