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
#include "CMessageFolder.h"
#include "PrefControls.h"
#include "CMailNewsContext.h"
#include "StGetInfoHandler.h"
#include "CNewsSubscriber.h"
#include "COfflinePicker.h"
#include "UMenuUtils.h"
//3P
#include <ICAPI.h>
#include <ICKeys.h>

//PP
#include <LTextColumn.h>
#include <LGAPushButton.h>
#include <UModalDialogs.h>
#include <LGARadioButton.h>
#include <LGACheckbox.h>

#ifdef MOZ_MAIL_NEWS

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
	mSecureBox = (LGACheckbox *) FindPaneByID(eSecureBox);
	XP_ASSERT(mSecureBox);
	mSavePasswordBox = dynamic_cast<LGACheckbox *>(FindPaneByID ( eSaveLDAPServerPasswordBox) );
	Assert_( mSavePasswordBox );
	
	mDownloadCheckBox = dynamic_cast<LGACheckbox* >( FindPaneByID ( eDownloadCheckBox ) );
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
		LGAPushButton *okButton = (LGAPushButton *) FindPaneByID(CPrefsDialog::eOKButtonID);
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
		LGACheckbox	*checkbox =
				(LGACheckbox *)superView->FindPaneByID(eSecureBox);
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
#endif // MOZ_MAIL_NEWS

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

#pragma mark -

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
			// if IC is being used don't bring up the file picker
			if( UseIC() )
				return;
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
					LGACheckbox *checkbox =
							(LGACheckbox *)FindPaneByID(inMessage);
					XP_ASSERT(checkbox);
					checkbox->SetValue(false);
				}
				else
				{
					fPicker->ListenToMessage(msg_Browse, nil);
					if (!fPicker->WasSet())
					{	// If the file picker is still unset, that means that the user
						// cancelled the file browse so we don't want the checkbox set.
						LGACheckbox *checkbox =
								(LGACheckbox *)FindPaneByID(inMessage);
						XP_ASSERT(checkbox);
						checkbox->SetValue(false);
					}
				}
			}
			break;
		case msg_FolderChanged:
			LGACheckbox *checkbox =
					(LGACheckbox *)FindPaneByID(eUseSigFileBox);
			XP_ASSERT(checkbox);
			checkbox->SetValue(true);
			break;
#endif // MOZ_LITE
		case eInternetConfigBox:
			Boolean	useIC = *(Int32 *)ioParam;
			UseIC(useIC);
			UpdateFromIC();
			#ifndef MOZ_MAIL_NEWS
				LButton *button =
						(LButton *)FindPaneByID(eLaunchInternetConfig);
				XP_ASSERT(button);
				if (UseIC())
					button->Enable();
				else
					button->Disable();
			#endif // MOZ_MAIL_NEWS
		
			break;
		default:
			Inherited::ListenToMessage(inMessage, ioParam);
			break;
	}
} // CMailNewsIdentityMediator::ListenToMessage

//----------------------------------------------------------------------------------------
void CMailNewsIdentityMediator::UpdateFromIC()
//----------------------------------------------------------------------------------------
{
	Boolean useIC = UseIC();
	#ifdef MOZ_LITE
			LButton *button =
					(LButton *)FindPaneByID(eLaunchInternetConfig);
			XP_ASSERT(button);
			if ( useIC )
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
									
			CFilePicker *picker =
					(CFilePicker *)FindPaneByID(eSigFilePicker);
			
			LCaption* caption = nil;
			if( picker )
				caption = (LCaption*)picker->FindPaneByID( 1 );
			
			if( picker && caption )
			{
				if ( useIC )
				{
					picker->Disable();
					LStr255 exampleString(12057, 1 );
					caption->SetDescriptor( exampleString);
				}
				else
				{
					picker->Enable();
					picker->SetCaptionForPath( caption, picker->GetFSSpec()  );
				}
			}
}



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

#ifdef MOZ_MAIL_NEWS

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

#pragma mark -

//----------------------------------------------------------------------------------------
CMailNewsOutgoingMediator::CMailNewsOutgoingMediator(LStream*)
//----------------------------------------------------------------------------------------
:	CPrefsMediator(class_ID)
{
	char** p = &mFolderURL[0];
	for (int i = 0; i < UFolderDialogs::num_kinds; i++,p++)
	{
		*p = nil;
	}
#ifdef HORRIBLE_HACK
	Boolean* isDefault = &mFolderIsDefault[0];
	for (int i = 0; i < UFolderDialogs::num_kinds; i++,isDefault++)
		*isDefault = false;
#endif // HORRIBLE_HACK
}

//----------------------------------------------------------------------------------------
CMailNewsOutgoingMediator::~CMailNewsOutgoingMediator()
//----------------------------------------------------------------------------------------
{
	char** p = &mFolderURL[0];
	for (int i = 0; i < UFolderDialogs::num_kinds; i++,p++)
		XP_FREEIF(*p); // BEWARE. The macro XP_FREEIF(*p++) increments three times!
}

//----------------------------------------------------------------------------------------
void CMailNewsOutgoingMediator::FixCaptionNameForFCC(UFolderDialogs::FolderKind kind, const char* mailOrNews)
// For mail/news_fcc, depending on a stupid boolean flag (blah.use_imap_sentmail),
// the prefname changes.  This overload figures out the server/local status using the saved pref,
// and calls the other overload, below.
//----------------------------------------------------------------------------------------
{
	// Check the boolean pref that determines the pref name to be used.
	CStr255 prefname = "^0.use_imap_sentmail";
	StringParamText(prefname, mailOrNews);
	XP_Bool onServer;
	PREF_GetBoolPref(prefname, &onServer);
	FixCaptionNameForFCC(kind, mailOrNews, onServer);
} // CMailNewsOutgoingMediator::FixCaptionNameForFCC

//----------------------------------------------------------------------------------------
void CMailNewsOutgoingMediator::FixCaptionNameForFCC(UFolderDialogs::FolderKind kind, const char* mailOrNews, Boolean onServer)
// For mail/news_fcc, depending on a stupid boolean flag (blah.use_imap_sentmail),
// the prefname changes. This overload is for the case where the prefs are not saved, and the
// onServer parameter is passed in by the caller.
//----------------------------------------------------------------------------------------
{
	// Work out which checkbox we have to fix up, and which string to use.  This only
	// applies to mail_fcc and news_fcc.
	PaneIDT checkboxPaneID;
	if (kind == UFolderDialogs::mail_fcc)
		checkboxPaneID = 'DoMF';
	else if (kind ==  UFolderDialogs::news_fcc)
		checkboxPaneID = 'DoNF';
	else
		return;
	
	// Set the caption pref name of the checkbox accordingly.
	CStr255 prefname;
	if (onServer)
		prefname = "^0.imap_sentmail_path";
	else
		prefname = "^0.default_fcc";
	StringParamText(prefname, mailOrNews);
	LControl* pb = dynamic_cast<LControl*>(FindPaneByID(checkboxPaneID));
	SignalIf_(!pb);
	if (pb)
		UPrefControls::NoteSpecialFolderChanged(pb, prefname);
} // CMailNewsOutgoingMediator::FixCaptionNameForFCC

//----------------------------------------------------------------------------------------
void CMailNewsOutgoingMediator::LoadPrefs()
//----------------------------------------------------------------------------------------
{
	FixCaptionNameForFCC(UFolderDialogs::news_fcc, "news");
	FixCaptionNameForFCC(UFolderDialogs::mail_fcc, "mail");	
	
	LControl* control = dynamic_cast<LControl*>(FindPaneByID(eMailMailSelfBox) ); 
	XP_ASSERT( control );
	CStr255 caption;
	control->GetDescriptor( caption );
	CStr255 email = FE_UsersMailAddress();
	StringParamText(caption, email);
	control->SetDescriptor( caption );
	control = dynamic_cast<LControl*>(FindPaneByID(eMailNewsSelfBox) );
	XP_ASSERT( control );
	control->SetDescriptor( caption );
} // CMailNewsOutgoingMediator::LoadPrefs

//----------------------------------------------------------------------------------------
void CMailNewsOutgoingMediator::WritePrefs()
//----------------------------------------------------------------------------------------
{
	char** p = &mFolderURL[0];
#ifdef HORRIBLE_HACK
	Boolean* isDefault = &mFolderIsDefault[0];
#endif // HORRIBLE_HACK
	CStr255 prefName;
	for (int i = 0; i < UFolderDialogs::num_kinds; i++,p++)
	{
		UFolderDialogs::FolderKind kind = (UFolderDialogs::FolderKind)i;
		if (*p)
		{
			// For fcc, we must write out the IMAP/Local pref first, so that the prefs name
			// will be set correctly in BuildPrefName(), below.
			XP_Bool onServer = (XP_Bool)(NET_URL_Type(*p) == IMAP_TYPE_URL);
			if (kind == UFolderDialogs::news_fcc)
				PREF_SetBoolPref("news.use_imap_sentmail", onServer);
			else if (kind == UFolderDialogs::mail_fcc)
				PREF_SetBoolPref("mail.use_imap_sentmail", onServer);
			Boolean saveAsAlias;
			UFolderDialogs::BuildPrefName(kind, prefName, saveAsAlias);
#ifdef HORRIBLE_HACK
			if (*isDefault++) 
			{
				// Hack-to-work-around-backend-design-mess #23339: if it's a local folder and it's
				// the server, then it's supposed to mean the default folder.  But the backend
				// doesn't deal with this properly.  So append the path here.
				CStr255 defaultFolderName;
				if (UFolderDialogs::GetDefaultFolderName(kind, defaultFolderName))
				{
					StrAllocCat(*p, "/");
					StrAllocCat(*p, (const char*)defaultFolderName);
				}
			}
#endif // HORRIBLE_HACK
#if DEBUG
			if (kind == UFolderDialogs::news_fcc || kind == UFolderDialogs::mail_fcc)
			{
				Assert_(saveAsAlias == !onServer);
			}
			else
			{
				Assert_(!saveAsAlias);
			}
#endif
			if (saveAsAlias)
			{
				// For backward compatibility with 4.0x, we have to save this as a path,
				// and not a URL in these two cases.
				if (kind == UFolderDialogs::news_fcc || kind == UFolderDialogs::mail_fcc)
				{
					Boolean isURL = NET_URL_Type(*p) == MAILBOX_TYPE_URL;
					Assert_(isURL);
					if (isURL)
					{
						char* temp = NET_ParseURL(*p, GET_PATH_PART);
						if (temp)
						{
							XP_FREE(*p);
							*p = temp;
						}
					}
				}
				PREF_SetPathPref(prefName, *p, FALSE);
			}
			else
				PREF_SetCharPref(prefName, *p);
		}
	}
} // CMailNewsOutgoingMediator::WritePrefs

//----------------------------------------------------------------------------------------
void CMailNewsOutgoingMediator::ListenToMessage(MessageT inMessage, void* ioParam)
//----------------------------------------------------------------------------------------
{
	UFolderDialogs::FolderKind kind;
	PaneIDT descriptionPaneID; // ID of the checkbox or caption corresponding to the button
	const char* mailOrNews = nil; // substitution string for the caption prefname.
	// We handle the four "choose folder" buttons here.
	switch (inMessage)
	{
		case 'ChMF':
			kind = UFolderDialogs::mail_fcc;
			descriptionPaneID = 'DoMF'; // a checkbox
			mailOrNews = "mail";
			break;
		case 'ChNF':
			kind = UFolderDialogs::news_fcc;
			descriptionPaneID = 'DoNF'; // a checkbox
			mailOrNews = "news";
			break;
		case 'ChDF':
			kind = UFolderDialogs::drafts;
			descriptionPaneID = 'Drft'; // a caption
			break;
		case 'ChTF':
			kind = UFolderDialogs::templates;
			descriptionPaneID = 'Tmpl'; // a caption
			break;
		default:
			Inherited::ListenToMessage(inMessage, ioParam);
			return;
	}
	CMessageFolder curFolder;
	const char* curURL = mFolderURL[kind];
	if (curURL)
		curFolder.SetFolderInfo(
			::MSG_GetFolderInfoFromURL(CMailNewsContext::GetMailMaster(), curURL, false));
	CMessageFolder folder = UFolderDialogs::ConductSpecialFolderDialog(kind, curFolder);
	if (folder.GetFolderInfo() != nil)
	{
		URL_Struct* ustruct = ::MSG_ConstructUrlForFolder(nil, folder);
		// Set the cached URL string for this folder (used in WritePrefs), and update
		// the text on the corresponding special folder checkbox.
		if (ustruct)
		{
			char* newURL = XP_STRDUP(ustruct->address);
			NET_FreeURLStruct(ustruct);
#ifdef HORRIBLE_HACK
			if (folder.IsMailServer() && !folder.IsIMAPMailFolder()) 
			{
				// Hack-to-work-around-backend-design-mess #23339: if it's a local folder and it's
				// the server, then it's supposed to mean the default folder.  But the backend
				// doesn't deal with this properly.
				mFolderIsDefault[kind] = true; // so we append the folder name when we write it out.
			}
#endif // HORRIBLE_HACK
			XP_FREEIF(mFolderURL[kind]);
			mFolderURL[kind] = newURL;
			
			LPane* paneShowingFolderText = FindPaneByID(descriptionPaneID);
			// If it's one of the fcc preferences, we have to make sure the caption is changed
			// because the server/local cases are different.  The checkbox will update to show
			// the folder currently saved for that pref name.
			if (mailOrNews)
				FixCaptionNameForFCC(kind, mailOrNews, folder.IsIMAPMailFolder());
			// Then we have to update the checkbox caption to reflect the actual folder the
			// user has just selected..
			UPrefControls::NoteSpecialFolderChanged(paneShowingFolderText, kind, folder);
		}
	}
} // CMailNewsOutgoingMediator::ListenToMessage

#pragma mark -

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
void MServerListMediatorMixin::SetHostDataForRow(TableIndexT inRow, const CellContents& inCellData, UInt32 inDataSize) const
//----------------------------------------------------------------------------------------
{
	TableIndexT rowCount, colCount;
	mServerTable->GetTableSize(rowCount, colCount);
	Assert_(inRow >0 && inRow <= rowCount);
	STableCell cell(inRow, 1);
	Uint32 cellDataSize = inDataSize;
	mServerTable->SetCellData(cell, &inCellData, cellDataSize);
	Assert_(cellDataSize >= inDataSize);
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
Boolean MServerListMediatorMixin::HostExistsElsewhereInTable(const CStr255& inHostName, TableIndexT &ioHostRow) const
//----------------------------------------------------------------------------------------
{
	Boolean		result = false;
	
	// Is this server name duplicated?
	TableIndexT	rows, cols;
	mServerTable->GetTableSize(rows, cols);

	TableIndexT		ignoreRow = ioHostRow;
	
	for (STableCell cell(1, 1); cell.row <= rows; ++cell.row)
	{
		if (cell.row == ignoreRow)
			continue;
			
		CellContents 	contents;
		Uint32 			cellSize = sizeof(contents);
		mServerTable->GetCellData(cell, &contents, cellSize);
		
		XP_ASSERT(sizeof(CellContents) <= cellSize);	// size in PPob is too small!
		
		if ( inHostName == contents.description )		// we have a duplicate server name.
														// Uses CStr == operator, which is case insensitive
		{
			result = true;
			ioHostRow = cell.row;
			break;
		}
	}
	
	return result;

} // CMailNewsMailServerMediator::GetHostFromRow

//----------------------------------------------------------------------------------------
void MServerListMediatorMixin::DeleteSelectedRow(Boolean inRefresh)
//----------------------------------------------------------------------------------------
{
	STableCell currentCell = mServerTable->GetFirstSelectedCell();
	if (currentCell.row)
		mServerTable->RemoveRows(1, currentCell.row, inRefresh);
		
} // CMailNewsMailServerMediator::GetHostFromRow

//----------------------------------------------------------------------------------------
void MServerListMediatorMixin::UpdateSelectedRow(const CellContents& inCellData,
										Uint32 inDataSize, Boolean inRefresh)
//----------------------------------------------------------------------------------------
{
	STableCell currentCell = mServerTable->GetFirstSelectedCell();
	if (currentCell.row)
		SetHostDataForRow(currentCell.row, inCellData, inDataSize);
	
	if (inRefresh)
		mServerTable->Refresh();
		
} // CMailNewsMailServerMediator::GetHostFromRow


//----------------------------------------------------------------------------------------
void MServerListMediatorMixin::AppendNewRow(const CellContents& inCellData,
										Uint32 inDataSize, Boolean inRefresh)
//----------------------------------------------------------------------------------------
{
	mServerTable->InsertRows(1, LArray::index_Last, &inCellData, inDataSize, inRefresh);
		
} // CMailNewsMailServerMediator::GetHostFromRow

//----------------------------------------------------------------------------------------
TableIndexT MServerListMediatorMixin::CountRows()
//----------------------------------------------------------------------------------------
{
	TableIndexT		numRows, numCols;
	
	mServerTable->GetTableSize(numRows, numCols);
	
	return numRows;
		
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
	LControl* addButton = (LControl*)mMediatorSelf->FindPaneByID('New…');
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
		case 'New…':
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
/*static */Boolean MServerListMediatorMixin::ServerIsInCommaSeparatedList(
					const char *inServerName, const char *inServerList)
//----------------------------------------------------------------------------------------
{
	char	*listCopy;
	char	*curToken;
	Boolean	result = false;
	
	if (inServerList == NULL) return false;
	
	listCopy = XP_STRDUP(inServerList);		// since strtok modifies it
	ThrowIfNil_(listCopy);
	
	curToken = XP_STRTOK(listCopy, ",");
	
	while (curToken != NULL)
	{
		// skip white space
		while (*curToken && *curToken == ' ')
			curToken ++;
			
		if (XP_STRCMP(inServerName, curToken) == 0)
		{
			result = true;
			break;
		}
		
		curToken = XP_STRTOK(NULL, ",");
	}
	
	XP_FREE(listCopy);
	return result;
}

#pragma mark -

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

#pragma mark -

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
#ifdef BEFORE_INVISIBLE_POPSERVER_NAME_EDITFIELD_TRICK_WAS_THOUGHT_OF
,	mPOPServerName(nil)
#endif
{
} // CMailNewsMailServerMediator::CMailNewsMailServerMediator

//----------------------------------------------------------------------------------------
CMailNewsMailServerMediator::~CMailNewsMailServerMediator()
//----------------------------------------------------------------------------------------
{
#ifdef BEFORE_INVISIBLE_POPSERVER_NAME_EDITFIELD_TRICK_WAS_THOUGHT_OF
	XP_FREEIF(mPOPServerName);
#endif
}

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::SetPOPServerName(const CStr255& inName)
//----------------------------------------------------------------------------------------
{
#ifdef BEFORE_INVISIBLE_POPSERVER_NAME_EDITFIELD_TRICK_WAS_THOUGHT_OF
	XP_FREEIF(mPOPServerName);
	if (inName.Length() != 0)
		mPOPServerName = XP_STRDUP(inName);
#else // Now:
	LEditField* editField = dynamic_cast<LEditField*>(FindPaneByID('SNAM'));
	Assert_(editField);
	if (editField)
		editField->SetDescriptor(inName);
#endif
} // CMailNewsMailServerMediator::SetPOPServerName

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::GetPOPServerName(CStr255& outName) const
//----------------------------------------------------------------------------------------
{
	outName[0] = 0;
#ifdef BEFORE_INVISIBLE_POPSERVER_NAME_EDITFIELD_TRICK_WAS_THOUGHT_OF
	if (mPOPServerName)
		outName = mPOPServerName;
#else // Now:
	LEditField* editField = dynamic_cast<LEditField*>(FindPaneByID('SNAM'));
	Assert_(editField);
	if (!editField)
		return;
	editField->GetDescriptor(outName);
#endif
} // CMailNewsMailServerMediator::GetPOPServerName

//----------------------------------------------------------------------------------------
Boolean CMailNewsMailServerMediator::NoAtSignValidationFunc(CValidEditField *noAtSign)
//----------------------------------------------------------------------------------------
{
	CStr255 value;
	noAtSign->GetDescriptor(value);
	unsigned char atPos = value.Pos("@");
	if (atPos)
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
/* static */ Boolean CMailNewsMailServerMediator::ValidateServerName(const CStr255& inServerName,
							Boolean inNewServer, const CServerListMediator* inServerList)
//----------------------------------------------------------------------------------------
{
	if (inServerName.IsEmpty())
		return false;
	
	const CMailNewsMailServerMediator *theMediator = dynamic_cast<const CMailNewsMailServerMediator *>(inServerList);
	ThrowIfNil_(theMediator);
	
	TableIndexT	foundRow = LArray::index_Bad;
	
	// Is this server name duplicated?
	if (!inNewServer)
	{
		STableCell 	currentCell = theMediator->mServerTable->GetFirstSelectedCell();	// this must be the server we are editing
		foundRow = currentCell.row;
	}
		
	return ! theMediator->HostExistsElsewhereInTable(inServerName, foundRow);
}

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
	// Check whether this is the first call on entry (we call LoadPrefs more than once).
	if (mServerType == eUnknownServerType)
	{
		int32 serverType = eUnknownServerType;
		int32 prefResult = PREF_GetIntPref("mail.server_type", &serverType);
		ThrowIf_(prefResult != PREF_NOERROR);
		mServerType = (ServerType)serverType;
	}
#ifdef BEFORE_INVISIBLE_POPSERVER_NAME_EDITFIELD_TRICK_WAS_THOUGHT_OF
	if (mServerType == ePOPServer)
	{
		char* value = nil;
		int32 prefResult = PREF_CopyCharPref("network.hosts.pop_server", &value);
		if (prefResult == PREF_NOERROR && value)
			if (*value)
				mPOPServerName = value;
			else
				XP_FREE(value);
	}
#endif
	mServersLocked = PREF_PrefIsLocked("network.hosts.pop_server")
					|| PREF_PrefIsLocked("mail.server_type")
					|| PREF_PrefIsLocked("mail.server_type_on_restart");

	// Make sure there's a default value
	Boolean userNameLocked = PaneHasLockedPref(eMailServerUserNameField);
	LEditField* popUserNameField =
			dynamic_cast<LEditField*>(FindPaneByID(eMailServerUserNameField));
	if (popUserNameField && !userNameLocked)
	{
		CStr255 username;
		popUserNameField->GetDescriptor(username);
		if (username.Length() == 0)
		{
			UGetInfo::GetDefaultUserName(username);
			popUserNameField->SetDescriptor(username);
		}
	}

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

	LEditField* popUserNameField = (LEditField*)FindPaneByID(eMailServerUserNameField);
	Assert_(popUserNameField);
	
	if ((userNameLocked && mServersLocked) || !UseIC())
	{
		popUserNameField->Enable();
		return;
	}

	Str255	s;
	long	port = POP3_PORT;		// port is only returned if the user has a
									// :port after the server name in IC. Use POP as default
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
		s[0] =  i-2 ;
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
		
#ifdef BEFORE_INVISIBLE_POPSERVER_NAME_EDITFIELD_TRICK_WAS_THOUGHT_OF
		XP_FREEIF(mPOPServerName);
#endif

		if (port == POP3_PORT)
		{
#ifdef BEFORE_INVISIBLE_POPSERVER_NAME_EDITFIELD_TRICK_WAS_THOUGHT_OF
			mPOPServerName = XP_STRDUP((const char*)(CStr255)server);
#endif

			// If there is just one server, and there is no inbox yet,
			// then set up that server from IC
			if (CountRows() <= 1 && CMailNewsContext::UserHasNoLocalInbox())
			{
				SetPOPServerName(server);
				mServerType = ePOPServer;
				
				CStr255			serverName(server);
				CellContents	cellData(serverName);
				
				if (CountRows() == 0)
					AppendNewRow(cellData, sizeof(CellContents));
				else
					SetHostDataForRow(1, cellData, sizeof(CellContents));
			}
			
		}
		else
		{
			// Only change the IMAP server settings if this is a new profile
			//	Change the cell data to show the new server name, and refresh.
		
			if (CountRows() <= 1 && CMailNewsContext::UserHasNoLocalInbox())
			{
				CStr255			serverName(server);
				CellContents	cellData(serverName);
				
				if (CountRows() == 0)
					AppendNewRow(cellData, sizeof(CellContents));
				else
					SetHostDataForRow(1, cellData, sizeof(CellContents));
					
				mServerType = eIMAPServer;
			}
			
		}
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
	
	CStr255 	serverName; // constructor sets to empty string.
	Boolean		allowServerTypeEdit = CountRows() == 0;
	Boolean		usePop = CountRows() == 0;
	
	// Pass this in as the super commander, because we want to defer the destruction
	// of the get info dialog. This is because we don't want to commit its prefs unless the
	// user OKs the whole prefs dialog.
	if (UGetInfo::ConductMailServerInfoDialog(serverName, usePop, true, true,
					allowServerTypeEdit, UGetInfo::kSubDialogOfPrefs, ValidateServerName, this, this))
	{
		NoteServerChanges(usePop, serverName);
		
		CellContents	cellData(serverName);
		AppendNewRow(cellData, sizeof(CellContents));
	}
	
} // CMailNewsMailServerMediator::AddButton

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::NoteServerChanges(Boolean inPOP, const CStr255& inServerName)
//----------------------------------------------------------------------------------------
{
	mServerType = inPOP ?  ePOPServer : eIMAPServer;
#ifdef BEFORE_INVISIBLE_POPSERVER_NAME_EDITFIELD_TRICK_WAS_THOUGHT_OF
	// Since the server info dialog writes out the new stuff immediately, for consistency
	// we should make these prefs take effect immediately also.  This means that "cancel"
	// doesn't, even more.
	if (inPOP && inServerName.Length() > 0)
	{
		SetPOPServerName(inServerName);
		int32 prefResult = PREF_SetCharPref("network.hosts.pop_server", inServerName);
		ThrowIf_(prefResult < PREF_NOERROR); // PREF_VALUECHANGED = 1 is expected!
	}
	int32 prefResult = PREF_SetIntPref("mail.server_type_on_restart", mServerType);
	ThrowIf_(prefResult < PREF_NOERROR); // PREF_VALUECHANGED = 1 is expected!
	// In Nova, we are attempting to make "convert immediately" work! so here goes:
	prefResult = PREF_SetIntPref("mail.server_type", mServerType);
	ThrowIf_(prefResult < PREF_NOERROR);
#else
	// Now.
	// Because of historic XP code, you have to have a POP server name, even for IMAP.  So
	// we might as well always set it to be the same as the IMAP server name.
	if (! inServerName.IsEmpty())
		SetPOPServerName(inServerName); // set the invisible server name field.
#endif
}

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::EditButton()
// Grab the selected item in the list, create a duplicate of the server associated with that
// item and pop up a dialog to edit this server. The OK/Cancel is handled in ObeyCommand().
//----------------------------------------------------------------------------------------
{
	CellContents	contents;
	GetHostFromSelectedRow(contents, sizeof(CellContents));
	Boolean usePOP = (mServerType == ePOPServer);
	
	// Allow them to edit the server name the first time through after setup.
	// Use the doCreate parameter for this.
	Boolean isNewServer = false;
	Boolean	allowServerNameEdit = (usePOP || CMailNewsContext::UserHasNoLocalInbox());
	Boolean	allowServerTypeEdit = allowServerNameEdit || (CountRows() == 1);
	
	if (UGetInfo::ConductMailServerInfoDialog(
			contents.description,
			usePOP,
			isNewServer,
			allowServerNameEdit,
			allowServerTypeEdit,
			UGetInfo::kSubDialogOfPrefs,
			(allowServerTypeEdit) ? ValidateServerName : nil,
			(allowServerTypeEdit) ? this : nil,
			this))
	{
		NoteServerChanges(usePOP, contents.description);
	}
	
	// Update the cell contents to save the handler
	UpdateSelectedRow(contents, sizeof(CellContents));
		
	
} // CMailNewsMailServerMediator::EditButton

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::DeleteButton()
//----------------------------------------------------------------------------------------
{
	CellContents	cellData;
	UInt32			dataSize = sizeof(CellContents);
	Boolean			showWarning = false;
	
	GetHostFromSelectedRow(cellData, dataSize);
	Assert_(dataSize <= sizeof(CellContents));
	if (! UsingPop())
	{
		char 	*curServerList = nil;
		PREF_CopyCharPref("network.hosts.imap_servers", &curServerList);
		
		showWarning = ServerIsInCommaSeparatedList(cellData.description, curServerList);
		XP_FREEIF(curServerList);
	}
	
	if (showWarning && !UStdDialogs::AskOkCancel(GetPString(MK_MSG_REMOVE_MAILHOST_CONFIRM + RES_OFFSET)))
		return;

	if (UsingPop())
		SetPOPServerName("mail");
		
	DeleteSelectedRow();

	UpdateButtons();
	
} // CMailNewsMailServerMediator::DeleteButton

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::LoadList()
// This should only be called once when loading the prefs now
//----------------------------------------------------------------------------------------
{
	ClearList();
	if (UsingPop())
	{
		CStr255 serverName;
		GetPOPServerName(serverName);
		
		// set empty server name to something useful
		Assert_( ! serverName.IsEmpty() );
		
		if (! serverName.IsEmpty())
		{
			CellContents contents(serverName);
			mServerTable->InsertRows(
				1, LArray::index_Last,
				&contents, sizeof(CellContents), false);
		}
	}
	else
	{
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
	}
	// Refreshing is redundant the first time, but necessary when called after a server info dlg.
	mServerTable->Refresh();
} // CMailNewsMailServerMediator::LoadList

//----------------------------------------------------------------------------------------
void CMailNewsMailServerMediator::WriteList()
//----------------------------------------------------------------------------------------
{
	TableIndexT	rows, cols;
	mServerTable->GetTableSize(rows, cols);
	if (mServersLocked)
		return;
	
	StSpinningBeachBallCursor		beachBall;		// because making new servers can take some time

#ifdef BEFORE_INVISIBLE_POPSERVER_NAME_EDITFIELD_TRICK_WAS_THOUGHT_OF
	if (mServerType == ePOPServer && mPOPServerName && *mPOPServerName)
	CStr255 serverName;
	GetPOPServerName(serverName);
	NoteServerChanges(mServerType, serverName);
#endif
#if 0
	// There may be no need to do this. The back end updates the list after every
	// add and delete.
	char *serverList = nil;
	for (STableCell cell(1, 1); cell.row <= rows; ++cell.row)
	{
		CellContents contents;
		Uint32 cellSize = sizeof(contents);
		mServerTable->GetCellData(cell, &contents, cellSize);
		XP_ASSERT(sizeof(CellContents) == cellSize);
		if (cell.row > 1)
			StrAllocCat(serverList, ",");
		StrAllocCat(serverList, (const char*)(CStr255)contents.description);
	}
	PREF_SetCharPref("network.hosts.imap_servers", serverList);
#endif // 0

	
	char	*newServerList = XP_STRDUP("");
	char 	*oldServerList = nil;
	PREF_CopyCharPref("network.hosts.imap_servers", &oldServerList);	
	
	if ( UsingPop() )
	{
		// the invisible POP server name field in the prefs dialog writes itself out,
		// so we have nothing to do but to remove deleted IMAP servers
	}
	else		// IMAP
	{
		// Here is where we actually write out the changed mail server prefs.
		// The strategy is:		Get the string of servers from the (old) prefs
		// 						For each server in the string, see if it's in our list
		//							If not, delete it
		//							If yes, we need do nothing.
		//	Servers in our list which are not in the prefs are added.

#ifdef BEFORE_INVISIBLE_POPSERVER_NAME_EDITFIELD_TRICK_WAS_THOUGHT_OF
		// nothing
#else
		// Make sure the pop_server does not write out when using IMAP
		MPreferenceBase::SetPaneWritePref(sWindow, 'SNAM', false);
#endif
	
		// The backend has a callback on mail.server_type, which will get called
		// when we write out the temp prefs buffer on closing the prefs dialog.
		// So, to avoid duplicate hosts being created by this callback, we need
		// to set the mail.server_type pref here if necessary.
		
		if (!oldServerList || !*oldServerList)	// no IMAP servers at the moment
		{
			int	prefError = PREF_SetIntPref("mail.server_type", eIMAPServer);
			Assert_(prefError == PREF_NOERROR || prefError == PREF_VALUECHANGED);
			
			// double check
			PREF_CopyCharPref("network.hosts.imap_servers", &oldServerList);
		}

		for (STableCell cell(1, 1); cell.row <= rows; ++cell.row)
		{
			CellContents 	contents;
			Uint32 			cellSize = sizeof(CellContents);
			
			mServerTable->GetCellData(cell, &contents, cellSize);
			Assert_(sizeof(CellContents) <= cellSize);
			
			// skip cells with empty text
			if (contents.description.Length() == 0)
				continue;
			
			// If the host does not exist, create it
			if ( ! ServerIsInCommaSeparatedList(contents.description, oldServerList) )
			{
				const char* 	userNameString = XP_STRDUP("");		// its filled in when the prefs are saved
				const char* 	serverNameString = XP_STRDUP(contents.description);
				
				// get user name from internet config if necessary
				if (UseIC())
				{
					CStr255		userAtHostString;
					long		port;
					
					CInternetConfigInterface::GetInternetConfigString(	kICMailAccount,
														userAtHostString,
														&port);
					if ( !userAtHostString.IsEmpty() )
					{
						unsigned char atPos = userAtHostString.Pos("@");
						
						if (atPos > 0)
						{
							userAtHostString[0] = atPos - 1;
							
							XP_FREEIF(const_cast<char *>(userNameString));
							userNameString = XP_STRDUP(userAtHostString);
						}
					}
				}
			
				try
				{
					ThrowIfNil_(userNameString);
					ThrowIfNil_(serverNameString);
					
					if (::MSG_GetIMAPHostByName(CMailNewsContext::GetMailMaster(), serverNameString))
						Assert_(false);
					else
						::MSG_CreateIMAPHost(
							CMailNewsContext::GetMailMaster(),
							serverNameString,
							false,				// XP_Bool isSecure,
							userNameString,		// const char *userName,
							false,			// XP_Bool checkNewMail,
							0,				// int	biffInterval,
							false,			// XP_Bool rememberPassword,
							true,			// XP_Bool usingSubscription,
							false,			// XP_Bool overrideNamespaces,
							nil,			// const char *personalOnlineDir,
							nil,			// const char *publicOnlineDir,
							nil				// const char *otherUsersOnlineDir
							);
				}
				catch (...)
				{
				}
				XP_FREEIF(const_cast<char*>(userNameString));
				XP_FREEIF(const_cast<char*>(serverNameString));
			}
			
			// save the name in the new servers list
			if (cell.row > 1)
				StrAllocCat(newServerList, ",");
				
			StrAllocCat(newServerList, (const char*)(CStr255)contents.description);
		}
	}		// using IMAP
	
	// Now handle deleted servers, which are in the old list but not the new
	if (oldServerList)
	{
		char *serverPtr = oldServerList;
		while (serverPtr)
		{
			
			char *endPtr = XP_STRCHR(serverPtr, ',');
			if (endPtr)
				*endPtr++ = '\0';
			
			if ( (*newServerList == 0) || !ServerIsInCommaSeparatedList(serverPtr, newServerList) )
			{
				// delete the server
				::MSG_DeleteIMAPHostByName(CMailNewsContext::GetMailMaster(), serverPtr);
			}

			serverPtr = endPtr;
		}
	}
	
	XP_FREEIF(oldServerList);
	XP_FREEIF(newServerList);
	
	// Did the user delete all their servers? Silly thing, let's make amends
	if (rows == 0)
	{
		// set the pop server name back to the default
		SetPOPServerName("mail");
		
		// just to make damn sure
		MPreferenceBase::SetPaneWritePref(sWindow, 'SNAM', true);
		::PREF_SetIntPref("mail.server_type", ePOPServer);
	}

	
} // CMailNewsMailServerMediator::WriteList

enum
{
//	eNewsServerEditText = 13001,
//	eNewsServerPortIntText,
//	eNewsServerSecureBox,
	eNotifyLargeDownloadBox,
	eNotifyLargeDownloadEditField
};

#pragma mark -

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
		LGACheckbox	*checkbox =
				(LGACheckbox *)superView->FindPaneByID(eNewsServerSecureBox);
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
		char* cserverName = XP_STRDUP(serverName);
		if (cserverName)
		{
			MSG_NewsHost* host = ::MSG_CreateNewsHost(
								CMailNewsContext::GetMailMaster(),
								serverName,
								port == eNNTPSecurePort,
								port);
			XP_FREE(cserverName);
		}
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
	// This creates the host in the BE straight away. This needs to be re-written so
	// that host changes are only committed when the user OKs the prefs dialog
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
	
	StDialogHandler		*dialogHandler = nil;
	
	if (UGetInfo::ConductNewsServerInfoDialog(contents.serverData, this))
	{
		// Fix me. News server changes take place right away, so are not handled like
		// the rest of the prefs. See CMailNewsMailServerMediator for the way it
		// should work
	}
	
} // CMailNewsNewsServerMediator::EditButton

//----------------------------------------------------------------------------------------
void CMailNewsNewsServerMediator::DeleteButton()
//----------------------------------------------------------------------------------------
{
	CellContents contents;
	GetHostFromSelectedRow(contents, sizeof(CellContents));
	
	// This deletes the host in the BE straight away. This needs to be re-written so
	// that host changes are only committed when the user OKs the prefs dialog

	::MSG_DeleteNewsHost(CMailNewsContext::GetMailMaster(),
				MSG_GetNewsHostFromMSGHost(contents.serverData));
	LoadList();
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
	// Refreshing is redundant the first time, but necessary when called after a server info dlg.
	mServerTable->Refresh();
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

#pragma mark -

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
	STableCell 		currentCell;
	CellContents 	cell;
	
	currentCell = mServerTable->GetFirstSelectedCell();
	XP_ASSERT(currentCell.row);
	
	Uint32 			cellDataSize = sizeof(CellContents);
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
				cell.description = newServer->description;
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
	eSelectMessageButton
//	eSelectMessageFolderPopup
};

#pragma mark -

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
		LGACheckbox	*checkbox =
				(LGACheckbox *)superView->FindPaneByID(eByDateBox);
		XP_ASSERT(checkbox);
		LGARadioButton	*radioButton =
				(LGARadioButton *)superView->FindPaneByID(eDaysRButton);
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
//void COfflineNewsMediator::WritePrefs()
//----------------------------------------------------------------------------------------
//{
//	CSelectFolderMenu* selectPopup =
//		(CSelectFolderMenu*)FindPaneByID(eSelectMessageFolderPopup);
//	XP_ASSERT(selectPopup);
//	selectPopup->CommitCurrentSelections();
//}

//----------------------------------------------------------------------------------------
void	COfflineNewsMediator::ListenToMessage(MessageT inMessage, void *ioParam)
//----------------------------------------------------------------------------------------
{
	switch (inMessage)
	{
		case eSelectMessageButton:
			COfflinePickerWindow::DisplayDialog();
			break;

		default:
			Inherited::ListenToMessage(inMessage, ioParam);
			break;
	}
}

#pragma mark -

//----------------------------------------------------------------------------------------
void CMailNewsAddressingMediator::LoadPrefs()
//----------------------------------------------------------------------------------------
{
	
	// Build popup menu and data
	LGAPopup* popup = dynamic_cast<LGAPopup*>(FindPaneByID( eDirectoryPopup ) );
	Assert_( popup );
	MenuHandle menu = popup->GetMacMenuH();
	mLDAPList = XP_ListNew();
	XP_List	*directories = CAddressBookManager::GetDirServerList();
	
	DIR_GetLdapServers( directories, mLDAPList )	;
	int16 selected = 0;
	int32 i = 0;
	if (XP_ListCount(mLDAPList))
	{
		DIR_Server	*server;
		XP_List*  listIterator = mLDAPList;
		while ( (server = (DIR_Server *)XP_ListNextObject(listIterator) ) != 0 )
		{

			CStr255 description(server->description);
			
			Int16 index =  UMenuUtils::AppendMenuItem( menu, description, true);
			if(  DIR_TestFlag( server, DIR_AUTO_COMPLETE_ENABLED) )
				selected = index;
		}
	}
	popup->SetPopupMinMaxValues();
	popup->SetValue( selected );
}

//----------------------------------------------------------------------------------------
void CMailNewsAddressingMediator::WritePrefs()
//----------------------------------------------------------------------------------------
{
	LGAPopup* popup = dynamic_cast<LGAPopup*>(FindPaneByID( eDirectoryPopup ));
	Int32 index = popup->GetValue();
	DIR_Server* server  = (DIR_Server*)XP_ListGetObjectNum( mLDAPList, index );
	if ( server )
		DIR_SetAutoCompleteEnabled( mLDAPList, server, true );
} 
#endif // MOZ_MAIL_NEWS
