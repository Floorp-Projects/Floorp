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

#include "CPrefsDialog.h"

#include "CMenuTable.h"
#include "CAssortedMediators.h"

#ifdef MOZ_MAIL_NEWS
#include "MailNewsAddressBook.h"
#include "MailNewsMediators.h"
#include "CReceiptsMediator.h"
#endif // MOZ_MAIL_NEWS
#include "CSpecialFoldersMediator.h"
#include "CMailNewsMainMediator.h"

#include "MPreference.h"

#include "UStdDialogs.h"
#include "CValidEditField.h"
#include "macutil.h"

#include "dirprefs.h"
#include "addrbook.h"
#include "abdefn.h"
#include "prefapi.h"

#include "UStdDialogs.h"

#include "resgui.h"	// for cmd_AboutPlugins

#include "uapp.h"

#include "CBrowserApplicationsMediator.h"
#include "CLocationIndependenceMediator.h"

// define this to have the prefs dialog open showing the last pane you
// used, with all twisties expanded
#define PREFS_DIALOG_REMEMBERS_PANE

CPrefsDialog*		CPrefsDialog::sThis = nil;
PrefPaneID::ID		CPrefsDialog::sLastPrefPane = PrefPaneID::eNoPaneSpecified;

//-----------------------------------
CPrefsDialog::CPrefsDialog()
//-----------------------------------
:	LCommander(GetTopCommander())
,	mWindow(nil)
,	mTable(nil)
{
	MPreferenceBase::SetWriteOnDestroy(false);
}

//-----------------------------------
CPrefsDialog::~CPrefsDialog()
//-----------------------------------
{
	// all panes are freed by PowerPlant since they are subcommanders of this dialog...
}

//-----------------------------------
void CPrefsDialog::EditPrefs(
	Expand_T 			expand,
	PrefPaneID::ID 		pane,
	Selection_T 		selection)
//-----------------------------------
{

#ifdef PREFS_DIALOG_REMEMBERS_PANE
	pane = (pane != PrefPaneID::eNoPaneSpecified) ? pane : sLastPrefPane;
#endif

	if (sThis)
	{
		// very suspicious
	}
	else
	{
		sThis = new CPrefsDialog();
	}
	XP_Bool	attachVCard;
	const	char *	const	usePABCPrefName = "mail.attach_vcard";	// fix me
	int	prefResult = PREF_GetBoolPref(usePABCPrefName, &attachVCard);
#ifdef MOZ_MAIL_NEWS
	sThis->mNeedToCheckForVCard = (	prefResult == PREF_NOERROR &&
									false == attachVCard &&
									!PREF_PrefIsLocked(usePABCPrefName));
#endif // MOZ_MAIL_NEWS
	const	char *	const	useInternetConfigPrefName = "browser.mac.use_internet_config";	// fix me
	XP_Bool	useIC;
	prefResult = PREF_GetBoolPref(useInternetConfigPrefName, &useIC);
	CPrefsMediator::UseIC(useIC);

	// make sure the prefs cache is empty before we may use it
	MPreferenceBase::InitTempPrefCache();
	
	sThis->DoPrefsWindow(expand, pane, selection);
}

#ifdef MOZ_MAIL_NEWS
//-----------------------------------
void CPrefsDialog::CheckForVCard()
//-----------------------------------
{
	const	char *	const	usePABCPrefName = "mail.attach_vcard";	// fix me
	XP_Bool	attachVCard;
	PREF_GetBoolPref(usePABCPrefName, &attachVCard);
	if (attachVCard)
	{
		char	*email = (char *)FE_UsersMailAddress();
		if (email)
			email = XP_STRDUP(email);
		char	*nameString = (char *)FE_UsersFullName();
		if (nameString)
		{
			nameString = XP_STRDUP(nameString);
		}
		XP_List	*directories = CAddressBookManager::GetDirServerList();
		DIR_Server	*pab;
		DIR_GetPersonalAddressBook(directories, &pab);
		XP_ASSERT(pab);
		if (pab)
		{
			ABID		entryID;
			PersonEntry	person;
			person.Initialize();
			person.pGivenName = nameString;
			person.pEmailAddress = email;
			ABook	*aBook = FE_GetAddressBook(nil);
			AB_GetEntryIDForPerson(pab, aBook, &entryID, &person);
			AB_BreakApartFirstName(aBook, &person);
			if (MSG_MESSAGEIDNONE == entryID)
			{
				LCommander	*super = LCommander::GetTopCommander();

				StStdDialogHandler theHandler(12007, super);
				theHandler.SetInitialDialogPosition(nil);
				LWindow* theDialog = theHandler.GetDialog();

				MessageT theMessage = msg_Cancel;
				if (UStdDialogs::TryToInteract())
				{
					theDialog->Show();
					theMessage = theHandler.WaitUserResponse();
				}
				if (msg_Cancel != theMessage)
				{
					if (true != FE_ShowPropertySheetFor(nil, entryID, &person))
					{
						theMessage = msg_Cancel;
					}
				}
				if (msg_Cancel == theMessage)
				{
					PREF_SetBoolPref(usePABCPrefName, false);
				}
			}
			person.CleanUp();
		}
	}
}
#endif // MOZ_MAIL_NEWS

//-----------------------------------
void CPrefsDialog::DoPrefsWindow(
	Expand_T 		expand,
	PrefPaneID::ID 	pane,
	Selection_T		selection)
//-----------------------------------
{
	if (!mWindow)
	{
		mCurrentMediator = nil;
		mWindow = LWindow::CreateWindow(PrefPaneID::eWindowPPob, this);
		if (mWindow)
		{
			UReanimator::LinkListenerToControls(	this,
													mWindow,
													PrefPaneID::eWindowPPob);
			mTable = (CMenuTable *)mWindow->FindPaneByID(eTableView);
			XP_ASSERT(mTable);
			mPanel = (LView *)mWindow->FindPaneByID(ePanelView);
			XP_ASSERT(mPanel);
			CPrefsMediator::SetStatics(mPanel, mWindow, selection);

			mTable->AddListener(this);
			
			PrefPaneID::ID panel = pane;
			if ( ! pane )
				panel = (PrefPaneID::ID)expand;
		
			// Find the row 
			TableIndexT wideOpenRow = mTable->FindMessage( panel );
			
			// Now have to collapse the all the rows except appearence
			// This is icky
#ifndef PREFS_DIALOG_REMEMBERS_PANE
			mTable->CollapseRow( mTable->FindMessage ( PrefPaneID::eBrowser_Main  ) );
			#ifdef MOZ_MAIL_NEWS
			mTable->CollapseRow( mTable->FindMessage ( PrefPaneID::eMailNews_Main ) );
			#endif // MOZ_MAIL_NEWS
			#ifdef EDITOR
				mTable->CollapseRow( mTable->FindMessage ( PrefPaneID::eEditor_Main ) );
			#endif // Editor
			#ifdef MOZ_OFFLINE
			mTable->CollapseRow( mTable->FindMessage ( PrefPaneID::eOffline_Main ) );
			#endif // MOZ_OFFLINE
			mTable->CollapseRow( mTable->FindMessage ( PrefPaneID::eAdvanced_Main ) );	
#endif
			
			mTable->UnselectAllCells();
			STableCell initialCell( 1, 1 );
			
			if (PrefPaneID::eNoPaneSpecified != panel)
			{	
				// Reveal the selection
				mTable->RevealRow( wideOpenRow );
				if ( pane == 0 )
					mTable->DeepExpandRow( wideOpenRow );
				
				initialCell.row =
					mTable->GetExposedIndex( wideOpenRow );
				
			}
			
			mTable->SelectCell(initialCell);
			// really should scroll selected category into view
			LCommander::SetUpdateCommandStatus(true);
			
			CStr255 windowTitle;
			GetUserWindowTitle(6, windowTitle);
			mWindow->SetDescriptor(windowTitle);
			
			mWindow->Show();
		}
	}
}

//-----------------------------------
void CPrefsDialog::LoadICDependent()
//-----------------------------------
{
	sThis->GetMediator(PrefPaneID::eBrowser_Main)->LoadPanes();			// home page
		// we don't actually load the eMailNews_Identity manager because it will always
		// alreay be loaded
//	sThis->GetMediator(eMailNews_Identity)->LoadPanes();		// User Name
																// User Email
																// Organization
#ifdef MOZ_MAIL_NEWS
	sThis->GetMediator(PrefPaneID::eMailNews_MailServer)->LoadPanes();	// POP ID
																// POP host
																// SMTP host
	sThis->GetMediator(PrefPaneID::eMailNews_NewsServer)->LoadPanes();	// News host
#endif // MOZ_MAIL_NEWS
} // CPrefsDialog::LoadICDependent



//-----------------------------------
CPrefsMediator* CPrefsDialog::FindMediator(ResIDT paneID)
//-----------------------------------
{
	LArrayIterator iterator(mPanels, LArrayIterator::from_Start);
	CPrefsMediator* prefManager;
	while (iterator.Next(&prefManager))
	{
		if ( prefManager->GetMainPaneID() == paneID )
			return prefManager;	
	}	
	return nil;	
} // CPrefsDialog::FindMediator

//-----------------------------------
CPrefsMediator* CPrefsDialog::GetMediator(ResIDT inPaneID)
//-----------------------------------
{
	if (inPaneID == PrefPaneID::eNoPaneSpecified)
		return nil;
	CPrefsMediator* result = FindMediator(inPaneID);
	if (result)
		return result;

	// The pane ID of the pane is equal to the class ID of the corresponding mediator
	// Currently, we don't read in any resource data for a mediator, there are no resources,
	// so the LStream* parameter is nil.  We just use the factory feature of the registrar.
	// Later, we may decide to use UReanimator::ObjectsFromStream();
	result = (CPrefsMediator*)URegistrar::CreateObject(inPaneID, nil);
	if (!result)
	{
		// There's no special mediator with this class ID.  So give them a default mediator.
		result = new CPrefsMediator(inPaneID);
	}
	mPanels.InsertItemsAt(1,LArray::index_Last, &result, sizeof( result ) );
	AddListener(result);
	return result;
} // CPrefsDialog::GetMediator

//-----------------------------------
void CPrefsDialog::FindCommandStatus(
	CommandT	inCommand,
	Boolean		&outEnabled,
	Boolean&	/* outUsesMark */,
	Char16&		/* outMark */,
	Str255		/* outName */)
//-----------------------------------
{
	// Don't enable any commands except cmd_About, and cmd_AboutPlugins
	// which will keep the Apple menu enabled. This function purposely
	// does not call the inherited FindCommandStatus, thereby suppressing
	// commands that are handled by SuperCommanders. Only those
	// commands enabled by SubCommanders will be active.
	//
	// This is usually what you want for a moveable modal dialog.
	// Commands such as "New", "Open" and "Quit" that are handled
	// by the Applcation are disabled, but items within the dialog
	// can enable commands. For example, an EditField would enable
	// items in the "Edit" menu.
		
	outEnabled = false;
	if ((cmd_About == inCommand) || (cmd_AboutPlugins == inCommand))
	{
		outEnabled = true;
	}
}

//-----------------------------------
Boolean CPrefsDialog::AllowTargetSwitch(LCommander *inNewTarget)
//-----------------------------------
{
	CValidEditField	*target = dynamic_cast<CValidEditField*>(LCommander::GetTarget());
	if (target)
		return target->AllowTargetSwitch(inNewTarget);
	return LCommander::AllowTargetSwitch(inNewTarget);
}

//-----------------------------------
void CPrefsDialog::ListenToMessage(
							MessageT		inMessage,
							void			*/*ioParam*/)
//-----------------------------------
{
	switch (inMessage)
	{
		case CMenuTable::msg_SelectionChanged:
			MessageT	theViewID = mTable->GetSelectedMessage();
			CPrefsMediator	*mediator = GetMediator(theViewID);
			if (mediator)
			{
				if (mCurrentMediator)
					mCurrentMediator->StepAside();
				mediator->StepForward(mWindow);
				mCurrentMediator = mediator;
			}
			break;
		case CPrefsMediator::eCommitPrefs:
#ifdef MOZ_MAIL_NEWS
			CMailNewsMailServerMediator	*serverMediator = nil;
			// check if the Mail server pane manager exist
			// if it does get it
			if ( FindMediator( PrefPaneID::eMailNews_MailServer ) )
			{
				serverMediator
					= dynamic_cast<CMailNewsMailServerMediator*>(
						GetMediator(PrefPaneID::eMailNews_MailServer));
			}
#endif // MOZ_MAIL_NEWS
			if (CPrefsMediator::CanSwitch())
			{
#ifdef MOZ_MAIL_NEWS // Is MOZ_MAIL_NEWS the correct symbol to use?
				Boolean forceQuit = false;
				Boolean allowCommit = true;
				// Add code here to check if we should quit or not...
				if (allowCommit)
				{
#endif // MOZ_MAIL_NEWS
					BroadcastMessage(CPrefsMediator::eCommitPrefs, nil);
					Finished();
#ifdef MOZ_MAIL_NEWS
					if (mNeedToCheckForVCard)
					{
						CheckForVCard();
					}
					if (forceQuit)
					{
						CFrontApp* app = dynamic_cast<CFrontApp*>(LCommander::GetTopCommander());
						if (app)
							app->SendAEQuit();
					}	
				}
				else
				{
					// we don't want to quit
					mCurrentMediator->StepAside();
					serverMediator->StepForward(mWindow);
				/* Don't reset the button back
					serverMediator->ResetServerButtons();
				*/
					TableIndexT wideOpenRow = mTable->FindMessage(PrefPaneID::eMailNews_MailServer);
					mTable->RevealRow( wideOpenRow  );
					
					STableCell mailServerCell;
					mTable->IndexToCell( wideOpenRow, mailServerCell );
					mTable->SelectCell( mailServerCell );
				}
#endif // MOZ_MAIL_NEWS
			}
			break;
		case CPrefsMediator::eCancelPrefs:
			BroadcastMessage(CPrefsMediator::eCancelPrefs, nil);
			Finished();
			break;
		case eHelpButtonID:
			if (mCurrentMediator)
				mCurrentMediator->ActivateHelp();
			break;
		default:
			break;
	}
} // CPrefsDialog::ListenToMessage

//-----------------------------------
void CPrefsDialog::Finished()
//-----------------------------------
{
	sLastPrefPane = (PrefPaneID::ID)mTable->GetSelectedMessage();
	
	mWindow->DoClose();
	mWindow = nil;
	sThis = nil;
	if (MPreferenceBase::GetWriteOnDestroy()) // the controls wrote themselves
	{
		// sub-dialogs of the main prefs dialog, e.g. the mail server edit dialog,
		// write their prefs into a temporary tree which MPreferenceBase knows about.
		// So tell MPreferenceBase to copy these temp prefs into the main prefs, and
		// delete that tree.
		MPreferenceBase::CopyCachedPrefsToMainPrefs();
	
		PREF_SavePrefFile();
	}
	delete this;
}

//-----------------------------------
void CPrefsDialog::RegisterViewClasses()
//-----------------------------------
{
	// Mediator classes:
	RegisterClass_(CAppearanceMainMediator);
	RegisterClass_(CAppearanceFontsMediator);
	RegisterClass_(CAppearanceColorsMediator);
	RegisterClass_(CBrowserMainMediator);
	RegisterClass_(CBrowserLanguagesMediator);
	RegisterClass_(CBrowserApplicationsMediator);
	RegisterClass_(CAdvancedCacheMediator);
	RegisterClass_(CAdvancedProxiesMediator);
	
	CBrowserApplicationsMediator::RegisterViewClasses();
#ifdef MOZ_MAIL_NEWS
	RegisterClass_(CMailNewsIdentityMediator);
	RegisterClass_(CMailNewsMainMediator);
	RegisterClass_(CMailNewsMessagesMediator);
	RegisterClass_(CMailNewsOutgoingMediator);
	RegisterClass_(CMailNewsMailServerMediator);
	RegisterClass_(CMailNewsNewsServerMediator);
	RegisterClass_(CReceiptsMediator);
	RegisterClass_(CMailNewsDirectoryMediator);
	RegisterClass_(CMailNewsAddressingMediator );
#endif // MOZ_MAIL_NEWS
#ifdef EDITOR
	RegisterClass_(CEditorMainMediator);
#endif
#ifdef MOZ_OFFLINE
	RegisterClass_(COfflineNewsMediator);
#endif
#ifdef MOZ_LDAP
	// And a dialog class:
	RegisterClass_(CLDAPServerPropDialog);
#endif
#ifdef MOZ_LOC_INDEP
	RegisterClass_(CLocationIndependenceMediator);
#endif
} // CPrefsDialog::RegisterViewClasses
