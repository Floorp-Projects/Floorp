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

// CAddressPickerWindow.cp

#include "CAddressPickerWindow.h"
// #include "CAddressBookWindows.h"
#ifdef MOZ_NEWADDR
#include "SearchHelpers.h"
#include "CAddressBookViews.h"
#include "xp_Help.h"
#include "UStdDialogs.h"
#include "libi18n.h"
#include "CProgressListener.h"
#include "StBlockingDialogHandler.h"
#include "CKeyStealingAttachment.h"

// Code in the tables depends on recieving key up events hence this class
class StKeyUpDialogHandler : public StBlockingDialogHandler
{
public:
	StKeyUpDialogHandler(
									ResIDT 			inDialogResID,
									LCommander*		inSuper): StBlockingDialogHandler( inDialogResID, inSuper) {};
	void EventKeyUp(const EventRecord	&inMacEvent)
	{
		/* if it's a CKeyUpReceiver, send the key up. We used to send keyups to
		 * targets if a browser view was on duty, which had two disadvantages: it required
		 * any potential targets in a browser view to handle a key up, and sometimes
		 * the commander chain would be incorrect so key ups were sent to a target in a 
		 * view that was not on duty.
		 */
		if (dynamic_cast<CKeyUpReceiver *>(sTarget))
			sTarget->ProcessKeyPress(inMacEvent);
		
	}
};

//------------------------------------------------------------------------------
//	¥	DoPickerDialog
//------------------------------------------------------------------------------
//	Bring up and runs the picker dialog
//
void CAddressPickerWindow::DoPickerDialog(  CComposeAddressTableView* initialTable )
{
	StKeyUpDialogHandler handler ( 8990, nil  );
	CAddressPickerWindow* dialog = dynamic_cast< CAddressPickerWindow* >( handler.GetDialog() );
	dialog->Setup( initialTable );
	dialog->Show();
	MessageT message;
	do 
	{
		message = handler.DoDialog();
	} while ( message != eOkayButton && message != eCancelButton ) ;
}

//------------------------------------------------------------------------------
//	¥	CAddressPickerWindow
//------------------------------------------------------------------------------
//
CAddressPickerWindow::CAddressPickerWindow(LStream *inStream)
		: Inherited( inStream ), CSaveWindowStatus( this ),  mInitialTable( nil ), mPickerAddresses(nil),
		 mAddressBookController(nil), mProgressListener( nil ), mMailNewsContext ( nil ), mLastAction ( eToButton )
{
}

//------------------------------------------------------------------------------
//	¥	~CAddressPickerWindow
//------------------------------------------------------------------------------
//
CAddressPickerWindow::~CAddressPickerWindow()
{
	if (mMailNewsContext)
		mMailNewsContext->RemoveUser(this);
	delete mProgressListener;
}


//------------------------------------------------------------------------------
//	¥	Setup
//------------------------------------------------------------------------------
// Copies takes in a CComposeAddressTableView and copies its data to the picker bucket
// The passed in table will be updated if necessary when the window is closed
//
void	CAddressPickerWindow::Setup( CComposeAddressTableView* initialTable )
{
	// Copy the old table to the new one
	Assert_( initialTable );
	Assert_( mPickerAddresses );
	
	TableIndexT numRows;
	mInitialTable = initialTable;
	initialTable->GetNumRows(numRows);
	STableCell cell;
	Uint32 size;
	char* address = NULL;
	for ( int32 i = 1; i <= numRows; i++ )
	{
		EAddressType type = initialTable->GetRowAddressType( i );
		cell.row = i;
		cell.col = 2;
		size = sizeof(address);
		initialTable->GetCellData(cell, &address, size);
		mPickerAddresses->InsertNewRow( type, address, false );
	}
}


//------------------------------------------------------------------------------
//	¥	FinishCreateSelf
//------------------------------------------------------------------------------
//		 		
void CAddressPickerWindow::FinishCreateSelf()
{
	Inherited::FinishCreateSelf();
	
	mAddressBookController = 
		dynamic_cast< CAddressBookController* >(FindPaneByID( CAddressBookWindow::paneID_AddressBookController) ) ;
	CSaveWindowStatus::FinishCreateWindow();
	
	// Context creation
	mMailNewsContext = new CMailNewsContext( MWContextAddressBook);
	if (!mProgressListener)
		mProgressListener = new CProgressListener(this, mMailNewsContext);
	ThrowIfNULL_(mProgressListener);
	// The progress listener should be "just a bit" lazy during network activity
	mProgressListener->SetLaziness(CProgressListener::lazy_NotAtAll);
	mMailNewsContext->AddListener(mProgressListener);
	mMailNewsContext->AddUser(this);
	
	mMailNewsContext->SetWinCSID(INTL_DocToWinCharSetID(mMailNewsContext->GetDefaultCSID()));
	
	// Have the AddressBook Controller listen to the   context
	mMailNewsContext->AddListener(mAddressBookController);
	mAddressBookController->SetContext( *mMailNewsContext );
	mAddressBookController->GetAddressBookContainerView()->Setup( *mMailNewsContext );
	mAddressBookController->GetAddressBookTable()->SetNotifyOnSelectionChange( true );
	mAddressBookController->GetAddressBookTable()->StartBroadcasting();
	
	mPickerAddresses = dynamic_cast< CComposeAddressTableView* >( FindPaneByID( 'Addr' ) );
	FailNILRes_( mPickerAddresses );
	
	(mAddressBookController->GetAddressBookTable() )->AddListener( this );
	UReanimator::LinkListenerToControls(this, this,CAddressPickerWindow:: res_ID);

// Setup KeySnatcher for our fancy default button behavior
	CKeyStealingAttachment*	keyStealer = new CKeyStealingAttachment(this);
	mAddressBookController->GetAddressBookTable()->AddAttachment(keyStealer);
	mAddressBookController->GetAddressBookContainerView()->AddAttachment(keyStealer );
	keyStealer->StealKey( char_Enter );
	keyStealer->StealKey( char_Return );
	
	
	// Adjust button state
	ListenToMessage (  CStandardFlexTable::msg_SelectionChanged, nil );
}


//------------------------------------------------------------------------------
//	¥	ListenToMessage
//------------------------------------------------------------------------------
//
void 	CAddressPickerWindow::ListenToMessage(MessageT inMessage, void * /* ioParam */ )
{
	switch( inMessage )
	{
		case eOkayButton:
			SaveStatusInfo();
			CComposeAddressTableStorage* oldTableStorage  =dynamic_cast< CComposeAddressTableStorage*> (mInitialTable->GetTableStorage() );
			mPickerAddresses->EndEditCell();
			mInitialTable->SetTableStorage( mPickerAddresses->GetTableStorage() );
			mInitialTable->Refresh();
			mPickerAddresses->SetTableStorage( oldTableStorage );
			break;
	
		case eHelp:
			ShowHelp( HELP_SELECT_ADDRESSES );
			break;
			
		case eToButton:
			mLastAction = eToButton;
			AddSelection ( eToType );
			break;
			
		case eCcButton:
			mLastAction = eCcButton;
			AddSelection ( eCcType );
			break;
			
		case eBccButton:
			mLastAction = eBccButton;
			AddSelection( eBccType );
			break;
			
		case ePropertiesButton:
			break;
			
		case CStandardFlexTable::msg_SelectionChanged:
			Boolean enable = mAddressBookController->GetAddressBookTable()->GetSelectedRowCount() >0;
			SetDefaultButton( mLastAction );			
			USearchHelper::EnableDisablePane( USearchHelper::FindViewControl( this ,eToButton ), enable, true );
			USearchHelper::EnableDisablePane( USearchHelper::FindViewControl( this ,eCcButton ), enable, true );
			USearchHelper::EnableDisablePane( USearchHelper::FindViewControl( this ,eBccButton ), enable, true );
			USearchHelper::EnableDisablePane( USearchHelper::FindViewControl( this ,ePropertiesButton ), enable, true );
			break;
		
		default:
			break;
	}
}

//------------------------------------------------------------------------------
//	¥	AddSelection
//------------------------------------------------------------------------------
// Adds the table selection to the address bucket.
// 
//
void CAddressPickerWindow::AddSelection( EAddressType inAddressType )
{
	char* address = NULL;
	Uint32 size= sizeof(address);
	TableIndexT row = 0;
	
	while ( mAddressBookController->GetAddressBookTable()->GetNextSelectedRow( row )  )
	{
		address = mAddressBookController->GetAddressBookTable()->GetFullAddress( row );		
		mPickerAddresses->InsertNewRow( inAddressType, address, false );
		XP_FREE( address );
	}
	
	SetDefaultButton( eOkayButton );
}

//------------------------------------------------------------------------------
//	¥	ReadWindowStatus
//------------------------------------------------------------------------------
//	Adjust the window to stored preferences.
//
void CAddressPickerWindow::ReadWindowStatus(LStream *inStatusData)
{
	if ( inStatusData != nil )
	{
		mAddressBookController->ReadStatus( inStatusData );
	}
	
}


//------------------------------------------------------------------------------
//	¥	WriteWindowStatus
//------------------------------------------------------------------------------
//	Write window stored preferences.
//
void CAddressPickerWindow::WriteWindowStatus(LStream *outStatusData)
{
	mAddressBookController->WriteStatus(outStatusData);
}

#endif //NEWADDR