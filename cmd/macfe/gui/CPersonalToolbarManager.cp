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

//
// CPersonalToolbarManager.cp
//
// Implementation of the CPersonalToolbarManager class which handles keeping all the
// personal toolbars in sync. This reads all its info from RDF, not the old BM UI.
//
// I REALLY dislike that most of this code is in Hungarian notation, but the Windows
// code that it originally came from was like that and instead of doing a slew of
// global replaces, I'll just complain about it.
//

#include "CPersonalToolbarManager.h"
#include "CPersonalToolbarTable.h"
#include "URDFUtilities.h"

#include "prefapi.h"

#pragma mark ее CPersonalToolbarManager
#pragma mark -- Public Methods


const char* CPersonalToolbarManager::kMaxButtonCharsPref = "browser.personal_toolbar_button.max_chars";
const char* CPersonalToolbarManager::kMinButtonCharsPref = "browser.personal_toolbar_button.min_chars";


//
// Constructor
//
// Setup. Assumes that preferences and RDF have been initialized fully by this point.
//
CPersonalToolbarManager :: CPersonalToolbarManager ( )
	: mUserButtonInfoArray ( sizeof(CUserButtonInfo*) )
{
	// setup our window into the RDF world and register this class as the one to be notified
	// when the personal toolbar changes.
	HT_Notification notifyStruct = CreateNotificationStruct();
	mToolbarView = HT_GetSelectedView ( HT_NewPersonalToolbarPane(notifyStruct) );
	Assert_(mToolbarView != NULL);
	mToolbarRoot = HT_TopNode ( mToolbarView );
	Assert_(mToolbarRoot != NULL);
	HT_SetOpenState ( mToolbarRoot, PR_TRUE );	// ensure container is open so we can see its contents
	
	// read in the maximum size we're allowing for personal toolbar items. If not
	// found in prefs, use our own
	int32 maxToolbarButtonChars, minToolbarButtonChars;
	if ( PREF_GetIntPref(kMaxButtonCharsPref, &maxToolbarButtonChars ) == PREF_ERROR )
		mMaxToolbarButtonChars = CPersonalToolbarManager::kMaxPersonalToolbarChars;
	else
		mMaxToolbarButtonChars = maxToolbarButtonChars;
	if ( PREF_GetIntPref(kMinButtonCharsPref, &minToolbarButtonChars ) == PREF_ERROR )
		mMinToolbarButtonChars = CPersonalToolbarManager::kMinPersonalToolbarChars;
	else
		mMinToolbarButtonChars = minToolbarButtonChars;

	InitializeButtonInfo();
	
} // constructor


//
// Destructor
//
// The toolbars in individual windows will delete themselves when the window dies and
// unregister themselves as listeners. We don't need to do anything along those
// lines here.
//
CPersonalToolbarManager :: ~CPersonalToolbarManager ( )
{
	//еее delete the contents of the button list
} // destructor


//
// RegisterNewToolbar
//
// This must be called for each new toolbar created in a browser window. It fills in
// the toolbar with the correct items and registers it as a listener to the messages
// we emit.
//
void
CPersonalToolbarManager :: RegisterNewToolbar ( CPersonalToolbarTable* inBar )
{
	AddListener ( inBar );
	BroadcastMessage ( k_PTToolbarChanged );
	
} // RegisterNewToolbar


//
// ToolbarChanged
//
// The user has just made some change to the toolbar, such as manipulating items in the nav center
// or changing the personal toolbar folder. Since these changes could be quite extensive, we
// just clear out the whole thing and start afresh. Tell all the toolbars that they have to
// clear themselves and grab the new information. Changes to the toolbar directly, such as by
// drag and drop, are handled elsewhere.
//
void 
CPersonalToolbarManager :: ToolbarChanged ( )
{
	RemoveToolbarButtons();			// out with the old...
	InitializeButtonInfo();			// ...in with the new
	BroadcastMessage ( k_PTToolbarChanged );

} // ToolbarHeaderChanged


//
// AddButton
//
// Given a pre-created HT item, add it to the personal toolbar folder before the
// given id and tell all the toolbars to update. This is called when the user
// drops an item from another RDF source on a personal toolbar
//
void 
CPersonalToolbarManager :: AddButton ( HT_Resource inBookmark, Uint32 inIndex )
{
	// Add this to RDF.
	if ( GetButtons().GetCount() ) {
		// If we get back a null resource then we must be trying to drop after
		// the last element, or there just plain aren't any in the toolbar. Re-fetch the last element
		// (using the correct index) and add the new bookmark AFTER instead of before or just add
		// it to the parent for the case of an empty toolbar
		PRBool before = PR_TRUE;
		HT_Resource dropOn = HT_GetNthItem( mToolbarView, URDFUtilities::PPRowToHTRow(inIndex) );
		if ( ! dropOn ) {
			dropOn = HT_GetNthItem( mToolbarView, URDFUtilities::PPRowToHTRow(inIndex) - 1 );
			before = PR_FALSE;
		}
		HT_DropHTRAtPos ( dropOn, inBookmark, before );
	} // if items in toolbar
	else
		HT_DropHTROn ( mToolbarRoot, inBookmark );
		
	// no need to tell toolbars to refresh because we will get an HT node added event and
	// refresh at that time.
	
} // Addbutton


//
// AddButton
//
// Given just a URL, add it to the personal toolbar folder before the
// given id and tell all the toolbars to update. This is called when the user
// drops an item from another RDF source on a personal toolbar
//
void 
CPersonalToolbarManager :: AddButton ( const string & inURL, const string & inTitle, Uint32 inIndex )
{
	// Add this to RDF.
	if ( GetButtons().GetCount() ) {
		// If we get back a null resource then we must be trying to drop after
		// the last element, or there just plain aren't any in the toolbar. Re-fetch the last element
		// (using the correct index) and add the new bookmark AFTER instead of before or just add
		// it to the parent for the case of an empty toolbar
		PRBool before = PR_TRUE;
		HT_Resource dropOn = HT_GetNthItem( mToolbarView, URDFUtilities::PPRowToHTRow(inIndex) );
		if ( ! dropOn ) {
			dropOn = HT_GetNthItem( mToolbarView, URDFUtilities::PPRowToHTRow(inIndex) - 1 );
			before = PR_FALSE;
		}
		HT_DropURLAndTitleAtPos ( dropOn, const_cast<char*>(inURL.c_str()), 
									const_cast<char*>(inTitle.c_str()), before );
	} // if items in toolbar
	else
		HT_DropURLAndTitleOn ( mToolbarRoot, const_cast<char*>(inURL.c_str()), 
									const_cast<char*>(inTitle.c_str()) );  
		
	// no need to tell toolbars to refresh because we will get an HT node added event and
	// refresh at that time.
	
} // Addbutton


//
// RemoveButton
//
// The user has explicitly removed a button from the toolbar in one of the windows (probably
// by dragging it to the trash). Tell all the other toolbars about it, remove it from our
// own bookkeeping list, then let RDF get rid of it.
//
void 
CPersonalToolbarManager :: RemoveButton ( Uint32 inIndex )
{
	CUserButtonInfo* deadBookmark;
	GetButtons().FetchItemAt ( inIndex, &deadBookmark );
	
	// remove it from RDF
	HT_Resource deadNode = HT_GetNthItem ( GetHTView(), URDFUtilities::PPRowToHTRow(inIndex) );
	HT_RemoveChild ( mToolbarRoot, deadNode );

	delete deadBookmark;

	// no need to tell toolbars to refresh because we will get an HT view update event and
	// refresh at that time.
	
} // RemoveButton


//
// RemoveToolbarButtons
//
// Clears out our own internal list of toolbar button information.
//
void 
CPersonalToolbarManager :: RemoveToolbarButtons (void)
{
	mUserButtonInfoArray.RemoveItemsAt(mUserButtonInfoArray.GetCount(), LArray::index_First);

} // RemoveToolbarButtons


#pragma mark -- Protected Utility Methods
///////////////////////////////////////////////////////////////////////////////////
//						CPersonalToolbarManager Helpers
///////////////////////////////////////////////////////////////////////////////////


//
// InitializeButtonInfo
//
// Create our list of information for each bookmark that belongs in the toolbar. This
// involves iterating over the RDF data in the folder pre-selected to be the personal
// toolbar folder, compiling some info about it, and adding to an internal list which
// will be used by each toolbar to build their tables.
//
// еееNOTE: this will put folders in the bar if they exist. Strange things may happen if the
// user clicks on them....
//
void 
CPersonalToolbarManager :: InitializeButtonInfo(void)
{
	HT_Cursor cursor = HT_NewCursor( mToolbarRoot );
	if ( cursor ) {
		HT_Resource currNode = HT_GetNextItem(cursor);
		while ( currNode ) {
			CUserButtonInfo *info = new CUserButtonInfo ( HT_GetNodeName(currNode), HT_GetNodeURL(currNode),
															0, 0, HT_IsContainer(currNode));
			mUserButtonInfoArray.InsertItemsAt(1, LArray::index_Last, &info);
			currNode = HT_GetNextItem(cursor);
		} // for each top-level node
	}
	
} // InitializeButtonInfo


//
// HandleNotification
//
// Called when the user makes a change to the personal toolbar folder from some other
// place (like the bookmarks view). Reset our internal list and tell our listeners
// that things are now different.
//
void
CPersonalToolbarManager  :: HandleNotification(
	HT_Notification	/*notifyStruct*/,
	HT_Resource		node,
	HT_Event		event)
{
	switch (event)
	{
		case HT_EVENT_NODE_ADDED:
		case HT_EVENT_VIEW_REFRESH:
		case HT_EVENT_NODE_VPROP_CHANGED:
		{
			// only update toolbar if the quickfile view changes
			if ( HT_GetView(node) == mToolbarView )						
				ToolbarChanged();
			break;
		}		
	}
}


#pragma mark ее class CUserButtonInfo

///////////////////////////////////////////////////////////////////////////////////
//									Class CUserButtonInfo
///////////////////////////////////////////////////////////////////////////////////

CUserButtonInfo::CUserButtonInfo( const string & pName, const string & pURL, Uint32 nBitmapID,
									 Uint32 nBitmapIndex, bool bIsFolder)
: mName(pName), mURL(pURL), mBitmapID(nBitmapID), mBitmapIndex(nBitmapIndex), 
		mIsResourceID(true), mIsFolder(bIsFolder)
{
}


CUserButtonInfo::~CUserButtonInfo()
{
	// string destructor takes care of everything...
}




