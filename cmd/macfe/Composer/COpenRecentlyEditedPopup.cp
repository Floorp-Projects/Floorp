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

// ===========================================================================
//	COpenRecentlyEditedPopup.cp
// ===========================================================================

#include "COpenRecentlyEditedPopup.h"
		
#include "CWindowMediator.h"
#include "CBrowserContext.h"
#include "CEditorWindow.h"
#include "UMenuUtils.h"
#include "PascalString.h"

#include "net.h"		// NET_CreateURLStruct
#include "structs.h"	// TagType which is needed in edt.h
#include "edt.h"

		
// ---------------------------------------------------------------------------
//		¥ CreateNavigationButtonPopupStream [static]
// ---------------------------------------------------------------------------

COpenRecentlyEditedPopup*
COpenRecentlyEditedPopup::CreateOpenRecentlyEditedPopupStream( LStream* inStream )
{
	return new COpenRecentlyEditedPopup(inStream);
}
		
// ---------------------------------------------------------------------------
//		¥ COpenRecentlyEditedPopup
// ---------------------------------------------------------------------------

COpenRecentlyEditedPopup::COpenRecentlyEditedPopup( LStream* inStream )
	:	mBrowserContext(nil),
		super(inStream)
{
}

// ---------------------------------------------------------------------------
//		¥ ~COpenRecentlyEditedPopup
// ---------------------------------------------------------------------------

COpenRecentlyEditedPopup::~COpenRecentlyEditedPopup()
{
}
	
#pragma mark -

// ---------------------------------------------------------------------------
//		¥ AdjustMenuContents
// ---------------------------------------------------------------------------

void
COpenRecentlyEditedPopup::AdjustMenuContents()
{
	if (!GetMenu() || !GetMenu()->GetMacMenuH())
		return;
	
	if (!AssertPreconditions())
		return;
		
	// Purge the menu
	UMenuUtils::PurgeMenuItems(GetMenu()->GetMacMenuH(), PERM_OPEN_ITEMS);
	
	// Fill the menu
	int i;
	char *urlp = NULL, *titlep = NULL;
	for ( i = 0; i < MAX_EDIT_HISTORY_LOCATIONS 
				&& EDT_GetEditHistory( ((MWContext *)(*mBrowserContext)), i, &urlp, &titlep ); i++ )
	{
		// strange logic:  if we have no URL (then how do we go there???) then use title if it exists
		if ( urlp == NULL )
			urlp = titlep;
		
		if ( urlp )
		{
			NET_UnEscape( urlp );
			InsertItemIntoMenu( urlp, PERM_OPEN_ITEMS + i );
		}
		else
			break;
	}
	
	// delete menu break line if we don't have any history items
	if ( i == 0 )
	{
		::DeleteMenuItem( GetMenu()->GetMacMenuH(), PERM_OPEN_ITEMS );
	}

	// Set the min/max values of the control since we populated the menu
	SetPopupMinMaxValues();
}

// ---------------------------------------------------------------------------
//		¥ InsertItemIntoMenu
// ---------------------------------------------------------------------------

void
COpenRecentlyEditedPopup::InsertItemIntoMenu( char *urlp, Int16 inAfterItem )
{
	Assert_(GetMenu() && GetMenu()->GetMacMenuH());
	Assert_(mBrowserContext);
	CStr255 thePString( urlp );

	// Insert a "blank" item first...
	::InsertMenuItem( GetMenu()->GetMacMenuH(), "\p ", inAfterItem + 1 );
	
	// Then change it. We do this so that no interpretation of metacharacters will occur.
	::SetMenuItemText( GetMenu()->GetMacMenuH(), inAfterItem + 1, thePString );
}
	
#pragma mark -

// ---------------------------------------------------------------------------
//		¥ HandleNewValue
// ---------------------------------------------------------------------------

Boolean
COpenRecentlyEditedPopup::HandleNewValue( Int32 inNewValue )
{
	if ( inNewValue >= 1 && inNewValue < PERM_OPEN_ITEMS )
	{
		// someone else will handle this
		return false;
	}
	
	if ( AssertPreconditions() && inNewValue )
	{
		MWContext *cntxt = ((MWContext *)(*mBrowserContext));
		if ( cntxt )
		{
			char *aURLtoOpen = NULL;
			// EDT_GetEditHistory is 0-based so deduct 1 from 2nd parameter
			if ( EDT_GetEditHistory( cntxt, inNewValue - PERM_OPEN_ITEMS - 1, &aURLtoOpen, NULL) )
			{
				URL_Struct* theURL = NET_CreateURLStruct( aURLtoOpen, NET_NORMAL_RELOAD );
				if ( theURL )
					CEditorWindow::MakeEditWindow( NULL, theURL );
			}
		}
	}
	
	return true;
}

// ---------------------------------------------------------------------------
//		¥ AssertPreconditions
// ---------------------------------------------------------------------------
//	Assert preconditions and fill in interesting member data

Boolean
COpenRecentlyEditedPopup::AssertPreconditions()
{	
	CMediatedWindow* topWindow = CWindowMediator::GetWindowMediator()->FetchTopWindow( WindowType_Any, regularLayerType );
	
	if (!topWindow || topWindow->GetWindowType() != WindowType_Editor)
		return false;
	
	CEditorWindow* composerWindow = dynamic_cast<CEditorWindow*>(topWindow);
	if ( !composerWindow )
		return false;
	
	if ( !(mBrowserContext = (CBrowserContext*)composerWindow->GetWindowContext()) )
		return false;
	
	return true;
}