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



#define DEBUGGER_ASSERTIONS
#include "CSearchTableView.h"

#include "CApplicationEventAttachment.h"
#include "CSearchWindowBase.h"
#include "CMessageSearchWindow.h"
#include "LFlexTableGeometry.h"
#include "CTableKeyAttachment.h"
#include "CThreadWindow.h"
#include "CThreadView.h"
#include "CMessageWindow.h"
#include "CMessageView.h"
#include "UMessageLibrary.h"
#include "UMailSelection.h"
#include "UMailFolderMenus.h"
#include "UGraphicGizmos.h"

#include "UException.h"
#include "UIHelper.h"
#include "CBrowserContext.h"
#include "LSharable.h"
#include "CComposeAddressTableView.h"
#include "libi18n.h"
#include "LCommander.h"

#include "resgui.h"
#include "MailNewsgroupWindow_Defines.h"

#include "ntypes.h"

//-----------------------------------
CSearchTableView::~CSearchTableView()
//-----------------------------------
{
	SetMessagePane( NULL );
}

void	CSearchTableView::SetSearchManager(CSearchManager *inSearchManager)
{
	mSearchManager = inSearchManager;
	SetMessagePane( inSearchManager->GetMsgPane() );
}

//-----------------------------------
void CSearchTableView::DrawCellContents( const STableCell&	inCell,		const Rect&			inLocalRect)

//-----------------------------------
{
	// Get the text to display
	CStr255 			displayText;
	MSG_SearchAttribute	inAttribute = CSearchWindowBase::AttributeFromDataType(GetCellDataType(inCell));
	
	if ( !GetDisplayText(inCell.row - 1, inAttribute, displayText) )
		return;		// Error occurred getting the text
							   	  
	// Display the text
	
	Boolean cellIsSelected = CellIsSelected(inCell);

	Rect cellDrawFrame = inLocalRect;
	::InsetRect(&cellDrawFrame, 2, 0);	// For a nicer look between cells

	StSectClipRgnState saveSetClip(&cellDrawFrame);

	displayText = NET_UnEscape(displayText);
	if ( cellIsSelected  )
	{
	
		UGraphicGizmos::PlaceTextInRect((char *) &displayText[1], displayText[0], cellDrawFrame, teFlushLeft, teCenter, 
			&mTextFontInfo, true, truncMiddle);
	//	PlaceHilitedTextInRect((char *) &displayText[1], displayText[0], cellDrawFrame, teFlushLeft, teCenter, 
	//		&mTextFontInfo, true, truncMiddle);
	}
	else
		UGraphicGizmos::PlaceTextInRect((char *) &displayText[1], displayText[0], cellDrawFrame, teFlushLeft, teCenter, 
			&mTextFontInfo, true, truncMiddle);
}



/*======================================================================================
	Set up helpers for the table.
======================================================================================*/

//-----------------------------------
void CSearchTableView::SetUpTableHelpers()
//-----------------------------------
{
	SetTableGeometry( new LFlexTableGeometry(this, mTableHeader) );
	SetTableSelector( new LTableRowSelector(this) );

	// standard keyboard navigation.
	AddAttachment( new CTableKeyAttachment(this) );
	
}

//-----------------------------------
void CSearchTableView::ListenToMessage(
	MessageT	inCommand,
	void		*ioParam)
//-----------------------------------
{
	if (!ObeyCommand((CommandT)inCommand, ioParam)) 
		CMailFlexTable::ListenToMessage(inCommand, ioParam);
}

//-----------------------------------
Boolean 
CSearchTableView::ObeyCommand( CommandT inCommand, void* ioParam )
//-----------------------------------
{
	Boolean commandObeyed = false;
	
	switch( inCommand )
	{
		case msg_TabSelect:
			break;
		
		default:
			commandObeyed = Inherited::ObeyCommand( inCommand, ioParam );
			break;
	}
	
	return commandObeyed;
}

//-----------------------------------
// Delete selected messages either via menu command or
// key.
//
//-----------------------------------
void CSearchTableView::DeleteSelection()
//-----------------------------------
{
	// see if you got a selection
	CMailSelection selection;
	if ( GetSelection( selection ) )
	{
		const MSG_ViewIndex	*indices = selection.GetSelectionList();
		MSG_ViewIndex		numIndices = selection.selectionSize;
		MSG_Pane*			searchPane = GetMessagePane();
		
		// try closing all the open message windows
		MSG_ResultElement*	outElement;
		MSG_FolderInfo*		outFolder;
		MessageKey			outKey;
		
		const MSG_ViewIndex	*indexCounter = indices;
		for ( int i = 0; i < numIndices; i++, indexCounter++ )
		{
			if( GetMessageResultInfo( *indexCounter, outElement, outFolder, outKey ) )
				CMessageWindow::CloseAll( outKey );
		}
		
		// Get correct BE message. It can either be for mail or news messages, but not for both
		CMessageFolder	messageFolder( outFolder );
		UInt32			folderFlags = messageFolder.GetFolderFlags();
		MSG_CommandType cmd = MSG_CancelMessage; 				// news posting - cancel command
		
		if ( (folderFlags & MSG_FOLDER_FLAG_NEWSGROUP) == 0 )
			cmd = UMessageLibrary::GetMSGCommand( cmd_Clear );	// mail message - delete command

		// send command to the BE.  Copy the index list, because it gets clobbered by
		// RemoveRows().
		MSG_ViewIndex* copyOfIndices = new MSG_ViewIndex[numIndices];
		const MSG_ViewIndex* src = &indices[0];
		MSG_ViewIndex* dst = &copyOfIndices[0];
		for (int i = 0; i < numIndices; i++, src++, dst++)
			*dst = *src;
			
		// BE delete command
		MSG_Command( searchPane, cmd, copyOfIndices, numIndices );
		
		// delete the list
		delete [] copyOfIndices;
		
		SelectionChanged();
	}
}
				
//-----------------------------------
void CSearchTableView::OpenRow(TableIndexT inRow)
//-----------------------------------
{
	MSG_ViewIndex index = inRow - 1;
	ShowMessage(index, kInMessageWindow);
} 

//-----------------------------------
void CSearchTableView::FindOrCreateThreadWindowForMessage(MSG_ViewIndex inMsgIndex)
//-----------------------------------
{
	AssertFail_(mSearchManager != nil);
	Try_
	{
		MSG_ResultElement *elem = NULL;
		MSG_GetResultElement(GetMessagePane(), inMsgIndex, &elem);
		if (!elem)
			return;

		MWContextType cxType = MSG_GetResultElementType(elem);
		if (! (cxType == MWContextMail || cxType == MWContextMailMsg
			|| cxType == MWContextNews || cxType == MWContextNewsMsg))
			return;

		// Get info about the message
		MSG_SearchValue *value;
		MSG_GetResultAttribute(elem, attribFolderInfo, &value);
		MSG_FolderInfo *folderInfo = value->u.folder;
		if (!folderInfo)
			return;

		// Create the thread window if it doesn't exist
		CThreadWindow *threadWindow = CThreadWindow::FindOrCreate( folderInfo, CThreadView::cmd_UnselectAllCells);
	}
	Catch_(inErr)
	{
		Throw_(inErr);
	}
	EndCatch_
} // CSearchTableView::FindOrCreateThreadWindowForMessage


//-----------------------------------
void CSearchTableView::FindCommandStatus(CommandT inCommand, Boolean &outEnabled,
	Boolean &outUsesMark, Char16 &outMark, Str255 outName)
//-----------------------------------
{
	ResIDT		menuID;
	Int16		menuItem;
	
	// Check for synthetic commands
	if( IsSyntheticCommand( inCommand, menuID, menuItem ) )
	{
		switch( menuID )
		{
			case menu_Navigator:	// enable menus
				outEnabled = true;
				break;
			
			case menu_Go:			// disable menus
			case menu_View:			
				outEnabled = false;
				break;
		}
	}
	else // check for regular commands
	{
		switch (inCommand) 
		{								
			case cmd_AboutPlugins:		// always enabled
			case cmd_About:
			case cmd_Quit:
			case cmd_Close:
			case cmd_Preferences:
				outEnabled = true;
				break;
			
			case cmd_SelectAll: 		// need at least one result
				if( IsValidRow( 1 ) ) 
					outEnabled = true;
				else
					outEnabled = false;
				break;	
			
			
			case cmd_Clear:				// need at least one result and one selection
				// Only support deleting of Mail Messages not news.
				CMailSelection selection;
				if ( GetSelection( selection ) )
				{
					const MSG_ViewIndex	*indices = selection.GetSelectionList();
			
				
					MSG_ResultElement*	outElement;
					MSG_FolderInfo*		outFolder;
					MessageKey			outKey;

					GetMessageResultInfo( *indices, outElement, outFolder, outKey );
					
					
					CMessageFolder	messageFolder( outFolder );
					UInt32			folderFlags = messageFolder.GetFolderFlags();
					
					if ( (folderFlags & MSG_FOLDER_FLAG_NEWSGROUP) == 0 )
						outEnabled = true;	// mail message - delete command
				}
				break;
														
			default:
				Inherited::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
				break;
		}
	}
}

//-----------------------------------
Boolean CSearchTableView::GetMessageResultInfo(
	MSG_ViewIndex inMsgIndex,
	MSG_ResultElement*& outElement,
	MSG_FolderInfo*& outFolder,
	MessageKey& outKey)
//-----------------------------------
{
	return GetMessageResultInfo(
		GetMessagePane(), inMsgIndex, outElement, outFolder, outKey);
} // SearchTableView::GetMessageResultInfo

//-----------------------------------
Boolean CSearchTableView::GetMessageResultInfo(
	MSG_Pane* inPane,
	MSG_ViewIndex inMsgIndex,
	MSG_ResultElement*& outElement,
	MSG_FolderInfo*& outFolder,
	MessageKey& outKey)
//-----------------------------------
{
	MSG_SearchError err;
	err = ::MSG_GetResultElement(inPane, inMsgIndex, &outElement);
	if (err != SearchError_Success || outElement == nil)
		return false;

	MWContextType cxType = ::MSG_GetResultElementType(outElement);
	if (! (cxType == MWContextMail || cxType == MWContextMailMsg
		|| cxType == MWContextNews || cxType == MWContextNewsMsg))
		return false;

	// Get info about the message
	MSG_SearchValue *value;
	err = ::MSG_GetResultAttribute(outElement, attribMessageKey, &value);
	if (err != SearchError_Success || value == nil)
		return false;

	outKey = value->u.key;
	if (outKey == MSG_MESSAGEKEYNONE)
		return false;

	// Get info about the folder the message is in
	err = ::MSG_GetResultAttribute(outElement, attribFolderInfo, &value);
	if (err == SearchError_Success && value != nil)
		outFolder = value->u.folder;
	return true;
} // SearchTableView::GetMessageResultInfo

//-----------------------------------
void CSearchTableView::AddRowDataToDrag(TableIndexT inRow, DragReference inDragRef)
// Base class implementation is for a message search.  Return the URL.
//-----------------------------------
{
	
	MSG_ResultElement *elem = NULL;
	MSG_FolderInfo *folderInfo = nil;
	MessageKey key;
	
	// get key and folder info
	if (!GetMessageResultInfo(inRow - 1, elem, folderInfo, key))
		return;
		
	if (folderInfo == nil)
		return;
		
//	MSG_Pane* pane = ::MSG_FindPaneOfType(
//		CMailNewsContext::GetMailMaster(), folderInfo, MSG_THREADPANE);
	
	MSG_Pane* pane = GetMessagePane(); 	// get search pane from the search manager

	if (!pane) 						// fails unless there is a threadpane open to this folder.  Too bad.
		return;						// That's why I turned off dragging.
	
		
	// create URL	
	URL_Struct* url = ::MSG_ConstructUrlForMessage( pane, key );
	if (!url)
		return;		// we always return here, as far as I can see
	
	// add the drag item	
	OSErr err = ::AddDragItemFlavor( inDragRef, inRow, 'TEXT', url->address, XP_STRLEN(url->address), flavorSenderOnly);
	
	NET_FreeURLStruct(url);
} 

//-----------------------------------
void CSearchTableView::ShowMessage(MSG_ViewIndex inMsgIndex, SearchWindowOpener inWindow)
//-----------------------------------
{
	AssertFail_(mSearchManager != nil);
	Try_
	{
		MSG_ResultElement *elem = NULL;
		MSG_FolderInfo *folderInfo = nil;
		MessageKey key;
		if (!GetMessageResultInfo(inMsgIndex, elem, folderInfo, key))
			return;
		if (folderInfo == nil && inWindow != kInMessageWindow)
			return;
			
		// Got the info we wanted: we can display now...
		if (inWindow == kInMessageWindow)
		{
			// Check if there's an open window with this message.
			CMessageWindow *messageWindow = CMessageWindow::FindAndShow(key);
			if (messageWindow)
			{
				messageWindow->Select();
				return;
			}

			// If option key down, try to reuse a window
			if ( CApplicationEventAttachment::CurrentEventHasModifiers(optionKey) )
			{
				messageWindow = CMessageWindow::FindAndShow(0);
				messageWindow->Select();
			}

			// Create the window
			if (messageWindow == nil)
			{
				// We should probably create a generic utility function to generate
				// windows that contain a CHTMLView
				CBrowserContext *theContext = new CBrowserContext(MWContextMailMsg);
				FailNIL_(theContext);
				StSharer theShareLock(theContext);
				
				// Create a message window with this view as its super commander.
				LWindow *theWindow = LWindow::CreateWindow(cMessageWindowPPobID, LCommander::GetTopCommander());
				FailNIL_(theWindow);
				messageWindow = dynamic_cast<CMessageWindow*>(theWindow);
				FailNIL_(messageWindow);
				
				messageWindow->SetWindowContext(theContext);
			}

			// Show the message
			messageWindow->GetMessageView()->ShowSearchMessage(
										CMailNewsContext::GetMailMaster(),
										elem,
										folderInfo == nil);
		}
		else
		{
			CommandT command;
			switch (inWindow)
			{
				case kInThreadWindow:			  command = CThreadView::cmd_UnselectAllCells;	break;
				case kAddToThreadWindowSelection: command = cmd_Nothing; 						break;
				default: return;
			}

			// Check if there's an open window with this folder, and open one if needed
			CThreadWindow *threadWindow = CThreadWindow::FindAndShow(folderInfo, true, command);
			if (threadWindow)
			{
				// Show the message
				threadWindow->ShowMessageKey(key);
			}
		}
	}
	Catch_(inErr)
	{
		Throw_(inErr);
	}
	EndCatch_
} // CSearchTableView::ShowMessage


//-----------------------------------
Boolean CSearchTableView::IsValidRow(TableIndexT inRow) const
//	*** WARNING ***
//  USING mRows is WRONG WHEN DELETING ROWS, it's possibly changed already!
//  Use the Table Selector's value instead.  That's why this override is here!
//-----------------------------------
{
	if (inRow < 1)
		return false;
	if (inRow > mRows ) /* ((LTableRowSelector*)mTableSelector)->GetRowCount()) */
		return false;
	return true;
}


//-----------------------------------
MessageKey CSearchTableView::GetRowMessageKey(TableIndexT inRow)
//	Get the message key for the specified row. Return MSG_MESSAGEKEYNONE if the
//	specified inRow is invalid.
//-----------------------------------
{
	AssertFail_(mSearchManager != nil);
	if ( !IsValidRow(inRow) ) return MSG_MESSAGEKEYNONE;
	MSG_ViewIndex inIndex = inRow - 1;
	
	UpdateMsgResult(inIndex);

	MSG_SearchValue *value = nil;
	
	MSG_SearchAttribute attribute = attribMessageKey;
	
	CSearchManager::FailSearchError(MSG_GetResultAttribute(mResultElement, attribute, &value));
	MessageKey key = value->u.key;
	
	MSG_SearchError error = MSG_DestroySearchValue(value);
    AssertFail_(error == SearchError_Success);	// What to do with an error?

	return key;
}

//-----------------------------------
MSG_ResultElement* CSearchTableView::GetCurrentResultElement(TableIndexT inRow)
//-----------------------------------
{
	AssertFail_(mSearchManager != nil);
	if ( !IsValidRow(inRow) ) return nil;
	MSG_ResultElement *result;
	CSearchManager::FailSearchError(MSG_GetResultElement(GetMessagePane(), inRow-1, &result));
	return result;
}


/*======================================================================================
	Get the current sort params for the result data.
======================================================================================*/

void CSearchTableView::GetSortParams(MSG_SearchAttribute *outSortKey, Boolean *outSortBackwards)
{
	AssertFail_(mTableHeader != nil);
	AssertFail_(mSearchManager != nil);
	PaneIDT sortColumnID;
	mTableHeader->GetSortedColumn(sortColumnID);
	*outSortKey = CSearchWindowBase::AttributeFromDataType(sortColumnID);
	AssertFail_(*outSortKey != kNumAttributes);
	*outSortBackwards = mTableHeader->IsSortedBackwards();
}


/*======================================================================================
	Force a sort to occur based upon the current sort column.
======================================================================================*/

void CSearchTableView::ForceCurrentSort()
{
	MSG_SearchAttribute sortKey;
	Boolean sortBackwards;
	
	GetSortParams(&sortKey, &sortBackwards);

	CSearchManager::FailSearchError(MSG_SortResultList(GetMessagePane(), sortKey, sortBackwards));
}

//-----------------------------------
void CSearchTableView::ChangeSort(const LTableHeader::SortChange *inSortChange)
//	Notification to sort the table.
//-----------------------------------
{
	AssertFail_(mSearchManager != nil);

	Inherited::ChangeSort(inSortChange);
	
	MSG_SearchAttribute	sortKey = CSearchWindowBase::AttributeFromDataType(inSortChange->sortColumnID);
	
	if ( sortKey != kNumAttributes ) {
	
		::SetCursor(*::GetCursor(watchCursor));
		// Call BE to sort the table
		AssertFail_(GetMessagePane() != nil);
		CSearchManager::FailSearchError(MSG_SortResultList(GetMessagePane(), sortKey, inSortChange->reverseSort));
	}
}

//-----------------------------------
void CSearchTableView::SetWinCSID(Int16 wincsid)
//	Set the font for the view
//-----------------------------------
{
	if (wincsid == INTL_CharSetNameToID(INTL_ResourceCharSet()))
		this->SetTextTraits( 8603 );
	else
		this->SetTextTraits(CPrefs::GetTextFieldTextResIDs(wincsid));	
	Refresh();
}

//-----------------------------------
//void CSearchTableView::AddSelectionToDrag(
//	DragReference	inDragRef,
//	RgnHandle		inDragRgn	)
//-----------------------------------
//{
	//	Since we inherit from CMailFlexTable, we want to avoid it and just call the
	//	CStandardFlexTable method, thereby avoiding the CMailFlexTable behavior
	
//	CStandardFlexTable::AddSelectionToDrag(inDragRef, inDragRgn);
	
//} // CAddressSearchTableView::AddSelectionToDrag


//-----------------------------------
void CSearchTableView::SelectionChanged()
// adjust the 'Move Message To' popup accordingly to the current selection
//-----------------------------------
{
	Inherited::SelectionChanged();

	// Set the 'Move Message' popup to the folder of the first selected item
	CMailSelection selection;
	if (GetSelection(selection))
	{
		CMessageSearchWindow* myWindow = dynamic_cast<CMessageSearchWindow*>
								(LWindow::FetchWindowObject(GetMacPort()));
		if (myWindow)
		{
			CMailFolderPopupMixin* fileMessagePopup;
			FindUIItemPtr(myWindow, CMessageSearchWindow::paneID_FilePopup, fileMessagePopup);

			const MSG_ViewIndex	*indices = selection.GetSelectionList();
			MSG_ViewIndex		numIndices = selection.selectionSize;
			MSG_ResultElement*	outElement;
			MSG_FolderInfo*		outFolder;
			MessageKey			outKey;

			if (GetMessageResultInfo(*indices, outElement, outFolder, outKey))
				fileMessagePopup->MSetSelectedFolder(outFolder, false);
		}
	}
}
// 

/*======================================================================================
	Get the display text for the specified attribute and table row index. Return
	true if the text could be gotten.
======================================================================================*/

Boolean CSearchTableView::GetDisplayText(MSG_ViewIndex inIndex, MSG_SearchAttribute inAttribute,
								       CStr255& outString)
{
	AssertFail_(GetMessagePane() != nil);
	UpdateMsgResult(inIndex);
	
	MSG_SearchValue *value = nil;
	
	MSG_SearchError error = MSG_GetResultAttribute(mResultElement, inAttribute, &value);
	//Assert_(error == SearchError_Success);
	if ( error != SearchError_Success ) return false;
	
	Int16 wincsid = 0;
	MSG_SearchValue *folderinfoValue = nil;
	error = MSG_GetResultAttribute(mResultElement, attribFolderInfo, &folderinfoValue);
	if( ( error == SearchError_Success ) && folderinfoValue && folderinfoValue->u.folder)
	{
		folderinfoValue->attribute = attribFolderInfo;
		wincsid = INTL_DocToWinCharSetID(MSG_GetFolderCSID(folderinfoValue->u.folder));
		error = MSG_DestroySearchValue(folderinfoValue);
		Assert_(error == SearchError_Success);
	}

	switch ( inAttribute ) {
	
		case attribSender:
		case attribSubject:
			outString = "";
			char *buf = IntlDecodeMimePartIIStr(value->u.string, 
							wincsid, 
							FALSE);
			if (buf) {
				outString = buf;
				XP_FREE(buf);
				break;
			}
			outString = value->u.string;
			break;
			
		case attribLocation:
		case attribCommonName:
		case attrib822Address:
		case attribOrganization:
		case attribLocality:
		case attribPhoneNumber:
			outString = value->u.string;
			break;

		case attribDate:
			outString = MSG_FormatDate(GetMessagePane(), value->u.date);
			break;
		
		case attribMsgStatus:
		case attribPriority:
			{
				char name[32];
				if ( inAttribute == attribMsgStatus ) {
					MSG_GetStatusName(value->u.msgStatus, name, sizeof(name));
				} else {
					MSG_GetPriorityName(value->u.priority, name, sizeof(name));
				}
				outString = name;
			}
			break;

		default: 
			AssertFail_(false); 
			break;
	}
	
	error = MSG_DestroySearchValue(value);
	Assert_(error == SearchError_Success);
	
	return true;
}

/*======================================================================================
	Call this method whenever the indexes in the message pane change.
======================================================================================*/

void CSearchTableView::UpdateMsgResult(MSG_ViewIndex inIndex) {

	AssertFail_(GetMessagePane() != nil);
	
	if ( (mResultElement == nil) || (mResultIndex != inIndex) ) {
	
		mResultElement = nil;
		
		CSearchManager::FailSearchError(MSG_GetResultElement(GetMessagePane(), inIndex, &mResultElement));
		
		mResultIndex = inIndex;
	}
}

/*======================================================================================
	React to search message.
======================================================================================*/

void CSearchTableView::StartSearch(MWContext *inContext, MSG_Master *inMaster, MSG_SearchAttribute inSortKey,
								 Boolean inSortBackwards) {

	if ( !mSearchManager->CanSearch() || mSearchManager->IsSearching() ) return;
	
	MsgPaneChanged();	// For results

	// Prepare for a new search session
	NewSearchSession(inContext, inMaster, inSortKey, inSortBackwards);
	mSearchManager->SetMSGPane( GetMessagePane() );
	// Add scopes to search
	mSearchManager->AddScopesToSearch(inMaster);
		
	// Add search parameters
	mSearchManager->AddParametersToSearch();
		
	CSearchManager::FailSearchError(MSG_Search(GetMessagePane()));
	
	mSearchManager->StartSearch();
	
	mSearchManager->SetIsSearching( true );
}

/*======================================================================================
	Prepare for a new search session.
======================================================================================*/

void CSearchTableView::NewSearchSession(MWContext *inContext, MSG_Master *inMaster, MSG_SearchAttribute inSortKey,
									  Boolean inSortBackwards)
{
	AssertFail_(inContext != nil);
	AssertFail_(inMaster != nil);
	
	if ( GetMessagePane() == nil )
	{
		// Create the search pane and store related date
		MSG_Pane* msgPane = MSG_CreateSearchPane(inContext, inMaster);
		FailNIL_(msgPane);
		SetMessagePane( msgPane );
		MSG_SetFEData( GetMessagePane(), CMailCallbackManager::Get() );

	} else
	{
		// Free any previously allocated search memory
		CSearchManager::FailSearchError(MSG_SearchFree(GetMessagePane()));
	}
	
	// Alloc mem for new search parameters
	CSearchManager::FailSearchError(MSG_SearchAlloc(GetMessagePane()));
	
	// Setup the sort order
	CSearchManager::FailSearchError(
	MSG_SortResultList(GetMessagePane(), inSortKey, inSortBackwards) );
	
}

void CSearchTableView::ChangeFinished(MSG_Pane * inPane, MSG_NOTIFY_CODE inChangeCode,
							  TableIndexT inStartRow, SInt32 inRowCount)
{
	MsgPaneChanged();
	CMailFlexTable::ChangeFinished( inPane, inChangeCode, inStartRow, inRowCount );
}
