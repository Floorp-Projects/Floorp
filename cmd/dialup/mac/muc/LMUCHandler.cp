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

#include "LMUCHandler.h"
#include "LMUCDialog.h"
#include <PP_Messages.h>
#include <UModalDialogs.h>
#include <URegistrar.h>
#include <LCaption.h>
#include <LString.h>
#include <LStdControl.h>
#include <stdio.h>
#include "muc.h"
//#include "profile.h"
#include "ListUtils.h"

extern "C" {
#include "ppp.interface.h"
}


#define		MODEM_SECTION		"[Modem]"
#define		MODEM_KEY			"Modem"
#define		ACCOUNT_SECTION		"[Account]"
#define		ACCOUNT_KEY			"Account"
#define		LOCATION_SECTION	"[Location]"
#define		LOCATION_KEY		"Location"

const MessageT msg_RunAccountSetup = 1025;
const MessageT cmd_AdvancedSettings = 4009;

const LStr255	kEmptyString( "" );

LMUCHandler::LMUCHandler(): LListener()
{
	mIniFile = NULL;
	mDirty = false;

	mAutoConfigCheck = NULL;
	mConfigSettings = NULL;
	mAdvancedButton = NULL;
	mConfigHelp = NULL;
	mLocationPopup = NULL;
	mAccountName = NULL;
	mModemName = NULL;
	mShowingHelp = false;
}

LMUCHandler::~LMUCHandler()
{
}

void LMUCHandler::GetDefaultConfiguration()
{
	GetCurrentAccountName( &mConfiguration.mAccountName );
	GetCurrentModemName( &mConfiguration.mModemName );
	GetCurrentLocationName( &mConfiguration.mLocationName );
}

void LMUCHandler::ClearConfiguration()
{
	mConfiguration.mAccountName[ 0 ] = 0;
	mConfiguration.mModemName[ 0 ] = 0;
	mConfiguration.mLocationName[ 0 ] = 0;
}

// • makes sure all the settings read from the configuration exist inside PPP,
// displays alerts for those that don't and resets them
void LMUCHandler::SetCurrentConfiguration()
{
	LStr255		temp;
	temp = mConfiguration.mModemName;
	this->SetupEntry( temp, GetModemsList, 130 );
	LString::CopyPStr( temp, mConfiguration.mModemName );
	
	temp = mConfiguration.mAccountName;
	this->SetupEntry( temp, GetAccountsList, 129 );
	LString::CopyPStr( temp, mConfiguration.mAccountName );
	
	temp = mConfiguration.mLocationName;
	this->SetupEntry( temp, GetLocationsList, 131 );
	LString::CopyPStr( temp, mConfiguration.mLocationName );
}

// • displays the edit dialog and allows editing of the settings
ExceptionCode LMUCHandler::EditProfile( FSSpec* profileParSpec )
{
	MessageT		ret;
	
	Try_
	{
		FSSpec			profile;
		this->GetDialConfigurationFile( *profileParSpec, profile );
		
		if ( mIniFile )
			delete mIniFile;
		
		mIniFile = new LWinIniFile;
		mIniFile->SetSpecifier( profile );
		LString::CopyPStr( profile.name, mConfiguration.mProfileName );
				
		StDialogHandler		handler( 133, NULL );
		LMUCEditDialog*		dlog = (LMUCEditDialog*)handler.GetDialog();

		dlog->UpdateLists();
		Try_
		{
			this->ReadConfiguration();
		}
		Catch_( inErr )
		{
			// • if we get an fnfErr, it means we don't have an configuration
			//	file and we should create one
			if ( inErr == fnfErr )
			{
				mIniFile->CreateNewDataFile( 'MOSS', 'TEXT', smSystemScript );
				this->GetDefaultConfiguration();
			}
		}
		EndCatch_
		
		dlog->SetInitialValue( mConfiguration.mModemName, mConfiguration.mAccountName, mConfiguration.mLocationName );
top:
		ret = handler.DoDialog();
		switch ( ret )
		{
			case msg_OK:
			{
				LStr255		modemName;
				LStr255		accountName;
				LStr255		locationName;
				dlog->GetNewValues( modemName, accountName, locationName );
				LString::CopyPStr( modemName, mConfiguration.mModemName );
				LString::CopyPStr( accountName, mConfiguration.mAccountName );
				LString::CopyPStr( locationName, mConfiguration.mLocationName );
				this->WriteConfiguration();
				return noErr;
			}
			break;
		
			case msg_Cancel:
				return errUserCancelledLaunch;
			break;

			case msg_RunAccountSetup:
				return errNeedToRunAccountSetup;
			break;
		
			default:
				goto top;
		}
	}
	Catch_( inErr )
	{
		return inErr;
	}
	EndCatch_
	
	return noErr;
}

// • this happens when the user clicks "ok" from the profile selector
ExceptionCode LMUCHandler::SelectProfile( FSSpec* profileParSpec, Boolean autoSelect )
{
	Try_
	{
		FSSpec		profile;

		if ( !autoSelect && mAutoConfigCheck && mAutoConfigCheck->GetValue() )
			return noErr;
			
		this->GetDialConfigurationFile( *profileParSpec, profile );
		
		ThrowIfNot_( this->ConnectionExists() == noErr );

		if ( mIniFile )
			delete mIniFile;
		
		mIniFile = new LWinIniFile;
		mIniFile->SetSpecifier( profile );
		LString::CopyPStr( profile.name, mConfiguration.mProfileName );

		this->ReadConfiguration();

		if ( !autoSelect && mLocationPopup )
			mLocationPopup->GetCurrentItemTitle( mConfiguration.mLocationName );
			
		this->SetCurrentConfiguration();
		if ( mConfiguration.mAccountName != kEmptyString )
			this->UpdateConfiguration();
		
		if ( mDirty )
			this->WriteConfiguration();
	}
	Catch_( inErr )
	{
		switch ( inErr )
		{
			case errProfileNotFound:
			case errNeedToRunAccountSetup:
			case errCannotSwitchDialSettings:
			case errUserCancelledLaunch:
				return inErr;
			case fnfErr:
			{
				mIniFile->CreateNewDataFile( 'MOSS', 'TEXT', smSystemScript );
				this->GetDefaultConfiguration();
				this->WriteConfiguration();
			}
			default:
				return errProfileNotFound;
		}
	}
	EndCatch_
	
	return noErr;
}

// • read the configuration in "profileParSpec" into a configuration struct
ExceptionCode LMUCHandler::GetProfile( const FSSpec* profileParSpec, FreePPPInfo* buffer )
{
	Try_
	{
		FSSpec		profile;
		this->GetDialConfigurationFile( *profileParSpec, profile );
		
		if ( mIniFile )
			delete mIniFile;
		
		mIniFile = new LWinIniFile;
		mIniFile->SetSpecifier( profile );
		
		this->ReadConfiguration();
		*buffer = mConfiguration;
	}
	Catch_( inErr )
	{
	}
	EndCatch_
	return noErr;
}

ExceptionCode LMUCHandler::SetProfile( const FSSpec* /*profileParSpec*/, const FreePPPInfo* /*buffer*/)
{
/*
	Try_
	{
		FSSpec		profile;
		this->GetDialConfigurationFile( *profileParSpec, profile );
		
		if ( mIniFile )
			delete mIniFile;
		
		mIniFile = new LWinIniFile;
		mIniFile->SetSpecifier( profile );
		
		this->ReadConfiguration();
		mConfiguration = *buffer;
		this->WriteConfiguration();
	}
	Catch_( inErr )
	{
	}
	EndCatch_
	return noErr;
*/
}
	
// • configure's PPP
void LMUCHandler::UpdateConfiguration()
{
	Str255		current;
		
	// • go set the PPP prefs
	GetCurrentAccountName( &current );
	if ( !::IdenticalString( current, mConfiguration.mAccountName, NULL ) )
		SetCurrentAccountName( &mConfiguration.mAccountName );

	GetCurrentModemName( &current );
	if ( !::IdenticalString( current, mConfiguration.mModemName, NULL ) )
		SetCurrentModemName( &mConfiguration.mModemName );

	GetCurrentLocationName( &current );
	if ( !::IdenticalString( current, mConfiguration.mLocationName, NULL ) )
		SetCurrentLocationName( &mConfiguration.mLocationName );
}


ExceptionCode LMUCHandler::HandleDialog( TraversePPPListFunc inFunc, ResIDT inDlogID,
	LStr255& outListItemPicked )
{
	MessageT		ret;
	StDialogHandler	handler( inDlogID, NULL );
	LMUCDialog*		dlog = (LMUCDialog*)handler.GetDialog();
		
	dlog->SetPPPFunction( inFunc );
	dlog->UpdateList();
	
top:
	ret = handler.DoDialog();
	switch ( ret )
	{
		case msg_OK:
			dlog->GetNewValue( outListItemPicked );
			return noErr;
		break;
		
		case msg_Cancel:
			return errUserCancelledLaunch;
		break;

		case msg_RunAccountSetup:
			return errNeedToRunAccountSetup;
		break;
		
		default:
			goto top;
	}
	return noErr;
}

// • reads the settings from the configuration file
void LMUCHandler::ReadConfiguration()
{
	this->ClearConfiguration();

	ThrowIfNot_( mIniFile );
	
	// • OpenDataFile will throw if the file isn't found…	
	mIniFile->OpenDataFork( fsRdPerm );
	
	LStr255		temp;
	
	this->GetEntry( temp, ACCOUNT_SECTION, ACCOUNT_KEY );
	LString::CopyPStr( temp, mConfiguration.mAccountName );
	this->GetEntry( temp, MODEM_SECTION, MODEM_KEY );
	LString::CopyPStr( temp, mConfiguration.mModemName );
	this->GetEntry( temp, LOCATION_SECTION, LOCATION_KEY );
	LString::CopyPStr( temp, mConfiguration.mLocationName );
	mIniFile->CloseDataFork();
}
		
ExceptionCode LMUCHandler::WriteConfiguration()
{
	if ( !mIniFile )
		return -1;
		
	Try_
	{
		mIniFile->CloseDataFork();
	}
	Catch_( inErr )
	{
		// • don't need to do anything
	}

	Try_
	{
		mIniFile->OpenDataFork( fsWrPerm );
		mIniFile->SetLength( 0 );
		
		*mIniFile << ACCOUNT_SECTION"\r" << ACCOUNT_KEY << "=" << (StringPtr)mConfiguration.mAccountName << "\r";
		*mIniFile << MODEM_SECTION << "\r" << MODEM_KEY << "=" << (StringPtr)mConfiguration.mModemName << "\r";
		*mIniFile << LOCATION_SECTION << "\r" << LOCATION_KEY << "=" << (StringPtr)mConfiguration.mLocationName << "\r";

		mIniFile->CloseDataFork();
		mDirty = false;
	}
	Catch_( inErr )
	{
		mIniFile->CloseDataFork();
		return errCannotSwitchDialSettings;
	}
	return noErr;
}

// • call this when you want to get the value for an entry out of the configuration file
ExceptionCode LMUCHandler::GetEntry( LStr255& outValue, const LStr255& inSectionName, const LStr255& inKeyName )
{
	ExceptionCode		err;
	
	ThrowIfNot_( mIniFile && ( mIniFile->GetDataForkRefNum() != refNum_Undefined ) );
	err = mIniFile->FindSection( inSectionName );
	if ( err != noErr )
		return err;
	err = mIniFile->GetValueForName( inKeyName, outValue );
	return err;
}

// • call this when you want to make sure an entry exists inside PPP
ExceptionCode LMUCHandler::SetupEntry( LStr255& inOutEntry, TraversePPPListFunc inFunc,
	ResIDT inDlogID )
{
	if ( inOutEntry == kEmptyString )
	{
		mDirty = true;
		return noErr;
	}
		
	if ( this->FindName( inFunc, inOutEntry ) != noErr )
	{
		// • the user's Config file specifies a configuration entry that
		//		doesn't exist… try to update it by showing the existing
		//		entries and let them pick one
		OSErr		err;
		LStr255		newString;
		
		err = this->HandleDialog( inFunc, inDlogID, newString );
		// • err can be: noErr, errUserCancelled, errNeedToRunAS
		if ( err != noErr )
			Throw_( err );
		
		inOutEntry = newString;
		mDirty = true;
	}
	return noErr;
}

ExceptionCode LMUCHandler::ConnectionExists()
{
	if ( IsFreePPPOpen() )
	{
		// • alert the user
		LStr255		name;
		LCaption*	caption;
		
		MessageT		ret;
		StDialogHandler	handler( 132, NULL );
		LDialogBox*		dlog = (LDialogBox*)handler.GetDialog();

		GetCurrentAccountName( (Str255*)&name );
		caption = (LCaption*)dlog->FindPaneByID( 'anam' );
		if ( caption )
			caption->SetDescriptor( name );
		GetCurrentModemName( (Str255*)&name );
		caption = (LCaption*)dlog->FindPaneByID( 'mnam' );
		if ( caption )
			caption->SetDescriptor( name );
		GetCurrentLocationName( (Str255*)&name );
		caption = (LCaption*)dlog->FindPaneByID( 'lnam' );
		if ( caption )
			caption->SetDescriptor( name );

		do
		{
			ret = handler.DoDialog();
			if ( ret == msg_OK )
				return noErr;
			else if ( ret == msg_Cancel )
				return errCannotSwitchDialSettings;
		}
		while ( 1 );
	}
	return noErr;
}		


ExceptionCode LMUCHandler::FindName( TraversePPPListFunc p, const LStr255& value )
{
	Str255*			list;
	int				number;
	OSErr			err;
	
	err = (*p)( &list );
	number = ::GetPtrSize( (Ptr)list ) / sizeof ( Str255 );
	
	LStr255			name;
	for ( int i = 0; i < number; i++ )
	{
		name = *list;
		if ( name == value )
			return noErr;
		list++;
	}
	if ( list )
		DisposePtr( (Ptr)list );
	return -1;
}



void LMUCHandler::InitDialog( LDialogBox* dialog )
{
	Rect			wFrame;
	SDimension16	helpFrameSize;
	SPoint32		helpFrameLoc;
	
	LView::SetDefaultView( dialog );
	UReanimator::ReadObjects( 'PPob', 135 );
	mConfigHelp = (LView*)dialog->FindPaneByID( 'Hcfg' );
	mConfigHelp->Enable();
	mConfigHelp->FinishCreate();
	
	mAutoConfigCheck = (LStdCheckBox*)dialog->FindPaneByID( 'Cnet' );
	mAutoConfigCheck->AddListener( this );
	mAdvancedButton = (LControl*)dialog->FindPaneByID( 'Abut' );
	mAdvancedButton->AddListener( this );
	mLocationPopup = (LPPPPopup*)dialog->FindPaneByID( 'Ploc' );
	mLocationPopup->SetPPPFunction( GetLocationsList );
	mLocationPopup->UpdateList();

	mAccountName = (LCaption*)dialog->FindPaneByID( 'Anam' );
	mModemName = (LCaption*)dialog->FindPaneByID( 'Mnam' );
	
	mConfigHelp->GetFrameSize( helpFrameSize );
	mConfigHelp->GetFrameLocation( helpFrameLoc );
	dialog->CalcPortFrameRect( wFrame );
	mSizeToGrow = helpFrameSize.height - ( wFrame.bottom - helpFrameLoc.v );
}

void LMUCHandler::PrettyPrint( short resNum, const LStr255& string, LStr255& buffer )
{
	LStr255		tmp;
	
	::GetIndString( tmp, kMUCStrings, resNum );
	buffer += tmp;
	buffer += string;
	buffer += LStr255( "\r" );
}

ExceptionCode LMUCHandler::ProfileSelectionChanged( const FSSpec* profileSpec )
{
	FreePPPInfo		pppInfo;
	LStr255			accDesc;
	LStr255			modemDesc;
	
	if ( mAccountName && mModemName )
	{
		if ( profileSpec && this->GetProfile( profileSpec, &pppInfo ) == noErr )
		{
			PrettyPrint( kAccountString, pppInfo.mAccountName, accDesc );
			PrettyPrint( kModemString, pppInfo.mModemName, modemDesc );
		}
		mAccountName->SetDescriptor( accDesc );
		mModemName->SetDescriptor( modemDesc );
	}
		
	if ( mLocationPopup )
	{
		if ( !profileSpec || !mLocationPopup->SetToNamedItem( pppInfo.mLocationName ) )
		{
			// • switch to the default

			Str255		tmp;
			GetCurrentLocationName( &tmp );
			mLocationPopup->SetToNamedItem( (LStr255)tmp );
		}
	}
	

	return noErr;
}

void LMUCHandler::ListenToMessage( MessageT inMessage, void* /*ioParam*/ )
{
	switch ( inMessage )
	{
		case cmd_AdvancedSettings:
		{
			LWindow*			window;
			Rect				wFrame;
			
			// • make the window bigger
			window = LWindow::FetchWindowObject( mAdvancedButton->GetMacPort() );
			if ( !window )
				return;
				
			window->CalcPortFrameRect( wFrame );	
			window->PortToGlobalPoint( topLeft( wFrame ) );
			window->PortToGlobalPoint( botRight( wFrame ) );
			wFrame.bottom += mShowingHelp ? -mSizeToGrow : mSizeToGrow;
			window->DoSetBounds( wFrame );
			mShowingHelp = !mShowingHelp;
		}
		break;
	}
}	

void LMUCHandler::GetDialConfigurationFile( const FSSpec& inPrefsFolder, FSSpec& configFile )
{
	CInfoPBRec			pb;

	// • this is the "Configuration" file… it sits inside the user's
	//		directory with his "Mail", "News", and other folders
	configFile.vRefNum = inPrefsFolder.vRefNum;
	
	pb.hFileInfo.ioNamePtr = inPrefsFolder.name;
	pb.hFileInfo.ioVRefNum = inPrefsFolder.vRefNum;
	pb.hFileInfo.ioDirID = inPrefsFolder.parID;
	pb.hFileInfo.ioFDirIndex = 0;
	OSErr err = PBGetCatInfoSync( &pb );
	if ( err == noErr )
		configFile.parID = pb.dirInfo.ioDrDirID;
	else
		return;

	::GetIndString( configFile.name, kMUCStrings, kConfigurationFileName );
}

void LMUCHandler::RegisterClasses()
{
	URegistrar::RegisterClass(
		LMUCDialog::class_ID, 
		(ClassCreatorFunc)LMUCDialog::CreateMUCDialogStream );
		
	URegistrar::RegisterClass(
		LSingleClickListBox::class_ID,
		(ClassCreatorFunc)LSingleClickListBox::CreateSingleClickListBox );
		
	URegistrar::RegisterClass(
		LPPPListBox::class_ID,
		(ClassCreatorFunc)LPPPListBox::CreatePPPListBox );
	
	URegistrar::RegisterClass(
		LMUCEditDialog::class_ID,
		(ClassCreatorFunc)LMUCEditDialog::CreateMUCEditDialogStream );

	URegistrar::RegisterClass(
		LPPPPopup::class_ID,
		(ClassCreatorFunc)LPPPPopup::CreatePPPPopup );
}


