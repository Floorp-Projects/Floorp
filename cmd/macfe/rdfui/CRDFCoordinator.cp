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
// It's responsible for coordinating the UI with the HT/RDF XP code. How does it
// do that? Let's take a look at the interactions.
//
// There are two kinds of messages that we need to respond to, and the goal is to
// keep them as separate as possible. The first kind are PowerPlant messages, sent
// via the broadcaster/listener architecture. These messages should denote that
// an event in the front end (eg, a user click) caused something to change and HT
// (along with the rest of the UI) needs to know about it. The second kind of messages
// come from HT itself, and are the result of a change within HT by calling HT functions
// directly (or possibly by javascript, etc).
//
// Let's say the shelf is closed, and no workspaces are selected. The user then clicks
// on one of the workspace icons. The CNavCenterSelectorPane figures out which HT view
// corresponds to this icon and updates its internal state. In doing this, a
// "active selection changed" message is broadcast to the coordinator. When this message
// is received by the coordinator (CRDFCoordinator::ListenToMessage()), it tells the tree 
// view to match the contents of the current HT view (CHyperTreeFlexTable::OpenView()). 
// The coordinator also tells HT about the new active workspace by calling HT_SetSelectedView(),
// but first it turns off HT notifications because we don't need to be told about this event
// again (it has already been handled by PP's messages in the FE). Control then returns to the
// selector pane which broadcasts a "shelf state changed" message to open the shelf. This 
// is again received by the coordinator which tells the Navcenter shelf to spring out. Note
// that this is done _after_ the tree view has been correctly set to avoid tons of asserts 
// in the flex table code.
//
// When the user clicks either the close box or that same workspace icon, a message is sent to
// the coordinator to close the shelf. The coordinator then tells the selector about the change
// by calling SetActiveWorkspace(NULL) and closes the shelf.
//
// There are several times when HT wants to set the active workspace on its own, the most notable
// occurs when a new window is created at startup. HT remembers the last active workspace
// when the user quits and will reset when the new window is created. It does this by making a
// call to HT_SetSelectedView() which sends an HT event to the coordinator (handled in 
// CRDFCoordinator::HandleNotification()). The first thing the coordinator does is to call
// SelectView() which opens the tree and ensures that the selector is up to date with the
// current selection. Then it sends messages to open the shelf (if need be) and to update
// the title in the title bar. Note that these last two things are done in different places
// when the change comes from the FE so we don't end up doing them over and over again when
// HT makes a change.
//
// My first stab at all this was to have one single flow of control that could handle view selection
// from both the user and HT in one line. While possible (the last implementation did it), it was
// very confusing. Hopefully this new world will make things somewhat easier to follow because
// the messages are more separated based on where the action that kicked it off came from.
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
#include "LIconControl.h"


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
	
	// Register the title bar as a listener to both this class and the selector bar. It will
	// receive messages from the selector when the _user_ changes the current workspace and 
	// will receive messages from this class when _HT_ changes the current workspace. Either
	// way it needs to know so it can update the title string.
	CNavCenterTitle* titleBar =
			dynamic_cast<CNavCenterTitle*>(FindPaneByID(CNavCenterTitle::pane_ID));
	if ( titleBar ) {
		AddListener(titleBar);
		if ( mSelectorPane )
			mSelectorPane->AddListener(titleBar);	
	}
		
	// If the close box is there, register this class as a listener so we get the
	// close message. It won't be there in the standalone window version		
	LIconControl* closeBox = dynamic_cast<LIconControl*>(FindPaneByID(CNavCenterTitle::kCloseBoxPaneID));
	if ( closeBox )
		closeBox->AddListener(this);
			
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
		}
			
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
				// HT_GetNodeIndex() will return the row where this new item should go
				// in a zero-based world. This translates, in a 1-based world of PP, to be
				// the node after which this new row will go. Conveniently, that is what
				// we want for InsertRows() so we don't have to add 1 like we normally do
				// when coverting between HT and PP.
				TableIndexT index = HT_GetNodeIndex(view, node);
				mTreePane->InsertRows(1, index, NULL, 0, true);
				mTreePane->SyncSelectionWithHT();
			}
			break;
		}

		case HT_EVENT_VIEW_SELECTED:
		{
			// if the current view is changed by HT (not interactively by the user), this
			// is the only place in the call chain where we will get notification. Make sure
			// the shelf is open/closed accordingly and the titlebar is updated.
			SelectView(view);
			
			bool openShelf = (view != NULL);
			ListenToMessage ( CNavCenterSelectorPane::msg_ShelfStateShouldChange, &openShelf );
			BroadcastMessage ( CNavCenterSelectorPane::msg_ActiveSelectorChanged, view );
			break;
		}
		
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

		//
		// we get this event when a new node is created and is to be put in "inline edit" mode.
		//
		case HT_EVENT_NODE_EDIT:
		{
			//¥¥¥ There are currently some problems with redraw here because of the way that
			// the drawing code and the inline editing code interact. You can uncomment the
			// line below and see that the cell does not draw correctly because it never gets
			// a drawCellContents() called on it (since it is the cell being edited). This
			// needs some work.....
			if ( view == mTreePane->GetHTView() ) {
				TableIndexT rowToEdit = URDFUtilities::HTRowToPPRow( HT_GetNodeIndex(view, node) );
//				mTreePane->DoInlineEditing(	rowToEdit );			
			}
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
			// update the sitemap icon if anything has changed
			if ( view == HT_GetViewType(mHTPane, HT_VIEW_SITEMAP) ) {
				//.....update the icon, but we don't have an icon or an API yet....
			}

			// only update if current view
			//
			// ¥¥¥ There are some fairly noticable redraw problems here. For example, when
			// nodes are deleted or added, the entire view is redrawn (and flashes). We
			// need some help from HT to prevent this.
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
// is up to date.
//
void CRDFCoordinator::SelectView(HT_View view)
{
	if ( view )
		mTreePane->OpenView(view);
	
	// find the appropriate workspace and make it active if it has not yet been set
	// (such as when HT sets it explicitly). We have to turn off listening to the selector 
	// pane to avoid infinite loops (changing the selector will send us a message that the 
	// active selector changed). This code should not be executed when the view change
	// is made by the FE.
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
		// the backend about it, but before we do that, turn off HT events so we don't actually
		// get the notification back -- we don't need it because the view change was caused
		// by the FE.
		case CNavCenterSelectorPane::msg_ActiveSelectorChanged:
		{
			HT_View newView = reinterpret_cast<HT_View>(ioParam);
			URDFUtilities::StHTEventMasking saveMask(mHTPane, HT_EVENT_NO_NOTIFICATION_MASK);
			HT_SetSelectedView(mHTPane, newView);
			SelectView(newView);
			break;
		}
		
		// expand/collapse the shelf to the state pointed to by |ioParam|. If we don't
		// switch the target, we run into the problem where we are still the active
		// commander and get called on to handle the menus. Since there will be no
		// view, HT will barf.
		case CNavCenterSelectorPane::msg_ShelfStateShouldChange:
			if ( mIsInChrome ) {
				bool nowOpen = *(reinterpret_cast<bool*>(ioParam));
				mNavCenter->SetShelfState ( nowOpen );
				if ( nowOpen ) {
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