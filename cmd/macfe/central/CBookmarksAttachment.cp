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

//	Handle creating and maintaining the top-level bookmarks menu. It pulls the info
//	out of the RDF container the user designates as their "quickfile menu" and listens
//	to the messages from RDF to update it.

#include "CBookmarksAttachment.h"

#include "htrdf.h"
#include "CNetscapeWindow.h"

#include "net.h"
#include "resgui.h"
#include "uapp.h"
#include "macutil.h"

#include "UMenuUtils.h"

#include <Icons.h>
#include <Sound.h>

#define PERM_BOOKMARK_ITEMS 4

const CommandT cmd_BookmarkHierItem = BOOKMARKS_MENU_BASE_LAST;

LMenu *CBookmarksAttachment::sMenu = NULL;
Boolean CBookmarksAttachment::sInvalidMenu = true;
LArray CBookmarksAttachment::sMenusList; // this will use the default constructor

HT_View CBookmarksAttachment::sQuickfileView = NULL;



//===========================================================
// CBookmarksAttachment
//===========================================================
CBookmarksAttachment::CBookmarksAttachment()
{
	InitQuickfileView();
}


//
// InitQuickfileView
//
// Called at startup to get a new view from the HT backend that represents the
// bookmarks menu. This can be called multiple times without problems.
//
void
CBookmarksAttachment :: InitQuickfileView ( )
{
	if ( ! sQuickfileView ) {
		HT_Notification notifyStruct = CreateNotificationStruct();
		HT_Pane quickfilePane = HT_NewQuickFilePane(notifyStruct);
		
		sQuickfileView = HT_GetSelectedView(quickfilePane);
	}
	
} // InitQuickfileView


void
CBookmarksAttachment :: HandleNotification( HT_Notification /* notifyStruct*/,
												HT_Resource node, HT_Event event)
{
	switch (event) {
	
		case HT_EVENT_NODE_ADDED:
		case HT_EVENT_VIEW_REFRESH:
			// only update menu if the quickfile view changes
			if ( HT_GetView(node) == sQuickfileView ) {			
				sInvalidMenu = true;
				UpdateMenu();
			}
			break;
			
		case HT_EVENT_NODE_VPROP_CHANGED:
			// optimization: only update when the name column changes
			break;
			
		case HT_EVENT_NODE_DELETED_DATA:
		case HT_EVENT_NODE_DELETED_NODATA:
			// free FE data, but don't update the menu yet (HT not in good state)
			break;
			
	} // case of which event

} // HandleNotification


// Processes:
// 
void CBookmarksAttachment::ExecuteSelf( MessageT inMessage, void* ioParam )
{
	mExecuteHost = FALSE;

	switch ( inMessage )	
	{
		CNetscapeWindow *bookmarkableWindow = nil;
		
		case msg_CommandStatus:
		{
			SCommandStatus*		status = (SCommandStatus*)ioParam;
				
			switch ( status->command )
			{

				default:
					if (CFrontApp::GetApplication()->HasBookmarksMenu())
					{
						if ( status->command >= BOOKMARKS_MENU_BASE && status->command <= BOOKMARKS_MENU_BASE_LAST )
						{
						 	*(status->enabled) = TRUE;
						 	*(status->usesMark) = FALSE;
							return;
						}
					}
				break;
			}
		}
		break;
		
		default:
		{
			if (CFrontApp::GetApplication()->HasBookmarksMenu())
			{
				if ( inMessage >= BOOKMARKS_MENU_BASE && inMessage <= BOOKMARKS_MENU_BASE_LAST )
				{
					Uint32 index = inMessage - BOOKMARKS_MENU_BASE;
					char* url = HT_GetNodeURL( HT_GetNthItem(sQuickfileView, index) );
					CFrontApp::DoGetURL ( url );

					return;
				}
			}
		}
		break;			
	}
	mExecuteHost = TRUE;	// Let application handle it
}


void CBookmarksAttachment::AddToBookmarks( const char* url, const CStr255& title )
{
	HT_Resource topNode = HT_TopNode ( sQuickfileView );
	HT_AddBookmark ( const_cast<char*>(url), title );
}


LMenu *CBookmarksAttachment::GetMenu()
{
	if (!sMenu)
	{
		sMenu = new LMenu(cBookmarksMenuID);

		if (sMenu)
		{
			MenuHandle macMenu = sMenu->GetMacMenuH();
			
			if (macMenu)
				UMenuUtils::ConvertToIconMenu(macMenu, 15312);
		}
	}
	return sMenu;
}

void CBookmarksAttachment::UpdateMenu()
{
	if (CFrontApp::GetApplication()->HasBookmarksMenu())
	{
		if (!sInvalidMenu || !GetMenu() || !LMenuBar::GetCurrentMenuBar() )
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

		// ¥ delete all the menu items after the line in Bookmark menu
		MenuHandle menu = sMenu->GetMacMenuH();
		if ( menu )
		{
			int howMany = ::CountMItems( menu );
			for ( i = howMany; i > PERM_BOOKMARK_ITEMS; i-- )
				sMenu->RemoveItem( i );
		}
		sMenusList.RemoveItemsAt( sMenusList.GetCount(), 1 );

		// ¥ walk through the list, and let the submenus be inserted recursively
		int nextMenuID = cBookmarksFirstHierMenuID;
		FillMenuFromList( HT_TopNode(sQuickfileView), sMenu, nextMenuID, PERM_BOOKMARK_ITEMS );
		
		sInvalidMenu = false;
	}
}

// ¥Êrecursively create submenus, given a list ptr
//		returns NULL if the menu cannot be created
//		it creates submenus recursively
void CBookmarksAttachment::FillMenuFromList(
	HT_Resource top, 
	LMenu* newMenu,
	int& nextMenuID,		// next menu to create
	int whichItem )			// id of the first item to insert
{
	if (CFrontApp::GetApplication()->HasBookmarksMenu())
	{
		Try_
		{
			ThrowIfNil_( newMenu );
			MenuHandle mHand = newMenu->GetMacMenuH();
		
			ThrowIfNil_( mHand );
			
			// ¥Êremove all the extra items if they exist
			long removeThese = ::CountMItems( mHand ) - whichItem;
			for ( long i = 1; i < removeThese; i++ )
				newMenu->RemoveItem( whichItem );
		
			MenuHandle theMacMenu = newMenu->GetMacMenuH();
			
			// Open up the container and get an iterator on its contents (we have to open it before
			// we can see anything inside it). If the cursor is null, it is probably because the container
			// is locked so just put up a leaf item (disabled, of course) and bail.
			HT_SetOpenState ( top, PR_TRUE );
			HT_Cursor cursor = HT_NewCursor( top );
			if ( !cursor ) {
				whichItem = UMenuUtils::InsertMenuItem(theMacMenu, "\pLocked", whichItem);
				newMenu->SetCommand(whichItem, 0);
				return;
			}				
			
			HT_Resource currNode = HT_GetNextItem(cursor);
			while ( currNode )
			{
				if ( HT_IsSeparator(currNode) )
					newMenu->InsertCommand( "\p-", cmd_Nothing, whichItem++ );
				else if ( ! HT_IsContainer(currNode) )
				{
				 	// ¥ should really convert this to menu chars
					CStr255 urlName( HT_GetNodeName(currNode) );
					CreateMenuString( urlName );

					whichItem = UMenuUtils::InsertMenuItem(theMacMenu, urlName, whichItem); // returns actual insert loc
					newMenu->SetCommand(whichItem, BOOKMARKS_MENU_BASE + HT_GetNodeIndex(sQuickfileView, currNode) );

				}
				else
				{
					CStr255 headerName( HT_GetNodeName(currNode) );
					CreateMenuString( headerName );

					whichItem = UMenuUtils::InsertMenuItem(theMacMenu, headerName, whichItem); // returns actual insert loc
					newMenu->SetCommand(whichItem, cmd_BookmarkHierItem);
					
					// ¥Êdo we have space to create more?
					if ( nextMenuID <= cBookmarksLastHierMenuID)
					{
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
						
						//	Skip the "Apple" menu or we're in deep donuts.
						if (nextMenuID == MENU_Apple)
							nextMenuID++;
		
						if ( subMenu )
						{
							sMenusList.InsertItemsAt( 1, LArray::index_Last,  &subMenu );
							// ¥Êmake item hierarchical
							::SetItemCmd( mHand, whichItem, hMenuCmd );
							::SetItemMark( mHand, whichItem, subMenu->GetMenuID() );
							if ( currNode )
								FillMenuFromList( currNode, subMenu, nextMenuID, 0 );
						}
					}
				}
				
				currNode = HT_GetNextItem ( cursor );
				
			} // while
		}
		Catch_( inErr )
		{
		}
		EndCatch_
	}
}

void CBookmarksAttachment::RemoveMenus()
{
	if (CFrontApp::GetApplication()->HasBookmarksMenu())
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
}

void CBookmarksAttachment::InstallMenus()
{
	if (CFrontApp::GetApplication()->HasBookmarksMenu())
	{
		if (GetMenu())
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
				
				LMenu *directoryMenu = currentMenuBar->FetchMenu(cDirectoryMenuID);
				
				if (directoryMenu)
				{
					CFrontApp::BuildConfigurableMenu( directoryMenu->GetMacMenuH(), "menu.places.item" );
					
					for (short index2 = CountMItems(directoryMenu->GetMacMenuH()); index2 > 0; --index2)
						directoryMenu->SetCommand(index2, DIR_MENU_BASE + index2 - 1);
				}
			}
		}
	}
}
