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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


// CAddressBookViews.cp
#include "CAddressBookViews.h" 
#include "CAddressBookWindows.h"
#ifdef MOZ_NEWADDR


#include "SearchHelpers.h"
#include "MailNewsgroupWindow_Defines.h"
#include "Netscape_Constants.h" 
#include "resgui.h" 
#include "CMailNewsWindow.h"
#include "CMailNewsContext.h"
#include "UGraphicGizmos.h"
#include "ufilemgr.h"
#include "uerrmgr.h"
#include "UStdDialogs.h"
#include "macutil.h"
#include "UStClasses.h"
#include <LGAPushButton.h>
#include <LDragAndDrop.h>
#include "divview.h"
#include "CTargetFramer.h" 
#include "msgcom.h" 
// get string constants
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS

#include "UProcessUtils.h" 
#include "CAppleEventHandler.h"
#include "secnav.h" 
#include "CTableKeyAttachment.h" 
#include "intl_csi.h"
#include "xp_help.h"
#include "CLDAPQueryDialog.h" 
#include "StSetBroadcasting.h" 
#include "UAddressBookUtilities.h"
#include "NetscapeDragFlavors.h"
#include "CKeyStealingAttachment.h"
#include "UMailSelection.h"
#pragma mark -

#define AssertFail_(test)		ThrowIfNot_(test)
struct SAddressDelayedDragInfo
{
	CMailSelection mDragSelection;
	AB_ContainerInfo* mDragContainer;
	AB_DragEffect mDragRequest;
};

//------------------------------------------------------------------------------
//	¥	CAddressBookPane
//------------------------------------------------------------------------------
//	Is the base class for tables which use the addressbook API's
// 

//------------------------------------------------------------------------------
//	¥	CAddressBookPane
//------------------------------------------------------------------------------
//	
CAddressBookPane::CAddressBookPane(LStream *inStream) : Inherited(inStream), mContainer ( NULL ), mDelayedDragInfo ( false )
{
	SetRefreshAllWhenResized(false);
	mColumnSortCommand [ 0 ]=  AB_SortByColumnID0;		mColumnAttribID[ 0 ] = AB_attribEntryType;
	mColumnSortCommand [ 1 ]=  AB_SortByColumnID1;		mColumnAttribID[ 1 ] = AB_attribDisplayName;
	mColumnSortCommand [ 2 ]=  AB_SortByColumnID2;		mColumnAttribID[ 2 ] = AB_attribEmailAddress;
	mColumnSortCommand [ 3 ]=  AB_SortByColumnID3;		mColumnAttribID[ 3 ] = AB_attribCompanyName;
	mColumnSortCommand [ 4 ]=  AB_SortByColumnID4; 		mColumnAttribID[ 4 ] = AB_attribNickName;
	mColumnSortCommand [ 5 ]=  AB_SortByColumnID5; 		mColumnAttribID[ 5 ] = AB_attribLocality;
}

//------------------------------------------------------------------------------
//	¥	GetFullAddress
//------------------------------------------------------------------------------
//	Gets an email address for a given row by combining fullname and email address
// 	MSG_MakeFullAddress	should be replaced by the address book equivalent when
//	available
//
char*	CAddressBookTableView::GetFullAddress( TableIndexT  inRow   )
{
		StABEntryAttribute fullName( GetMessagePane(), inRow, AB_attribFullName );
		StABEntryAttribute email( GetMessagePane(), inRow, AB_attribEmailAddress);
		return MSG_MakeFullAddress( fullName.GetChar(), email.GetChar() );
}
		
		
//------------------------------------------------------------------------------
//	¥	CurrentBookIsPersonalBook
//------------------------------------------------------------------------------
//	Checks to see if the currenlty loaded container is a PAB	
//
Boolean	CAddressBookPane::CurrentBookIsPersonalBook()
{
	
	Boolean returnResult = false;
	if ( mContainer )
	{
		AB_ContainerAttribValue * value;
		AB_GetContainerAttribute( mContainer, attribContainerType ,& value); 
		if( value->u.containerType == AB_PABContainer )
			returnResult = true;
		AB_FreeContainerAttribValue( value );
	}
	return returnResult;
}


//------------------------------------------------------------------------------
//	¥	DrawCellContents
//------------------------------------------------------------------------------
//	Over ride CStandardFlexTable. If the cell is AB_attribEntryType an icon is 
// drawn. Otherwise a string is drawn
//
void CAddressBookPane::DrawCellContents(const STableCell &inCell, const Rect &inLocalRect)
{
	ResIDT icon = 0;
	CStr255 displayString;
	GetCellDisplayData ( inCell, icon, displayString );
	if ( icon )
	{
		ResIDT iconID = GetIconID(inCell.row);
		IconTransformType transformType = kTransformNone;
		
		if (iconID)
			DrawIconFamily(iconID, 16, 16, transformType, inLocalRect);
	}
	else
	{
		
		DrawTextString( displayString , &mTextFontInfo, 0, inLocalRect);
	}
	if (inCell.row == mDropRow)
				::InvertRect(&inLocalRect);		
} 

void CAddressBookPane::GetCellDisplayData(const STableCell &inCell, ResIDT& ioIcon, CStr255 &ioDisplayString )
{
	
	AB_AttribID attrib = GetAttribForColumn( inCell.col  );
	if ( attrib == AB_attribEntryType )
	{
		ioIcon = GetIconID( inCell.row );
	}
	else
	{
	
		StABEntryAttribute value ( GetMessagePane(), inCell.row  , attrib );
		ioDisplayString = value.GetChar();
	}
}

//------------------------------------------------------------------------------
//	¥	GetIconID
//------------------------------------------------------------------------------
//	Over ride CStandardFlexTable. 
//
ResIDT CAddressBookPane::GetIconID(TableIndexT inRow) const
{
	ResIDT iconID = 0;

	StABEntryAttribute value ( GetMessagePane(), inRow , AB_attribEntryType );
	
	switch( value.GetEntryType() )
	{
		case AB_Person:
			iconID = ics8_Person;
			break;
		
		case AB_MailingList:
			iconID = ics8_List;
			break;
			
		default:
			Assert_( 0 ); // Shouldn't be happening
			break;
	};

	return iconID;
}

//------------------------------------------------------------------------------
// ¥	DrawCellText
//------------------------------------------------------------------------------
// Gets the given attribute string for the cell and draws it in the passed in the passed in Rect
//
void CAddressBookPane::DrawCellText( const STableCell& inCell, const Rect& inLocalRect, AB_AttribID inAttrib )
{

	StABEntryAttribute value ( GetMessagePane(), inCell.row  , inAttrib );

	DrawTextString( value.GetChar(), &mTextFontInfo, 0, inLocalRect);
	if (inCell.row == mDropRow)
				::InvertRect(&inLocalRect);				
}



//------------------------------------------------------------------------------
//	¥ AddRowDataToDrag
//------------------------------------------------------------------------------
//	Adds the type text to the drag. The text data is the full email address (ie "John Doe <jdoe@mine.com>" )
// 	
void CAddressBookTableView::AddRowDataToDrag(TableIndexT inRow, DragReference inDragRef)
{
#if 0
	char* fullName = GetFullAddress( inRow );
	if( fullName )
	{
		Size  size  = XP_STRLEN ( fullName );
		OSErr err = ::AddDragItemFlavor(inDragRef, inRow, 'TEXT', 
										fullName, size, 0 );
		XP_FREEIF( fullName );
		FailOSErr_(err);
	}
#else
#pragma unused(inRow)
#pragma unused(inDragRef)
#endif
}


//------------------------------------------------------------------------------
//	¥	SortCommandFromColumnType
//------------------------------------------------------------------------------
//	Convert a FE column type into an AB_CommandType 
//
AB_CommandType CAddressBookPane::SortCommandFromColumnType(EColType inColType)
{
	Int32 index  = inColType - eTableHeaderBase;
	return mColumnSortCommand[ index ];
}


//------------------------------------------------------------------------------
//	¥	GetAttribForColumn
//------------------------------------------------------------------------------
//	Converts a FE column enumeration into a AB_AttribID
//
AB_AttribID CAddressBookPane::GetAttribForColumn( TableIndexT col )
{
	STableCell cell (1, col );
	PaneIDT id =   GetCellDataType(cell) ;
	Int32 index = id - eTableHeaderBase;
	return mColumnAttribID[ index ];
}


//------------------------------------------------------------------------------
//	¥	FindCommandStatus
//------------------------------------------------------------------------------
// Overrides LCommander
//	
void CAddressBookPane:: FindCommandStatus(CommandT inCommand, Boolean &outEnabled,
									  Boolean &outUsesMark, Char16 &outMark,
									  Str255 outName)
{

	CStandardFlexTable::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);

}


//------------------------------------------------------------------------------
//	¥	ObeyCommand
//------------------------------------------------------------------------------
// Overrides LCommander
//
Boolean CAddressBookPane::ObeyCommand(CommandT inCommand, void *ioParam)
{
	Boolean rtnVal = true;

	switch ( inCommand ) {
	
		case CAddressBookPane::eCol0:
		case CAddressBookPane::eCol1:
		case CAddressBookPane::eCol2:
		case CAddressBookPane::eCol3:
		case CAddressBookPane::eCol4:
		case CAddressBookPane::eCol5:
			GetTableHeader()->SimulateClick(inCommand);
			rtnVal = true;
			break;
	
		case CAddressBookPane::cmd_SortAscending:
		case CAddressBookPane::cmd_SortDescending:
			GetTableHeader()->SetSortOrder(inCommand == CAddressBookPane::cmd_SortDescending);
			break;
			
		default:
			AB_CommandType abCommand = UAddressBookUtilites::GetABCommand( inCommand);
			rtnVal = UAddressBookUtilites::ABCommand( this,  abCommand) == AB_SUCCESS ;
			if ( !rtnVal )
				rtnVal = Inherited::ObeyCommand(inCommand, ioParam);
			break;
	}
	
	return rtnVal;
}

//------------------------------------------------------------------------------
//	¥	OpenRow
//------------------------------------------------------------------------------
//	Overrides CStandardFlexTable
//	Delegates to ObeyCommand
//
void CAddressBookPane::OpenRow(TableIndexT inRow)
{
	ObeyCommand( UAddressBookUtilites::cmd_EditProperties, (void*)inRow );
}

//------------------------------------------------------------------------------
//	¥	DeleteSelection
//------------------------------------------------------------------------------
//	Overrides CStandardFlexTable
//	Delegates to ObeyCommand
//
void CAddressBookPane::DeleteSelection()
{
	ObeyCommand( UAddressBookUtilites::cmd_DeleteEntry, nil  );
}


//------------------------------------------------------------------------------
//	¥	DestroyMessagePane
//------------------------------------------------------------------------------
//
void CAddressBookPane::DestroyMessagePane(MSG_Pane* inPane)
{
	if ( inPane != nil )
	{
		::SetCursor(*::GetCursor(watchCursor));	// Could take forever
		AB_ClosePane( inPane );
	}
}


//-----------------------------------
Boolean CAddressBookPane::ItemIsAcceptable(
	DragReference	inDragRef, 
	ItemReference 	inItemRef	)
//-----------------------------------
{
	FlavorFlags flavorFlags;
	if (::GetFlavorFlags(inDragRef, inItemRef, mDragFlavor, &flavorFlags) == noErr)
	{
		CMailSelection selection;
		if (!mIsInternalDrop && GetSelectionFromDrag(inDragRef, selection))
			mIsInternalDrop = (selection.xpPane == GetMessagePane());
		return true;
	}
	return false;			
} // CMessageFolderView::ItemIsAcceptable

//------------------------------------------------------------------------------
// ¥ RowCanAcceptDrop
//------------------------------------------------------------------------------
//
Boolean CAddressBookPane::RowCanAcceptDrop( DragReference inDragRef, TableIndexT inDropRow)
{
	Boolean dropOK = false;

	SInt16 modifiers;
	::GetDragModifiers(inDragRef, NULL, &modifiers, NULL);
	Boolean doCopy = ((modifiers & optionKey) != 0);
	CMailSelection selection;
	AB_DragEffect effect;
	AB_DragEffect desiredAction = AB_Default_Drag;
	if ( doCopy )
		desiredAction = AB_Require_Copy;
	if (inDropRow >= 1 && inDropRow <= mRows)
	{
		if (!GetSelectionFromDrag(inDragRef,  selection ))
			return false;	// Should handle text drags
		AB_ContainerInfo* container = GetContainer( inDropRow );
		effect = AB_DragEntriesIntoContainerStatus(
			 GetMessagePane(), selection.GetSelectionList(), selection.selectionSize, container, desiredAction );
	}
	
	if ( effect == AB_Require_Copy)
		doCopy = true;
	
	if ( effect > 0 )
		dropOK = true;
		
	if (dropOK && doCopy)
	{
		CursHandle copyDragCursor = ::GetCursor(6608); // finder's copy-drag cursor
		if (copyDragCursor != nil)
			::SetCursor(*copyDragCursor);
	}
	else
		::SetCursor(&qd.arrow);

	return dropOK;
}


//------------------------------------------------------------------------------
//	¥ RowCanAcceptDropBetweenAbove
//------------------------------------------------------------------------------
//
Boolean CAddressBookPane::RowCanAcceptDropBetweenAbove(
	DragReference	inDragRef,
	TableIndexT		inDropRow)
{
	if (inDropRow >= 1 && inDropRow <= mRows)
	{
		AB_DragEffect effect;
		AB_DragEffect desiredAction = AB_Default_Drag;
		CMailSelection selection;
		if (GetSelectionFromDrag(inDragRef, selection))
		{
			AB_ContainerInfo* container = GetContainer( inDropRow );
			
			effect = AB_DragEntriesIntoContainerStatus(
				GetMessagePane(), selection.GetSelectionList(), selection.selectionSize, container, desiredAction );
			if( effect > 0 )
				return true;
		}
	}
	return false;
} 


//------------------------------------------------------------------------------
//	¥ ReceiveDragItem
//------------------------------------------------------------------------------
//	Drags can potentially bring up dialogs which will hang the machine. So instead of
//	doing the drag right away do it during SpendTime
//
void CAddressBookPane::ReceiveDragItem(
	DragReference		inDragRef,
	DragAttributes		/* inDragAttrs */,
	ItemReference		/* inItemRef */,
	Rect&				/* inItemBounds */)
{
	mDelayedDragInfo = new SAddressDelayedDragInfo;
	if ( GetSelectionFromDrag(inDragRef, mDelayedDragInfo->mDragSelection) )
	{
		mDelayedDragInfo->mDragContainer = GetContainer( mDropRow );
		mDelayedDragInfo->mDragRequest = AB_Default_Drag;
		
		StartIdling();
	}
	// Should handle text drag
};


//------------------------------------------------------------------------------
//	¥ SpendTime
//------------------------------------------------------------------------------
//	See ReceiveDragItem
//
void CAddressBookPane::SpendTime(const EventRecord &/* inMacEvent*/)
{
	if ( mDelayedDragInfo )
	{
		
		 AB_DragEntriesIntoContainer( 
								mDelayedDragInfo->mDragSelection.xpPane,
								mDelayedDragInfo->mDragSelection.GetSelectionList(),
								mDelayedDragInfo->mDragSelection.selectionSize,
								 mDelayedDragInfo->mDragContainer, 
								 mDelayedDragInfo->mDragRequest ) ;
		delete mDelayedDragInfo;
		mDelayedDragInfo = NULL;
		
	}
	
	StopIdling();
}


//------------------------------------------------------------------------------
//	¥	PaneChanged
//------------------------------------------------------------------------------
//
void CAddressBookPane::PaneChanged(MSG_Pane *inPane,
										MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
										int32 value)
{
	switch (inNotifyCode)
	{
		case MSG_PaneNotifyStartSearching:
			EnableStopButton(true);
			break;

		case MSG_PaneNotifyStopSearching:
			EnableStopButton(false);
			break;
		
		case MSG_PaneNotifyTypeDownCompleted:
			if( value != MSG_VIEWINDEXNONE )
			{
				UnselectAllCells();
				STableCell cell(value + 1, 1);
				SelectCell( cell );
				ScrollCellIntoFrame( cell );
			}
		break;
			
		// Not sure if I will still get this message
		case MSG_PaneDirectoriesChanged:
			BroadcastMessage( MSG_PaneDirectoriesChanged, NULL );
			break;
		default:
			Inherited::PaneChanged(inPane, inNotifyCode, value);
			break;
	}
}

//------------------------------------------------------------------------------
//	¥	SetCellExpansion
//------------------------------------------------------------------------------
//
void CAddressBookPane::SetCellExpansion( const STableCell& inCell,  Boolean inExpanded)
{
	Boolean currentlyExpanded;
	if (!CellHasDropFlag(inCell, currentlyExpanded) || (inExpanded == currentlyExpanded))
		return;
	ToggleExpandAction(inCell.row);
}

#pragma mark -
//------------------------------------------------------------------------------
//	¥	CAddressBookTableView
//------------------------------------------------------------------------------
//	Table for showing an  addressbook contents

//------------------------------------------------------------------------------
//	¥	~CAddressBookTableView
//------------------------------------------------------------------------------
//
CAddressBookTableView::~CAddressBookTableView()
{
	SetMessagePane( NULL );
}


//------------------------------------------------------------------------------
//	¥	LoadAddressBook
//------------------------------------------------------------------------------
//	Loads the given container into the table
//	Will create initialize the MSG_Pane if necessary
//
Boolean CAddressBookTableView::LoadAddressBook(AB_ContainerInfo *inContainer, MWContext* inContext )
{
	
	Try_ {
		::SetCursor(*::GetCursor(watchCursor));	// Could take forever
		if ( GetMessagePane() != nil )
		{
			CAddressBookManager::FailAddressError( AB_ChangeABContainer( GetMessagePane(), inContainer) );
			mContainer = inContainer;
			mTableHeader->SetSortOrder(false);
		}
		else
		{
			
			
			MSG_Pane* pane;
			CAddressBookManager::FailAddressError(
				AB_CreateABPane( &pane, inContext, CMailNewsContext::GetMailMaster() )
				);
			SetMessagePane( pane );
			
			uint32 pageSize = 25;// Fix this. Should be calculated based on window size and recomputed when the window resizes
			AB_SetFEPageSizeForPane(  pane, pageSize);   
			MSG_SetFEData((MSG_Pane *) GetMessagePane(), CMailCallbackManager::Get());
			mContainer = inContainer;
			CAddressBookManager::FailAddressError( AB_InitializeABPane( GetMessagePane(), inContainer ) );
			SetColumnHeaders(  );
			
			// Register callbacks
			CAddressBookManager::FailAddressError(
				AB_SetShowPropertySheetForEntryFunc ( GetMessagePane(), MacFe_ShowModelessPropertySheetForAB2 ));
		}
	}
	Catch_(inErr) {
		SetMessagePane( NULL );
		mContainer = NULL; 
		Throw_(inErr);
	}
	EndCatch_
	
	return true;
} 


//------------------------------------------------------------------------------
//	¥	ConferenceCall
//------------------------------------------------------------------------------
//	Needs to be update for new addressbook API's
//
void	CAddressBookTableView::ConferenceCall( )
{
#if 0
	TableIndexT selectedRow = 0;
	GetNextSelectedRow( selectedRow );
	if ( !CurrentBookIsPersonalBook() ) 
		return;

	ABID id = GetEntryID( selectedRow );
	char emailAddress[kMaxEmailAddressLength];
	char ipAddress[kMaxCoolAddress];
	short serverType;
	AB_GetUseServer( sCurrentBook, CAddressBookManager::GetAddressBook(), id, &serverType );
	AB_GetEmailAddress( sCurrentBook, CAddressBookManager::GetAddressBook(), id, emailAddress );
	AB_GetCoolAddress(  sCurrentBook, CAddressBookManager::GetAddressBook(), id, ipAddress );
	
	// Check to see if we have all the data we need
	if ( (serverType==kSpecificDLS || serverType==kHostOrIPAddress ) && !XP_STRLEN( ipAddress ))
	{
		FE_Alert( CAddressBookWindow::GetMailContext(), XP_GetString(MK_MSG_CALL_NEEDS_IPADDRESS));
		return;
	}
	
	if ( (serverType==kSpecificDLS || serverType==kDefaultDLS) && !XP_STRLEN( emailAddress ))
	{
		FE_Alert( CAddressBookWindow::GetMailContext(), XP_GetString(MK_MSG_CALL_NEEDS_EMAILADDRESS));
		return;
	}
	
	char *ipAddressDirect = NULL;
	char *serverAddress = NULL;
	char *address = NULL;  
	switch( serverType)
	{
		case kDefaultDLS:
			address = &emailAddress[0];		
			break;
		case kSpecificDLS:
			address = &emailAddress[0];
			serverAddress = &ipAddress[0];
			break;
		case kHostOrIPAddress:
			ipAddressDirect = &ipAddress[0];
			break;
		default:
			break;
	}
	// And now the AE fun begins
	AppleEvent event,reply;
	OSErr error;
	ProcessSerialNumber targetPSN;
	Boolean isAppRunning;
	static const OSType kConferenceAppSig = 'Ncq¹'; // This needs to be in a header file 
													// so that uapp and us share it
	isAppRunning = UProcessUtils::ApplicationRunning( kConferenceAppSig, targetPSN );
	if( !isAppRunning)
	{
		ObeyCommand(cmd_LaunchConference, NULL);
		isAppRunning = UProcessUtils::ApplicationRunning( kConferenceAppSig, targetPSN );
	}
	
	if( !isAppRunning ) // for some reason we were unable to open app
			return;
	Try_
	{
		error = AEUtilities::CreateAppleEvent( 'VFON', 'CALL', event, targetPSN );
		ThrowIfOSErr_(error);	
			
		if( ipAddressDirect)
 		{
 			error = ::AEPutParamPtr(&event, '----', typeChar, ipAddressDirect, XP_STRLEN(ipAddressDirect) );
 			ThrowIfOSErr_(error);
 		}
 		
 		if( address)
 		{
 			error = ::AEPutParamPtr(&event, 'MAIL', typeChar, address, XP_STRLEN(address) );
 			ThrowIfOSErr_(error);
 		}
 		
 		if( serverAddress )
 		{
 			error = ::AEPutParamPtr(&event, 'SRVR', typeChar, serverAddress, XP_STRLEN(serverAddress) );
 			ThrowIfOSErr_(error);
 		}
 	
		// NULL reply parameter
		reply.descriptorType = typeNull;
		reply.dataHandle = nil;
		
		error = AESend(&event,&reply,kAENoReply + kAENeverInteract 
			+ kAECanSwitchLayer+kAEDontRecord,kAENormalPriority,-1,nil,nil);
		ThrowIfOSErr_(error);
		
		UProcessUtils::PullAppToFront( targetPSN );
		
	}
	Catch_(inError) // in case of errors do nothing
 	{
 	}
	AEDisposeDesc(&reply);
	AEDisposeDesc(&event);
#endif
}

//------------------------------------------------------------------------------
//	¥	SetColumnHeaders
//------------------------------------------------------------------------------
//	Updates column headers for a new container
//
void CAddressBookTableView::SetColumnHeaders( )
{
	
	LTableViewHeader*		tableHeader = GetTableHeader();
	PaneIDT					headerPaneID;

	for (short col = 1; col < tableHeader->CountColumns(); col ++)
	{
		headerPaneID = tableHeader->GetColumnPaneID(col);
		Int32 index = headerPaneID - eTableHeaderBase+1;
		AB_ColumnInfo *info = AB_GetColumnInfo(	mContainer, AB_ColumnID( index) );
		mColumnAttribID[ col ] = info->attribID;
			
		LCaption* headerPane = dynamic_cast<LCaption*>(FindPaneByID(headerPaneID));
		
		if (headerPane)
		{
			
			headerPane->SetDescriptor( CStr255 (info->displayString) );
		}
		AB_FreeColumnInfo( info);
	}
}


//------------------------------------------------------------------------------
//	¥	ChangeSort
//------------------------------------------------------------------------------
// called from LTableHeader to change the column that is sorted on
//
void CAddressBookTableView::ChangeSort(const LTableHeader::SortChange *inSortChange)
{
	Assert_(GetMessagePane() != nil);

	AB_CommandType sortCmd = SortCommandFromColumnType((EColType) inSortChange->sortColumnID);
	
	::SetCursor(*::GetCursor(watchCursor));

	// Call BE to sort the table
	
	CAddressBookManager::FailAddressError(AB_CommandAB2( GetMessagePane(), sortCmd, nil, 0));
}


//------------------------------------------------------------------------------
//	¥	UpdateToTypedownText
//------------------------------------------------------------------------------
//
void CAddressBookTableView::UpdateToTypedownText(CStr255  inTypedownText  )
{
	CAddressBookManager::FailAddressError(
		AB_TypedownSearch(  GetMessagePane(), inTypedownText,  MSG_VIEWINDEXNONE)  );
}


//------------------------------------------------------------------------------
//	¥	ChangeFinished
//------------------------------------------------------------------------------
// If LDAP searching, pass the results to the XP layer before updating
//
void CAddressBookTableView::ChangeFinished(MSG_Pane *inPane, MSG_NOTIFY_CODE inChangeCode,
							  			   TableIndexT inStartRow, SInt32 inRowCount) {
	
	// If it's an initial insert call, and we're doing LDAP search, then simply pass
	//	this info on to the addressbook backend.  We'll get a MSG_NotifyAll when
	//	the backend thinks we should redraw. 
	
	if ( /* (mMysticPlane < (kMysticUpdateThreshHold + 2)) && */
		 (inChangeCode == MSG_NotifyInsertOrDelete) && 
		 IsLDAPSearching() ) {
		
		AB_LDAPSearchResultsAB2( GetMessagePane(), inStartRow - 1, inRowCount);
	}
	
	
	if( inChangeCode == MSG_NotifyLDAPTotalContentChanged )
	{
		Inherited::ChangeFinished( inPane,  MSG_NotifyScramble,	0, 0 );	
		ScrollRowIntoFrame( inStartRow -1 );
	}
	else 
		Inherited::ChangeFinished(inPane, inChangeCode, inStartRow, inRowCount);
 	//	Because the search is asynchronous, we
	//	don't want to cause a redraw every time a row is inserted, because that causes
	//	drawing of rows in random, unsorted order.

	if (inChangeCode == MSG_NotifyInsertOrDelete && IsLDAPSearching())
		DontRefresh();
	else
		Refresh();
	
}

//------------------------------------------------------------------------------
//	¥	EnableStopButton
//------------------------------------------------------------------------------
//	
void CAddressBookTableView::EnableStopButton(Boolean inBusy)
{
	SetLDAPSearching( inBusy );
	Inherited::EnableStopButton( inBusy);
}




#pragma mark -
//------------------------------------------------------------------------------
//	¥	CMailingListTableView
//------------------------------------------------------------------------------
//	Handle the table in the mailing list window

//------------------------------------------------------------------------------
//	¥ ~CMailingListTableView
//------------------------------------------------------------------------------
//
CMailingListTableView::~CMailingListTableView()
{
	SetMessagePane( NULL );
}


//------------------------------------------------------------------------------
//	¥	DestroyMessagePane
//------------------------------------------------------------------------------
//	 The Window is responsible for disposing the pane since it is the owner 
//
void CMailingListTableView::DestroyMessagePane( MSG_Pane* /* inPane */ )
{
}


//------------------------------------------------------------------------------
//	¥ 	LoadMailingList
//------------------------------------------------------------------------------
//	Initializes the MailingList
//
void CMailingListTableView::LoadMailingList(MSG_Pane* inPane)
{
	SetMessagePane( inPane );
	MSG_SetFEData((MSG_Pane *) GetMessagePane(), CMailCallbackManager::Get());	
	CAddressBookManager::FailAddressError( AB_InitializeMailingListPaneAB2( GetMessagePane() ) );	
}

//------------------------------------------------------------------------------
//	¥	GetContainer
//------------------------------------------------------------------------------
//
AB_ContainerInfo* 	CMailingListTableView::GetContainer( TableIndexT /* inRow */ )
{
	return AB_GetContainerForMailingList( GetMessagePane() ); 
}


//------------------------------------------------------------------------------
//	¥	DrawCellContents
//------------------------------------------------------------------------------
//	Over ride CStandardFlexTable. If the cell is AB_attribEntryType an icon is 
// drawn. Otherwise a string is drawn
//
void CMailingListTableView::GetCellDisplayData(const STableCell &inCell, ResIDT& ioIcon, CStr255 &ioDisplayString )
{
	AB_AttribID attrib = GetAttribForColumn( inCell.col );
	ioIcon = 0;
	if ( attrib == AB_attribEntryType )
	{
		ioIcon = GetIconID(inCell.row);
	}
	else
	{
		uint16 numItems = 1;
		AB_AttributeValue* value;
		CAddressBookManager::FailAddressError(
			 AB_GetMailingListEntryAttributes(GetMessagePane(), inCell.row-1, &attrib, &value, & numItems) ); 
		ioDisplayString = value->u.string ;
		AB_FreeEntryAttributeValue ( value );
	}
} 

#pragma mark -
//------------------------------------------------------------------------------
//	¥	CAddressBookContainerView
//------------------------------------------------------------------------------
// Handles displaying the container table

//------------------------------------------------------------------------------
//	¥	CAddressBookContainerView
//------------------------------------------------------------------------------
CAddressBookContainerView::CAddressBookContainerView(LStream *inStream) :
								CAddressBookPane(inStream), mDirectoryRowToLoad( 1 )
{
};


//------------------------------------------------------------------------------
//	¥	~CAddressBookContainerView
//------------------------------------------------------------------------------
CAddressBookContainerView::~CAddressBookContainerView()
{
	// This must be done here in order to use the correct DestroyMessagePaneFunction
	SetMessagePane( NULL ); 
}


//------------------------------------------------------------------------------
//	¥	Setup
//------------------------------------------------------------------------------
//
void CAddressBookContainerView::Setup( MWContext* inContext )
{
	MSG_Pane *msgPane;
	Assert_( inContext );
	CAddressBookManager::FailAddressError (
		AB_CreateContainerPane( &msgPane, inContext, CMailNewsContext::GetMailMaster() ) );
	SetMessagePane( msgPane );
	// I think this should be done in set pane
	MSG_SetFEData( (MSG_Pane *) msgPane, CMailCallbackManager::Get() );
	
	CAddressBookManager::FailAddressError (
		AB_InitializeContainerPane( GetMessagePane() ) );

	CAddressBookManager::FailAddressError(
				AB_SetShowPropertySheetForEntryFunc ( GetMessagePane(), MacFe_ShowModelessPropertySheetForAB2 ));
	
	CAddressBookManager::FailAddressError(
				AB_SetShowPropertySheetForDirFunc ( GetMessagePane(), MacFE_ShowPropertySheetForDir ));
	SelectRow( mDirectoryRowToLoad ) ;
}


//------------------------------------------------------------------------------
//	¥	CellHasDropFlag
//------------------------------------------------------------------------------
//
Boolean CAddressBookContainerView::CellHasDropFlag(
	const STableCell& 		inCell, 
	Boolean& 				outIsExpanded) const
{
	Int32 msgRow = inCell.row-1;
	Boolean returnValue = false;
	outIsExpanded = false;
	
	StABContainerAttribute value ( GetMessagePane(), inCell.row ,attribNumChildren );
	
	if ( value.GetNumber() ) 
	{
		returnValue = true;
		
		Int32 expansionDelta = MSG_ExpansionDelta ( GetMessagePane(), msgRow );
		if ( expansionDelta <= 0 )
			outIsExpanded = true;
	}

	return returnValue;
}

//------------------------------------------------------------------------------
//	¥	GetNestedLevel
//------------------------------------------------------------------------------
//
UInt16 CAddressBookContainerView::GetNestedLevel(TableIndexT inRow) const
{
	StABContainerAttribute value ( GetMessagePane(), inRow , attribDepth );
	return value.GetNumber();
} 


//------------------------------------------------------------------------------
//	¥	GetIconID
//------------------------------------------------------------------------------
//
ResIDT CAddressBookContainerView::GetIconID(TableIndexT inRow) const
{
	ResIDT iconID = 0;
	StABContainerAttribute value ( GetMessagePane(), inRow , attribContainerType );
	
	switch( value.GetContainerType() )
	{
		case AB_LDAPContainer:
			iconID = ics8_LDAP;
			break;
		
		case AB_MListContainer:
			iconID = ics8_List;
			break;
		
		case AB_PABContainer:
			iconID = ics8_PAB;
			break;
			
		default:
			Assert_( 0 ); // shouldn't be happening
			break;
	};

	return iconID;
}


//------------------------------------------------------------------------------
//	¥	DrawCellText
//------------------------------------------------------------------------------
//
void CAddressBookContainerView::DrawCellText( const STableCell& inCell, const Rect& inLocalRect )
{
	StABContainerAttribute value ( GetMessagePane(), inCell.row  , attribName );
	DrawTextString( value.GetChar(), &mTextFontInfo, 0, inLocalRect);
	// Is this a drop target
	if (inCell.row == mDropRow)
			::InvertRect(&inLocalRect);				
}


//------------------------------------------------------------------------------
//	¥	DrawCellContents
//------------------------------------------------------------------------------
//
void CAddressBookContainerView::DrawCellContents( const STableCell& inCell, const Rect& inLocalRect)
{
	//	Draw the Container Icon
	
	SInt16 iconRight = DrawIcons(inCell, inLocalRect);
	
	// Draw the Container name
	Rect textRect = inLocalRect;
	textRect.left = iconRight+2;
	DrawCellText( inCell, textRect );
} 

//------------------------------------------------------------------------------
//	¥	DrawIconsSelf
//------------------------------------------------------------------------------
//	CStandardflexTable applies an undesired transform when the row is selected
//
void CAddressBookContainerView::DrawIconsSelf(
	const STableCell& inCell, IconTransformType /*inTransformType*/, const Rect& inIconRect) const
{
	ResIDT iconID = GetIconID(inCell.row);
	if (iconID)
		DrawIconFamily(iconID, 16, 16, kTransformNone, inIconRect);
}

//------------------------------------------------------------------------------
//	¥	GetContainer
//------------------------------------------------------------------------------
//	Returns the currently selected container
//
AB_ContainerInfo* 	CAddressBookContainerView::GetContainer( TableIndexT inRow )
{
	StABContainerAttribute value ( GetMessagePane(), inRow , attribContainerInfo );
	return value.GetContainerInfo();
}

#pragma mark -
//------------------------------------------------------------------------------
//	¥	CAddressBookController
//------------------------------------------------------------------------------
//	Handles the interaction between the container pane, addressbook pane, 
//	the popup menu and the edit field

//------------------------------------------------------------------------------
//	¥	CAddressBookController
//------------------------------------------------------------------------------
//
CAddressBookController::CAddressBookController(LStream* inStream )
			:LView( inStream),
			mNextTypedownCheckTime(eDontCheckTypedown), 
			mTypedownName(nil),  mSearchButton(nil),mStopButton(nil)				  	   
{
};


//------------------------------------------------------------------------------
//	¥	~CAddressBookController
//------------------------------------------------------------------------------
//
CAddressBookController::~CAddressBookController()
{  
};


//------------------------------------------------------------------------------
//	¥	ReadStatus
//------------------------------------------------------------------------------
//
void	CAddressBookController::ReadStatus(LStream *inStatusData)
{
	mDividedView->RestorePlace(inStatusData);
	mAddressBookTable->GetTableHeader()->ReadColumnState(inStatusData);
	Int32 index;
	*inStatusData >> index;
	mABContainerView->SetIndexToSelectOnLoad( index );
}


//------------------------------------------------------------------------------
//	¥	WriteStatus
//------------------------------------------------------------------------------
//
void	CAddressBookController::WriteStatus(LStream *outStatusData)
{
	mDividedView->SavePlace(outStatusData);
	mAddressBookTable->GetTableHeader()->WriteColumnState(outStatusData);
	STableCell	cell;
	cell = mABContainerView->GetFirstSelectedCell();
	*outStatusData << Int32( cell.row); 
}

//------------------------------------------------------------------------------
//	¥	HandleKeyPress
//------------------------------------------------------------------------------
// Tab groups don't work since the commanders are not at the same level
//
Boolean  CAddressBookController::HandleKeyPress( const EventRecord	&inKeyEvent)
{
	Boolean	keyHandled = true;
	Char16	theChar = inKeyEvent.message & charCodeMask;
	
		// Process Tab or Shift-Tab. Pass up if there are any other
		// modifiers keys pressed.
	
	if ((theChar == char_Tab) && (( inKeyEvent.modifiers & ( cmdKey + optionKey + controlKey ) )== 0 ) )
	{
		Boolean backwards = (inKeyEvent.modifiers & shiftKey) != 0;
		LCommander * currentCommander = LCommander::GetTarget();
		int32 commanderToSelect = 0;
		LCommander* commanders[3];
		
		commanders[0] = dynamic_cast<LCommander* >(mTypedownName);
		commanders[1] = dynamic_cast<LCommander* >(mAddressBookTable);
		commanders[2] = dynamic_cast<LCommander* >(mABContainerView);
		Int32 i = 2;
		
		while( i>=0 )
		{
			if ( commanders[i] == currentCommander )
			{
				commanderToSelect = i + ( backwards ? -1 : 1 );
				break;
			} 
			i--;
		}
		commanderToSelect = (commanderToSelect+3)%3;
		Assert_( commanderToSelect >=0 && commanderToSelect<= 2 );
		SwitchTarget( commanders[ commanderToSelect ] );	
	}
	else
		keyHandled = LCommander::HandleKeyPress(inKeyEvent);

	return keyHandled;
}
		
//------------------------------------------------------------------------------
//	¥	FinishCreateSelf
//------------------------------------------------------------------------------
// Initializes a bunch of member variables, listens to the controls, and adds the target framer
//
void	CAddressBookController::FinishCreateSelf()
{
	mAddressBookTable = dynamic_cast<CAddressBookTableView *>( FindPaneByID( paneID_AddressBookTable ));
	FailNILRes_(mAddressBookTable);
	
	mABContainerView = dynamic_cast< CAddressBookContainerView*>( FindPaneByID( paneID_ContainerView ) );
	FailNILRes_( mABContainerView );
	mABContainerView->AddListener( this );
	
	mDividedView = dynamic_cast<LDividedView *>(FindPaneByID( paneID_DividedView ));
	FailNILRes_(mDividedView);
	
	mSearchButton = dynamic_cast<LGAPushButton *>(FindPaneByID( paneID_Search));
	FailNILRes_(mSearchButton);
	
	mStopButton = dynamic_cast<LGAPushButton *>(FindPaneByID( paneID_Stop));
	FailNILRes_(mStopButton);
	
	mTypedownName = dynamic_cast<CSearchEditField *>(FindPaneByID( paneID_TypedownName) );
	FailNILRes_(mTypedownName);
	mTypedownName->AddListener(this);
	
	mTypedownName->SetSuperCommander( mAddressBookTable );
	mAddressBookTable->SetSuperCommander(this );
	
	UReanimator::LinkListenerToControls(this, this, 8920);
	//
	CKeyStealingAttachment*	keyStealer = new CKeyStealingAttachment(mAddressBookTable);
	mTypedownName->AddAttachment(keyStealer);
	keyStealer->StealKey( char_UpArrow );
	keyStealer->StealKey( char_DownArrow );
	keyStealer->StealKey( char_PageUp );
	keyStealer->StealKey( char_PageDown );
	
	USearchHelper::SelectEditField(mTypedownName);
	// Frame Highlighting
	CTargetFramer* framer = new CTargetFramer();
	mAddressBookTable->AddAttachment(framer);
	
	framer = new CTargetFramer();
	mTypedownName->AddAttachment(framer);
	SetLatentSub( mTypedownName );
	
	framer = new CTargetFramer();
	mABContainerView->AddAttachment(framer);	
}


//------------------------------------------------------------------------------
//	¥	ListenToMessage
//------------------------------------------------------------------------------
//	
void	CAddressBookController::ListenToMessage(MessageT inMessage, void *ioParam)
{
	switch ( inMessage )
	{
		case CSearchEditField::msg_UserChangedText:
			// User changed the typedeown text
			/*if ( mNextTypedownCheckTime == eDontCheckTypedown )*/ {
				mNextTypedownCheckTime = LMGetTicks() + eCheckTypedownInterval;
			}
			break;
			
		case UAddressBookUtilites::cmd_NewAddressCard:
		case UAddressBookUtilites::cmd_NewAddressList:
		case UAddressBookUtilites::cmd_EditProperties:
		case UAddressBookUtilites::cmd_DeleteEntry:
		case UAddressBookUtilites::cmd_ComposeMailMessage:
			LCommander::GetTarget()->ProcessCommand( inMessage, NULL );
			break;
		
		case CStandardFlexTable::msg_SelectionChanged:
			// if the message is from the container view, load a new container into mAddressBookTableView
			if( ioParam == mABContainerView && mABContainerView->GetSelectedRowCount() == 1)
			{				
				StABContainerAttribute container( mABContainerView->GetMessagePane(), mABContainerView->GetTableSelector()->GetFirstSelectedRow(), attribContainerInfo );
				SelectDirectoryServer( container.GetContainerInfo() );
			}
			break;
		
		case UAddressBookUtilites::cmd_ConferenceCall:
			mAddressBookTable->ConferenceCall();
			break;
		
		// Status messages	
		case msg_NSCAllConnectionsComplete:
			StopSearch( );				
			break;

		case paneID_Search:
			Search();
			break;
			
		case paneID_Stop:
			StopSearch( );
			break;	
		
		default:
			// No superclass method
			break;
	}
	
}

//------------------------------------------------------------------------------
//	¥ ObeyCommand
//------------------------------------------------------------------------------
//
Boolean	CAddressBookController::ObeyCommand(CommandT inCommand, void *ioParam)
{
	Boolean cmdHandled = true;
	switch ( inCommand )
	{
		case paneID_Search:
			Search();
			break;
			
		case paneID_Stop:
			StopSearch( );
			break;
		
		case UAddressBookUtilites::cmd_HTMLDomains:
			MSG_DisplayHTMLDomainsDialog( CAddressBookWindow::GetMailContext() );
			break;
			
		case cmd_SecurityInfo:
			MWContext * context = CAddressBookWindow::GetMailContext();
			SECNAV_SecurityAdvisor( context, NULL );
			break;
			
		default:
			CAddressBookPane*	pane = mABContainerView;
			if ( mABContainerView->IsTarget() )
				pane = mAddressBookTable;
			AB_CommandType abCommand = UAddressBookUtilites::GetABCommand( inCommand);
			cmdHandled = UAddressBookUtilites::ABCommand( pane,  abCommand) == AB_SUCCESS ;
			if ( !cmdHandled )
				cmdHandled = LCommander::ObeyCommand(inCommand, ioParam);
			break;
	}	
	return cmdHandled;
}


//------------------------------------------------------------------------------
//	¥	SpendTime
//------------------------------------------------------------------------------
// If enought time has passed either initiate a LDAP search or perform typedown
//
void CAddressBookController::SpendTime(const EventRecord &inMacEvent)
{

	if ( inMacEvent.when >= mNextTypedownCheckTime )
	{
		Assert_(mTypedownName);
		Assert_(mAddressBookTable);
	
		CStr255 typedownText;
		mTypedownName->GetDescriptor(typedownText);
		if( typedownText.Length() > 0 )
		{
			mAddressBookTable->UpdateToTypedownText(typedownText);
		}
		mNextTypedownCheckTime = eDontCheckTypedown;
	}
}


//------------------------------------------------------------------------------
//	¥	SelectDirectoryServer
//------------------------------------------------------------------------------
//	Load the given AB_ContainerInfo into the AddressBookTableView and then update
// 	the popup menu
//
void CAddressBookController::SelectDirectoryServer( AB_ContainerInfo* inContainer )
{
	AssertFail_(mAddressBookTable != nil);
	SetCursor(*GetCursor(watchCursor));
	
	// Load server into address book table

	if ( !mAddressBookTable->LoadAddressBook(inContainer, mContext) )
		 return;
	DIR_Server* server = AB_GetDirServerForContainer( inContainer );
	
	const Boolean isLDAPServer = (server->dirType == LDAPDirectory);
	USearchHelper::EnableDisablePane(mSearchButton, isLDAPServer, true);
	
	USearchHelper::EnableDisablePane(mStopButton, false,true );
	
	mAddressBookTable->SetColumnHeaders();
	// Update the name caption
	LCaption* nameCaption = dynamic_cast<LCaption* >( FindPaneByID( paneID_DirectoryName ) );
	if ( nameCaption )
	{
		AB_ContainerAttribValue* value;
       	CAddressBookManager::FailAddressError( AB_GetContainerAttribute( 
              inContainer, attribName, &value) ); 
      	nameCaption->SetDescriptor( CStr255(value->u.string ) );
      	AB_FreeContainerAttribValue ( value );
	}
} 


//------------------------------------------------------------------------------
//	¥	Search
//------------------------------------------------------------------------------
//	handles the Search Button. Brings up a dialog and initiates a search
//
void CAddressBookController::Search()
{

	if ( GetAddressBookTable()->CurrentBookIsPersonalBook() ||
		 mAddressBookTable->IsLDAPSearching() ) return;
	StDialogHandler handler(8980, nil);

	CLDAPQueryDialog* dialog = dynamic_cast< CLDAPQueryDialog*>( handler.GetDialog() );
	Assert_( dialog );
	AB_ContainerInfo* container = mAddressBookTable->GetContainer( 0 ) ;
	
	DIR_Server* server = AB_GetDirServerForContainer( container );
	dialog->Setup(  mAddressBookTable->GetMessagePane(), server );
	
	
	Boolean doSearch = false;
	// Run the dialog
	MessageT message;
			
	do {
		message = handler.DoDialog();
	} while (message != paneID_Search && message != msg_Cancel);
	
	if ( message == paneID_Search )
	{
		CAddressBookManager::FailAddressError(AB_SearchDirectoryAB2( mAddressBookTable->GetMessagePane(), NULL));

		USearchHelper::EnableDisablePane(mSearchButton, false,true);
		USearchHelper::EnableDisablePane(mStopButton, true, true);
	}
}


//------------------------------------------------------------------------------
//	¥	StopSearch
//------------------------------------------------------------------------------
//
void CAddressBookController::StopSearch()
{

	if ( GetAddressBookTable()->CurrentBookIsPersonalBook() ||
		 !mAddressBookTable->IsLDAPSearching() ) return;
	AB_FinishSearchAB2(	mAddressBookTable->GetMessagePane()	);

	USearchHelper::EnableDisablePane(mStopButton, false,true);
	USearchHelper::EnableDisablePane(mSearchButton, true ,true);
	USearchHelper::SelectEditField(mTypedownName);
	Refresh();
}


void 	CAddressBookController::FindCommandStatus(CommandT inCommand, Boolean &outEnabled,
									  Boolean &outUsesMark, Char16 &outMark,
									  Str255 outName)
{
	AB_CommandType abCommand = UAddressBookUtilites::GetABCommand( inCommand);
	if ( abCommand != UAddressBookUtilites::invalid_command )
	{
		CAddressBookPane*	pane = mABContainerView;
		if ( mAddressBookTable->IsTarget() )
			pane = mAddressBookTable;
			
		UAddressBookUtilites::ABCommandStatus( 
			pane, abCommand, outEnabled, outUsesMark, outMark, outName);
		if( !outEnabled )
		{
			CAddressBookPane*	pane = mABContainerView;
			if ( mABContainerView->IsTarget() )
				pane = mAddressBookTable;	
			UAddressBookUtilites::ABCommandStatus( 
			pane, abCommand, outEnabled, outUsesMark, outMark, outName);
		}
	}
	else
	{
		switch ( inCommand )
		{
			case cmd_Stop:
				outEnabled = mAddressBookTable->IsLDAPSearching();
				break;
			case UAddressBookUtilites::cmd_HTMLDomains:
				outEnabled = true;
				break;
			
			default:
				LCommander::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
				break;
		}
	}
}

#endif // NEWADDR
