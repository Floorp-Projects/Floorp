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
// Mike Pinkerton, Netscape Communications
//
// The CRDFCoordinator is the view that contains the
// "selector widget," object that selects the different RDF "views" stored
// in the RDF database, and the view that displays the "hyper tree"
// representation of the selected RDF view.
//
// It's responsible for coordinating the UI with the HT/RDF XP code.
//

#include "CRDFCoordinator.h"

#include "CHyperTreeFlexTable.h"
#include "CNavCenterTitle.h"


// XP headers
#include "xp_str.h"
#include "htrdf.h"
#include "xp_ncent.h"

// MacFE specific headers
#include "URDFUtilities.h"
#include "CNavCenterSelectorPane.h"
#include "Netscape_Constants.h"
#include "URobustCreateWindow.h"
#include "BookmarksDialogs.h"
#include "divview.h"
#include "LGAIconSuiteControl.h"


const char* CRDFCoordinator::Pref_EditWorkspace = "browser.editWorkspace";
const char* CRDFCoordinator::Pref_ShowNavCenterSelector = "browser.chrome.show_navcenter_selector";
const char* CRDFCoordinator::Pref_ShowNavCenterShelf = "browser.chrome.show_navcenter_shelf";

CRDFCoordinator::CRDFCoordinator(LStream* inStream)
:	LView(inStream),
	LDragAndDrop ( GetMacPort(), this ),
	mSelectorPane(NULL),
	mTreePane(NULL),
	mHTPane(NULL),
	mIsInChrome(false),
	mNavCenter(NULL),
	mSelector(NULL)
{
	*inStream >> mSelectorPaneID;
	*inStream >> mTreePaneID;
	
} // constructor


CRDFCoordinator::~CRDFCoordinator()
{
	// if we don't do this, the LCommander destructor will try to clear the selection and
	// of course, the HTPane won't be around anymore to update the selection....boom....
	SwitchTarget(nil);
	HT_SetNotificationMask ( mHTPane, HT_EVENT_NO_NOTIFICATION_MASK );
	
	delete mNavCenter;
	delete mSelector;
	
	UnregisterNavCenter();
	HT_DeletePane ( mHTPane );
	
} // destructor


void
CRDFCoordinator::FinishCreateSelf()
{
	mSelectorPane = dynamic_cast<CNavCenterSelectorPane*>(FindPaneByID(mSelectorPaneID));
	mTreePane = dynamic_cast<CHyperTreeFlexTable*>(FindPaneByID(mTreePaneID));

	Assert_((mSelectorPane != NULL) && (mTreePane != NULL));

	// initialize the navCenter shelf. If we are a standalone window, we won't have
	// a LDividedView so no expando-collapso stuff happens.
	LDividedView* navCenter = dynamic_cast<LDividedView*>(FindPaneByID('ExCt'));
	if ( navCenter ) {
		mIsInChrome = true;
		mSelectorPane->SetEmbedded(true);
		mNavCenter = new CShelf ( navCenter, Pref_ShowNavCenterShelf );
	}
	
	// initialize the navCenter selector shelf. Again, if we're standalone, there won't
	// be a LDividedView.
	LDividedView* navCenterSelector = dynamic_cast<LDividedView*>(FindPaneByID('ExSe'));
	if ( navCenterSelector )
		mSelector = new CShelf ( navCenterSelector, Pref_ShowNavCenterSelector );
	
	// setting view selection comes via CRDFNotificationHandler, so don't do it here.
	mHTPane = CreateHTPane();
	if (mHTPane)
	{
		if (mSelectorPane)
		{
			mSelectorPane->SetHTPane ( mHTPane );
			mSelectorPane->AddListener(this);
			// fill selector pane with list of RDF "views"
			Uint32 numViews = HT_GetViewListCount(mHTPane);
			for (Uint32 i = 0; i < numViews; i++)
			{
				HT_View view = HT_GetNthView(mHTPane, i);
				SelectorData* selector = new SelectorData(view);
				HT_SetViewFEData ( view, selector );
			}
			
			// register the title area as a listener of the selector so that it gets
			// notitfications to update the title of the selected view.
			CNavCenterTitle* title = 
				dynamic_cast<CNavCenterTitle*>(FindPaneByID(CNavCenterTitle::pane_ID));			
			if ( title )
				mSelectorPane->AddListener(title);		
		}

		// If the close box is there, register this class as a listener so we get the
		// close message. It won't be there in the standalone window version		
		LGAIconSuiteControl* closeBox = 
				dynamic_cast<LGAIconSuiteControl*>(FindPaneByID(CNavCenterTitle::kCloseBoxPaneID));
		if ( closeBox )
			closeBox->AddListener(this);
				
		// receive notifications from the tree view
		if (mTreePane)
			mTreePane->AddListener(this);

	} // if HT pane is valid
	
} // FinishCreateSelf


//
// SavePlace
//
// Pass through to the tree view so it can save the shelf width
//
void
CRDFCoordinator :: SavePlace ( LStream* outStreamData )
{
	if ( !outStreamData )
		return;
		
	if ( mIsInChrome )
		mNavCenter->GetShelf()->SavePlace(outStreamData);
	
} // SavePlace


//
// RestorePlace
//
// Pass through to the tree view so it can restore the shelf width
//
void
CRDFCoordinator :: RestorePlace ( LStream* inStreamData )
{
	if ( !inStreamData )	// if reading from new/empty prefs, the stream will be null
		return;
		
	if ( mIsInChrome )
		mNavCenter->GetShelf()->RestorePlace(inStreamData);
	
} // RestorePlace


//
// HandleNotification
//
// Process notification events from HT. Things like nodes being added or deleted, etc come
// here and then are farmed out to the appropriate subview.
//
void CRDFCoordinator::HandleNotification(
	HT_Notification	/*notifyStruct*/,
	HT_Resource		node,
	HT_Event		event)
{
	PRBool isOpen;
	HT_Error err;

	HT_View view = HT_GetView(node);		// everyone needs this, more or less
	
	switch (event)
	{
		case HT_EVENT_NODE_ADDED:
		{
			if ( view == mTreePane->GetHTView() ) { 
				TableIndexT index = HT_GetNodeIndex(view, node);
				mTreePane->InsertRows(1, index + 1, NULL, 0, true);
				mTreePane->SyncSelectionWithHT();
			}
			break;
		}

		case HT_EVENT_VIEW_SELECTED:
			SelectView(view);
			break;

		//
		// we get this event before the node opens/closes. This is useful for closing so
		// we can compute the number of visible children of the node being closed before
		// all that info goes away.
		//
		case HT_EVENT_NODE_OPENCLOSE_CHANGING:
			err = HT_GetOpenState(node, &isOpen);
			if (isOpen)
				CollapseNode(node);
			break;
		
		//
		// we get this event after the node has finished opening/closing.
		//
		case HT_EVENT_NODE_OPENCLOSE_CHANGED:
			err = HT_GetOpenState(node, &isOpen);
			if (isOpen)
				ExpandNode(node);
			break;
			
		case HT_EVENT_NODE_DELETED_DATA:
		case HT_EVENT_NODE_DELETED_NODATA:
		{
			// delete FE data if any is there....
			break;
		}
		
		case HT_EVENT_NODE_VPROP_CHANGED:
		{
			//¥¥¥optimization? only redraw the cell that changed
//			TableIndexT index = HT_GetNodeIndex(HT_GetView(node), node);
			mTreePane->Refresh();
			break;
		}
		
		case HT_EVENT_NODE_SELECTION_CHANGED:
			mTreePane->SyncSelectionWithHT();
			break;
			
		case HT_EVENT_VIEW_REFRESH:
		{
#if 0
			// update the sitemap icon if anything has changed
			if ( view == HT_GetViewType(mHTPane, HT_VIEW_SITEMAP) ) {
				
			}
#endif

			// only update if current view
			if ( view == mTreePane->GetHTView() ) { 
				uint32 newRowsInView = HT_GetItemListCount ( view );
				TableIndexT numRows, numCols;
				mTreePane->GetTableSize ( numRows, numCols );
			
				int32 delta = newRowsInView - numRows;
				if ( delta > 0 )
					mTreePane->InsertRows ( delta, 1 );
				else
					mTreePane->RemoveRows ( abs(delta), 1, false );
				mTreePane->SyncSelectionWithHT();
				mTreePane->Refresh();
			} // if refresh for current view
			break;
		}
		
		
		case HT_EVENT_VIEW_WORKSPACE_REFRESH:
			mSelectorPane->Refresh();
			break;
			
		case HT_EVENT_VIEW_ADDED:
		{
			//¥¥¥ adds new view at end because we don't have enough data from HT to
			//¥¥¥ do it right
			SelectorData* selector = new SelectorData(view);
			HT_SetViewFEData ( view, selector );
			mSelectorPane->Refresh();
			break;
		}
					
		case HT_EVENT_VIEW_DELETED:
		{
			SelectorData* sel = static_cast<SelectorData*>(HT_GetViewFEData(view));
			delete sel;
			break;
		}
		
	} // case of which HT event

} // HandleNotification


//
// SelectView
//
// Make the given view the current view and ensure that the selector widget 
// is up to date. Also make sure the shelf is in the appropriate state.
//
void CRDFCoordinator::SelectView(HT_View view)
{
	bool openShelf = (view != NULL);
	if ( openShelf )
		mTreePane->OpenView(view);
	
	// if the current view is changed by HT (not interactively by the user), this
	// is the only place in the call chain where we will get notification. Make sure
	// the shelf is open/closed accordingly. This may be redundant for the cases
	// where the user is the one changing things.
	//
	// As a side effect, HT can now open/close the shelf w/out the user doing anything. This
	// may be quite useful, or it may be annoying. Let's let the net decide.
	ListenToMessage ( CNavCenterSelectorPane::msg_ShelfStateShouldChange, &openShelf );

	// find the appropriate workspace and make it active. We have to turn off
	// listening to the selector pane to avoid infinite loops.
	if ( !mSelectorPane->GetActiveWorkspace() || mSelectorPane->GetActiveWorkspace() != view ) {
		StopListening();
		mSelectorPane->SetActiveWorkspace(view);
		StartListening();
	} // if no selection or current selection outdated
	
} // SelectView


//
// SelectView
//
// Make the given type of view the current view and (by calling the above routine)
// ensure that the selector widget is up to date. HT_SetSelectedView() will send
// us a notification event to open the given view, so we don't have to do it here.
//
void CRDFCoordinator::SelectView ( HT_ViewType inPane )
{
	HT_View view = HT_GetViewType ( mHTPane, inPane );
	HT_SetSelectedView ( mHTPane, view );

} // SelectView



void CRDFCoordinator::ExpandNode(HT_Resource node)
{
	mTreePane->ExpandNode(node);
}

void CRDFCoordinator::CollapseNode(HT_Resource node)
{
	mTreePane->CollapseNode(node);
}


//
// ListenToMessage
//
// Process the various messages we get from the FE, such as requests to open/close the tree shelf
// or change which workspace is currently selected.
void CRDFCoordinator::ListenToMessage(
	MessageT		inMessage,
	void			*ioParam)
{
	switch (inMessage) {
	
		// the user clicked in the selector pane to change the selected workspace. Tell
		// the backend about it. This will cause a message to be sent to us by HT to
		// update the tree with the new data (processed by HandleNotification()).
		case CNavCenterSelectorPane::msg_ActiveSelectorChanged:
			HT_View newView = reinterpret_cast<HT_View>(ioParam);
			HT_SetSelectedView(mHTPane, newView);
			break;
		
		// expand/collapse the shelf to the state pointed to by |ioParam|. If we don't
		// switch the target, we run into the problem where we are still the active
		// commander and get called on to handle the menus. Since there will be no
		// view, HT will barf.
		case CNavCenterSelectorPane::msg_ShelfStateShouldChange:
			if ( mIsInChrome ) {
				bool* nowOpen = reinterpret_cast<bool*>(ioParam);
				mNavCenter->SetShelfState ( *nowOpen );
				if ( *nowOpen ) {
					mTreePane->SetRightmostVisibleColumn(1);	//¥¥Êavoid annoying columns
					SwitchTarget(this);
				}
				else
					SwitchTarget(GetSuperCommander());
			}
			break;
		
		// similar to above, but can cut out the crap because we are closing things
		// down explicitly. Also make sure to tell the selector pane that nothing is 
		// active, which the above message cannot do because it is responding to the
		// code that just changed the workspace.
		case CNavCenterTitle::msg_CloseShelfNow:
			mNavCenter->SetShelfState ( false );
			mSelectorPane->SetActiveWorkspace ( NULL );
			SwitchTarget(GetSuperCommander());
			break;
			
	} // case of which message
	
} // ListenToMessage


//
// ObeyCommand
//
// Do what PowerPlant tells us. =)
//
Boolean
CRDFCoordinator :: ObeyCommand ( CommandT inCommand, void* ioParam )
{
	Boolean cmdHandled = false;

	if ( inCommand >= cmd_NavCenterBase && inCommand <= cmd_NavCenterCap ) {
		// ¥make sure nav center is open???
		HT_Error err = HT_DoMenuCmd ( mHTPane, (HT_MenuCmd)(inCommand - cmd_NavCenterBase) );
		Assert_( err == HT_NoErr );
		return true;
	}

	switch ( inCommand ) {
	
		//
		// handle commands that we have to share with other parts of the UI
		//
		case cmd_Cut:
			HT_DoMenuCmd(mHTPane, HT_CMD_CUT );
			cmdHandled = true;
			break;
		case cmd_Copy:
			HT_DoMenuCmd(mHTPane, HT_CMD_COPY );
			cmdHandled = true;
			break;
		case cmd_Paste:
			HT_DoMenuCmd(mHTPane, HT_CMD_PASTE );
			cmdHandled = true;
			break;
		case cmd_Clear:
			HT_DoMenuCmd(mHTPane, HT_CMD_DELETE_FILE );
			cmdHandled = true;
			break;

		case cmd_NCFind:
			URobustCreateWindow::CreateWindow ( CBookmarksFindDialog::res_ID, nil );
			cmdHandled = true;
			break;
		
		default:
			cmdHandled = LCommander::ObeyCommand ( inCommand, ioParam );
			break;
			
	} // case on command

	return cmdHandled;
	
} // ObeyCommand


//
// HandleKeyPress
//
// Handle changing the nav center view on cmd-tab
// 
Boolean
CRDFCoordinator :: HandleKeyPress(const EventRecord &inKeyEvent)
{
	char key = inKeyEvent.message & charCodeMask;
	if ( inKeyEvent.modifiers & cmdKey && key == kTabCharCode )
		mSelectorPane->CycleCurrentWorkspace();
	else
		return LCommander::HandleKeyPress(inKeyEvent);

	return true;
	
} // HandleKeyPress


//
// RegisterNavCenter
//
// Tell XP about this navCenter & context for site map stuff
//
void
CRDFCoordinator :: RegisterNavCenter ( MWContext* inContext )
{
	XP_RegisterNavCenter ( mHTPane, inContext );

} // RegisterNavCenter


//
// UnregisterNavCenter
//
// Tell XP this window is going away.
//
void
CRDFCoordinator :: UnregisterNavCenter ( )
{
	XP_UnregisterNavCenter ( mHTPane );	

} // UnregisterNavCenter