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

#include "LMUCDialog.h"
#include "ListUtils.h"
#include <PP_Messages.h>
#include <LButton.h>

extern "C" {
#include "ppp.interface.h"
}

// ¥ÊLSingleClickListBox
void* LSingleClickListBox::CreateSingleClickListBox( LStream* inStream )
{
	return ( new LSingleClickListBox( inStream ) );
}

LSingleClickListBox::LSingleClickListBox( LStream* inStream ): LListBox( inStream )
{
	mSingleClickMessage = msg_Nothing;
}


void LSingleClickListBox::ClickSelf( const SMouseDownEvent& inMouseDown )
{
	if ( SwitchTarget( this ) )
	{
		FocusDraw(this);
		short		modifiers = inMouseDown.macEvent.modifiers & ~(shiftKey | cmdKey );
		if ( ::LClick( inMouseDown.whereLocal, modifiers, mMacListH ) ) 
			BroadcastMessage( mDoubleClickMessage, this );
		else
			BroadcastMessage( mSingleClickMessage, this );
	}
}

Boolean	LSingleClickListBox::HandleKeyPress( const EventRecord& inKeyEvent )
{
	if ( LListBox::HandleKeyPress( inKeyEvent ) )
	{
		BroadcastMessage( mSingleClickMessage, this );
		return true;
	}
	else
		return false;
}

void LSingleClickListBox::SetSingleClickMessage( MessageT inMessage )
{
	mSingleClickMessage = inMessage;
}

// ¥ÊLPPPListBox
void* LPPPListBox::CreatePPPListBox( LStream* inStream )
{
	return new LPPPListBox( inStream );
}

LPPPListBox::LPPPListBox( LStream* inStream ): LSingleClickListBox( inStream )
{
	mFunction = NULL;
}

void LPPPListBox::SetPPPFunction( TraversePPPListFunc p )
{
	mFunction = p;
}

void LPPPListBox::UpdateList()
{
	if ( mFunction )
	{
		ListHandle		listH;
		listH = this->GetMacListH();
		
		OSErr			err;
		Str255*			list;
		int				number;
		
		err = ( *mFunction )( &list );
		number = ::GetPtrSize( (Ptr)list ) / sizeof ( Str255 );
		SynchronizeRowsTo( listH, number );
		SynchronizeColumnsTo( listH, 1 );
		
		Cell			cell;
		cell.h = 0;	
		for ( cell.v = 0; cell.v < number; cell.v++ )
		{
			LStr255		text;
			text = *list;
			::LSetCell( (Ptr)&text[ 1 ], text.Length(), cell, listH );
			list++;
		}
		if ( list )
			DisposePtr( (Ptr)list );
		// ¥ deselect all cells
		this->SetValue( -1 );
	}
}

void LPPPListBox::SetToNamedItem( LStr255& name )
{
	Cell		searchAt;
	ListHandle	listH;
	
	searchAt.v = searchAt.h = 0;
	listH = this->GetMacListH();
	
	if ( ::LSearch( (Ptr)&name[ 1 ], name.Length(), NULL, &searchAt, listH ) )
		this->SetValue( searchAt.v );
}

Boolean LPPPListBox::FocusDraw( LPane* pane )
{
	const RGBColor	rgbWhite = { 0xFFFF, 0xFFFF, 0xFFFF };
	const RGBColor	rgbBlack = { 0x0000, 0x0000, 0x0000 };
	const RGBColor	rgbGray = { 0xBBBB, 0xBBBB, 0xBBBB };
	
	if ( LSingleClickListBox::FocusDraw( pane ) )
	{
		::RGBBackColor( &rgbWhite );
		::RGBForeColor( &rgbBlack );
		if ( !this->IsEnabled() )
			::RGBForeColor( &rgbGray );

//		if ( this->IsEnabled() )
//			::TextMode( srcCopy );
//		else
//			::TextMode( grayishTextOr );

		return true;
	}
	return false;
}


void LPPPListBox::DrawSelf()
{
	Rect	frame;
	
	this->CalcLocalFrameRect( frame );
	::EraseRect( &frame );
	LSingleClickListBox::DrawSelf();	
}

void LPPPListBox::EnableSelf()
{
	LSingleClickListBox::EnableSelf();
//	::LSetDrawingMode( true, mMacListH );
	::LActivate( true, mMacListH );
	this->Refresh();
	this->UpdatePort();
//	if ( FocusDraw() )
//		this->DrawSelf();
}

void LPPPListBox::DisableSelf()
{
	LSingleClickListBox::DisableSelf();
	::LActivate( false, mMacListH );
//	::LSetDrawingMode( false, mMacListH );
	this->Refresh();
	this->UpdatePort();
//	if ( FocusDraw() )
//		this->DrawSelf();
}

// ¥ÊLMUCDialog
void* LMUCDialog::CreateMUCDialogStream( LStream* inStream )
{
	return new LMUCDialog( inStream );
}

LMUCDialog::LMUCDialog( LStream* inStream ): LGADialogBox( inStream )
{
}

void LMUCDialog::SetPPPFunction( TraversePPPListFunc p )
{
	if ( mListBox )
		mListBox->SetPPPFunction( p );
}

void LMUCDialog::GetNewValue( LStr255& str )
{
	if ( mListBox )
		mListBox->GetDescriptor( str );
}

void LMUCDialog::UpdateList()
{
	if ( mListBox )
	{
		mListBox->UpdateList();
		this->UpdateButtonState();
	}
}

void LMUCDialog::UpdateButtonState()
{
	LButton*		okButton;
	okButton = (LButton*)this->FindPaneByID( mDefaultButtonID );
	if ( !okButton || !mListBox )
		return;
		
	if ( mListBox->GetValue() != -1 )
	{
		if ( !okButton->IsEnabled() )
			okButton->Enable();
	}
	else
	{
		if ( okButton->IsEnabled() )
			okButton->Disable();
	}
}
	
void LMUCDialog::ListenToMessage( MessageT inMessage, void* ioParam )
{
	LGADialogBox::ListenToMessage( inMessage, ioParam );
	
	if ( inMessage == msg_SingleClick )
		this->UpdateButtonState();
}

void LMUCDialog::FinishCreateSelf()
{
	LGADialogBox::FinishCreateSelf();
	
	mListBox = (LPPPListBox*)this->FindPaneByID( 'lstB' );

	mListBox->AddListener( this );
	mListBox->SetSingleClickMessage( msg_SingleClick );
	
	this->UpdateList();
}



void* LMUCEditDialog::CreateMUCEditDialogStream( LStream* inStream )
{
	return new LMUCEditDialog( inStream );
}

LMUCEditDialog::LMUCEditDialog( LStream* inStream ): LGADialogBox( inStream )
{
	mModemsList = NULL;
	mAccountsList = NULL;
	mLocationsList = NULL;
	mDisabled = NULL;
}

void LMUCEditDialog::UpdateLists()
{
	if ( mAccountsList )
		mAccountsList->UpdateList();
	if ( mModemsList )
		mModemsList->UpdateList();
	if ( mLocationsList )
		mLocationsList->UpdateList();
	this->UpdateButtonState();
}

void LMUCEditDialog::GetNewValues( LStr255& modem, LStr255& account, LStr255& location )
{
	if ( mDisabled && mDisabled->GetValue() )
	{
		modem = account = location = "";
	}
	else
	{
		if ( mAccountsList )
			mAccountsList->GetNameValue( account );
		if ( mModemsList )
			mModemsList->GetNameValue( modem );
		if ( mLocationsList )
			mLocationsList->GetNameValue( location );
	}
}

void LMUCEditDialog::SetInitialValue( const LStr255& modem, const LStr255& account, const LStr255& location )
{
	if ( mAccountsList )
		mAccountsList->SetToNamedItem( account );
	if ( mModemsList )
		mModemsList->SetToNamedItem( modem );
	if ( mLocationsList )
		mLocationsList->SetToNamedItem( location );

	LStr255		emptyString( "" );
	if ( modem == emptyString || account == emptyString || location == emptyString )
		mDisabled->SetValue( 1 );
	else
		mDisabled->SetValue( 0 );
}

void LMUCEditDialog::UpdateButtonState()
{
	LButton*		okButton;
	okButton = (LButton*)this->FindPaneByID( mDefaultButtonID );
	if ( !okButton )
		return;

	if ( mDisabled && mDisabled->GetValue() )
	{
		mModemsList->Disable();
		mAccountsList->Disable();
		mLocationsList->Disable();
		okButton->Enable();
		return;
	}
	else if ( mDisabled && !mDisabled->GetValue() )
	{
		mModemsList->Enable();
		mAccountsList->Enable();
		mLocationsList->Enable();
	}
	if (	mModemsList->GetValue() != -1 &&
			mAccountsList->GetValue() != -1 &&
			mLocationsList->GetValue() != -1 )
		okButton->Enable();
	else
		okButton->Disable();
}
	
void LMUCEditDialog::ListenToMessage( MessageT inMessage, void* ioParam )
{
	LGADialogBox::ListenToMessage( inMessage, ioParam );
	
	if ( inMessage == msg_SingleClick || inMessage == 9181 )
		this->UpdateButtonState();
}

void LMUCEditDialog::FinishCreateSelf()
{
	LGADialogBox::FinishCreateSelf();
	
	mAccountsList = (LPPPPopup*)this->FindPaneByID( 'popA' );
	mAccountsList->AddListener( this );
	//mAccountsList->SetSingleClickMessage( msg_SingleClick );
	mAccountsList->SetPPPFunction( GetAccountsList );
		
	mModemsList = (LPPPPopup*)this->FindPaneByID( 'popM' );
	mModemsList->AddListener( this );
	//mModemsList->SetSingleClickMessage( msg_SingleClick );
	mModemsList->SetPPPFunction( GetModemsList );
	
	mLocationsList = (LPPPPopup*)this->FindPaneByID( 'popL' );
	mLocationsList->AddListener( this );
	//mLocationsList->SetSingleClickMessage( msg_SingleClick );
	mLocationsList->SetPPPFunction( GetLocationsList );

	mDisabled = (LControl*)this->FindPaneByID( 'disC' );
	mDisabled->AddListener( this );
}
