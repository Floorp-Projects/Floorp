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



// CSearchWindowBase.cp

#define DEBUGGER_ASSERTIONS

#include "CSearchWindowBase.h"
#include "SearchHelpers.h"
#include "CMailNewsContext.h"
#include "CSearchTableView.h"
#include "libi18n.h"
#include "LGAPushButton.h"
#include "CPatternProgressBar.h"
#include "MailNewsgroupWindow_Defines.h"
#include "resgui.h"
#include <UModalDialogs.h>
#include "prefapi.h"
#include "LGARadioButton.h"
#include "UStClasses.h"
#include "CThreadWindow.h"
#include "CMessageWindow.h"
#include "CProgressListener.h"

const UInt16 			resIDT_SearchOptionsDialog = 8851;
const char* const		searchSubfolderPrefName = "mailnews.searchSubFolders";
const char* const		searchServerPrefName = "mailnews.searchServer";

//-----------------------------------
CSearchWindowBase::CSearchWindowBase(LStream *inStream, DataIDT inWindowType) : 
		 		  CMailNewsWindow(inStream, inWindowType),
		 		  LListener(),
				  LPeriodical(),
		 		  mSearchManager(),
		 		  mResultsEnclosure(nil),
		 		  mResultsTable(nil),
		 		  mResultsVertWindExtension(0),
		 		  mResultsTableVisible(true),
		 		  mSearchFolders(),
		 		  mProgressBar(nil),
		 		  mSearchButton(nil),
		 		  mSearchOptionsButton(nil)
//-----------------------------------
{
	SetRefreshAllWhenResized(false);
} 

CSearchWindowBase::~CSearchWindowBase()
//	Stop any current search.
//-----------------------------------
{
	Boolean canRotate = false;
	if ( IsVisible() )
		USearchHelper::FindWindowTabGroup(&mSubCommanders)->SetRotateWatchValue(&canRotate);
} // CSearchWindowBase::~CSearchWindowBase

// --------------------------------------------------------------------------
UInt16 		
CSearchWindowBase::GetValidStatusVersion() const
//
// -- 10/30/97 ---------------------------------------------------------
//
//  This is the proper place for this method, because 
//  every time the two subclasses of this class stream out they stream out a member of 
//	this class ( CSearchManager). Therefore when the search manager parameters for stream out 
//	are changed both subclasses are affected. 
//
// 	Copied the following comments from the CAddressSearchWindow implementation, now defunct.
//
// ---------------------------------------------------------------------------
//	So here's the skinny on the directory (aka address) search window.
//	You see, when we shipped version 4.01, there were three "levels" of criteria,
//	and the version we shipped with was:
//
//		static const UInt16 cAddressSearchSaveWindowStatusVersion = 0x0203;
//
//	Then on 97/06/30, the number of levels has changed from 3 to 5 in MailNewsSearch.cnst,
//	in the enclosures include view.  That meant that builds from 7/1 - 7/30 were writing
//	out saved window status records containing five levels of criteria.
//	Unfortunately, the data is written out as:
//
//		bounds/vertextension/levelsdata/tableheaderdata
//
//	so that the extra data was written out in the middle.  In version 4.01, the routine
//	CSearchManager::ReadSavedSearchStatus was not skipping the extra fields, so a crash
//	occurred when reading in the table header stuff.  This happened when the search window
//	was used in 4.01 after running it in 4.02 (7/30 build).
//
//	This is bug #78713.  There are two parts of the fix.  (1) start with a version number
//	of zero.  Then, version 4.01 will ignore the resource as being "too old", and write
//	out a resource with version 0x203.  This will be handled OK by version 4.02.
//
//	The other part of the fix is that CSearchManager::ReadSavedSearchStatus will now skip
//	extra fields, so that this crash won't happen if somebody further increases the number
//	of levels.
//
//	FINAL NOTE (97/10/13) Because of this same problem, I changed CSaveWindowStatus so that
//	it always writes out zero in the first field, which will be interpreted as a zero version
//	by Communicator 4.0x, and ignored.    So now it's OK to increment this as originally
//	intended.
{
	return eValidStreamVersion;
}

//-----------------------------------
void CSearchWindowBase::AboutToClose()
//-----------------------------------
{
	// Not re-entrant!  Don't call this twice.
	CSearchWindowBase::MessageWindStop(true);
	
	Inherited::AttemptCloseWindow(); // Do this first: uses table
	
	// Bug fix: Do these things in the right order here
	if (mMailNewsContext)
		mMailNewsContext->RemoveUser(this);
	mMailNewsContext = nil;
	mSearchManager.AboutToClose();
	
	if (mResultsTable)
	{
		if (mMailNewsContext)
			mMailNewsContext->RemoveListener(mResultsTable); // bad time to listen for all connections complete.		
		delete mResultsTable;
		mResultsTable = nil;
	}
}

//-----------------------------------
void CSearchWindowBase::FinishCreateSelf()
//-----------------------------------
{
	// Create context and and progress listener
	
	mMailNewsContext = new CMailNewsContext(MWContextSearch);
	FailNIL_(mMailNewsContext);
	
	mMailNewsContext->AddUser(this);
	mMailNewsContext->AddListener(this);
	
	CMediatedWindow::FinishCreateSelf();	// Call CMediatedWindow for now since we need to
											// create a different context than CMailNewsWindow
	if (mProgressListener)
		mMailNewsContext->AddListener(mProgressListener);
	else
		mProgressListener = new CProgressListener(this, mMailNewsContext);

	// SetAttribute(windAttr_DelaySelect); put in resource
		// should be in resource, but there's a resource freeze.

	mMailNewsContext->SetWinCSID(INTL_DocToWinCharSetID(mMailNewsContext->GetDefaultCSID()));

	USearchHelper::FindWindowTabGroup(&mSubCommanders)->SetRotateWatchValue(&mCanRotateTarget);

	// Populate scope window
	
	mResultsEnclosure = USearchHelper::FindViewSubview(this, paneID_ResultsEnclosure);
	FailNILRes_(mResultsEnclosure);
	mResultsTable = dynamic_cast<CSearchTableView *>(USearchHelper::FindViewSubview(this, paneID_ResultsTable));
	FailNILRes_(mResultsTable);
	mSearchButton = dynamic_cast<LGAPushButton *>(USearchHelper::FindViewControl(this, paneID_Search));
	FailNILRes_(mSearchButton);
	mProgressBar = dynamic_cast<CPatternProgressCaption *>(USearchHelper::FindViewSubpane(this, paneID_ProgressBar));
	FailNILRes_(mProgressBar);
	
	mResultsTable->AddListener(this);
	mSearchButton->SetDefaultButton(true, false);
	mResultsEnclosure->SetRefreshAllWhenResized(false);
	
	Rect windowBounds;
	this->CalcPortFrameRect(windowBounds);
	mMinVResultsSize = windowBounds.bottom - windowBounds.top;
	
	UReanimator::LinkListenerToControls(this, this, GetStatusResID());
	
	ShowHideSearchResults(false);
	
	// Initialize the search manager
	mSearchManager.AddListener(this);
	mSearchManager.InitSearchManager(
		this, USearchHelper::FindWindowTabGroup(&mSubCommanders),
		GetWindowScope(),
		&mSearchFolders );
	mResultsTable->SetSearchManager(&mSearchManager);
	
	// Call inherited method
	FinishCreateWindow();
}

//-----------------------------------
Boolean CSearchWindowBase::ObeyCommand(CommandT inCommand, void *ioParam)
//-----------------------------------
{
	if ( IsResultsTableVisible() )
	{
		switch ( inCommand )
		{
			case cmd_PreviousMessage:
			case cmd_NextMessage:
			case cmd_NextUnreadMessage:
				return true;
			
			case cmd_SortBySubject:
			case cmd_SortBySender:
			case cmd_SortByDate:
			case cmd_SortByLocation:
			case cmd_SortByPriority:
			case cmd_SortByStatus:
				{
					LTableHeader *theHeader = mResultsTable->GetTableHeader();
					PaneIDT dataType = DataTypeFromSortCommand(inCommand);
					if ( theHeader != nil )
					{
						theHeader->SetSortedColumn(theHeader->ColumnFromID(dataType),
												   mResultsTable->IsSortedBackwards(), true);
					}
				}
				return true;	
			
			case cmd_SortAscending:
			case cmd_SortDescending:
				LTableHeader *theHeader = mResultsTable->GetTableHeader();
				if ( theHeader != nil )
				{
					theHeader->SetSortedColumn(theHeader->ColumnFromID(mResultsTable->GetSortedColumn()),
						inCommand == cmd_SortDescending, true);
				}
				return true;

		case msg_TabSelect:
			break;

		}
	}

	switch ( inCommand ) {
	
		default:
			return Inherited::ObeyCommand(inCommand, ioParam);
	}
}

//-----------------------------------
void CSearchWindowBase::FindCommandStatus(CommandT inCommand, Boolean &outEnabled,
									  Boolean &outUsesMark, Char16 &outMark,
									  Str255 outName)
//-----------------------------------
{
	//	Boolean isLineSelected = (GetSelectedRowCount() > 0);

	if ( IsResultsTableVisible() )
	{
		switch ( inCommand )
		{
			case cmd_PreviousMessage:
			case cmd_NextMessage:
			case cmd_NextUnreadMessage:
				outEnabled = true;
				outUsesMark = false;
				return;
			
			case cmd_SortBySubject:
			case cmd_SortBySender:
			case cmd_SortByDate:
			case cmd_SortByLocation:
			case cmd_SortByPriority:
			case cmd_SortByStatus:
				{
					PaneIDT dataType = DataTypeFromSortCommand(inCommand);
					PaneIDT paneID = mResultsTable->GetSortedColumn();
					outUsesMark = true;
					outEnabled = true;
					outMark = ((dataType == paneID) ? checkMark : noMark);
				}
				return;		
			
			case cmd_SortAscending:
			case cmd_SortDescending:
				outMark =
					((inCommand == cmd_SortDescending) == mResultsTable->IsSortedBackwards()) ?
						checkMark : 0;
				outUsesMark = true;
				outEnabled = true;
				return;	
		}
	}

	switch ( inCommand ) {
	
		default:
			if (inCommand == cmd_OpenSelection)	// enabled in base class
				::GetIndString(outName, kMailNewsMenuStrings, kOpenMessageStrID);

			Inherited::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark,
								 	   		   outName);
			break;
	}
}

//-----------------------------------
void CSearchWindowBase::ListenToMessage(MessageT inMessage, void *ioParam)
//-----------------------------------
{
	switch ( inMessage )
	{	
		case paneID_Search:
			if (mProgressBar) mProgressBar->SetDescriptor("\p");
			MessageWindSearch();
			break;

		case paneID_SearchOptions:
			SearchOptions();
			break;	

		case paneID_Stop:
			MessageWindStop(true);
			UpdateTableStatusDisplay();
			break;

		case CStandardFlexTable::msg_SelectionChanged:
			UpdateTableStatusDisplay();
			break;
			
		case CSearchManager::msg_SearchParametersCleared:
			ShowHideSearchResults(false);
			break;
			
		case CSearchManager::msg_SearchParametersResized:
			MessageSearchParamsResized(*((Int16 *) ioParam));
			break;
			
		// Status messages
			
		case msg_NSCAllConnectionsComplete:
			MessageWindStop(false);
			UpdateTableStatusDisplay();
			// no break here		
		
		default:
			// No superclass method
			break;
	}
} // CSearchWindowBase::ListenToMessage

//-----------------------------------
void CSearchWindowBase::SearchOptions()
// This function allows the user to set the following prefs:
//		- search in subfolders [yes / no]
//		- search locally / on server
// After changing these prefs, it is necessary to reconstruct the attributes menu
// because a search on server usually doesn't offer as many criteria as a local search.
// So we call PopulateAttributesMenus() which will do a MSG_GetAttributesForSearchScopes().
//-----------------------------------
{
	StDialogHandler theOptionsDialogHandler( resIDT_SearchOptionsDialog, dynamic_cast<LCommander*>(this) );
	
	// get the options dialog
	LWindow*	theOptionsDialog = theOptionsDialogHandler.GetDialog();
	AssertFail_( theOptionsDialog != nil );

	int prefResult;
	XP_Bool	searchSubFolderPref = false;
	XP_Bool	searchServerPref = false;
	
	// set the default value for the subfolder search from the preferences
	LGACheckbox* 	searchSubChkb
		= dynamic_cast<LGACheckbox*>( theOptionsDialog->FindPaneByID( 'SUBF' ) );
	AssertFail_( searchSubChkb != nil ); 
	
	prefResult = PREF_GetBoolPref( searchSubfolderPrefName, &searchSubFolderPref );
	if ( prefResult == PREF_ERROR ) searchSubFolderPref = true; // default from spec.
	searchSubFolderPref ? searchSubChkb->SetValue( Button_On ) : searchSubChkb->SetValue( Button_Off );
	
	// set the default value for the local/server search from the preferences
	LGARadioButton* searchServerBtn
		= dynamic_cast<LGARadioButton*>( theOptionsDialog->FindPaneByID( 'SRCS' ) );
	LGARadioButton* searchLocalBtn
		= dynamic_cast<LGARadioButton*>( theOptionsDialog->FindPaneByID( 'SRCL' ) );
	AssertFail_( searchServerBtn != nil );
	
	prefResult = PREF_GetBoolPref( searchServerPrefName, &searchServerPref );
	if ( prefResult == PREF_ERROR ) searchServerPref = true; // default from spec.
	if( searchServerPref )
	{
		searchServerBtn->SetValue( Button_On ); // Other radio will be turned off via LTabGroup
	}
	else
	{
		searchLocalBtn->SetValue( Button_On ); // ditto
	}

	MessageT		theMessage;

	while( true )
	{
		theMessage = theOptionsDialogHandler.DoDialog();
		
		if( theMessage == msg_OK )
		{
			Boolean		choice;
			if( ( choice = searchSubChkb->IsSelected() ) != searchSubFolderPref )
				PREF_SetBoolPref( searchSubfolderPrefName, choice );
			if( ( choice = searchServerBtn->IsSelected() ) != searchServerPref )
				PREF_SetBoolPref( searchServerPrefName, choice );
			// See the comment in the function header
			mSearchManager.PopulateAttributesMenus(GetWindowScope());
			break;
		}
		else
			if( theMessage == msg_Cancel )
				break;
	}
}

//-----------------------------------
Boolean CSearchWindowBase::HandleKeyPress(const EventRecord &inKeyEvent)
//-----------------------------------
{
	Int16 theKey = inKeyEvent.message & charCodeMask;
	
	if ( ((theKey == char_Enter) || (theKey == char_Return)) && !mSearchManager.IsSearching() )
	{		
		mSearchButton->SimulateHotSpotClick(kControlButtonPart);
		return true;
	}
	else if ( (((theKey == char_Escape) && ((inKeyEvent.message & keyCodeMask) == vkey_Escape)) ||
				 UKeyFilters::IsCmdPeriod(inKeyEvent)) && mSearchManager.IsSearching() )
	{
	
		USearchHelper::FindViewControl(this, paneID_Stop)->SimulateHotSpotClick(kControlButtonPart);
		return true;
	}
	
	return Inherited::HandleKeyPress(inKeyEvent);
}

//-----------------------------------
void CSearchWindowBase::SpendTime(const EventRecord &/*inMacEvent*/)
//-----------------------------------
{
	USearchHelper::EnableDisablePane(mSearchButton, mSearchManager.CanSearch());
}

//-----------------------------------
void CSearchWindowBase::Activate()
//	Start repeating.
//-----------------------------------
{
	StopIdling();
	StartRepeating();

//	CSearchTabGroup *theTabGroup = USearchHelper::FindWindowTabGroup(&mSubCommanders);
//	Boolean couldRotate = theTabGroup->SetCanRotate(false);	// Don't rotate when activating

	Inherited::Activate();

//	theTabGroup->SetCanRotate(couldRotate);
}

//-----------------------------------
void CSearchWindowBase::DeactivateSelf()
//-----------------------------------
{
	StopRepeating();
	StartIdling();

	Inherited::DeactivateSelf();
}

//-----------------------------------
void CSearchWindowBase::DrawSelf()
//-----------------------------------
{
	Boolean doResultsTable = IsResultsTableVisible();
	Rect frame;
	
	if ( doResultsTable && mResultsTable->CalcPortFrameRect(frame) &&
		 ::RectInRgn(&frame, mUpdateRgnH) )
	{
		{	
			StExcludeVisibleRgn excludeRgn(mResultsTable);
			Inherited::DrawSelf();
		}

		StColorPenState::Normalize();
		::EraseRect(&frame);
	}
	else
	{
		Inherited::DrawSelf();
	}

	USearchHelper::RemoveSizeBoxFromVisRgn(this);
}

//-----------------------------------
void CSearchWindowBase::SetDescriptor(ConstStr255Param inDescriptor)
//-----------------------------------
{
	if ( !::EqualString(inDescriptor, *((WindowPeek) GetMacPort())->titleHandle, true, true) )
	{
		Inherited::SetDescriptor(inDescriptor);
	}
}

//-----------------------------------
void CSearchWindowBase::MessageSearchParamsResized(Int16 inResizeAmount)
//	React to more message.
//-----------------------------------
{
	LPane *resultsView = USearchHelper::FindViewSubpane(this, paneID_ResultsEnclosure);
	
	if ( inResizeAmount > 0 ) {	// Make refresh look good!
		resultsView->ResizeFrameBy(0, -inResizeAmount, true);
		resultsView->MoveBy(0, inResizeAmount, true);
	}
	else
	{
		resultsView->MoveBy(0, inResizeAmount, true);
		resultsView->ResizeFrameBy(0, -inResizeAmount, true);
	}
	
	if ( !IsResultsTableVisible() )
	{
		Rect bounds;
		CSaveWindowStatus::GetPaneGlobalBounds(this, &bounds);
		bounds.bottom += inResizeAmount;
		this->DoSetBounds(bounds);	// Update window size
	}
	
	RecalcMinMaxStdSizes();
}

//-----------------------------------
void CSearchWindowBase::MessageWindSearch()
//-----------------------------------
{
	AssertFail_(mSearchManager.CanSearch());
		
	MSG_SearchAttribute attrib;
	Boolean sortBackwards;
	mResultsTable->GetSortParams(&attrib, &sortBackwards);

	mResultsTable->StartSearch(*mMailNewsContext, CMailNewsContext::GetMailMaster(),
							   attrib, sortBackwards);

	mSearchButton->Hide();
	USearchHelper::FindViewSubpane(this, paneID_Stop)->Show();
	USearchHelper::FindViewSubpane(this, paneID_ScopeEnclosure)->Disable();

	ShowHideSearchResults(true);
	UpdatePort();
} // CSearchWindowBase::MessageWindSearch()

//-----------------------------------
void CSearchWindowBase::RecalcMinMaxStdSizes()
//	Recalculate the window's min/max and std sizes based on the current window state.
//-----------------------------------
{
	if ( IsResultsTableVisible() )
	{
		mMinMaxSize.top = mMinVResultsSize;
		mMinMaxSize.bottom = 16000;
		mStandardSize.height = max_Int16;
	}
	else
	{
		Rect bounds;
		CSaveWindowStatus::GetPaneGlobalBounds(this, &bounds);
		mMinMaxSize.top = mMinMaxSize.bottom = mStandardSize.height = (bounds.bottom - bounds.top);
	}
}

//-----------------------------------
void CSearchWindowBase::ShowHideSearchResults(Boolean inDoShow)
//	Show or hide the search results in this window by resizing the window vertically.
//	If the results are being hidden, the last window vertical extension is stored in
//	mResultsVertWindExtension, otherwise mResultsVertWindExtension is used for determining
//	the amount to resize the window.
//
//	If search results are hidden, all current results are deleted from the result table.
//-----------------------------------
{
	if ( IsResultsTableVisible() )
		return;
		
	Rect curBounds;
	
	CSaveWindowStatus::GetPaneGlobalBounds(this, &curBounds);
	
	Rect newBounds = curBounds;

	if ( inDoShow )
	{
		if ( mResultsVertWindExtension < 0 ) mResultsVertWindExtension = -mResultsVertWindExtension;	// Undo initialization
		newBounds.bottom += mResultsVertWindExtension;
		mResultsEnclosure->Show();
		if ( (newBounds.bottom - newBounds.top) < mMinVResultsSize )
		{
			newBounds.bottom = newBounds.top + mMinVResultsSize;
		}
	}
	else
	{
		Rect bounds2;
		CSaveWindowStatus::GetPaneGlobalBounds(USearchHelper::FindViewSubpane(this, CSearchManager::paneID_ParamEncl), 
										   &bounds2);
		Int16 newBottom = (bounds2.left - newBounds.left) + bounds2.bottom;
		if ( mResultsVertWindExtension >= 0 ) mResultsVertWindExtension = newBounds.bottom - newBottom;
		newBounds.bottom = newBottom;
		mResultsEnclosure->Hide();
	}

	mResultsTableVisible = inDoShow;
	mMinMaxSize.bottom = max_Int16;	// Set so that VerifyWindowBounds() can do its work correctly!
	mMinMaxSize.top = 100;	// Set so that VerifyWindowBounds() can do its work correctly!
	
	CSaveWindowStatus::VerifyWindowBounds(this, &newBounds);
	if ( !::EqualRect(&newBounds, &curBounds) )
	{
		this->DoSetBounds(newBounds);
	}

	RecalcMinMaxStdSizes();
}

//-----------------------------------
void CSearchWindowBase::UpdateResultsVertWindExtension()
//	Update the vertical window extension variable.
//-----------------------------------
{
	if ( IsResultsTableVisible() && (mResultsVertWindExtension >= 0) )
	{
		Rect windBounds, enclBounds;
		CSaveWindowStatus::GetPaneGlobalBounds(this, &windBounds);
		CSaveWindowStatus::GetPaneGlobalBounds(USearchHelper::FindViewSubpane(
											this, CSearchManager::paneID_ParamEncl), &enclBounds);
		Int16 enclBottom = (enclBounds.left - windBounds.left) + enclBounds.bottom;
		mResultsVertWindExtension = windBounds.bottom - enclBottom;
	}
}

//-----------------------------------
void CSearchWindowBase::ReadWindowStatus(LStream *inStatusData)
//-----------------------------------
{
	if ( inStatusData != nil )
	{
		Rect bounds;
		*inStatusData >> bounds;
		
		CSaveWindowStatus::MoveWindowTo(this, topLeft(bounds));

		Int16 resultsVertWindExtension;
		
		*inStatusData >> resultsVertWindExtension;
		if ( resultsVertWindExtension > 0 )
		{
			mResultsVertWindExtension = -resultsVertWindExtension;	// See the ShowHideSearchResults(), set to negative
																	// to mean initialization
		}
	}
	else
	{
		CSaveWindowStatus::MoveWindowToAlertPosition(this);
	}
	
	mSearchManager.ReadSavedSearchStatus(inStatusData);
	if( inStatusData )
		mResultsTable->GetTableHeader()->ReadColumnState(inStatusData);
}

//-----------------------------------
void CSearchWindowBase::WriteWindowStatus(LStream *outStatusData)
//-----------------------------------
{
	CSaveWindowStatus::WriteWindowStatus(outStatusData);

	UpdateResultsVertWindExtension();
	
	*outStatusData << ((Int16) ((mResultsVertWindExtension > 0) ? mResultsVertWindExtension : 
														   (-mResultsVertWindExtension)));
														   
	mSearchManager.WriteSavedSearchStatus(outStatusData);

	mResultsTable->GetTableHeader()->WriteColumnState(outStatusData);
}

//-----------------------------------
void CSearchWindowBase::UpdateTableStatusDisplay()
//-----------------------------------
{
	if ( !IsResultsTableVisible() ) return;
	
	AssertFail_(mResultsTable != nil);
	
	TableIndexT numItems, numSelectedItems;
	
	mResultsTable->GetTableSize(numItems, numSelectedItems);
	numSelectedItems = mResultsTable->GetSelectedRowCount();
	
	CStr255 messageString;
	if ( numItems > 0 )
	{
		CStr31 numString, selectedString;
		::NumToString(numItems, numString);
		if ( numSelectedItems > 0 )
		{
			::NumToString(numSelectedItems, selectedString);
			USearchHelper::AssignUString(
				(numItems == 1) ?
					USearchHelper::USearchHelper::uStr_OneMessageFoundSelected :
					USearchHelper::uStr_MultipleMessagesSelected, 
				messageString);
		}
		else
			USearchHelper::AssignUString((numItems == 1) ? USearchHelper::uStr_OneMessageFound : USearchHelper::uStr_MultipleMessagesFound, 
										 messageString);
		::StringParamText(messageString, numString, selectedString);
		
	}
	else
		USearchHelper::AssignUString(USearchHelper::uStr_NoMessagesFound, messageString);
	
	mProgressBar->SetDescriptor(messageString);

}

//-----------------------------------
Boolean CSearchWindowBase::GetDefaultSearchTable(
	CMailNewsWindow*& outMailNewsWindow,
	CMailFlexTable*& outMailFlexTable)
//	Using the current PP window hierarchy, determine if the current top regular window is
//	a window that we can use to associate with our search window. This method is
//	called just before the search window is brought to the front of the regular window
//	hierarchy. If the method returns false, no window was found that could be associated
//	with our search window and the output parameters are set to nil. If the method returns 
//	true, a window was found that has an active flex table, and both objects are returned 
//	in the output parameters.
//-----------------------------------
{
	outMailNewsWindow = nil;
	outMailFlexTable = nil;

	// Get the current top regular window of the type we need
	CMediatedWindow *theFoundWindow = nil;
	CWindowIterator theIterator(WindowType_Any);
	do
	{
		if (theIterator.Next(theFoundWindow))
		{
			DataIDT windowType = theFoundWindow->GetWindowType();
			switch (windowType)
			{
				case WindowType_SearchMailNews:
					continue; 	// Found ourselves.

				case WindowType_MailNews:
					outMailNewsWindow = dynamic_cast<CMailNewsFolderWindow *>(theFoundWindow);
					break;

				case WindowType_MailThread:
				case WindowType_Newsgroup:
					outMailNewsWindow = dynamic_cast<CThreadWindow *>(theFoundWindow);
					break;

				case WindowType_Message:
					outMailNewsWindow = dynamic_cast<CMessageWindow *>(theFoundWindow);
					break;
			}
			if (outMailNewsWindow)
				break;	// Found a MailNews window that can return a "search table"
		}
		else
			theFoundWindow = nil;
	} 
	while (theFoundWindow != nil);
	
	if (outMailNewsWindow)
	{
		// Is there an active target table?
		outMailFlexTable = outMailNewsWindow->GetSearchTable();
		if (outMailFlexTable == nil)
			outMailNewsWindow = nil;
			
	}
	return (outMailFlexTable != nil);
} 


//-----------------------------------
void CSearchWindowBase::MessageWindStop(Boolean inUserAborted)
//-----------------------------------
{
	if ( mSearchManager.IsSearching() )
	{
		mSearchManager.StopSearch(inUserAborted ? ((MWContext *) *mMailNewsContext) : nil);
		mSearchButton->Show();
		USearchHelper::FindViewSubpane(this, paneID_Stop)->Hide();
		USearchHelper::FindViewSubpane(this, paneID_ScopeEnclosure)->Enable();

		// This call fixes a bug in the BE search row insertion
		mResultsTable->ForceCurrentSort();
	}
}

/*======================================================================================
	Return a display name for the specified folder name.
======================================================================================*/

CStr255& CSearchWindowBase::GetFolderDisplayName(const char *inFolderName, CStr255& outFolderName)
{
	outFolderName = inFolderName;
	char* temp = (char*)outFolderName;
	NET_UnEscape(temp);
	outFolderName = temp;
	return outFolderName;
}


/*======================================================================================
	Get the display text for the specified attribute and table row index. Return
	kNumAttributes if the inCellDataType is invalid.
======================================================================================*/

MSG_SearchAttribute CSearchWindowBase::AttributeFromDataType(PaneIDT inCellDataType)
{
	MSG_SearchAttribute rtnVal;

	switch ( inCellDataType )
	{
		// Message search columns
		case kSubjectMessageColumn: rtnVal = attribSubject; break;
		case kSenderMessageColumn: rtnVal = attribSender; break;
		case kDateMessageColumn: rtnVal = attribDate; break;
		case kStatusMessageLocation: rtnVal = attribLocation; break;
		case kPriorityMessageColumn: rtnVal = attribPriority; break;
		case kStatusMessageColumn: rtnVal = attribMsgStatus; break;
		default: rtnVal = kNumAttributes; break;
	}
	
	return rtnVal;
}




/*======================================================================================
	Return a sort data type from the specified sort command. Return 0L if the 
	inSortCommand is invalid.
======================================================================================*/

PaneIDT CSearchWindowBase::DataTypeFromSortCommand(CommandT inSortCommand)
{
	PaneIDT rtnVal;

	switch ( inSortCommand )
	{
		case cmd_SortBySubject: rtnVal = kSubjectMessageColumn; break;
		case cmd_SortBySender: rtnVal = kSenderMessageColumn; break;
		case cmd_SortByDate: rtnVal = kDateMessageColumn; break;
		case cmd_SortByLocation: rtnVal = kStatusMessageLocation; break;
		case cmd_SortByPriority: rtnVal = kPriorityMessageColumn; break;
		case cmd_SortByStatus: rtnVal = kStatusMessageColumn; break;
		// Do we need these commands for column sorting in the LDAP search result pane?
		default: rtnVal = cmd_Nothing; break;
	}
	
	return rtnVal;
}


//-----------------------------------
void CSearchWindowBase::AddOneScopeMenuItem(
	Int16 inStringIndex,
	Int16 inAttrib
	)
//-----------------------------------
{
	CStr255 string;
	USearchHelper::AssignUString(inStringIndex, string);
	AddOneScopeMenuItem(string, inAttrib);
}

//-----------------------------------
void CSearchWindowBase::AddOneScopeMenuItem(
	const CStr255& inString,
	Int16 inAttrib
	)
//-----------------------------------
{
	Assert_(mNumBasicScopeMenuItems < MAX_SEARCH_MENU_ITEMS);
	if (mNumBasicScopeMenuItems < MAX_SEARCH_MENU_ITEMS)
	{
		MSG_SearchMenuItem& curItem = mSearchMenuItems[mNumBasicScopeMenuItems];
		LString::CopyPStr(inString, (UInt8*)curItem.name, sizeof(curItem.name));
		curItem.attrib = inAttrib;
		curItem.isEnabled = true;
		++mNumBasicScopeMenuItems;
	}
}


//-----------------------------------
void CSearchWindowBase::SetWinCSID(Int16 wincsid)
//-----------------------------------
{
	if(mMailNewsContext)
		mMailNewsContext->SetWinCSID(wincsid);
	if(mResultsTable)
		mResultsTable->SetWinCSID(wincsid);
	mSearchManager.SetWinCSID(wincsid);
}

