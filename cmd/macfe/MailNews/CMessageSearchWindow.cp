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



// CMessageSearchWindow.cp

#define DEBUGGER_ASSERTIONS

#include "CMessageSearchWindow.h"
#include "LGAPushButton.h"
#include "SearchHelpers.h"
#include "CMessageWindow.h"
#include "CMessageView.h"
#include "msgcom.h"
#include "CMailNewsContext.h"
#include "CSearchTableView.h"
#include "CMailFolderButtonPopup.h"
#include "libi18n.h"
#include "nc_exception.h"
#include "UIHelper.h"
#include "UMailSelection.h"

//-----------------------------------
CMessageSearchWindow::CMessageSearchWindow(LStream *inStream) :
	CSearchWindowBase(inStream, WindowType_SearchMailNews),
	mGoToFolderBtn( nil ), mMsgScopePopup( nil ),
	mFileMessagePopup( nil ), scopeInfo( nil )
//-----------------------------------
{
	SetPaneID( pane_ID );
	mNumBasicScopeMenuItems = 3;
}

//-----------------------------------
CMessageSearchWindow::~CMessageSearchWindow()
//-----------------------------------
{
}

//-----------------------------------
void CMessageSearchWindow::FinishCreateSelf()
//-----------------------------------
{
	// get controls
	FindUIItemPtr( this, paneID_MsgScopePopup, mMsgScopePopup );
	FindUIItemPtr( this, paneID_GoToFolder, mGoToFolderBtn );
	FindUIItemPtr( this, paneID_FilePopup, mFileMessagePopup );

	// initialize the move popup control 
	CMailFolderMixin::FolderChoices filePopupChoices
		= static_cast<CMailFolderMixin::FolderChoices>(CMailFolderMixin::eWantPOP + CMailFolderMixin::eWantIMAP);
	mFileMessagePopup->MSetFolderChoices( filePopupChoices );
	CMailFolderMixin::UpdateMailFolderMixinsNow( mFileMessagePopup );
	
	// add listeners
	mGoToFolderBtn->AddListener( this );
	mFileMessagePopup->AddListener( this );
	mMsgScopePopup->AddListener( this );
	
	// disable the selection controls until we get a result
	DisableSelectionControls();
	
	Inherited::FinishCreateSelf();
} 

//-----------------------------------
void CMessageSearchWindow::SetUpBeforeSelecting()
//	Set up the window just before it is selected and brought to the front of other
//	windows in the app.
//-----------------------------------
{	
	// Clear the selected search list
	mSearchFolders.RemoveItemsAt(LArray::index_Last, LArray::index_First);

	// Determine front window status
	CMailFlexTable*		frontSearchTable = nil;
	CMailNewsWindow* 	frontMailWindow = nil;
	GetDefaultSearchTable(frontMailWindow, frontSearchTable);

	//Figure out which folderinfo to use as the scope.
	MSG_FolderInfo* msgFolderInfo = nil;
	
	if (frontSearchTable)
	{
		// Get the xp message pane
		MSG_Pane* msgPane = frontSearchTable->GetMessagePane();
		// Set up search flags and selected folder list
		AssertFail_(msgPane != nil);
		MSG_PaneType paneType = MSG_GetPaneType(msgPane);
		switch ( paneType )
		{
			case MSG_FOLDERPANE:
				if (!frontSearchTable->IsValidRow(1))
					return;
				// If there's a single selection, make it the default.  Otherwise,
				// use the first row.
				CMailSelection theSelection;
				frontSearchTable->GetSelection(theSelection);
				AssertFail_(MSG_GetPaneType(theSelection.xpPane) == MSG_FOLDERPANE);
				AssertFail_(theSelection.xpPane == msgPane);
				MSG_ViewIndex defaultIndex = 0;
				if (theSelection.selectionSize == 1)
					defaultIndex = *theSelection.GetSelectionList();
				msgFolderInfo = ::MSG_GetFolderInfo(
					theSelection.xpPane,
					defaultIndex);
				break;
				
			case MSG_THREADPANE:
				msgFolderInfo = ::MSG_GetCurFolder(msgPane);
				AssertFail_(msgFolderInfo);
				break;
				
			default:
				AssertFail_(false);	// No other types accepted now!
				break;
		} // switch
	}
	else // message window case
	{
		CMessageWindow* messageWindow = dynamic_cast<CMessageWindow*>(frontMailWindow);
		if (messageWindow)
		{
			CMessageView* messageView = messageWindow->GetMessageView();
			if (messageView)
				msgFolderInfo = messageView->GetFolderInfo();
		}
	}
	// As a last resort, use the inbox
	if (!msgFolderInfo)
	{
		::MSG_GetFoldersWithFlag(
			CMailNewsContext::GetMailMaster(),
			MSG_FOLDER_FLAG_INBOX,
			&msgFolderInfo,
			1);
	}
	AssertFail_(msgFolderInfo);
	mMsgScopePopup->MSetSelectedFolder(msgFolderInfo, false);
	
	// Get the folder csid and set it
	SetWinCSID(INTL_DocToWinCharSetID( MSG_GetFolderCSID( msgFolderInfo ) ));

	mSearchFolders.InsertItemsAt(1, LArray::index_Last, &msgFolderInfo);
	mSearchManager.SetSearchScope(
		(MSG_ScopeAttribute)CSearchManager::cScopeMailSelectedItems,
		&mSearchFolders);
} // CMessageSearchWindow::SetUpBeforeSelecting

//-----------------------------------
void CMessageSearchWindow::MessageWindStop(Boolean inUserAborted)
//-----------------------------------
{
	if ( mSearchManager.IsSearching() )
	{
		Inherited::MessageWindStop(inUserAborted);
		
		// enable controls
		EnableSelectionControls();
		UpdatePort();
	}
}

//-----------------------------------
void CMessageSearchWindow::MessageWindSearch()
//-----------------------------------
{
	// Disable controls
	DisableSelectionControls();
	UpdatePort();
	
	Inherited::MessageWindSearch();
} // CMessageSearchWindow::MessageWindSearch()


//-----------------------------------
void CMessageSearchWindow::UpdateTableStatusDisplay()
//-----------------------------------
{
	AssertFail_(mResultsTable != nil);
	Inherited::UpdateTableStatusDisplay();

	XP_Bool enabled = false;
	TableIndexT numSelectedItems = mResultsTable->GetSelectedRowCount();
	if ( numSelectedItems > 0 )
	{
		CMailSelection	selection;
		mResultsTable->GetSelection(selection);
		enabled = MSG_GoToFolderStatus(mSearchManager.GetMsgPane(), 
					(MSG_ViewIndex*)selection.GetSelectionList(),
						selection.selectionSize);
	}
	
	if (enabled)
		EnableSelectionControls();
	else
		DisableSelectionControls();
} // CMessageSearchWindow::UpdateTableStatusDisplay()

//-----------------------------------
void CMessageSearchWindow::EnableSelectionControls()
// 
//	Enable selection controls
//-----------------------------------
{
	mGoToFolderBtn->Enable();
	mFileMessagePopup->Enable();
}

//-----------------------------------
void CMessageSearchWindow::DisableSelectionControls()
// 
//	Disable selection controls
//-----------------------------------
{
	mGoToFolderBtn->Disable();
	mFileMessagePopup->Disable();
}

//-----------------------------------
void CMessageSearchWindow::ListenToMessage(MessageT inMessage, void *ioParam)
//-----------------------------------
{
	switch (inMessage)
	{
		case msg_FilePopup:
			// make sure we have a result table
			AssertFail_(mResultsTable != nil);
			
			// get the selection from the table
			CMailSelection	moveSelection;
			mResultsTable->GetSelection( moveSelection );
						
			// get the destination
			MSG_FolderInfo* selectedDestination = ( (MSG_FolderInfo*) ioParam );
			
			// get the search pane
			MSG_Pane *searchPane = mSearchManager.GetMsgPane();

			// sanity check
			#ifdef DEBUG
			if( selectedDestination == nil || searchPane == nil )
				throw ;
			#endif
				
			try
			{
				// move the selected messages
				int status = MSG_MoveMessagesIntoFolder( searchPane, moveSelection.GetSelectionList(), 
					moveSelection.selectionSize, selectedDestination ); 
				
				// Currently the enum eSuccess is doubly defined in two different BE headers. So in order
				// to prevent possible conflicts I just check for the succes value ( 0 )
				if ( status != 0 ) 				
					throw mail_exception( eMoveMessageError );	
			}
			catch( mail_exception& error )
			{
				error.DisplaySimpleAlert();
			}						
			break;	
	
		case msg_GoToFolder:
			AssertFail_(mResultsTable != nil);
			CMailSelection	selection;
			mResultsTable->GetSelection(selection);
			for (MSG_ViewIndex index = 0; index < selection.selectionSize; index ++)
			{
				mResultsTable->FindOrCreateThreadWindowForMessage(
									selection.GetSelectionList()[index]);
			}

			{
				// HACK to support opening of more than one folder
				EventRecord ignoredEvent = {0};
				LPeriodical::DevoteTimeToIdlers(ignoredEvent);
			}

			for (MSG_ViewIndex index = 0; index < selection.selectionSize; index ++)
			{
				mResultsTable->ShowMessage(
									selection.GetSelectionList()[index],
										CSearchTableView::kAddToThreadWindowSelection);
			}
			break;

		case msg_MsgScope:
			// get folder info
			scopeInfo = ( ( MSG_FolderInfo*)  ioParam );
			if (mSearchFolders.GetCount() == 0)
				mSearchFolders.InsertItemsAt(1, 1, &scopeInfo);
			else
				mSearchFolders.AssignItemsAt(1, 1, &scopeInfo);
			
			// set the scope	
			mSearchManager.SetSearchScope( static_cast<MSG_ScopeAttribute>( CSearchManager::cScopeMailSelectedItems ),
				&mSearchFolders );
			
			SetWinCSID(INTL_DocToWinCharSetID(MSG_GetFolderCSID((MSG_FolderInfo *) ioParam)));
			break;

		default:
			Inherited::ListenToMessage(inMessage, ioParam);
			break;
	}
} // CMessageSearchWindow::ListenToMessage

MSG_ScopeAttribute	
CMessageSearchWindow::GetWindowScope() const
{
	return static_cast<MSG_ScopeAttribute>( CSearchManager::cScopeMailSelectedItems );
}
