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



// UNewFolderDialog.cp

#ifndef MOZ_LITE

#include "UNewFolderDialog.h"

#include "CMessageFolder.h"
#include "PascalString.h"
#include "UMailFolderMenus.h"
#include "CMailFolderButtonPopup.h"
#include "CMailNewsContext.h"
#include "CMailProgressWindow.h"
#include "MailNewsCallbacks.h"

#include "prefapi.h"
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS

#include "macutil.h"
#include "uerrmgr.h"
#include "ufilemgr.h"
#include "uprefd.h"

#include <UModalDialogs.h>
#include <LGAEditField.h>
#include <LGARadioButton.h>

//----------------------------------------------------------------------------------------
Boolean UFolderDialogs::ConductNewFolderDialog(
	const CMessageFolder& inParentFolder,
	CMailCallbackListener* inListener)
//----------------------------------------------------------------------------------------
{
	// Put up dialog
	StDialogHandler handler(14010, NULL);

	// Select the "Host" edit field
	LWindow* dialog = handler.GetDialog();
	LGAEditField *namefield = (LGAEditField*)dialog->FindPaneByID('Name');
	SignalIf_(!namefield);
	CMailFolderGAPopup* folderPopup = (CMailFolderGAPopup*)dialog->FindPaneByID('MfPM');
	SignalIf_(!folderPopup);
	if (!folderPopup || ! namefield)
		return false;
	dialog->SetLatentSub(namefield);
	CMailFolderMixin::FolderChoices c
		= (CMailFolderMixin::FolderChoices)(
			(int)CMailFolderMixin::eWantPOP
			| (int)CMailFolderMixin::eWantIMAP
			| (int)CMailFolderMixin::eWantHosts);
	folderPopup->MSetFolderChoices(c);
	CMailFolderMixin::UpdateMailFolderMixinsNow(folderPopup);
	MSG_Master* master = CMailNewsContext::GetMailMaster();
	MSG_FolderInfo* suggestedParent
		= ::MSG_SuggestNewFolderParent(inParentFolder, master); 
	folderPopup->MSetSelectedFolder(suggestedParent, false);
	
	// Run the dialog
	MessageT message = msg_Nothing;
	Boolean userChangedPort = false;
	do {
		message = handler.DoDialog();
	} while (message != msg_OK && message != msg_Cancel);
	
	// Use the result.
	Boolean folderCreated  = false;
	if (message == msg_OK)
	{
		CStr255 nametext;
		namefield->GetDescriptor(nametext);
		CStr255 commandName;
		dialog->GetDescriptor(commandName);

		try
		{
			dialog->Hide();
			MSG_Pane* pane = CMailProgressWindow::JustGiveMeAPane(commandName);
				// this throws if appropriate
			if (inListener)
				inListener->SetPane(pane);
			folderCreated = ::MSG_CreateMailFolderWithPane(
				pane,
				master,
				(MSG_FolderInfo*)folderPopup->MGetSelectedFolder(),
				nametext) == 0;
			if( folderCreated == false )
			{
				// Send a message out to close the progress window
				FE_PaneChanged(  pane, false , MSG_PaneProgressDone, 0);
			}
		}
		catch (...)
		{
			throw;
		}
	}
	return folderCreated;
} // ConductNewFolderDialog

//----------------------------------------------------------------------------------------
Boolean UFolderDialogs::GetDefaultFolderName(
	FolderKind inKind,
	CStr255& outFolderName)
//----------------------------------------------------------------------------------------
{
	switch (inKind)
	{
		case mail_fcc:
		case news_fcc:
			outFolderName = XP_GetString(MK_MSG_SENT_L10N_NAME);
			return true;
		case drafts:
			outFolderName = XP_GetString(MK_MSG_DRAFTS_L10N_NAME);
			return true;
		case templates:
			outFolderName = XP_GetString(MK_MSG_TEMPLATES_L10N_NAME);
			return true;
	}
	return false;
} // GetDefaultFolderName

//----------------------------------------------------------------------------------------
Boolean UFolderDialogs::FindLocalMailHost(CMessageFolder& outFolder)
//----------------------------------------------------------------------------------------
{
	MSG_Master* master = CMailNewsContext::GetMailMaster();
	// See also UMailFolderMenus::UpdateMailFolderMixinsNow
	Int32 numFoldersMail = ::MSG_GetFoldersWithFlag(master, MSG_FOLDER_FLAG_MAIL, nil, 0);
	Assert_(numFoldersMail > 0);	// Should have at least some permanent mail folders!
	StPointerBlock folderInfoData(sizeof(MSG_FolderInfo *) * numFoldersMail);
	MSG_FolderInfo **folderInfo = (MSG_FolderInfo **) folderInfoData.mPtr;
	Int32 numFolders2 = ::MSG_GetFoldersWithFlag(
		master, MSG_FOLDER_FLAG_MAIL, folderInfo, numFoldersMail);
	Assert_(numFolders2 > 0);	// Should have at least some permanent mail folders!
	Assert_(numFolders2 == numFoldersMail);
	for (int i = 0; i < numFoldersMail; i++, folderInfo++)
	{
		CMessageFolder f(*folderInfo);
		if (!f.IsIMAPMailFolder() && f.GetLevel() == kRootLevel)
		{
			outFolder = f;
			return true;
		}
	}
	return false;
} // UFolderDialogs::FindLocalMailHost

//----------------------------------------------------------------------------------------
Boolean UFolderDialogs::GetFolderAndServerNames(
	const CMessageFolder& inFolder,
	FolderKind inKind,
	CStr255& outFolderName,
	CMessageFolder& outServer,
	Boolean* outMatchesDefault)
//----------------------------------------------------------------------------------------
{
	CStr255 defaultFolderName;
	if (!GetDefaultFolderName(inKind, defaultFolderName))
		return false;
	if (inFolder.GetFolderInfo() == nil)
	{
		if (!FindLocalMailHost(outServer))
			return false;
		outFolderName = defaultFolderName;
		if (outMatchesDefault)
			*outMatchesDefault = true;
		return true;
	}
	else if (inFolder.IsMailServer())
	{
		outServer = inFolder;
		outFolderName = defaultFolderName;
		if (outMatchesDefault)
			*outMatchesDefault = true;
		return true;
	}
	else if (inFolder.IsLocalMailFolder())
	{
		// GetHostFolderinfo doesn't work for local folders...
		if (!FindLocalMailHost(outServer))
			return false;
	}
	else
		outServer.SetFolderInfo(GetHostFolderInfo(inFolder));
	outFolderName = inFolder.GetName();
	if (outMatchesDefault)
		*outMatchesDefault = (defaultFolderName == outFolderName
						&& inFolder.GetLevel() == kSpecialFolderLevel);
	return true;
} // UFolderDialogs::GetFolderAndServerNames

//----------------------------------------------------------------------------------------
Boolean UFolderDialogs::GetFolderAndServerNames(
	const char* inPrefName,
	CMessageFolder& outFolder,
	CStr255& outFolderName,
	CMessageFolder& outServer,
	Boolean* outMatchesDefault)
//----------------------------------------------------------------------------------------
{
	FolderKind kind;
	if (XP_STRSTR(inPrefName, ".default_fcc"))
	{
		if (XP_STRSTR(inPrefName, "news") == inPrefName)
			kind = news_fcc;
		else
			kind = mail_fcc;
		// The Dogbert (v4.0) pref was binary (a mac alias); If we're upgrading from Dogbert,
		// Even in 4.5, local folders still use this
		// scheme.
		AliasHandle aliasH = NULL;
		int size;
		void* alias;
		if (PREF_CopyBinaryPref(inPrefName, &alias, &size ) == 0)
		{
			PtrToHand(alias, &(Handle)aliasH, size);
			XP_FREE(alias);

			FSSpec	target;
			Boolean	wasChanged; // ignored
			OSErr err = ResolveAlias(nil, aliasH, &target, &wasChanged);
			DisposeHandle((Handle)aliasH);

			if (err == noErr)
			{
				// We have to work out whether this is the default folder.  There are two
				// possibilities that should cause a match.
				CStr255 defaultFolderName;
				GetDefaultFolderName(kind, defaultFolderName); // eg "Sent", "Templates"...
				
				// Case (1)
				// The spec is the spec of the
				// root of the local mail folder tree (usually "Mail", but user-settable).
				// This, by convention, signifies the folder with the default name (eg "Sent")
				// for this type of special folder.
				FSSpec mailTop = CPrefs::GetFolderSpec(CPrefs::MailFolder);
				Boolean matchesDefault = mailTop. vRefNum == target.vRefNum
										&& mailTop.parID == target.parID
										&& *(CStr63*)mailTop.name == *(CStr63*)target.name;
				
				if (matchesDefault)
				{
					// Change target spec to be the spec of the actual folder
					CStr255 relativePath;
					relativePath += ':';
					relativePath += *(CStr63*)mailTop.name;
					relativePath += ':';
					relativePath += defaultFolderName;
					err = FSMakeFSSpec(
							mailTop.vRefNum,
							mailTop.parID,
							relativePath,
							&target);
					if (err != fnfErr)
						ThrowIfOSErr_(err); // This would be bad?
				}
				// Case (2)
				// The spec is explicitly the spec of the default folder.
				else
				{
					// Get a prototype for a file inside the top mail folder
					// Try again with that plus the default name.
					mailTop = CPrefs::GetFilePrototype(CPrefs::MailFolder);
					matchesDefault = mailTop. vRefNum == target.vRefNum
										&& mailTop.parID == target.parID
										&& defaultFolderName == *(CStr63*)target.name;
				}
				if (outMatchesDefault)
					*outMatchesDefault = matchesDefault;
				outFolderName = target.name;
				// Check whether the directory exists.
				FInfo info;
				err = FSpGetFInfo(&target, &info);
				if (err == fnfErr)
				{
					outFolder.SetFolderInfo(nil);
				}
				else
				{
					char* appPath = CFileMgr::EncodedPathNameFromFSSpec(target, true);
					if (!appPath)
						return false;
					char* url = XP_STRDUP("mailbox:");
					StrAllocCat(url, appPath);
					XP_FREE(appPath);
					if (!url)
						return false;
					MSG_FolderInfo* info = ::MSG_GetFolderInfoFromURL(
						CMailNewsContext::GetMailMaster(),
						url, false);
					XP_FREE(url);
					outFolder.SetFolderInfo(info);
				}
				FindLocalMailHost(outServer);
				return true;
			}
		}
	}
	// Remaining cases are URL paths.
	else if (XP_STRCMP(inPrefName, "news.imap_sentmail_path") == 0)
		kind = news_fcc;
	else if (XP_STRCMP(inPrefName, "mail.imap_sentmail_path") == 0)
		kind = mail_fcc;
	else if (XP_STRSTR(inPrefName, "drafts"))
		kind = drafts;
	else if (XP_STRSTR(inPrefName, "templates"))
		kind = templates;
	// Here's the new way.  It's always a char pref, and a URL.
	char* url;
	int	prefResult = PREF_CopyCharPref(inPrefName, &url);
	if (prefResult == PREF_NOERROR)
	{
		MSG_FolderInfo* info = ::MSG_GetFolderInfoFromURL(
			CMailNewsContext::GetMailMaster(),
			url, false);
		outFolder.SetFolderInfo(info);
	}
	return GetFolderAndServerNames(
						outFolder,
						kind,
						outFolderName,
						outServer,
						outMatchesDefault);
} // UFolderDialogs::GetFolderAndServerNames

//----------------------------------------------------------------------------------------
static Boolean SavePrefAsAlias(UFolderDialogs::FolderKind kind)
// More mess here, because the mail_fcc and news_fcc prefs are either:
//		Binary prefs (aliases) if they're local, or
//		String prefs (URLs) if they're IMAP.
//----------------------------------------------------------------------------------------
{
	XP_Bool useIMAP;
	if (kind == UFolderDialogs::mail_fcc)
	{
		if (PREF_NOERROR != PREF_GetBoolPref("mail.use_imap_sentmail", &useIMAP) || !useIMAP)
			return true;
	}
	else if (kind == UFolderDialogs::news_fcc)
	{
		if (PREF_NOERROR != PREF_GetBoolPref("news.use_imap_sentmail", &useIMAP) || !useIMAP)
			return true;
	}
	return false;
} // SavePrefAsAlias

//----------------------------------------------------------------------------------------
void UFolderDialogs::BuildPrefName(FolderKind kind, CStr255& outPrefName, Boolean& outSaveAsAlias) // e.g. Sent
//----------------------------------------------------------------------------------------
{
	outPrefName = "\p^0.default_^1";
	outSaveAsAlias = false;
	const char* r1 = (kind == news_fcc) ? "news" : "mail";
	const char* r2;
	switch (kind)
	{
		case drafts:
			r2 = "drafts";
			break;
		case templates:
			r2 = "templates";
			break;
		case news_fcc:
		case mail_fcc:
			outSaveAsAlias = SavePrefAsAlias(kind);
			if (!outSaveAsAlias)
			{
				// IMAP case : completely different prefname
				outPrefName = "^0.imap_sentmail_path";
			}
			r2 = "fcc";
			break;
	}
	StringParamText(outPrefName, r1, r2);
} // UFolderDialogs::BuildPrefName

//======================================
class CCreateFolderListener
// This is so we can get notified when the new folder is created, and select it in
// the popup menu.
//======================================
:	public CMailCallbackListener
{
public:
					CCreateFolderListener(CMailFolderGAPopup* inPopup)
					:	CMailCallbackListener()
					,	mPopup(inPopup) {}
	virtual void 	PaneChanged(
						MSG_Pane*,
						MSG_PANE_CHANGED_NOTIFY_CODE inNotifyCode,
						int32 value)
		{
			MSG_FolderInfo* info = nil;
			// not notified for local folders.  GRR!!!
			if (inNotifyCode == MSG_PaneNotifySelectNewFolder)
			{
				MSG_ViewIndex index = (MSG_ViewIndex)value;
				if (index == MSG_VIEWINDEXNONE)
					return; // This is what we used to get
				MSG_Pane* folderPane = ::MSG_FindPaneOfType(
					CMailNewsContext::GetMailMaster(),
					nil,
					MSG_FOLDERPANE);
				if (!folderPane)
					return;
				info = ::MSG_GetFolderInfo(folderPane, index);
			}
			else if (inNotifyCode != MSG_PaneProgressDone)
				return;
			if (mPopup)
				CMailFolderMixin::UpdateMailFolderMixinsNow(mPopup);
			if (info)
				mPopup->MSetSelectedFolder(info, false);
		}
	CMailFolderGAPopup*	mPopup;
}; // class CCreateFolderListener

//----------------------------------------------------------------------------------------
CMessageFolder UFolderDialogs::ConductSpecialFolderDialog(
	FolderKind				inKind,		// e.g. Sent
	const CMessageFolder&	inCurFolder	// URL of currentURL if pref is not saved yet
	)
// Returns nil if user cancels, URL otherwise.
// Sorry about the spaghetti.  It's all single-purpose code.  Ugh.
//----------------------------------------------------------------------------------------
{
	// Get the strings we'll need
	CStr255 defaultFolderName;
	GetDefaultFolderName(inKind, defaultFolderName);
	
	// Construct the prefname from the kind
	CStr255 prefName;
	Boolean saveAsAlias;
	BuildPrefName(inKind, prefName, saveAsAlias);

	// Put up dialog (which is initially invisible, because fiddling with the title
	// is unsightly after it is visible).
	StDialogHandler handler(14011, NULL);

	LWindow* dialog = handler.GetDialog();
	CStr255 workString;
	dialog->GetDescriptor(workString);
	StringParamText(workString, defaultFolderName);
	dialog->SetDescriptor(workString);

	LCaption* blurb = (LCaption*)dialog->FindPaneByID('Blrb');
	SignalIf_(!blurb);
	if (!blurb)
		return nil;
	blurb->GetDescriptor(workString);
	StringParamText(workString, defaultFolderName);
	blurb->SetDescriptor(workString);
	
	LGARadioButton* simpleCaseRadio = (LGARadioButton*)dialog->FindPaneByID('Fldr');
	SignalIf_(!simpleCaseRadio);
	if (!simpleCaseRadio)
		return nil;
	simpleCaseRadio->GetDescriptor(workString);
	StringParamText(workString, defaultFolderName);
	simpleCaseRadio->SetDescriptor(workString);
	
	CMailFolderGAPopup* serverPopup = (CMailFolderGAPopup*)dialog->FindPaneByID('Srvr');
	SignalIf_(!serverPopup);
	if (!serverPopup)
		return nil;

	LGARadioButton* customCaseRadio = (LGARadioButton*)dialog->FindPaneByID('Othr');
	SignalIf_(!customCaseRadio);
	if (!customCaseRadio)
		return nil;

	CMailFolderGAPopup* folderPopup = (CMailFolderGAPopup*)dialog->FindPaneByID('MfPM');
	SignalIf_(!folderPopup);
	if (!folderPopup)
		return nil;

	// Set up the popup folder menus, and select the current preference
	CStr255 curFolderName;
	CMessageFolder curFolder = inCurFolder, curServer;
	Boolean matchesDefault;
	// If we were passed a current default from an unsaved pref, use that.
	if (inCurFolder.GetFolderInfo() != nil)
		GetFolderAndServerNames(
			inCurFolder,
			inKind,
			curFolderName,
			curServer,
			&matchesDefault);
	else if (!GetFolderAndServerNames(
			prefName,
			curFolder,
			curFolderName,
			curServer,
			&matchesDefault))
		Assert_(false);

	if (matchesDefault)
		simpleCaseRadio->SetValue(1);
	else
		customCaseRadio->SetValue(1);
	
	CMailFolderMixin::FolderChoices c
		= (CMailFolderMixin::FolderChoices)((int)CMailFolderMixin::eWantPOP + (int)CMailFolderMixin::eWantIMAP);
	folderPopup->MSetFolderChoices(c);
	CMailFolderMixin::UpdateMailFolderMixinsNow(folderPopup);
	folderPopup->MSetSelectedFolder(curFolder, false);
	CCreateFolderListener creationListener(folderPopup);
		// create a listener to reset the menu when a folder's created.
	
	c = (CMailFolderMixin::FolderChoices)((int)CMailFolderMixin::eWantHosts);
	serverPopup->MSetFolderChoices(c);
	CMailFolderMixin::UpdateMailFolderMixinsNow(serverPopup);
	serverPopup->MSetSelectedFolder(curServer, false);
	
	// Run the dialog
	MessageT message = msg_Nothing;
	dialog->Show();
	do {
		message = handler.DoDialog();
		switch (message)
		{
			case 'NewÉ': // the "new folder" button
			{
				CMessageFolder parent(nil);
				ConductNewFolderDialog(parent, &creationListener);
				break;
			}
			case 'HELP':
				SysBeep(1);
				break;
		}
	} while (message != msg_OK && message != msg_Cancel);
	
	// Use the result.
	if (message == msg_OK)
	{
		if (simpleCaseRadio->GetValue() == 1)
			return serverPopup->MGetSelectedFolder();
		else
			return folderPopup->MGetSelectedFolder();
	}
	return nil;
} // ConductSpecialFolderDialog

#endif // MOZ_LITE
