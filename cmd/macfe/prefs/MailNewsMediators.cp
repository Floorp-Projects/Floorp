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

#include "MailNewsMediators.h"

//XP
extern "C"
{
#include "xp_help.h"
}
#include "prefapi.h"
#include "dirprefs.h"
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS

//Old Macintosh
#include "resgui.h"
#include "macgui.h"
#include "uerrmgr.h"
#include "macutil.h"

//New Macintosh
#include "InternetConfig.h"
#include "CTSMEditField.h"
#include "MailNewsAddressBook.h"
#include "UNewFolderDialog.h"
#include "CMessageFolder.h"
#include "PrefControls.h"
#include "CMailNewsContext.h"
#include "StGetInfoHandler.h"
#include "CNewsSubscriber.h"

//3P
#include <ICAPI.h>
#include <ICKeys.h>

//PP
#include <LTextColumn.h>
#include <UModalDialogs.h>

#ifndef MOZ_LITE
#pragma mark ---CLDAPServerPropDialog---

//----------------------------------------------------------------------------------------
CLDAPServerPropDialog::CLDAPServerPropDialog( LStream* inStream )
//----------------------------------------------------------------------------------------
:	LGADialogBox ( inStream )
,	mHelpTopic(HELP_LDAP_SERVER_PROPS)
,	mServer(NULL)
,	mIsPAB(false)
,	mNewServer(eNewServer)
{
} // CLDAPServerPropDialog::CLDAPServerPropDialog


//----------------------------------------------------------------------------------------
CLDAPServerPropDialog::~CLDAPServerPropDialog( )
//----------------------------------------------------------------------------------------
{
	
} // CLDAPServerPropDialog::~CLDAPServerPropDialog


//----------------------------------------------------------------------------------------
void CLDAPServerPropDialog::FinishCreateSelf( )
//----------------------------------------------------------------------------------------
{
	Inherited::FinishCreateSelf();

	mDescription = (CTSMEditField *) FindPaneByID(eDescriptionEditField);
							// use CTSMEditField to support Asian Inline input
	XP_ASSERT(mDescription);
	mLdapServer = (LEditField *) FindPaneByID(eLDAPServerEditField);
	XP_ASSERT(mLdapServer);
	mSearchRoot = (LEditField *) FindPaneByID(eSearchRootEditField);
	XP_ASSERT(mSearchRoot);
	mPortNumber = (CValidEditField *)FindPaneByID(ePortNumberEditField);
	XP_ASSERT(mPortNumber);
	mMaxHits = (CValidEditField *) FindPaneByID(eMaxHitsEditField);
	XP_ASSERT(mMaxHits);
	mSecureBox = dynamic_cast<LControl*>(FindPaneByID(eSecureBox));
	XP_ASSERT(mSecureBox);
	mSavePasswordBox = dynamic_cast<LControl*>(FindPaneByID ( eSaveLDAPServerPasswordBox) );
	Assert_( mSavePasswordBox );
	
	mDownloadCheckBox = dynamic_cast<LControl*>( FindPaneByID ( eDownloadCheckBox ) );
	Assert_( mDownloadCheckBox );

	UReanimator::LinkListenerToControls( this, this, eLDAPServerPropertiesDialogResID );
	
} // FinishCreateSelf

//----------------------------------------------------------------------------------------
void CLDAPServerPropDialog::ListenToMessage( MessageT inMessage, void* ioParam )
//----------------------------------------------------------------------------------------
{
	switch ( inMessage ) {
	
		case CLDAPServerPropDialog::cmd_HelpButton:				// help button
			ShowHelp(mHelpTopic);
			break;
	
		// toggle the port numbers in the port field
		case CLDAPServerPropDialog::eSecureBox:
			if (mSecureBox->GetValue())
			{
				mPortNumber->SetValue(eLDAPSecurePort);
				mPortNumber->SelectAll();
			}
			else
			{
				mPortNumber->SetValue(eLDAPStandardPort);
				mPortNumber->SelectAll();
			}
			break;
		
		
		case CLDAPServerPropDialog::eUpdateButton:
			// NEED XP API
			break;

		// On an OK, fish out all of the data from the fields and stuff it into the
		// server data structure.
		case CLDAPServerPropDialog::cmd_OKButton:
		{		
			// first check to make sure that the two validated edit fields are ok. If not,
			// then break out of here and don't accept the OK, forcing the user to
			// fix them.
			if ( !MaxHitsValidationFunc(mPortNumber) || !PortNumberValidationFunc(mMaxHits) )
				break;
				
			if (mServer->searchBase)
				XP_FREE(mServer->searchBase);
			if (mServer->serverName)
				XP_FREE(mServer->serverName);
			if (mServer->description)
				XP_FREE(mServer->description);

			Str255 pBuffer;
			char *stringStart = (char *)&pBuffer[1];
			mDescription->GetDescriptor(pBuffer);
			pBuffer[pBuffer[0] + 1] = '\0';
			mServer->description = XP_STRDUP(stringStart);

			if (!mIsPAB)
			{
				mLdapServer->GetDescriptor(pBuffer);
				pBuffer[pBuffer[0] + 1] = '\0';
				mServer->serverName = XP_STRDUP(stringStart);

				mSearchRoot->GetDescriptor(pBuffer);
				pBuffer[pBuffer[0] + 1] = '\0';
				mServer->searchBase = XP_STRDUP(stringStart);

				mServer->port = mPortNumber->GetValue();
				mServer->maxHits = mMaxHits->GetValue();

				mServer->isSecure = mSecureBox->GetValue()? true: false;
				mServer->savePassword = mSavePasswordBox->GetValue()? true: false;
				DIR_SetServerFileName(mServer, mServer->serverName);
			}
			
			Inherited::ListenToMessage( inMessage, ioParam );	// pass along OK
			break;
		}				
		default:												// pass along Cancel, etc
			Inherited::ListenToMessage( inMessage, ioParam );
			
	} // case of message

} // ListenToMessage 

//----------------------------------------------------------------------------------------
void  CLDAPServerPropDialog::SetUpDialog(
	DIR_Server* inServer, Boolean inNewServer, Boolean inIsPAB, 
	Boolean inAllLocked )
// Enables/disables all the fields and puts the initial values into them.
//----------------------------------------------------------------------------------------
{
	mServer = inServer;
	mNewServer = inNewServer;
	mIsPAB = inIsPAB;
	XP_ASSERT(inServer != nil);
	
	mPortNumber->SetValidationFunction(PortNumberValidationFunc);
	mMaxHits->SetValidationFunction(MaxHitsValidationFunc);

	if (mServer->description)
	{
		LStr255	descriptionPStr(mServer->description);
		mDescription->SetDescriptor(descriptionPStr);
		mDescription->SelectAll();
	}
	else
	{
		mDescription->SetDescriptor("\p");
	}
	mDescription->SelectAll();

	if (inIsPAB)
	{
		mLdapServer->Disable();
		mSearchRoot->Disable();
		mPortNumber->Disable();
		mMaxHits->Disable();
		mSecureBox->Disable();
		mSavePasswordBox->Disable();
	}
	else
	{
		if (mServer->serverName)
		{
			LStr255	ldapServerPStr(mServer->serverName);
			mLdapServer->SetDescriptor(ldapServerPStr);
			mLdapServer->SelectAll();
		}
		else
		{
			mLdapServer->SetDescriptor("\p");
		}
		mLdapServer->SelectAll();
		if (mServer->searchBase)
		{
			LStr255	searchRootPStr(mServer->searchBase);
			mSearchRoot->SetDescriptor(searchRootPStr);
			mSearchRoot->SelectAll();
		}
		else
		{
			mSearchRoot->SetDescriptor("\p");
		}
		mSearchRoot->SelectAll();
		mPortNumber->SetValue(mServer->port);
		mPortNumber->SelectAll();
		mMaxHits->SetValue(mServer->maxHits);
		mMaxHits->SelectAll();
		mSecureBox->SetValue(mServer->isSecure ? 1: 0);
		mSavePasswordBox->SetValue(mServer->savePassword ? 1: 0);
	}

	// If the directories are locked, disable everything so the user can't make any changes. This
	// allows them to view the information but not edit it.
	if ( inAllLocked )
	{
		mDescription->Disable();
		mLdapServer->Disable();
		mSearchRoot->Disable();
		mPortNumber->Disable();
		mMaxHits->Disable();
		mSecureBox->Disable();
//		pwBox->Disable();
		LControl *okButton = dynamic_cast<LControl*>(FindPaneByID(CPrefsDialog::eOKButtonID));
		XP_ASSERT(okButton);
		okButton->Disable();
	}

} // SetUpDialog

//----------------------------------------------------------------------------------------
Boolean CLDAPServerPropDialog::PortNumberValidationFunc(CValidEditField *portNumber)
// Makes sure the port number field of the dialog is between 0 and 32767, but sets
// a reasonable default if the field is left blank.
//----------------------------------------------------------------------------------------
{
	Boolean	result;
	Str255	currentValue;
	portNumber->GetDescriptor(currentValue);
	if (!currentValue[0])
	{
		// If the user wipes out the port number field it is evaluated as zero, which
		// is a valid port number, but it is not what we want to happen if the field
		// is blank.
		// We want to put in the Well Known port number, which depends on whether or
		// not the server is secure. So we need to get the secure checkbox value.
		// LDAP standard port = 389
		// LDAP secure (SSL) standard port = 636
		LView	*superView = portNumber->GetSuperView();
		XP_ASSERT(superView);
		LControl* checkbox = dynamic_cast<LControl*>(superView->FindPaneByID(eSecureBox));
		XP_ASSERT(checkbox);
		portNumber->SetValue(checkbox->GetValue() ? eLDAPSecurePort : eLDAPStandardPort);
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

//----------------------------------------------------------------------------------------
Boolean CLDAPServerPropDialog::MaxHitsValidationFunc(CValidEditField *maxHits)
// Makes sure the max hits field of the dialog is between 1 and 65535.
//----------------------------------------------------------------------------------------
{
	Boolean		result;
	result = ConstrainEditField(maxHits, 1, 65535);
	if (!result)
	{
		// If it was constrained to 1 then make it 100 instead.
		if (1 == maxHits->GetValue())
		{
			maxHits->SetValue(100);
			maxHits->SelectAll();
		}
		StPrepareForDialog	prepare;
		::StopAlert(1069, NULL);
	}
	return result;
}
#endif // MOZ_LITE

enum
{
	eNameField = 12701,
	eEMailField,
	eReplyField,
	eOrgField,
	eUseSigFileBox,
	eSigFilePicker,
	eAttachPABCBox,
	eEditCardButton,	// not used
	eInternetConfigBox,
	eLaunchInternetConfig	// currently only used in (MOZ_LITE)
};

//----------------------------------------------------------------------------------------
CMailNewsIdentityMediator::CMailNewsIdentityMediator(LStream*)
//----------------------------------------------------------------------------------------
:	CPrefsMediator(class_ID)
{
}

//----------------------------------------------------------------------------------------
void CMailNewsIdentityMediator::ListenToMessage(MessageT inMessage, void *ioParam)
//----------------------------------------------------------------------------------------
{
	switch (inMessage)
	{
#ifdef MOZ_LITE
		case eLaunchInternetConfig:
				const OSType kInternetConfigAppSig = 'ICAp';
				FSSpec	appSpec;
				if (CFileMgr::FindApplication(kInternetConfigAppSig, appSpec) == noErr) {
					LaunchParamBlockRec launchThis;
					launchThis.launchBlockID = extendedBlock;
					launchThis.launchEPBLength = extendedBlockLen;
					launchThis.launchFileFlags = NULL;
					launchThis.launchControlFlags = launchContinue + launchNoFileFlags;
					launchThis.launchAppSpec = (FSSpecPtr)&appSpec;
					launchThis.launchAppParameters = NULL;
					LaunchApplication(&launchThis);
				}
			break;
#endif // MOZ_LITE
#ifndef MOZ_LITE
		case eUseSigFileBox:
			CFilePicker *fPicker =
					(CFilePicker *)FindPaneByID(eSigFilePicker);
			XP_ASSERT(fPicker);
			if (!fPicker->WasSet() && *(Int32 *)ioParam)
			{	// If the user has clicked the checkbox and the file picker is not set
				// then we may want to trigger the file picker.
				if (mNeedsPrefs)
				{
					// If mNeedsPrefs, then we are setting up the pane. If the picker
					// is not set (can happen if the sig file was physically deleted),
					// then we need to unset the "use" check box.
					LControl* checkbox = dynamic_cast<LControl*>(FindPaneByID(inMessage));
					XP_ASSERT(checkbox);
					checkbox->SetValue(false);
				}
				else
				{
					fPicker->ListenToMessage(msg_Browse, nil);
					if (!fPicker->WasSet())
					{	// If the file picker is still unset, that means that the user
						// cancelled the file browse so we don't want the checkbox set.
						LControl *checkbox = dynamic_cast<LControl*>(FindPaneByID(inMessage));
						XP_ASSERT(checkbox);
						checkbox->SetValue(false);
					}
				}
			}
			break;
		case msg_FolderChanged:
			LControl *checkbox = dynamic_cast<LControl*>(FindPaneByID(eUseSigFileBox));
			XP_ASSERT(checkbox);
			checkbox->SetValue(true);
			break;
#endif // MOZ_LITE
		case eInternetConfigBox:
			Boolean	useIC = *(Int32 *)ioParam;
			UseIC(useIC);
#ifdef MOZ_LITE
			LButton *button =
					(LButton *)FindPaneByID(eLaunchInternetConfig);
			XP_ASSERT(button);
			if (UseIC())
				button->Enable();
			else
				button->Disable();
#endif // MOZ_LITE
			if (useIC)
			{
				CPrefsDialog::LoadICDependent();
			}
			// load IC prefs for this pane
			SetEditFieldsWithICPref(kICRealName,
									eNameField);
			SetEditFieldsWithICPref(kICEmail,
									eEMailField);
#ifndef MOZ_LITE
			SetEditFieldsWithICPref(kICEmail,
									eReplyField);
#endif // MOZ_LITE
			SetEditFieldsWithICPref(kICOrganization,
									eOrgField);
			break;
		default:
			Inherited::ListenToMessage(inMessage, ioParam);
			break;
	}
} // CMailNewsIdentityMediator::ListenToMessage

//----------------------------------------------------------------------------------------
void CMailNewsIdentityMediator::LoadPrefs()
//----------------------------------------------------------------------------------------
{
	if (CPrefsDialog::eEmailAddress == sInitialSelection)
	{	
		LEditField *theField = (LEditField *)FindPaneByID(eEMailField);
		XP_ASSERT(theField);
		sWindow->SetLatentSub(theField);
	}
} // CMailNewsIdentityMediator::LoadPrefs

#ifndef MOZ_LITE

enum
{
	eMoreMessageOptionsDialogResID = 12005,
	eHTMLBox = 12801,
	eAutoQuoteBox,
	eWrapLengthIntegerField = 12811,
	eMailMailSelfBox = 12803,
	eMailMailCCEditField = 12804,
	eMailNewsSelfBox,
	eMailNewsCCEditField,
	eMailFCCPopup = 12808,
	eNewsFCCPopup = 12810,
	eMoreOptionsButton = 12812,
	eNamesAndNickNamesRButton,
	eNickNamesOnlyRButton,
	eAsIsRButton,
	eStrictlyMIMERButton,
	eAskRButton,
	eConvertRButton,
	eSendHTMLRButton,
	eSendBothRButton,
	eOnlineSentFolderBox
};

//----------------------------------------------------------------------------------------
CMailNewsMessagesMediator::CMailNewsMessagesMediator(LStream*)
//----------------------------------------------------------------------------------------
:	CPrefsMediator(class_ID)
{
}

//----------------------------------------------------------------------------------------
Boolean CMailNewsMessagesMediator::WrapLinesValidationFunc(CValidEditField *wrapLines)
//----------------------------------------------------------------------------------------
{
	Boolean		result = true;
	if (1 > wrapLines->GetValue())
	{
		int32	newLength = 72;
		PREF_GetDefaultIntPref("mailnews.wraplength", &newLength);
		wrapLines->SetValue(newLength);
		wrapLines->SelectAll();
		result = false;
	}
	if (!result)
	{
		StPrepareForDialog	prepare;
		::StopAlert(1067, NULL);
	}
	return result;
}

//----------------------------------------------------------------------------------------
void CMailNewsMessagesMediator::LoadMainPane()
//----------------------------------------------------------------------------------------
{
	Inherited::LoadMainPane();
	SetValidationFunction(eWrapLengthIntegerField, WrapLinesValidationFunc);
}

#pragma mark ---CMailNewsOutgoingMediator---
//----------------------------------------------------------------------------------------
CMailNewsOutgoingMediator::CMailNewsOutgoingMediator(LStream*)
//----------------------------------------------------------------------------------------
:	CPrefsMediator(class_ID)
{
	Assert_(kNumFolderKinds == UFolderDialogs::num_kinds);
	char**p = &mFolderURL[0];
	for (int i = 0; i < kNumFolderKinds; i++)
		*p++ = nil;
}

//----------------------------------------------------------------------------------------
CMailNewsOutgoingMediator::~CMailNewsOutgoingMediator()
//----------------------------------------------------------------------------------------
{
	char**p = &mFolderURL[0];
	for (int i = 0; i < kNumFolderKinds; i++)
		XP_FREEIF(*p++);
}

//----------------------------------------------------------------------------------------
void CMailNewsOutgoingMediator::LoadPrefs()
//----------------------------------------------------------------------------------------
{
} // CMailNewsOutgoingMediator::LoadPrefs

//----------------------------------------------------------------------------------------
void CMailNewsOutgoingMediator::WritePrefs()
//----------------------------------------------------------------------------------------
{
	char**p = &mFolderURL[0];
	CStr255 prefName;
	for (int i = 0; i < kNumFolderKinds; i++,p++)
	{
		if (*p)
		{
			UFolderDialogs::BuildPrefName(
				(UFolderDialogs::FolderKind)i,
				prefName);
			PREF_SetCharPref(prefName, *p);
		}
	}
} // CMailNewsOutgoingMediator::WritePrefs

//----------------------------------------------------------------------------------------
void CMailNewsOutgoingMediator::ListenToMessage(MessageT inMessage, void* ioParam)
//----------------------------------------------------------------------------------------
{
	UFolderDialogs::FolderKind kind;
	switch (inMessage)
	{
		case 'ChMF':
			kind = UFolderDialogs::mail_fcc;
			break;
		case 'ChNF':
			kind = UFolderDialogs::news_fcc;
			break;
		case 'ChDF':
			kind = UFolderDialogs::drafts;
			break;
		case 'ChTF':
			kind = UFolderDialogs::templates;
			break;
		default:
			Inherited::ListenToMessage(inMessage, ioParam);
			return;
	}
	CMessageFolder curFolder;
	const char* curURL = mFolderURL[kind];
	if (curURL)
		curFolder.SetFolderInfo(
			::MSG_GetFolderInfoFromURL(CMailNewsContext::GetMailMaster(), curURL));
	CMessageFolder folder = UFolderDialogs::ConductSpecialFolderDialog(kind, curFolder);
	if (folder.GetFolderInfo() != nil)
	{
		URL_Struct* ustruct = ::MSG_ConstructUrlForFolder(nil, folder);
		if (ustruct)
		{
			char* newURL = XP_STRDUP(ustruct->address);
			NET_FreeURLStruct(ustruct);
			XP_FREEIF(mFolderURL[kind]);
			mFolderURL[kind] = newURL;
			
			PaneIDT ckID;
			switch (inMessage)
			{
				case 'ChMF':
					ckID = 12807;
					break;
				case 'ChNF':
					ckID = 12809;
					break;
				case 'ChDF':
					ckID = 'Drft';
					break;
				case 'ChTF':
					ckID = 'Tmpl';
					break;
			}
			LControl* checkbox = dynamic_cast<LControl*>(FindPaneByID(ckID));
			XP_ASSERT(checkbox);
			UPrefControls::NoteSpecialFolderChanged(checkbox, kind, folder);
		}
	}
} // CMailNewsOutgoingMediator::ListenToMessage

enum
{
	eMoreMailServerOptionsDialogResID = 12003
,	eMailServerUserNameField = 12901
,	eSMTPServerField = 12902
//,	eIncomingServerField = 12903
//,	eUsePOPRButton = 12904
//,	eLeaveOnServerBox = 12905
//,	eUseIMAPRButton = 12906
//,	eLocalCopiesBox = 12907
//,	eServerSSLBox = 12908
//,	eMailServerMoreButton = 12909

//,	eIMAPMailDirEditField = 12911
//,	eOnRestartText = 12916
//,	eIMAPDeleteIsMoveToTrash = 12917
};

enum
{
	eUnknownServerType = -1,
	ePOPServer = 0,
	eIMAPServer = 1
};

//----------------------------------------------------------------------------------------
void MServerListMediatorMixin::ClearList()
//----------------------------------------------------------------------------------------
{
	TableIndexT	rows, cols;
	mServerTable->GetTableSize(rows, cols);
	if (rows > 0)
		mServerTable->RemoveRows(rows, 1, false);
}

//----------------------------------------------------------------------------------------
Boolean MServerListMediatorMixin::GetHostFromRow(
	TableIndexT inRow,
	CellContents& outCellData,
	UInt32 inDataSize) const
//----------------------------------------------------------------------------------------
{
	if (inRow == 0)
		return false;
	TableIndexT rowCount, colCount;
	mServerTable->GetTableSize(rowCount, colCount);
	if (inRow > rowCount)
		return false;
	STableCell cell(inRow, 1);
	Uint32 cellDataSize = inDataSize;
	mServerTable->GetCellData(cell, &outCellData, cellDataSize);
	XP_ASSERT(cellDataSize >= inDataSize);
	return true;
} // CMailNewsMailServerMediator::GetHostFromRow

//----------------------------------------------------------------------------------------
Boolean MServerListMediatorMixin::GetHostFromSelectedRow(
	CellContents& outCellData,
	UInt32 inDataSize) const
//----------------------------------------------------------------------------------------
{
	STableCell currentCell = mServerTable->GetFirstSelectedCell();
	if (currentCell.row)
		return GetHostFromRow(currentCell.row, outCellData, inDataSize);
	return false;
} // CMailNewsMailServerMediator::GetHostFromRow

//----------------------------------------------------------------------------------------
void MServerListMediatorMixin::LoadMainPane()
//----------------------------------------------------------------------------------------
{
	mServerTable = (CDragOrderTextList*)mMediatorSelf->FindPaneByID('LIST');
	ThrowIfNil_(mServerTable);
	mServerTable->AddListener(mMediatorSelf);
	UpdateButtons();
}

//----------------------------------------------------------------------------------------
void MServerListMediatorMixin::UpdateButtons()
//----------------------------------------------------------------------------------------
{
	XP_ASSERT(mServerTable);
	STableCell currentCell = mServerTable->GetFirstSelectedCell();
	LControl* deleteButton = (LControl*)mMediatorSelf->FindPaneByID('Dele');
	LControl* editButton = (LControl*)mMediatorSelf->FindPaneByID('Edit');
	LControl* addButton = (LControl*)mMediatorSelf->FindPaneByID('NewÉ');
	if (currentCell.row)
	{
		editButton->Enable();
		deleteButton->Enable();
	}
	else
	{
		editButton->Disable();
		deleteButton->Disable();
	}
	if (mServersLocked || CPrefsMediator::UseIC())
	{
		addButton->Disable();
		deleteButton->Disable();
		mServerTable->LockOrder();
	}
	else
		addButton->Enable();
} // MServerListMediatorMixin::UpdateButtons

//----------------------------------------------------------------------------------------
Boolean MServerListMediatorMixin::Listen(MessageT inMessage, void *ioParam)
//----------------------------------------------------------------------------------------
{
	switch (inMessage)
	{
		case CDragOrderTextList::msg_ChangedOrder:
			mServersDirty = true;
			break;
		case eSelectionChanged:
			Assert_((CDragOrderTextList*)ioParam = mServerTable);
			UpdateButtons();
			break;
		case 'NewÉ':
			AddButton();
			break;
		case eDoubleClick:
		case 'Edit':
			EditButton();
			break;
		case 'Dele':
			DeleteButton();
			break;
		default:
			return false;
			break;
	}
	return true;
}

//----------------------------------------------------------------------------------------
MServerListMediatorMixin::CellContents::CellContents(const char* inName)
//----------------------------------------------------------------------------------------
{
	*(CStr255*)&description[0] = inName;
}

//----------------------------------------------------------------------------------------
CServerListMediator::CServerListMediator(PaneIDT inMainPaneID)
//----------------------------------------------------------------------------------------
:	CPrefsMediator(inMainPaneID)
,	MServerListMediatorMixin(this)
{
}

//----------------------------------------------------------------------------------------
void CServerListMediator::LoadMainPane()
//----------------------------------------------------------------------------------------
{
	CPrefsMediator::LoadMainPane();
	MServerListMediatorMixin::LoadMainPane();
}

//----------------------------------------------------------------------------------------
void CServerListMediator::ListenToMessage(MessageT inMessage, void *ioParam)
//----------------------------------------------------------------------------------------
{
	if (!MServerListMediatorMixin::Listen(inMessage, ioParam))
		CPrefsMediator::ListenToMessage(inMessage, ioParam);
}

enum
{
	POP3_PORT = 110,				/* the iana port for pop3 */
	IMAP4_PORT = 143,				/* the iana port for imap4 */
	IMAP4_PORT_SSL_DEFAULT = 993	/* use a separate port for imap4 over ssl */
};

//----------------------------------------------------------------------------------------
CMailNewsMailServerMediator::CMailNewsMailServerMediator(LStream*)
//----------------------------------------------------------------------------------------
:	CServerListMediator(class_ID)
,	mServerType(eUnknownServerType)
,	mPopServerName(nil)
{
}

//----------------------------------------------------------------------------------------
CMailNewsMailServerMediator::~CMailNewsMailServerMediator()
//----------------------------------------------------------------------------------------
{
	XP_FREEIF(mPopServerName);
}

//----------------------------------------------------------------------------------------
Boolean CMailNewsMailServerMediator::NoAtSignValidationFunc(CValidEditField *noAtSign)
//----------------------------------------------------------------------------------------
{
	CStr255 value;
	noAtSign->GetDescriptor(value);
	unsigned char atPos = value.Pos("@");
	if (atPos)
	// ¥¥¥ FIX ME l10n. Why?  This is an email address
	{
		value.Delete(atPos, 256);		// if there is an at sign delete it
		noAtSign->SetDescriptor(value);	// and anything after it
		noAtSign->SelectAll();
		ErrorManager::PlainAlert(POP_USERNAME_ONLY);
		return false;
	}
	return true;
} // CMailNewsMailServerMediator::NoAtSignValidationFunc

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::LoadMainPane()
//----------------------------------------------------------------------------------------
{
	Inherited::LoadMainPane();
	SetValidationFunction(eMailServerUserNameField, NoAtSignValidationFunc);
} // CMailNewsMailServerMediator::LoadMainPane

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::LoadPrefs()
//----------------------------------------------------------------------------------------
{
	int32 serverType = eUnknownServerType;
	int32 prefResult = PREF_GetIntPref("mail.server_type", &serverType);
	mServerType = (ServerType)serverType;	
	if (mServerType == ePOPServer)
	{
		char* value = nil;
		int	prefResult = PREF_CopyCharPref("network.hosts.pop_server", &value);
		if (prefResult == PREF_NOERROR && value)
			if (*value)
				mPopServerName = value;
			else
				XP_FREE(value);
	}
	mServersLocked = PREF_PrefIsLocked("network.hosts.pop_server")
					|| PREF_PrefIsLocked("mail.server_type");

	LoadList();
} // CMailNewsMailServerMediator::LoadPrefs

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::WritePrefs()
//----------------------------------------------------------------------------------------
{
	WriteList();
} // CMailNewsMailServerMediator::WritePrefs

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::UpdateFromIC()
//----------------------------------------------------------------------------------------
{
	SetEditFieldsWithICPref(kICSMTPHost, eSMTPServerField, true);
	Boolean userNameLocked = PaneHasLockedPref(eMailServerUserNameField);

	if ((userNameLocked && mServersLocked) || !UseIC())
		return;

	LEditField* popUserNameField =
			(LEditField*)FindPaneByID(eMailServerUserNameField);
	Assert_(popUserNameField);

	Str255 s;
	long port;
	CInternetConfigInterface::GetInternetConfigString(	kICMailAccount,
														s,
														&port);
	// parse as user@server
	int i;
	unsigned char* server = nil;
	// Search the pascal string for '@'
	unsigned char* cp = &s[1];
	for (i = 1; i <= s[0] && !server; ++i, ++cp)
	{
		if ('@' == (char)*cp)
			server = cp;
	}
	// If found, poke in the length bytes to split s into two pascal strings user/server.
	// Otherwise, assume s is the server only (and the user name is empty)
	unsigned char* user = "\p";
	if (server)
	{
		server[0] = s[0] - i+1;
		s[0] = s[0] - i-1;
		user = s;
	}
	else
		server = s;

	if (!userNameLocked)
	{
		popUserNameField->SetDescriptor(user);
		popUserNameField->SelectAll();
	}
	popUserNameField->Disable();
	if (!mServersLocked)
	{
		ClearList();
		XP_FREEIF(mPopServerName);
		if (port == POP3_PORT)
		{
			mPopServerName = XP_STRDUP((const char*)(CStr255)server);
			mServerType = ePOPServer;
		}
		else
		{
			PREF_SetCharPref("network.hosts.imap_servers", (const char*)(CStr255)server);
			mServerType = eIMAPServer;
		}
		LoadList(); // rebuild the list, which will now show the new server.
	}
} // CMailNewsMailServerMediator::UpdateFromIC

//----------------------------------------------------------------------------------------
Boolean CMailNewsMailServerMediator::UsingPop() const
//----------------------------------------------------------------------------------------
{
	return (mServerType == ePOPServer);
} // CMailNewsMailServerMediator::UsingPop

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::AddButton()
// Create a brand new (empty) server and pop up a dialog to let the user edit it. The
// OK/Cancel is handled in ObeyCommand().
//----------------------------------------------------------------------------------------
{
	TableIndexT	rows, cols;
	mServerTable->GetTableSize(rows, cols);
	if (UsingPop() && rows > 0)
	{
		//	See allxpstr.h for the following string:
		//	"You cannot connect to more than one server, because you are using "
		//		"a POP server." // 25
		//::GetIndString(reason, 7099, 25);
		ErrorManager::PlainAlert(GetPString(MK_POP3_ONLY_ONE + RES_OFFSET));
		return;
	}
	CStr255 serverName; // constructor sets to empty string.
#ifdef PRELIMINARY_DIALOG
	{ 	// <-----Start scope for name/type dialog
		// Put up the dialog to get the name and type...
		// If
		MPreferenceBase::StWriteOnDestroy setter2(false);
		StDialogHandler handler(12011, nil);
		LWindow* dialog = handler.GetDialog();
		LEditField* nameField = dynamic_cast<LEditField*>(dialog->FindPaneByID('NAME'));
		SignalIf_(!nameField);
		if (!nameField)
			return;
		LControl* popButton = dynamic_cast<LControl*>(dialog->FindPaneByID('POP3'));
		SignalIf_(!popButton);
		if (!popButton)
			return;
		if (rows == 0)
		{
			// default for first server is POP
			popButton->SetValue(1);
		}
		else
		{
			// If they already have a server (and to get here, it must be IMAP) then
			// the POP button must be disabled (it is off and enabled by default
			// in the resource).
			popButton->Disable();
		}
		
		// Run the dialog
		MessageT message = msg_Nothing;
		dialog->Show();
		do {
			message = handler.DoDialog();
		} while (message != msg_OK && message != msg_Cancel);
		if (message == msg_Cancel)
			return;
		nameField->GetDescriptor(serverName);
		mServerType = popButton->GetValue() ? ePOPServer : eIMAPServer;
	} // <-----End scope for name/type dialog
#endif // PRELIMINARY_DIALOG
	Boolean usePOP = (mServerType == ePOPServer);
	if (UGetInfo::ConductMailServerInfoDialog(
		serverName,
		usePOP,
		true))
	{
		mServerType = usePOP ?  ePOPServer : eIMAPServer;
		if (usePOP)
		{
			XP_FREEIF(mPopServerName);
			mPopServerName = XP_STRDUP(serverName);
		}
		LoadPrefs();
		mServerTable->Refresh();
	}
} // CMailNewsMailServerMediator::AddButton

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::EditButton()
// Grab the selected item in the list, create a duplicate of the server associated with that
// item and pop up a dialog to edit this server. The OK/Cancel is handled in ObeyCommand().
//----------------------------------------------------------------------------------------
{
	CellContents contents;
	GetHostFromSelectedRow(contents, sizeof(CellContents));
	Boolean usePOP = (mServerType == ePOPServer);
	if (UGetInfo::ConductMailServerInfoDialog(
			(CStr255&)contents.description,
			usePOP,
			false))
	{
		mServerType = usePOP ?  ePOPServer : eIMAPServer;
	}
} // CMailNewsMailServerMediator::EditButton

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::DeleteButton()
//----------------------------------------------------------------------------------------
{
	CellContents contents;
	GetHostFromSelectedRow(contents, sizeof(CellContents));
	if (UStdDialogs::AskOkCancel(
		GetPString(MK_MSG_REMOVE_MAILHOST_CONFIRM + RES_OFFSET)))
	{
		if (UsingPop())
		{
			XP_FREEIF(mPopServerName);
		}
		else
		{
			::MSG_DeleteIMAPHost(
				CMailNewsContext::GetMailMaster(),
				FindMSG_Host((const char*)(CStr255)contents.description));
		}
		LoadList();
		mServerTable->Refresh();
		UpdateButtons();
	}
} // CMailNewsMailServerMediator::DeleteButton

//----------------------------------------------------------------------------------------
MSG_IMAPHost* CMailNewsMailServerMediator::FindMSG_Host(const char* inHostName)
// Back end will match only on the name, so pass garbage for all the other
// parameters.
//----------------------------------------------------------------------------------------
{
	return ::MSG_CreateIMAPHost(
							CMailNewsContext::GetMailMaster(),
							inHostName,
							false, // XP_Bool isSecure,
							nil, //const char *userName,
							false, // XP_Bool checkNewMail,
							0, // int	biffInterval,
							false, // XP_Bool rememberPassword,
							false, // XP_Bool usingSubscription,
							false, //XP_Bool overrideNamespaces,
							nil, // const char *personalOnlineDir,
							nil, // const char *publicOnlineDir,
							nil //const char *otherUsersOnlineDir
							);
}

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::LoadList()
//----------------------------------------------------------------------------------------
{
	ClearList();
	if (UsingPop())
	{
		if (mPopServerName)
		{
			CellContents contents(mPopServerName);
			mServerTable->InsertRows(
				1, LArray::index_Last,
				&contents, sizeof(CellContents), false);
		}
		return;
	}
	char *serverList = nil;
	PREF_CopyCharPref("network.hosts.imap_servers", &serverList);
	if (serverList)
	{
		char *serverPtr = serverList;
		while (serverPtr)
		{
			char *endPtr = XP_STRCHR(serverPtr, ',');
			if (endPtr)
				*endPtr++ = '\0';
			CellContents cell(serverPtr);
			mServerTable->InsertRows(1, LArray::index_Last, &cell, sizeof(CellContents), false);
			serverPtr = endPtr;
		}
		XP_FREE(serverList);
	}
} // CMailNewsMailServerMediator::LoadList

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::WriteList()
// Here's what we're doing currently.  We're allowing the properties dialogs to write out
// their preferences each time they are dismissed.  The bad thing about this is that "Cancel"
// on the prefs window will not cancel the results of all the properties dialogs that have
// been made.  Instead, we write out the list of IMAP servers.  Some of the properties dialogs
// may be orphaned by this, in the sense that we will have written out preferences for servers
// that are not currently on the server list.  I think this might be acceptable, since these
// servers will not be used by the back end.  Their properties will remain dormant till
// the server is again added to the list.
//----------------------------------------------------------------------------------------
{
	TableIndexT	rows, cols;
	mServerTable->GetTableSize(rows, cols);
	if (rows == 0 || mServersLocked)
		return;
	PREF_SetIntPref("mail.server_type", mServerType);
	if (mServerType == ePOPServer)
	{
		PREF_SetCharPref("network.hosts.pop_server", mPopServerName);
		return;
	}
#if 0
	// There may be no need to do this. The back end updates the list after every
	// add and delete.
	char *serverList = nil;
	for (STableCell cell(1, 1); cell.row <= rows; ++cell.row)
	{
		CellContents contents;
		Uint32 cellSize = sizeof(contents);
		mServerTable->GetCellData(cell, &contents, cellSize);
		XP_ASSERT(sizeof(cell) == cellSize);
		if (cell.row > 1)
			StrAllocCat(serverList, ",");		
		StrAllocCat(serverList, (const char*)(CStr255)contents.description);
	}
	PREF_SetCharPref("network.hosts.imap_servers", serverList);
#endif // 0
} // CMailNewsMailServerMediator::WriteList

enum
{
//	eNewsServerEditText = 13001,
//	eNewsServerPortIntText,
//	eNewsServerSecureBox,
	eNotifyLargeDownloadBox,
	eNotifyLargeDownloadEditField
};

//----------------------------------------------------------------------------------------
CMailNewsNewsServerMediator::CMailNewsNewsServerMediator(LStream*)
//----------------------------------------------------------------------------------------
:	CServerListMediator(class_ID)
,	mNewsServerPortLocked(false)
{
}

#if 0
//----------------------------------------------------------------------------------------
Boolean CMailNewsNewsServerMediator::PortNumberValidationFunc(CValidEditField *portNumber)
//----------------------------------------------------------------------------------------
{
	Boolean	result;
	Str255	currentValue;
	portNumber->GetDescriptor(currentValue);
	if (!currentValue[0])
	{
		// If the user wipes out the port number field it is evaluated as zero, which
		// is a valid port number, but it is not what we want to happen if the field
		// is blank.
		// We want to put in the Well Known port number, which depends on whether or
		// not the server is secure. So we need to get the secure checkbox value.
		// NNTP standard port = 119
		// NNTP secure (SSL) standard port = 563
		LView	*superView = portNumber->GetSuperView();
		XP_ASSERT(superView);
		LControl *checkbox = dynamic_cast<LControl*>(superView->FindPaneByID(eNewsServerSecureBox));
		XP_ASSERT(checkbox);
		portNumber->SetValue(checkbox->GetValue() ? eNNTPSecurePort : eNNTPStandardPort);
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
#endif

//----------------------------------------------------------------------------------------
void CMailNewsNewsServerMediator::UpdateFromIC()
//----------------------------------------------------------------------------------------
{
	// Note: should we initialize the list from IC if there are no entries yet?
	if (mServersLocked)
		return;
#if 1
	if (UseIC())
	{
		long port = eNNTPStandardPort;
		CStr255 serverName;
		CInternetConfigInterface::GetInternetConfigString(kICNNTPHost, serverName, &port);
		
		// Create or find the host
		MSG_NewsHost* host = ::MSG_CreateNewsHost(
								CMailNewsContext::GetMailMaster(),
								serverName,
								port == eNNTPSecurePort,
								port);
		// Stick it in the list by rebuilding the list
		LoadList();
	}
#else
	SetEditFieldsWithICPref(kICNNTPHost,
							eNewsServerEditText,
							true,
							eNewsServerPortIntText,
							mNewsServerPortLocked,
							eNNTPStandardPort);
#endif // 1
}

//----------------------------------------------------------------------------------------
void CMailNewsNewsServerMediator::AddButton()
// Create a brand new (empty) server and pop up a dialog to let the user edit it. The
// OK/Cancel is handled in ObeyCommand().
//----------------------------------------------------------------------------------------
{
	CNewsSubscriber::DoAddNewsHost();
	LoadPrefs();
	mServerTable->Refresh();
} // CMailNewsNewsServerMediator::AddButton

//----------------------------------------------------------------------------------------
void CMailNewsNewsServerMediator::EditButton()
// Grab the selected item in the list, create a duplicate of the server associated with that
// item and pop up a dialog to edit this server. The OK/Cancel is handled in ObeyCommand().
//----------------------------------------------------------------------------------------
{
	CellContents contents;
	GetHostFromSelectedRow(contents, sizeof(CellContents));
	UGetInfo::ConductNewsServerInfoDialog(contents.serverData);
} // CMailNewsNewsServerMediator::EditButton

//----------------------------------------------------------------------------------------
void CMailNewsNewsServerMediator::DeleteButton()
//----------------------------------------------------------------------------------------
{
	CellContents contents;
	GetHostFromSelectedRow(contents, sizeof(CellContents));
	::MSG_DeleteNewsHost(CMailNewsContext::GetMailMaster(),
				MSG_GetNewsHostFromMSGHost(contents.serverData));
	LoadList();
	mServerTable->Refresh();
	UpdateButtons();
} // CMailNewsNewsServerMediator::DeleteButton

//----------------------------------------------------------------------------------------
void CMailNewsNewsServerMediator::LoadPrefs()
//----------------------------------------------------------------------------------------
{
	mServersLocked = PREF_PrefIsLocked("network.hosts.nntp_server");
	LoadList();
}

//----------------------------------------------------------------------------------------
void CMailNewsNewsServerMediator::WritePrefs()
//----------------------------------------------------------------------------------------
{
	/*
	mcmullen and andrewb 97/08/11
	
	Fix for bug 80691. If no MSG_FolderPane has been created when we tweak
	news.server_change_xaction then we will run into trouble when the user
	opens up the Message Center. The new default news server will appear
	twice in the Messsage Center. This is not only bad from a visual standpoint,
	but if the user tries to delete one copy and then the second, they will
	crash.
	
	This fix is a Good Kludge(tm) because it's localized to this little piece of
	code right here, we clean up after ourselves and we incur no performance
	penalty if the MSG_FolderPane has already been created since that is a unique
	object that is reference counted.
	*/
#ifdef DEFAULT_NEWS_SERVER_KLUDGE
	MSG_Pane* pane = NULL;
	CMailNewsContext* context = new CMailNewsContext(MWContextMailNewsProgress);
	Assert_(context);
#ifdef DEBUG
	if (context)
#endif
		pane = ::MSG_CreateFolderPane(*context, CMailNewsContext::GetMailMaster());
#endif

	// ### mwelch 97 July
	// Since preferences are not transactionalized, and
	// the preferences above need to trigger a single response in
	// the back end, we set an additional preference with
	// any different value, so that the back end callback
	// will be called by the prefs API. (This is not an FE
	// issue, merely an issue with the prefs API.)	
	int32 tempInt;
	PREF_GetIntPref("news.server_change_xaction", &tempInt);
	PREF_SetIntPref("news.server_change_xaction", ++tempInt);
	
#ifdef DEFAULT_NEWS_SERVER_KLUDGE
	if (context)
		::XP_InterruptContext((MWContext*)*context);
	if (pane)
		::MSG_DestroyPane(pane);
#endif
} // CMailNewsNewsServerMediator::WritePrefs

//----------------------------------------------------------------------------------------
void CMailNewsNewsServerMediator::LoadList()
//----------------------------------------------------------------------------------------
{
	ClearList();
	const int32 kMaxLists = 32;
	MSG_NewsHost* serverList[kMaxLists];
	int32 listSize = ::MSG_GetNewsHosts(
				CMailNewsContext::GetMailMaster(),
				(MSG_NewsHost**)serverList,
				kMaxLists);
	for (int i = 0; i < listSize; i++)
	{
		MSG_Host* host = ::MSG_GetMSGHostFromNewsHost(serverList[i]);
		const char* name = ::MSG_GetHostName(host);
		CellContents contents(name, host);
		mServerTable->InsertRows(1, LArray::index_Last, &contents, sizeof(CellContents), false);
	}
} // CMailNewsNewsServerMediator::LoadList

//----------------------------------------------------------------------------------------
void CMailNewsNewsServerMediator::WriteList()
//----------------------------------------------------------------------------------------
{
} // CMailNewsNewsServerMediator::WriteList

enum
{
	eOKButton = 900,
//	eServerTable = 13101,
//	eNewDirectoryButton = 13104,
//	eEditDirectoryButton,
//	eDeleteDirectoryButton,
	eFirstLastRButton = 13107,
	eLastFirstRButton = 13108
};

//----------------------------------------------------------------------------------------
CMailNewsDirectoryMediator::CMailNewsDirectoryMediator(LStream*)
//----------------------------------------------------------------------------------------
:	CServerListMediator(class_ID)
,	mDeletedServerList(nil)
{
}

//----------------------------------------------------------------------------------------
void CMailNewsDirectoryMediator::AddButton()
// Create a brand new (empty) server and pop up a dialog to let the user edit it. The
// OK/Cancel is handled in ObeyCommand().
//----------------------------------------------------------------------------------------
{
	DIR_Server	*newServer = (DIR_Server *)XP_ALLOC(sizeof(DIR_Server));
	DIR_InitServer(newServer);

	CLDAPServerPropDialog* dialog = (CLDAPServerPropDialog*) 
					LWindow::CreateWindow ( CLDAPServerPropDialog::eLDAPServerPropertiesDialogResID, this );
	dialog->SetUpDialog( newServer, CLDAPServerPropDialog::eNewServer, IsPABServerSelected(newServer),
							mServersLocked );

} // AddButton

//----------------------------------------------------------------------------------------
void CMailNewsDirectoryMediator::EditButton()
// Grab the selected item in the list, create a duplicate of the server associated with that
// item and pop up a dialog to edit this server. The OK/Cancel is handled in ObeyCommand().
//----------------------------------------------------------------------------------------
{
	// get the data
	STableCell currentCell;
	CellContents cell;
	currentCell = mServerTable->GetFirstSelectedCell();
	XP_ASSERT(currentCell.row);
	Uint32 cellDataSize = sizeof(CellContents);
	mServerTable->GetCellData(currentCell, &cell, cellDataSize);
	XP_ASSERT(cellDataSize == sizeof(CellContents));

	// copy it
	DIR_Server* tempServer;
	DIR_CopyServer(cell.serverData, &tempServer);
	
	// show dialog
	CLDAPServerPropDialog* dialog = (CLDAPServerPropDialog*) 
					LWindow::CreateWindow ( CLDAPServerPropDialog::eLDAPServerPropertiesDialogResID, this );
	dialog->SetUpDialog( tempServer, CLDAPServerPropDialog::eEditServer, IsPABServerSelected(tempServer),
							mServersLocked );
} // EditButton

//----------------------------------------------------------------------------------------
void CMailNewsDirectoryMediator::DeleteButton()
//----------------------------------------------------------------------------------------
{
	STableCell currentCell;
	currentCell = mServerTable->GetFirstSelectedCell();
	CellContents cell;
	Uint32 cellDataSize = sizeof(CellContents);
	mServerTable->GetCellData(currentCell, &cell, cellDataSize);
	XP_ASSERT(cellDataSize == sizeof(CellContents));
	if (!mDeletedServerList)
		mDeletedServerList = XP_ListNew();
	if (mDeletedServerList)
		XP_ListAddObjectToEnd(mDeletedServerList, cell.serverData);
//	DIR_DeleteServer(cell.serverData);
	mServerTable->RemoveRows(1, currentCell.row, true);
	UpdateButtons();
	mServersDirty = true;
}

//----------------------------------------------------------------------------------------
Boolean CMailNewsDirectoryMediator::IsPABServerSelected(LTextColumn *serverTable)
//----------------------------------------------------------------------------------------
{
	Boolean	result = false;
	STableCell		currentCell;
	currentCell = serverTable->GetFirstSelectedCell();
	if (currentCell.row)
	{
		if (!serverTable)
		{
			serverTable = mServerTable;
			XP_ASSERT(serverTable);
		}
		CellContents cell;
		Uint32 cellDataSize = sizeof(CellContents);
		serverTable->GetCellData(currentCell, &cell, cellDataSize);
		XP_ASSERT(cellDataSize == sizeof(CellContents));
		result = IsPABServerSelected(cell.serverData);
	}
	return result;
}

//----------------------------------------------------------------------------------------
Boolean CMailNewsDirectoryMediator::IsPABServerSelected(DIR_Server *server)
//----------------------------------------------------------------------------------------
{
	Boolean	result = (PABDirectory == server->dirType);
	return result;
}

//----------------------------------------------------------------------------------------
void CMailNewsDirectoryMediator::UpdateButtons()
//----------------------------------------------------------------------------------------
{
	MServerListMediatorMixin::UpdateButtons();
	STableCell currentCell = mServerTable->GetFirstSelectedCell();
	if (IsPABServerSelected(mServerTable))
	{
		LControl* deleteButton = (LControl*)FindPaneByID('Dele');
		deleteButton->Disable();
	}
}

//----------------------------------------------------------------------------------------
Boolean CMailNewsDirectoryMediator::ObeyCommand( CommandT inCommand, void* ioParam )
// Used to receive the commands passed along from the LDAPServerProperties dialog (the
// OK and Cancel messages).
//----------------------------------------------------------------------------------------
{
	switch (inCommand)
	{
		// Handle cancel from the server properties dialog. All we have to do is dispose
		// of the server previously allocated.
		case CLDAPServerPropDialog::cmd_CancelButton: {
			SDialogResponse* theResponse = (SDialogResponse*) ioParam;
			Assert_ ( theResponse );		
			CLDAPServerPropDialog* dialog = (CLDAPServerPropDialog*) theResponse->dialogBox;
			Assert_ ( dialog );
			
			DIR_DeleteServer ( dialog->GetServer() );			
			UpdateButtons();			
			delete dialog;
			break;
		}
		
		// Handle OK from the server properties dialog. If we're adding a new server, we
		// have to add a row to the server table. If we're modifying an existing one,
		// update the UI (name of server), delete the old one, and hold on to the new one.
		// The data from the dialog fields has already been extracted into the server
		// held by the dialog class.
		case CLDAPServerPropDialog::cmd_OKButton: {
		
			mServersDirty = true;				// flag that we need to write out prefs
			
			SDialogResponse* theResponse = (SDialogResponse*) ioParam;
			Assert_ ( theResponse );		
			CLDAPServerPropDialog* dialog = (CLDAPServerPropDialog*) theResponse->dialogBox;
			Assert_ ( dialog );
			
			// initialize some stuff for updating the server list
			STableCell currentCell;
			currentCell = mServerTable->GetFirstSelectedCell();
			if (!currentCell.row)		// We want to add new servers at the end of the list
			{							// if there is no current selection.
				TableIndexT	rows, cols;
				mServerTable->GetTableSize(rows, cols);
				currentCell.row = rows;	
			}

			Uint32 cellDataSize = sizeof(CellContents);
			DIR_Server* newServer = dialog->GetServer();
			if ( dialog->IsEditingNewServer() ) {
				CellContents cell;
				
				// add the new server
				LStr255	tempDescription(newServer->description);
				LString::CopyPStr(tempDescription, cell.description);
				cell.serverData = newServer;
				mServerTable->InsertRows(1, currentCell.row, &cell, cellDataSize, true);
			}
			else {
				CellContents cell;
				
				// delete the old one...
				mServerTable->GetCellData(currentCell, &cell, cellDataSize);
				DIR_DeleteServer(cell.serverData);
				
				// ...then add the new one
				cell.description[0] = strlen(newServer->description);
				strcpy((char *)&(cell.description[1]), newServer->description);
				cell.serverData = newServer;
				mServerTable->SetCellData(currentCell, &cell, cellDataSize);
			}
			UpdateButtons();
			delete dialog;
			break;
		}

		default:
			LCommander::ObeyCommand( inCommand, ioParam );
		
	} // switch on command

	return true;
	
} // ObeyCommand

//----------------------------------------------------------------------------------------
void CMailNewsDirectoryMediator::LoadMainPane()
//----------------------------------------------------------------------------------------
{
	Inherited::LoadMainPane();
	mServersLocked = PREF_PrefIsLocked("ldap_1.number_of_directories");
} // LoadMainPane

//----------------------------------------------------------------------------------------
void CMailNewsDirectoryMediator::LoadList()
//----------------------------------------------------------------------------------------
{
	ClearList();
	XP_List	*directories = CAddressBookManager::GetDirServerList();
	if (XP_ListCount(directories))
	{
		DIR_Server	*server;
		#pragma warn_possunwant off
		while (server = (DIR_Server *)XP_ListNextObject(directories))
		#pragma warn_possunwant reset
		{
			if (server->description)
			{
				LStr255 description(server->description);
				CellContents cell;
				LString::CopyPStr(description, cell.description);
				cell.serverData = (DIR_Server *)XP_ALLOC(sizeof(DIR_Server));
				DIR_InitServer(cell.serverData);
				DIR_CopyServer(server, &cell.serverData);
				TableIndexT	rows, columns;
				mServerTable->GetTableSize(rows, columns);
				mServerTable->InsertRows(1, rows, &cell, sizeof(CellContents), false);
			}
		}
	}
}

//----------------------------------------------------------------------------------------
void CMailNewsDirectoryMediator::WriteList()
//----------------------------------------------------------------------------------------
{
	if (mServersDirty)
	{
		XP_List *xpList = XP_ListNew();
		TableIndexT	rows;
		STableCell	cellLocation;
		mServerTable->GetTableSize(rows, cellLocation.col);
		for (TableIndexT i = 1; i <= rows; ++i)
		{
			CellContents	cell;
			Uint32			cellSize = sizeof(cell); // it's an io parameter! jrm 97/03/19
			cellLocation.row = i;
			mServerTable->GetCellData(cellLocation, &cell, cellSize);
			XP_ASSERT(sizeof(cell) == cellSize);
			XP_ListAddObjectToEnd(xpList, cell.serverData);
		}
		CAddressBookManager::SetDirServerList(xpList);	// The CAddressBookMediator will
														// even write out the prefs for us.
		if (mDeletedServerList)
			DIR_CleanUpServerPreferences(mDeletedServerList);	// this call now owns the list
	}
} // CMailNewsDirectoryMediator::WriteList

//----------------------------------------------------------------------------------------
void CMailNewsDirectoryMediator::LoadPrefs()
//----------------------------------------------------------------------------------------
{
	LoadList();
}

//----------------------------------------------------------------------------------------
void CMailNewsDirectoryMediator::WritePrefs()
//----------------------------------------------------------------------------------------
{
	WriteList();
}

enum
{
	eUnreadOnlyBox = 13701,
	eByDateBox,
	eIncrementRButton,
	eIncrementMenu,
	eDaysRButton,
	eDaysEditField,
	eSelectMessageFolderPopup
};

//----------------------------------------------------------------------------------------
COfflineNewsMediator::COfflineNewsMediator(LStream*)
//----------------------------------------------------------------------------------------
:	CPrefsMediator(class_ID)
{
}

//----------------------------------------------------------------------------------------
// static
Boolean COfflineNewsMediator::SinceDaysValidationFunc(CValidEditField *sinceDays)
//----------------------------------------------------------------------------------------
{
	// If the checkbox or radio button isn't set then this value is really
	// ignored, so I will only put up the alert if the checkbox
	// is set, but I will force a valid value in any case.

	Boolean		result = true;

	// force valid value
	if (1 > sinceDays->GetValue())
	{
		int32	newInterval = 30;
		PREF_GetDefaultIntPref("offline.news.download.days", &newInterval);
		sinceDays->SetValue(newInterval);
		sinceDays->SelectAll();
		result = false;
	}
	if (!result)	// if the value is within the range, who cares
	{
		// Check for the check box and radio button...
		// We are assuming that the checkbox and radio button are subs of the
		// field's superview.
		LView	*superView = sinceDays->GetSuperView();
		XP_ASSERT(superView);
		LControl *checkbox = dynamic_cast<LControl*>(superView->FindPaneByID(eByDateBox));
		XP_ASSERT(checkbox);
		LControl *radioButton = dynamic_cast<LControl*>(superView->FindPaneByID(eDaysRButton));
		XP_ASSERT(radioButton);
		if (checkbox->GetValue() && radioButton->GetValue())
		{
			StPrepareForDialog	prepare;
			::StopAlert(1067, NULL);
		}
		else
		{
			result = true;	// go ahead and let them switch (after correcting the value)
							// if either the checkbox or radio button isn't set
		}
	}
	return result;
} // COfflineNewsMediator::SinceDaysValidationFunc

//----------------------------------------------------------------------------------------
void COfflineNewsMediator::LoadMainPane()
//----------------------------------------------------------------------------------------
{
	CPrefsMediator::LoadMainPane();
	SetValidationFunction(eDaysEditField, SinceDaysValidationFunc);
}

//----------------------------------------------------------------------------------------
void COfflineNewsMediator::WritePrefs()
//----------------------------------------------------------------------------------------
{
	CSelectFolderMenu* selectPopup =
		(CSelectFolderMenu*)FindPaneByID(eSelectMessageFolderPopup);
	XP_ASSERT(selectPopup);
	selectPopup->CommitCurrentSelections();
}

#endif // MOZ_LITE

