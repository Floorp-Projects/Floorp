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

#include "CFontMenuAttachment.h"
#include "CEditView.h"
#include "CBrowserContext.h"		// operator MWContext*()
#include "resgui.h"					// cmd_FormatViewerFont, cmd_FormatFixedFont, FONT_MENU_BASE
#include "macutil.h"				// CMediatedWindow
#include "edt.h"



LMenu *CFontMenuAttachment::sMenu = NULL;

//===========================================================
// CFontMenuAttachment
//===========================================================
CFontMenuAttachment::CFontMenuAttachment()
{
	UpdateMenu();
}

MWContext *CFontMenuAttachment::GetTopWindowContext()
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


// Processes:
// 
void CFontMenuAttachment::ExecuteSelf( MessageT inMessage, void* ioParam )
{
	mExecuteHost = FALSE;
	switch ( inMessage )	
	{
		case msg_CommandStatus:
		{
			SCommandStatus*	status = (SCommandStatus*)ioParam;

			EDT_CharacterData* better;
			
			if ( status->command == cmd_FormatViewerFont || status->command == cmd_FormatFixedFont 
				|| ( status->command >= FONT_MENU_BASE && status->command <= FONT_MENU_BASE_LAST ) )
			{
				*(status->enabled) = true;
				*(status->usesMark) = false;

				better = NULL;
				MWContext *cntxt = GetTopWindowContext();
				if ( cntxt )
					better = EDT_GetCharacterData( cntxt );
				if ( better == NULL )
				{
					*(status->enabled) = false;
					return;
				}
			}
			
			switch ( status->command )
			{
				case cmd_FormatViewerFont:
					*(status->usesMark) = ( ! ( better->values & TF_FIXED ) && !( better->values & TF_FONT_FACE ) );
					*(status->mark) = *(status->usesMark) ? checkMark : 0;
					*(status->usesMark) = true;
					
					EDT_FreeCharacterData(better);
					mExecuteHost = false;
				 	return;
					break;
				
				case cmd_FormatFixedFont:
					*(status->usesMark) = ( better->values & TF_FIXED ) != 0;
					*(status->mark) = *(status->usesMark) ? checkMark : 0;
					*(status->usesMark) = true;
					
					EDT_FreeCharacterData(better);
					mExecuteHost = false;
					return;
					break;
				
				default:
					if ( status->command >= FONT_MENU_BASE && status->command <= FONT_MENU_BASE_LAST )
					{
						// get font menu item
						Str255	fontItemString;
						fontItemString[0] = 0;
						MenuHandle menuh = GetMenu()->GetMacMenuH();
						::GetMenuItemText ( menuh, status->command - FONT_MENU_BASE + PERM_FONT_ITEMS + 1, fontItemString );
						p2cstr( fontItemString );

// in mixed situation the mask bit will be cleared
						*(status->usesMark) = ( better->values & TF_FONT_FACE && better->pFontFace && XP_STRLEN((char *)fontItemString) > 0 
												&& XP_STRSTR( better->pFontFace, (char *)fontItemString ) != NULL );
						*(status->mark) = *(status->usesMark) ? checkMark : 0;
						*(status->usesMark) = true;

						EDT_FreeCharacterData(better);
						mExecuteHost = false;
						return;
					}
			}
		}
		break;
		
		case cmd_FormatViewerFont:
			MWContext *cntxt2 = GetTopWindowContext();
			if ( cntxt2 )
				EDT_SetFontFace( cntxt2, NULL, 0, NULL );
			break;
		
		case cmd_FormatFixedFont:
			MWContext *cntxt3 = GetTopWindowContext();
			if ( cntxt3 )
				EDT_SetFontFace( cntxt3, NULL, 1, NULL );
			break;
		
		default:
		{
			if ( inMessage >= FONT_MENU_BASE && inMessage <= FONT_MENU_BASE_LAST )
			{
				MWContext *cntxt2 = GetTopWindowContext();
				if ( cntxt2 )
				{
					// get font menu item
					Str255	newFontItemString;
					newFontItemString[0] = 0;
					MenuHandle menuh = GetMenu()->GetMacMenuH();
					::GetMenuItemText ( menuh, inMessage - FONT_MENU_BASE + PERM_FONT_ITEMS + 1, newFontItemString );
					p2cstr( newFontItemString );
					
					EDT_SetFontFace( cntxt2, NULL, -1, (char *)newFontItemString );
					
					mExecuteHost = false;
					return;
 				}
			}
		}
		break;			
	}
	
	mExecuteHost = TRUE;	// Let application handle it
}

LMenu *CFontMenuAttachment::GetMenu()
{
	if (!sMenu)
		sMenu = new LMenu(cFontMenuID);
	
	return sMenu;
}

// build the font menu from the system
void CFontMenuAttachment::UpdateMenu()
{
	if (!GetMenu() || !LMenuBar::GetCurrentMenuBar())
		return;
	
	int i;
	
	// ¥ delete all the menu items after the separator line
	MenuHandle menu = sMenu->GetMacMenuH();
	if ( menu )
	{
		for ( i = ::CountMItems( menu ); i > PERM_FONT_ITEMS; i-- )
			sMenu->RemoveItem( i );
	}
	
	Try_
	{
		ThrowIfNil_( menu );
	
		// Add fonts to menu
		::InsertResMenu( menu, 'FONT', PERM_FONT_ITEMS );
		
		int	commandNum = FONT_MENU_BASE;
		int newHowMany = ::CountMItems( menu );
		for (i = PERM_FONT_ITEMS + 1; i <= newHowMany; i++ )
			sMenu->SetCommand( i, commandNum++ );
	}
	Catch_( inErr )
	{
	}
	EndCatch_
}


void CFontMenuAttachment::RemoveMenus()
{
	if (sMenu)
	{
		LMenuBar *currentMenuBar = LMenuBar::GetCurrentMenuBar();
		if (currentMenuBar)
			currentMenuBar->RemoveMenu(sMenu);
	}
}


void CFontMenuAttachment::InstallMenus()
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

#pragma mark -

CFontMenuPopup::CFontMenuPopup( LStream *inStream ) : CPatternButtonPopupText( inStream )
{
}

CFontMenuPopup::~CFontMenuPopup()
{
}

void CFontMenuPopup::FinishCreateSelf( void )
{
	CPatternButtonPopupText::FinishCreateSelf();

	int i;
	
	// ¥ delete all the menu items after the separator line
	LMenu *ppmenu = GetMenu();
	MenuHandle menuh = ppmenu ? ppmenu->GetMacMenuH() : NULL;
	if ( menuh )
	{
		for ( i = ::CountMItems( menuh ); i > CFontMenuAttachment::PERM_FONT_ITEMS; i-- )
			ppmenu->RemoveItem( i );
	}
	
	Try_
	{
		ThrowIfNil_( menuh );
	
		// Add fonts to menu
		::InsertResMenu( menuh, 'FONT', CFontMenuAttachment::PERM_FONT_ITEMS );
		
		int	commandNum = FONT_MENU_BASE;
		int newHowMany = ::CountMItems( menuh );
		for (i = CFontMenuAttachment::PERM_FONT_ITEMS + 1; i <= newHowMany; i++ )
			ppmenu->SetCommand( i, commandNum++ );
		
		SetMaxValue( newHowMany );
	}
	Catch_( inErr )
	{
	}
	EndCatch_
}
