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

#include "rosetta.h"
#include "CABContainerDialogs.h"
#include "ABcom.h"
#ifdef MOZ_NEWADDR
#include "xp_mcom.h"
#include "msgnet.h"
#include "prefapi.h"
#include "dirprefs.h"

#include "CValidEditField.h"

#include "RandomFrontEndCrap.h" 

#include "pascalString.h"


#include "CTSMEditField.h"
#include "LGAPushButton.h"
#include  "LGACheckbox.h"
#include  "LGADialogBox.h"
#include "UmodalDialogs.h"
#include "PP_Messages.h"
#include "macgui.h"

CLDAPPropertyDialogManager::CLDAPPropertyDialogManager(  DIR_Server* server, MSG_Pane* inPane )
{
	StDialogHandler dialogHandler( eLDAPServerPropertiesDialogResID, NULL );
	LGADialogBox* dialog = dynamic_cast<LGADialogBox*>( dialogHandler.GetDialog() ) ;
	Assert_( dialog );
	Setup( dialog, server );
	
	if( dialog )
	{
		dialog->Show();
		MessageT theMessage = msg_Nothing;	
		while (	(theMessage != msg_Cancel) && (theMessage != msg_OK) )
		{
			theMessage = dialogHandler.DoDialog();
			switch( theMessage )
			{
				case cmd_HelpButton:				// help button
				 // ShowHelp(mHelpTopic);
				break;
	
			// toggle the port numbers in the port field
				case eGarbledBox:
					Int32 value = eLDAPStandardPort;
					if (mGarbledBox->GetValue())
						value = eLDAPGarbledPort;	
					mPortNumber->SetValue(eLDAPGarbledPort);
					break;

				case eUpdateButton:
						UpdateDirServerToUI ();
						// The Net_ReplicationDirectory call requires that this flag be set
						DIR_ForceFlag( mServer, DIR_REPLICATION_ENABLED, true  );
						NET_ReplicateDirectory( NULL, mServer );
					break;
				case msg_OK:
					
					if( !UpdateDirServerToUI() )
						theMessage = msg_Nothing;
					break;
				default:
					break;
			}
		}
			
		if ( theMessage == msg_OK )
		{
			// I think I am supposed to go context digging for the pane?
			AB_UpdateDIRServerForContainerPane( inPane, mServer	);
		}
	}
}

Boolean CLDAPPropertyDialogManager::UpdateDirServerToUI()
{
	// first check to make sure that the two validated edit fields are ok. If not,
	// then break out of here and don't accept the OK, forcing the user to
	// fix them.
	Boolean rtnValue = true;
	if ( !MaxHitsValidationFunc(mPortNumber) || !PortNumberValidationFunc(mMaxHits) )
		rtnValue = false;
					
	XP_FREEIF(mServer->searchBase);		
	XP_FREEIF(mServer->serverName);
	XP_FREEIF(mServer->description);

	CStr255 pBuffer;
	mDescription->GetDescriptor(pBuffer);
	mServer->description = XP_STRDUP(CStr255( pBuffer ) );
	mLdapServer->GetDescriptor(pBuffer);
	mServer->serverName = XP_STRDUP(CStr255( pBuffer ));
	mSearchRoot->GetDescriptor(pBuffer);
	mServer->searchBase = XP_STRDUP(CStr255( pBuffer ));
	
	DIR_ForceFlag( mServer, DIR_REPLICATION_ENABLED,  mDownloadCheckBox->GetValue(  )  );
	mServer->port = mPortNumber->GetValue();
	mServer->maxHits = mMaxHits->GetValue();

	HG51388
	mServer->savePassword = mSavePasswordBox->GetValue()? true: false;
	DIR_SetServerFileName(mServer, mServer->serverName);
	return rtnValue;
}


void CLDAPPropertyDialogManager::Setup( LGADialogBox* inDialog , DIR_Server *inServer )
{
	mServer = inServer;
	
	mDescription = (CTSMEditField *) inDialog->FindPaneByID(eDescriptionEditField);
	XP_ASSERT(mDescription);
	mLdapServer = (LEditField *) inDialog->FindPaneByID(eLDAPServerEditField);
	XP_ASSERT(mLdapServer);
	mSearchRoot = (LEditField *) inDialog->FindPaneByID(eSearchRootEditField);
	XP_ASSERT(mSearchRoot);
	mPortNumber = (CValidEditField *) inDialog->FindPaneByID(ePortNumberEditField);
	XP_ASSERT(mPortNumber);
	mMaxHits = (CValidEditField *) inDialog->FindPaneByID(eMaxHitsEditField);
	XP_ASSERT(mMaxHits);
	mGarbledBox = (LGACheckbox *) inDialog->FindPaneByID(eGarbledBox);
	XP_ASSERT(mGarbledBox);
	mSavePasswordBox = dynamic_cast<LGACheckbox *>(inDialog->FindPaneByID ( eSaveLDAPServerPasswordBox) );
	Assert_( mSavePasswordBox );
	mDownloadCheckBox = dynamic_cast<LGACheckbox* >( inDialog->FindPaneByID ( eDownloadCheckBox ) );
	Assert_( mDownloadCheckBox );
	
	UReanimator::LinkListenerToControls( inDialog, inDialog, eLDAPServerPropertiesDialogResID );
	
	mPortNumber->SetValidationFunction(PortNumberValidationFunc);
	mMaxHits->SetValidationFunction(MaxHitsValidationFunc);

	mDescription->SetDescriptor(CStr255( mServer->description) );
	mLdapServer->SetDescriptor(CStr255 (mServer->serverName ));
	mSearchRoot->SetDescriptor(CStr255( mServer->searchBase ));
	mDownloadCheckBox->SetValue( DIR_TestFlag( mServer, DIR_REPLICATION_ENABLED ) );
	mPortNumber->SetValue(mServer->port);
	mMaxHits->SetValue(mServer->maxHits);
	
	HG51389
	mSavePasswordBox->SetValue(mServer->savePassword ? 1: 0);

	// If the directories are locked, disable everything  but cancel so the user can't make any changes. This
	// allows them to view the information but not edit it.
	XP_Bool serversLocked = PREF_PrefIsLocked("ldap_1.number_of_directories");	
	if ( serversLocked )
	{
		inDialog->Disable();
		LGAPushButton *canelButton = (LGAPushButton *)(inDialog->FindPaneByID('CnBt') );
		XP_ASSERT(canelButton);
		canelButton->Enable();
	}
} 


Boolean CLDAPPropertyDialogManager::PortNumberValidationFunc(CValidEditField *portNumber)
// Makes sure the port number field of the dialog is between 0 and 32767, but sets
// a reasonable default if the field is left blank.
{
	Boolean	result;
	Str255	currentValue;
	portNumber->GetDescriptor(currentValue);
	if (!currentValue[0])
	{
		portNumber->SetValue(eLDAPStandardPort);
		HG51387
		portNumber->SelectAll();
		result = false;
	}
	else
	{
		result = ConstrainEditField(portNumber, 0, 32767);
	}
	if (!result)
	{
		StPrepareForDialog	prepare;
		::StopAlert(1068, NULL);
	}
	return result;
}


Boolean CLDAPPropertyDialogManager::MaxHitsValidationFunc(CValidEditField *maxHits)
// Makes sure the max hits field of the dialog is between 1 and 65535.
{
	Boolean		result;
	result = ConstrainEditField(maxHits, 1, 65535);
	if (!result)
	{
		// If it was constrained to 1 then make it 100 instead.
		if (1 == maxHits->GetValue() )
		{
			maxHits->SetValue(100);
			maxHits->SelectAll();
		}
		StPrepareForDialog	prepare;
		::StopAlert(1069, NULL);
	}
	return result;
}

#pragma mark --CPABPropertyDialogManager--
CPABPropertyDialogManager::CPABPropertyDialogManager(  DIR_Server *inServer, MSG_Pane* inPane  )
{
	StDialogHandler dialogHandler( ePABPropertyWindowID, NULL );
	LGADialogBox* dialog = dynamic_cast<LGADialogBox*>( dialogHandler.GetDialog() ) ;
	Assert_( dialog );
	
	LGAEditField* nameField = dynamic_cast<LGAEditField*>( dialog->FindPaneByID( eNamePaneID ) );
	nameField->SetDescriptor( CStr255 ( inServer->description ) );
	
	MessageT theMessage = msg_Nothing;
	while (	(theMessage != msg_Cancel) && (theMessage != msg_OK) )
	{
		theMessage = dialogHandler.DoDialog();
	}
	
	if ( theMessage == msg_OK )
	{
		XP_FREEIF( inServer->description  );
		CStr255 pBuffer;
		nameField->GetDescriptor(pBuffer);
		inServer->description = XP_STRDUP(CStr255( pBuffer ) );
		AB_UpdateDIRServerForContainerPane( inPane, inServer );
	}
}
#endif
