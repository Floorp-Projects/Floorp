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



// StGetInfoHandler.cp

#include "StGetInfoHandler.h"

#include "XP_Core.h" // typedefs
#include "msgcom.h"

#include "CMessageFolder.h"
#include "CMailNewsContext.h"
#include "MPreference.h"
#include "CPasteSnooper.h"
#include "UMailFilters.h"
//#include "MailNewsMediators.h"

#include "ufilemgr.h"
#include "prefapi.h"

#include <LBroadcasterEditField.h>


//======================================
#pragma mark
class StFolderGetInfoHandler : public StGetInfoHandler
//======================================
{
	public:
							StFolderGetInfoHandler(
								ResIDT resID,
								CMessageFolder& folder,
								LCommander *inSuperCommander);
//----
// Data
//----
		CMessageFolder		mFolder;
		Int32				mFolderPrefFlags;
}; // class StFolderGetInfoHandler

//======================================
#pragma mark
class StMailFolderGetInfoHandler : public StFolderGetInfoHandler
//======================================
{
	public:
							StMailFolderGetInfoHandler(
								CMessageFolder& folder,
								LCommander *inSuperCommander)
								:	StFolderGetInfoHandler(12500, folder, inSuperCommander) {}
							~StMailFolderGetInfoHandler();
		virtual void		ReadDataFields();
		virtual void		WriteDataFields();
		virtual Boolean		UserDismissed();
}; // class StMailFolderGetInfoHandler

//======================================
#pragma mark
class StNewsGroupGetInfoHandler : public StFolderGetInfoHandler
//======================================
{
	public:
							StNewsGroupGetInfoHandler(
								CMessageFolder& folder,
								LCommander *inSuperCommander)
								:	StFolderGetInfoHandler(12501, folder, inSuperCommander) {}
							~StNewsGroupGetInfoHandler();
		virtual void		ReadDataFields();
		virtual void		WriteDataFields();
		virtual Boolean		UserDismissed();
//----
// Data
//----
	XP_Bool	mUseRetrievalDefaults, mUseBodyPurgeDefaults, mUseHeaderPurgeDefaults;
	XP_Bool	mByReadness;
	XP_Bool	mUnreadOnly;
	XP_Bool	mByDate;
	XP_Bool	mHTML;
	int32 	mDaysOld;
	int32 	mHeaderDaysKeep;
	int32 	mBodyDaysKeep;
	XP_Bool mKeepUnreadOnly;
	MSG_PurgeByPreferences mBodyPurgeBy;
	MSG_PurgeByPreferences mHeaderPurgeBy;
	int32 	mNumHeadersToKeep;
}; // class StNewsGroupGetInfoHandler

//======================================
class StNewsHostGetInfoHandler : public StGetInfoHandler
//======================================
{
	public:
							StNewsHostGetInfoHandler(
								MSG_Host* inHost,
								LCommander *inSuperCommander);
							~StNewsHostGetInfoHandler();
		virtual void		ReadDataFields();
		virtual void		WriteDataFields();
//----
// Data
//----
	MSG_Host* mHost;
	XP_Bool mPromptPassword;
}; // class StNewsHostGetInfoHandler


//======================================
#pragma mark
class StMailServerInfoHandler : public StGetInfoHandler
//======================================
{
	private:
		typedef StGetInfoHandler Inherited;
		
	public:
		enum {
			kMailServerNameCaption			= 'SNAM',
			kMailServerNameEditText			= 'SNME',
			kMailServerTypeCaption			= 'TYPE',
			kMailServerTypePopup			= 'TYPP',
			kIMAPServerUserNameEditText		= 'IUSR',
			kPOPServerUserNameEditText		= 'PUSR',
			kIMAPServerSavePasswordCheckbox	= 'IRMB',
			kPOPServerSavePasswordCheckbox	= 'PRMB',
			kIMAPServerCheckMailCheckbox	= 'ICHK',
			kPOPServerCheckMailCheckbox		= 'PCHK',
			kIMAPServerCheckTimeEditText	= 'IMIN',
			kPOPServerCheckTimeEditText		= 'PMIN'
		};
		
							StMailServerInfoHandler(
								CStr255& 					ioName,
								Boolean& 					ioPOP,
								Boolean 					inIsNewServer,
								Boolean						inAllowServerNameEdit,
								Boolean						inAllowServerTypeEdit,
								ServerNameValidationFunc	inValidationFunc,
								CServerListMediator*		inValidationServerList,
								LCommander*					inSuperCommander);
												
							~StMailServerInfoHandler();
		virtual void		ReadDataFields();
		virtual void		WriteDataFields();
		virtual Boolean		UserDismissed();
		virtual Boolean		GetPop() { return mPOP; }
		virtual void		GetServerName(CStr255 &outName);
			
	protected:
				void		SetPOP(Boolean inDialogInit);
				void		SetIMAP(Boolean inDialogInit);
				void		SetDefaultUserName();
//----
// Data
//----
	protected:
	
	
		Boolean						mPOP;
		Boolean						mOriginallyPOP;
		Boolean						mIsNewServer;
		Boolean						mAllowServerNameEdit;
		Boolean						mAllowServerTypeEdit;
		CStr255						mName;
		ServerNameValidationFunc	mServerNameValidationFunc;
		CServerListMediator*		mValidationServerList;
		
		enum 		{ kNumPolyprefControls = 4 };
 		enum		PopupValues { kPOP = 1, kIMAP = 2 };
					// Popup menu values are 1-based. So
					// these are the actual IntPref values PLUS ONE
}; // class StMailServerInfoHandler

#pragma mark -

//----------------------------------------------------------------------------------------
StGetInfoHandler::~StGetInfoHandler()
//----------------------------------------------------------------------------------------
{
	Assert_( mMessage == msg_Cancel || mDataWritten);
}

//----------------------------------------------------------------------------------------
MessageT StGetInfoHandler::DoDialog()
//----------------------------------------------------------------------------------------
{
	ReadDataFields();
	do {
		mMessage = StStdDialogHandler::DoDialog();
	} while (!UserDismissed());
	
	return mMessage;
}

//----------------------------------------------------------------------------------------
Boolean StGetInfoHandler::UserDismissed()
//----------------------------------------------------------------------------------------
{
	return (mMessage == msg_OK) || (mMessage == msg_Cancel);
}


//----------------------------------------------------------------------------------------
void StGetInfoHandler::HideAndCripplePane(PaneIDT paneID)
//----------------------------------------------------------------------------------------
{
	HidePane(paneID);
	MPreferenceBase::SetPaneWritePref(GetDialog(), paneID, false);
}


//----------------------------------------------------------------------------------------
void StGetInfoHandler::ShowAndUncripplePane(PaneIDT paneID)
//----------------------------------------------------------------------------------------
{
	ShowPane(paneID);
	MPreferenceBase::SetPaneWritePref(GetDialog(), paneID, true);
}


#pragma mark -

//----------------------------------------------------------------------------------------
StFolderGetInfoHandler::StFolderGetInfoHandler(
	ResIDT inResID,
	CMessageFolder& inFolder,
	LCommander *inSuperCommander)
//----------------------------------------------------------------------------------------
:	StGetInfoHandler(inResID, inSuperCommander)
,	mFolder(inFolder)
,	mFolderPrefFlags((inFolder.GetFolderPrefFlags()))
{
	mFolder.FolderInfoChanged(); // make sure it's the latest.
}

#pragma mark -

//----------------------------------------------------------------------------------------
StMailFolderGetInfoHandler::~StMailFolderGetInfoHandler()
//----------------------------------------------------------------------------------------
{
	if (mMessage != msg_Cancel)
		WriteDataFields();
}

//----------------------------------------------------------------------------------------
void StMailFolderGetInfoHandler::ReadDataFields()
//----------------------------------------------------------------------------------------
{
	{ // <- scope for buffer string (allow compiler to recycle)
		char buffer[256];
		strcpy(buffer, mFolder.GetName());
		NET_UnEscape(buffer);
		SetText('FNAM', CStr255(buffer));
	} // <- scope for buffer string (allow compiler to recycle)
	
	if (mFolder.IsLocalMailFolder())
	{
		HidePane('SHAR');	// Hide the tab button for "sharing"
		HidePane('CNTC'); 	// Hide the tab button for "download"
		
		const char* unixPath = MSG_GetFolderNameFromID(mFolder.GetFolderInfo());
			// note: this is a shared reference, should not be freed.
		if (unixPath)
		{
			XP_StatStruct stats;
			if (XP_Stat(unixPath, &stats, xpMailFolder) == 0)
			{
				SetNumberText('USED', stats.st_size);
			}
			char* macPath = CFileMgr::MacPathFromUnixPath(unixPath);
			if (macPath)
				SetText('LNAM', CStr255(macPath));
			XP_FREEIF((char*)macPath);
		}
		int32 deletedBytes = mFolder.GetDeletedBytes();
		if (deletedBytes >= 0)
			SetNumberText('WAST', deletedBytes);
	}
	else		// remote folder
	{
		HidePane('WPan'); // Hide all the disk/wasted stats and the cleanup button.
		
		
		MSG_FolderInfo*		theFolderInfo = mFolder.GetFolderInfo();
		
		Assert_(theFolderInfo);
		
		// fill in the folder type in caption 'FTYP'
		char	*folderType = ::MSG_GetFolderTypeName(theFolderInfo);
		
		if (folderType)
			SetText('FTYP', CStr255(folderType));
		XP_FREEIF((char*)folderType);
		
		
		// fill in the folder description in caption 'FDSC'
		char	*folderDesc = ::MSG_GetFolderTypeDescription(theFolderInfo);
		
		if (folderDesc)
			SetText('FDSC', CStr255(folderDesc));
		XP_FREEIF((char*)folderDesc);
		
		// fill in folder permissions in 'PERT'
		
		char	*folderRightsString = ::MSG_GetACLRightsStringForFolder(theFolderInfo);
		if (folderRightsString)
			SetText('PERT', CStr255(folderRightsString));
		XP_FREEIF((char*)folderRightsString);
		
		if ( ::MSG_HaveAdminUrlForFolder( theFolderInfo, MSG_AdminFolder) )
		{
			// show ACL panel 'SPRP'
			LPane	*aclPanelView = GetPane('SPRP');
			ThrowIfNil_(aclPanelView);
			aclPanelView->Show();
			
			// hide message 'NSHF'
			LPane	*noSharingMessage = GetPane('NSHF');
			ThrowIfNil_(noSharingMessage);
			noSharingMessage->Hide();
		}
		else
		{
			LPane	*aclPanelView = GetPane('SPRP');
			ThrowIfNil_(aclPanelView);
			aclPanelView->Hide();
			
			// hide message 'NSHF'
			LPane	*noSharingMessage = GetPane('NSHF');
			ThrowIfNil_(noSharingMessage);
			noSharingMessage->Show();
		}
		
	}
	
	SInt32 count = mFolder.CountUnseen();
	if (count >= 0)
		SetNumberText('UNRD', count);
	count = mFolder.CountMessages();
	if (count >= 0)
		SetNumberText('TOTL', count);
	SetBoolean('DnMe', (mFolderPrefFlags & MSG_FOLDER_PREF_OFFLINE) != 0);
}

//----------------------------------------------------------------------------------------
void StMailFolderGetInfoHandler::WriteDataFields()
//----------------------------------------------------------------------------------------
{
	CStr255 newName;
	// we do this to get around the fact that CString (from which CStr255
	// is derived) implememts operator const char*() by using a circular buffer
	// of 4 static strings.  If more than four clients use this "cast", the
	// first client's string will be overwritten.  Here, MSG_RenameMailFolder
	// ends up presenting a prompt dialog, and enough calls to
	// CString::operator const char*() are made that this happens!
	GetText('FNAM', newName);
	if (newName != CStr255(mFolder.GetName()))
	{
		char* temp = ::NET_Escape((const char *)newName, URL_PATH);
		if (temp)
			::MSG_RenameMailFolder(mFolder.GetFolderPane(), mFolder.GetFolderInfo(), temp);
		XP_FREEIF(temp);
	}
	if (GetBoolean('DnMe'))
		mFolderPrefFlags |= MSG_FOLDER_PREF_OFFLINE;
	else
		mFolderPrefFlags &= ~MSG_FOLDER_PREF_OFFLINE;
	MSG_SetFolderPrefFlags(mFolder.GetFolderInfo(), mFolderPrefFlags);
	mDataWritten = true;
}

//----------------------------------------------------------------------------------------
Boolean StMailFolderGetInfoHandler::UserDismissed()
//----------------------------------------------------------------------------------------
{
	if (mMessage == 'CMPR')
	{
		MSG_ViewIndex index = mFolder.GetIndex();
		::MSG_Command(
			mFolder.GetFolderPane(),
			MSG_CompressFolder,
			&index,
			1);
		return true;
	}
	
	// user clicked privileges button. Do ACL URL
	if (mMessage == 'PRIV')
	{
		// Get ourselves a context for the URL dispatch.
		// If it's not a browser context, the right thing happens and a browser context is made
		MSG_Pane	*folderPane = mFolder.GetFolderPane();
				
		Assert_(folderPane);		// since this properties dialog was brough up from the folder pane,
									// one should exist
		
		::MSG_GetAdminUrlForFolder(::MSG_GetContext(folderPane), mFolder.GetFolderInfo(), MSG_AdminFolder);
	}
	
	return StFolderGetInfoHandler::UserDismissed();
}

#pragma mark -

//----------------------------------------------------------------------------------------
StNewsGroupGetInfoHandler::~StNewsGroupGetInfoHandler()
//----------------------------------------------------------------------------------------
{
	if (mMessage != msg_Cancel)
		WriteDataFields();
}

//----------------------------------------------------------------------------------------
void StNewsGroupGetInfoHandler::ReadDataFields()
//----------------------------------------------------------------------------------------
{
	CStr255 prettyName(mFolder.GetPrettyName());
	if (prettyName.Length() > 0)
	{
		SetText('FNAM', prettyName);
		CStr255 geekNameFormat;
		GetText('GNAM', geekNameFormat);
		char buf[255];
		XP_SPRINTF(buf, geekNameFormat, mFolder.GetName());
		SetText('GNAM', CStr255(buf));
	}
	else
	{
		SetText('FNAM', CStr255(mFolder.GetName()));
		SetText('GNAM', "\p");
	}
	const char* unixPath = MSG_GetFolderNameFromID(mFolder.GetFolderInfo());
	if (unixPath)
	{
		XP_StatStruct stats;
		if (XP_Stat(unixPath, &stats, xpMailFolder) == 0)
		{
			SetNumberText('USED', stats.st_size);
		}
		char* macPath = CFileMgr::MacPathFromUnixPath(unixPath);
		XP_FREEIF((char*)unixPath);
	}

	SInt32 count = mFolder.CountUnseen();
	if (count >= 0)
		SetNumberText('UNRD', count);
	count = mFolder.CountMessages();
	if (count >= 0)
		SetNumberText('TOTL', count);
	SetBoolean('DnMe', (mFolderPrefFlags & MSG_FOLDER_PREF_OFFLINE) != 0);

	MSG_GetOfflineRetrievalInfo(
		(MSG_FolderInfo*)mFolder.GetFolderInfo(),
		&mUseRetrievalDefaults,
		&mByReadness,
		&mUnreadOnly,
		&mByDate,
		&mDaysOld);
	SetBoolean('UDFL', mUseRetrievalDefaults);
	if (mUseRetrievalDefaults)
	{
		LControl *theControl;

		XP_Bool	unreadOnly = true;
		PREF_GetBoolPref("offline.news.download.unread_only", &unreadOnly);
		theControl = (LControl *)mDialog->FindPaneByID('OUNR');
		XP_ASSERT(theControl);
		theControl->SetValue(unreadOnly);
		theControl->Disable();

		XP_Bool	byDate	= true;
		PREF_GetBoolPref("offline.news.download.by_date", &byDate);
		theControl = (LControl *)mDialog->FindPaneByID('BYDT');
		XP_ASSERT(theControl);
		theControl->SetValue(byDate);
		theControl->Disable();

		int32	daysOld = 30;
		PREF_GetIntPref("offline.news.download.days", &daysOld);
		theControl = (LControl *)mDialog->FindPaneByID('DAYS');
		XP_ASSERT(theControl);
		theControl->SetValue(daysOld);
		theControl->Disable();
	}
	else
	{
		SetBoolean('OUNR', mUnreadOnly);
		SetBoolean('BYDT', mByDate);
		SetNumberText('DAYS', mDaysOld);
	}

	SetBoolean(
		'HTML',
		::MSG_IsHTMLOK(CMailNewsContext::GetMailMaster(), mFolder.GetFolderInfo())
		);
	//MSG_PurgeByPreferences bodyPurgeBy;
	MSG_GetArticlePurgingInfo(
		(MSG_FolderInfo*)mFolder.GetFolderInfo(),
		&mUseBodyPurgeDefaults,
		&mBodyPurgeBy,
		&mBodyDaysKeep);
	Assert_(mBodyPurgeBy != MSG_PurgeByNumHeaders);
	SetBoolean('RmBd', mBodyPurgeBy == MSG_PurgeByAge);
	SetNumberText('BdDy', mBodyDaysKeep);
	
	MSG_GetHeaderPurgingInfo(
		(MSG_FolderInfo*)mFolder.GetFolderInfo(),
		&mUseHeaderPurgeDefaults,
		&mHeaderPurgeBy,
		&mKeepUnreadOnly,
		&mHeaderDaysKeep,
		&mNumHeadersToKeep);
	switch (mHeaderPurgeBy)
	{
		case MSG_PurgeNone:
			SetBoolean('KpAl', true);
			break;
		case MSG_PurgeByAge:
			SetBoolean('KpDa', true);
			break;
		case MSG_PurgeByNumHeaders:
			SetBoolean('KpNw', true);
			break;
	}
	SetBoolean('KpUn', mKeepUnreadOnly);
	SetNumberText('KDYS', mHeaderDaysKeep);
	SetNumberText('KpCt', mNumHeadersToKeep);
	
	// if we are offline the download now button needs to be disabled
	LPane *downloadButton = mDialog->FindPaneByID('DOWN');
	XP_ASSERT(downloadButton != nil);
	XP_Bool online = true;
	PREF_GetBoolPref("network.online", &online);
	if (!online)
		downloadButton->Disable();
		
} // StNewsGroupGetInfoHandler::ReadDataFields

//----------------------------------------------------------------------------------------
void StNewsGroupGetInfoHandler::WriteDataFields()
//----------------------------------------------------------------------------------------
{
	if (GetBoolean('DnMe'))
		mFolderPrefFlags |= MSG_FOLDER_PREF_OFFLINE;
	else
		mFolderPrefFlags &= ~MSG_FOLDER_PREF_OFFLINE;
	MSG_SetFolderPrefFlags((MSG_FolderInfo*)mFolder.GetFolderInfo(), mFolderPrefFlags);

	XP_Bool useDefaults = GetBoolean('UDFL');
	XP_Bool unreadOnly;
	XP_Bool byReadness;
	XP_Bool byDate;
	int32 daysOld;
	int32 headerDaysKeep  = GetNumberText('KDYS');
	if (useDefaults)
	{
		// if we are using pref defaults then just restore what we read in
		byReadness = mByReadness;
		unreadOnly = mUnreadOnly;
		byDate = mByDate;
		daysOld = mDaysOld;
	}
	else 
	{
		byReadness = unreadOnly = GetBoolean('OUNR');
		byDate = GetBoolean('BYDT');
		daysOld = GetNumberText('DAYS');
	}
	MSG_SetOfflineRetrievalInfo(
		(MSG_FolderInfo*)mFolder.GetFolderInfo(),
		useDefaults,
		byReadness,
		unreadOnly,
		byDate,
		daysOld);

	
	int32 bodyDaysKeep  = GetNumberText('BdDy');
	MSG_PurgeByPreferences bodyPurgeBy = MSG_PurgeNone;
	if (GetBoolean('RmBd'))
		bodyPurgeBy = MSG_PurgeByAge;
	useDefaults = (mUseBodyPurgeDefaults
				&& bodyPurgeBy == mBodyPurgeBy
				&& bodyDaysKeep == mBodyDaysKeep);
	MSG_SetArticlePurgingInfo(
		(MSG_FolderInfo*)mFolder.GetFolderInfo(), 
		useDefaults,
		bodyPurgeBy,
		bodyDaysKeep);

	MSG_PurgeByPreferences headerPurgeBy = MSG_PurgeNone;
	if (GetBoolean('KpDa'))
		headerPurgeBy = MSG_PurgeByAge;
	else if (GetBoolean('KpNw'))
		headerPurgeBy = MSG_PurgeByNumHeaders;
	unreadOnly = GetBoolean('KpUn');
	int32 numHeadersToKeep = GetNumberText('KpCt');
	useDefaults = (mUseHeaderPurgeDefaults
				&& headerPurgeBy == mHeaderPurgeBy
				&& unreadOnly == mUnreadOnly
				&& headerDaysKeep == mHeaderDaysKeep
				&& numHeadersToKeep == mNumHeadersToKeep
				&& unreadOnly == mKeepUnreadOnly);
	MSG_SetHeaderPurgingInfo(
		(MSG_FolderInfo*)mFolder.GetFolderInfo(), 
		useDefaults,
		headerPurgeBy,
		unreadOnly,
		headerDaysKeep,
		numHeadersToKeep
	);
	::MSG_SetIsHTMLOK(
		CMailNewsContext::GetMailMaster(),
		mFolder.GetFolderInfo(),
		nil,
		GetBoolean('HTML'));
	mDataWritten = true;
} // StNewsGroupGetInfoHandler::WriteDataFields

//----------------------------------------------------------------------------------------
Boolean StNewsGroupGetInfoHandler::UserDismissed()
//----------------------------------------------------------------------------------------
{
	if (mMessage == 'SYNC')
		return true;
	// 'DOWN' is the pane ID of the download now button
	if (mMessage == 'DOWN')
	{
		// only allowed to download now once
		LPane *downloadButton = mDialog->FindPaneByID('DOWN');
		XP_ASSERT(downloadButton != nil);
		downloadButton->Disable();
		// make call to backend
		::MSG_DownloadFolderForOffline(
			CMailNewsContext::GetMailMaster(),
			mFolder.GetFolderPane(),
			(MSG_FolderInfo*)mFolder.GetFolderInfo());
		return false;
	}
	if ('UDFL' == mMessage)
	{
		LControl *theControl;

		if (GetBoolean('UDFL'))
		{
			XP_Bool	unreadOnly = true;
			PREF_GetBoolPref("offline.news.download.unread_only", &unreadOnly);
			theControl = (LControl *)mDialog->FindPaneByID('OUNR');
			XP_ASSERT(theControl);
			theControl->SetValue(unreadOnly);
			theControl->Disable();

			XP_Bool	byDate	= true;
			PREF_GetBoolPref("offline.news.download.by_date", &byDate);
			theControl = (LControl *)mDialog->FindPaneByID('BYDT');
			XP_ASSERT(theControl);
			theControl->SetValue(byDate);
			theControl->Disable();

			int32	daysOld = 30;
			PREF_GetIntPref("offline.news.download.days", &daysOld);
			theControl = (LControl *)mDialog->FindPaneByID('DAYS');
			XP_ASSERT(theControl);
			theControl->SetValue(daysOld);
			theControl->Disable();
		}
		else
		{
			theControl = (LControl *)mDialog->FindPaneByID('OUNR');
			XP_ASSERT(theControl);
			theControl->SetValue(mUnreadOnly);
			theControl->Enable();

			theControl = (LControl *)mDialog->FindPaneByID('BYDT');
			XP_ASSERT(theControl);
			theControl->SetValue(mByDate);
			theControl->Enable();

			theControl = (LControl *)mDialog->FindPaneByID('DAYS');
			XP_ASSERT(theControl);
			theControl->SetValue(mDaysOld);
			theControl->Enable();
		}
		return false;
	}
	return StGetInfoHandler::UserDismissed();
}

#pragma mark -

//----------------------------------------------------------------------------------------
StNewsHostGetInfoHandler::StNewsHostGetInfoHandler(
	MSG_Host* inHost,
	LCommander *inSuperCommander)
//----------------------------------------------------------------------------------------
:	StGetInfoHandler(12502, inSuperCommander)
,	mHost(inHost)
{
}

//----------------------------------------------------------------------------------------
StNewsHostGetInfoHandler::~StNewsHostGetInfoHandler()
//----------------------------------------------------------------------------------------
{
	if (mMessage != msg_Cancel)
		WriteDataFields();
}

//----------------------------------------------------------------------------------------
void StNewsHostGetInfoHandler::ReadDataFields()
//----------------------------------------------------------------------------------------
{
	SetText('FNAM', CStr255(::MSG_GetHostName(mHost)));
	SetNumberText('PORT', ::MSG_GetHostPort(mHost));
	CStr255 securityString;
	::GetIndString(securityString, 7099, 13 + ::MSG_IsHostSecure(mHost));
	SetText('SECU', securityString);
	mPromptPassword = ::MSG_GetNewsHostPushAuth(MSG_GetNewsHostFromMSGHost(mHost));
	if (mPromptPassword)
		SetBoolean('AskP', true);
	else
		SetBoolean('Anon', true);
} // StNewsHostGetInfoHandler::ReadDataFields

//----------------------------------------------------------------------------------------
void StNewsHostGetInfoHandler::WriteDataFields()
//----------------------------------------------------------------------------------------
{
	XP_Bool promptPassword = GetBoolean('AskP');
	if (promptPassword != mPromptPassword)
		::MSG_SetNewsHostPushAuth(MSG_GetNewsHostFromMSGHost(mHost), promptPassword);
	mDataWritten = true;
} // StNewsHostGetInfoHandler::WriteDataFields

#pragma mark -



//----------------------------------------------------------------------------------------
StMailServerInfoHandler::StMailServerInfoHandler(
	CStr255& 					ioName,
	Boolean& 					ioPOP,
	Boolean 					inIsNewServer,
	Boolean						inAllowServerNameEdit,
	Boolean						inAllowServerTypeEdit,
	ServerNameValidationFunc	inValidationFunc,
	CServerListMediator*		inValidationServerList,
	LCommander*					inSuperCommander)
//----------------------------------------------------------------------------------------
:	StGetInfoHandler(12503, inSuperCommander)
,	mPOP(ioPOP)
,	mOriginallyPOP(ioPOP)
,	mIsNewServer(inIsNewServer)
,	mAllowServerNameEdit(inAllowServerNameEdit)
,	mAllowServerTypeEdit(inAllowServerTypeEdit)
,	mName(ioName)
,	mServerNameValidationFunc(inValidationFunc)
,	mValidationServerList(inValidationServerList)
{
}

//----------------------------------------------------------------------------------------
StMailServerInfoHandler::~StMailServerInfoHandler()
//----------------------------------------------------------------------------------------
{
	if (mMessage == msg_OK)
	{
		MPreferenceBase::SetWriteOnDestroy(true);
		WriteDataFields();
	}
}

//----------------------------------------------------------------------------------------
Boolean StMailServerInfoHandler::UserDismissed()
//----------------------------------------------------------------------------------------
{
	LWindow* window = GetDialog();
	
	if (mMessage == kMailServerTypePopup) // type popup menu (IMAP/POP)
	{	
		Boolean wantPOP = (GetValue(kMailServerTypePopup) == kPOP);
		if (wantPOP != mPOP)
		{
			if (wantPOP)
				SetPOP(false);
			else
				SetIMAP(false);
		}
	}
	
	// Disable OK if server name is a duplicate of an existing server
	LEditField	*nameEdit = dynamic_cast<LEditField *>(window->FindPaneByID(kMailServerNameEditText));
	Assert_(nameEdit);
	
	CStr255		serverName;
	nameEdit->GetDescriptor(serverName);
	
	if (mServerNameValidationFunc != nil)
	{
		if ( (*mServerNameValidationFunc)(serverName, mIsNewServer, mValidationServerList) )
			EnablePane('OKOK');
		else
			DisablePane('OKOK');
	}
	
	// should also disable OK if the user name field is empty
	
	
	MessageT	result = Inherited::UserDismissed();
	
	if (mMessage == msg_OK)
	{
		mName = serverName;
	}
	
	return result;
}

//----------------------------------------------------------------------------------------
void StMailServerInfoHandler::SetPOP(Boolean inDialogInit)
//----------------------------------------------------------------------------------------
{
	HidePane('IMPB'); // hide the imap tab
	HidePane('ADVB'); // hide the advanced tab
	ShowPane('POPB'); // show the POP tab
	
	SetText(kMailServerTypeCaption, "\pPOP"); 	// I don't think this has to be localized
	SetValue(kMailServerTypePopup, kPOP); 		// set the POP popup to true

	// Copy the user name from the 
	HideAndCripplePane(kIMAPServerUserNameEditText);
	HideAndCripplePane(kIMAPServerSavePasswordCheckbox);
	HideAndCripplePane(kIMAPServerCheckMailCheckbox);
	HideAndCripplePane(kIMAPServerCheckTimeEditText);
	
	ShowAndUncripplePane(kPOPServerUserNameEditText);
	ShowAndUncripplePane(kPOPServerSavePasswordCheckbox);
	ShowAndUncripplePane(kPOPServerCheckMailCheckbox);
	ShowAndUncripplePane(kPOPServerCheckTimeEditText);

	if (! inDialogInit)			// copy the user name over
	{
		CopyTextValue(kIMAPServerUserNameEditText, kPOPServerUserNameEditText);
		CopyTextValue(kIMAPServerCheckTimeEditText, kPOPServerCheckTimeEditText);

		CopyNumberValue(kIMAPServerSavePasswordCheckbox, kPOPServerSavePasswordCheckbox);
		CopyNumberValue(kIMAPServerCheckMailCheckbox, kPOPServerCheckMailCheckbox);
	}
	
	mPOP = true;
} // StMailServerInfoHandler::SetPOP

//----------------------------------------------------------------------------------------
void StMailServerInfoHandler::SetIMAP(Boolean inDialogInit)
//----------------------------------------------------------------------------------------
{
	HidePane('POPB'); // hide the POP tab
	ShowPane('IMPB'); // show the imap tab
	ShowPane('ADVB'); // show the advanced tab
	
	SetText(kMailServerTypeCaption, "\pIMAP"); // I don't think this has to be localized
	SetValue(kMailServerTypePopup, kIMAP); // set the POP popup to IMAP

	HideAndCripplePane(kPOPServerUserNameEditText);
	HideAndCripplePane(kPOPServerSavePasswordCheckbox);
	HideAndCripplePane(kPOPServerCheckMailCheckbox);
	HideAndCripplePane(kPOPServerCheckTimeEditText);

	ShowAndUncripplePane(kIMAPServerUserNameEditText);
	ShowAndUncripplePane(kIMAPServerSavePasswordCheckbox);
	ShowAndUncripplePane(kIMAPServerCheckMailCheckbox);
	ShowAndUncripplePane(kIMAPServerCheckTimeEditText);
	
	if (! inDialogInit)			// copy the user name over
	{
		CopyTextValue(kPOPServerUserNameEditText, kIMAPServerUserNameEditText);
		CopyTextValue(kPOPServerCheckTimeEditText, kIMAPServerCheckTimeEditText);
	
		CopyNumberValue(kPOPServerSavePasswordCheckbox, kIMAPServerSavePasswordCheckbox);
		CopyNumberValue(kPOPServerCheckMailCheckbox, kIMAPServerCheckMailCheckbox);
	}

	mPOP = false;
} // StMailServerInfoHandler::SetIMAP

//----------------------------------------------------------------------------------------
void StMailServerInfoHandler::SetDefaultUserName()
// Set username to identity.username if it is not set. This
// gives a sensible default for the username field.
//----------------------------------------------------------------------------------------
{
	CStr255 uname;
	
	if (mPOP)
		GetText(kPOPServerUserNameEditText, uname);
	else
		GetText(kIMAPServerUserNameEditText, uname);
		
	if (uname.IsEmpty())
	{
		UGetInfo::GetDefaultUserName(uname);
		
		if (mPOP)
			SetText(kPOPServerUserNameEditText, uname);
		else
			SetText(kIMAPServerUserNameEditText, uname);
	}
}


//----------------------------------------------------------------------------------------
void StMailServerInfoHandler::GetServerName(CStr255 &outName)
//----------------------------------------------------------------------------------------
{
	outName = mName;
}


//----------------------------------------------------------------------------------------
void StMailServerInfoHandler::ReadDataFields()
//----------------------------------------------------------------------------------------
{
	LWindow* window = GetDialog();
	// It's only legal to switch from POP to IMAP or back if there is exactly one server
	// in question.
	// In this case we also need to be careful to copy the text between the POP user name
	// and IMAP user name fields for consistency when flipping between them
	
	if (mAllowServerTypeEdit)
	{
		HidePane(kMailServerTypeCaption);		// the static text
		ShowPane(kMailServerTypePopup); 		// the popup menu
		SetValue(kMailServerTypePopup, kIMAP); 	// set the popup to IMAP as default
	}
	else
	{
		HidePane(kMailServerTypePopup); 		// the popup menu.
		ShowPane(kMailServerTypeCaption);		// the static text
	}
	
	if (mAllowServerNameEdit)
	{
		HidePane(kMailServerNameCaption);
		ShowPane(kMailServerNameEditText); // edittext
		SetText(kMailServerNameEditText, mName);
		
		// A paste snooper to filter out bad characters
		LBroadcasterEditField	*serverNameEditField = dynamic_cast<LBroadcasterEditField *>(window->FindPaneByID(kMailServerNameEditText));
		ThrowIfNil_(serverNameEditField);
		serverNameEditField->AddAttachment(new CPasteSnooper("\r\n\t :;,/", "\b\b\b\b____"));
		
		// Also set a key filter. Are we paranoid or what?
		serverNameEditField->SetKeyFilter(UMailFilters::ServerNameKeyFilter);
		
	}
	else
	{
		ShowPane(kMailServerNameCaption);
		HidePane(kMailServerNameEditText); // edittext
		SetText(kMailServerNameCaption, mName);
		SetText(kMailServerNameEditText, mName);		// used to write prefs on destruction
	}
	
	// show and hide the right controls
	if (mPOP)
		SetPOP(true);
	else
		SetIMAP(true);
		
	SetDefaultUserName();
	window->Show();
	// No need to do anything about the other prefs,
	// the prefs controls have already read and initialize themselves.
} // StMailServerInfoHandler::ReadDataFields

//----------------------------------------------------------------------------------------
void StMailServerInfoHandler::WriteDataFields()
//----------------------------------------------------------------------------------------
{
	mDataWritten = true;			// this is just for the assert in the base class destructor
		
	if (! MPreferenceBase::GetWriteOnDestroy())
		return;
	
#ifdef BEFORE_INVISIBLE_POPSERVER_NAME_EDITFIELD_TRICK_WAS_THOUGHT_OF
	// Popup is now a pref control.  Server name is now an invisible edit field in the prefs
	// pane (and that's the only case where it is changeable).
	PREF_SetIntPref("mail.server_type", (int)!mPOP); // 0=POP, 1=IMAP
	if (mPOP)
		PREF_SetCharPref("network.hosts.pop_server", mName);
#endif

#if 0
	if (mCreate || (mOriginallyPOP && !mPOP))
	{
		// If we changed from POP to IMAP, we have to create the IMAP server doohicky here

		if (!mPOP)
		{
			// Create the host date in msglib first, using default values.  Then, write out
			// the preferences (automatic when the dialog is destroyed) so that they get
			// suitably modified.
			CStr255 uname;
			GetText(kIMAPServerUserNameEditText, uname);
			const char* userNameString = nil;
			const char* serverNameString = nil;
			try
			{
				userNameString = XP_STRDUP(uname);
				ThrowIfNil_(userNameString);
				serverNameString = XP_STRDUP(mName);
				ThrowIfNil_(userNameString);
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
			catch(...)
			{
			}
			XP_FREEIF(const_cast<char*>(userNameString));
			XP_FREEIF(const_cast<char*>(serverNameString));
		}
	}
#endif

	// No need to do anything else, the prefs controls will write themselves if WriteOnDestroy is true
} // StMailServerInfoHandler::WriteDataFields

#pragma mark -

//----------------------------------------------------------------------------------------
UInt32 UGetInfo::CountIMAPServers()
//----------------------------------------------------------------------------------------
{
	char *serverList = nil;
	PREF_CopyCharPref("network.hosts.imap_servers", &serverList);
	char* residue = serverList;
	UInt32 result = 0;
	do
	{
		if (residue)
		{
			residue++; // skip the comma (or, on the first pass, the initial char).
			result++;
			residue = XP_STRCHR(residue, ',');
		}
	} while (residue);
	XP_FREEIF(serverList);
	return result;
}

//----------------------------------------------------------------------------------------
void UGetInfo::ConductFolderInfoDialog(CMessageFolder& inFolder, LCommander* inSuper)
//----------------------------------------------------------------------------------------
{
	if (inFolder.IsMailServer())
	{
		try
		{
			CStr255 pname(inFolder.GetName());
			if (inFolder.IsLocalMailFolder())
			{
				// More special POP dreck
				char* name = nil;
				::PREF_CopyCharPref("network.hosts.pop_server", &name);
				pname = name;
				XP_FREEIF(name);
			}
			Boolean usePOP = !inFolder.IsIMAPMailFolder();
			
			if (ConductMailServerInfoDialog(pname, usePOP, false, false, false,
										kStandAloneDialog, nil, nil, inSuper))
			{
				PREF_SavePrefFile();
			}
		}
		catch(...)
		{
		}
	}
	else if (inFolder.IsMailFolder())
	{
		StMailFolderGetInfoHandler handler(inFolder, inSuper);
		handler.DoDialog();
	}
	else if (inFolder.IsNewsHost())
	{
		StNewsHostGetInfoHandler handler(::MSG_GetHostForFolder(inFolder), inSuper);
		handler.DoDialog();
	}
	else if (inFolder.IsNewsgroup())
	{
		StNewsGroupGetInfoHandler handler(inFolder, inSuper);
		handler.DoDialog();
	}
} // UGetInfo::ConductGetInfoDialog

//----------------------------------------------------------------------------------------
Boolean UGetInfo::ConductMailServerInfoDialog(
	CStr255& 					ioName,
	Boolean& 					ioPOP,
	Boolean 					inIsNewServer,
	Boolean						inAllowServerNameEdit,
	Boolean						inAllowTypeChange,
	Boolean						inUsePrefsCache,
	ServerNameValidationFunc	inValidationFunc,
	CServerListMediator*		inValidationServerMediator,
	LCommander* 				inSuper)
//
//	The mail server properties dialog is brought up from two places; the folder
//	pane, as a Get Info dialog, and in the preferences, as the mail server edit
//	dialog. When instantiated from the prefs dialog, we only want to save out
//	the prefs if the user OKs the entire prefs dialog. To achieve this we
//	have the controls write out their values into a temp prefs tree, e.g.
//	"temp_prefs_mac.network.mail.check_time". When the user OKs the prefs dialog
//	all these prefs are copied to the main preferences, in CPrefsDialog::Finished().
//	
//	This prefs cacheing functionality is perfomed by MPreferenceBase.
//
//	When used as a properties dialog, the controls write their values directly
//	into the main prefs.
//	
//----------------------------------------------------------------------------------------
{
	MessageT result;
	
	// Use the nifty "mail.imap.server.<hostname>.<property>" preference convention.  Our
	// constructor resources have "mail.imap.server.^0.<property>", and the MPreferenceBase
	// class knows how to replace ^0 by the replacement string.
		
	{
		MPreferenceBase::StReplacementString 	stringSetter(ioName);			// used for replacing ^0 in the controls prefs
																				// strings with the server name
		MPreferenceBase::StWriteOnDestroy 		writeSetter(false);
		MPreferenceBase::StUseTempPrefCache		tempSetter(inUsePrefsCache);	// used to force the controls to read (if pos)
																				// and write to a temp prefs tree
		
		{ // <-- StMailServerInfoHandler scope
			StMailServerInfoHandler	theHandler(ioName, ioPOP, inIsNewServer, inAllowServerNameEdit, inAllowTypeChange,
										inValidationFunc, inValidationServerMediator, inSuper);
			
			result = theHandler.DoDialog();
		
			// The user may have modified the server name. Update the replacement string before handler destruction
			CStr255		newServerName;
			theHandler.GetServerName(newServerName);
			
			ioName = newServerName;
			ioPOP = theHandler.GetPop();
			
			MPreferenceBase::SetReplacementString(newServerName);
			
			// Handler is destroyed, and prefs saved. Write on destroy is set in the destructor
		} // <-- StMailServerInfoHandler scope
	}
	
	return (result == msg_OK);
} // UGetInfo::ConductMailServerInfoDialog

//----------------------------------------------------------------------------------------
Boolean UGetInfo::ConductNewsServerInfoDialog(
	MSG_Host* 			inHost,
	LCommander* 		inSuper)
//----------------------------------------------------------------------------------------
{
	StNewsHostGetInfoHandler handler(inHost, inSuper);
	MessageT result = handler.DoDialog();
	return (result == msg_OK);
} // UGetInfo::ConductNewsServerInfoDialog

//----------------------------------------------------------------------------------------
/* static */ void UGetInfo::GetDefaultUserName(CStr255& outName)
// Set username to identity.username if it is not set. This
// gives a sensible default for the username field.
//----------------------------------------------------------------------------------------
{
	char cname[256];
	int cnamelength = sizeof(cname);
	// Many people set up their profiles as "user@bar.com".  Check for
	// that format (no space, and an '@')
	PREF_GetCharPref("mail.identity.username", cname, &cnamelength);
	char* atPos = XP_STRCHR(cname, '@');
	char* spacePos = XP_STRCHR(cname, ' ');
	if (spacePos || !atPos)
	{
		// Oh well, try using the email address up to the '@'
		PREF_GetCharPref("mail.identity.useremail", cname, &cnamelength);
		// strip off the username from the email address.
		atPos = XP_STRCHR(cname, '@');
	}
	if (atPos)
		*atPos = 0;
	outName = cname;
}

//----------------------------------------------------------------------------------------
void UGetInfo::RegisterGetInfoClasses()
//----------------------------------------------------------------------------------------
{
}

