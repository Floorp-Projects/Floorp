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

#pragma once

#include "CPrefsMediator.h"

#include "MPreference.h"

#include <LGADialogBox.h>

class CTSMEditField;
class LGACheckbox;
class MSG_IMAPHost;
class MSG_Host;
class CDragOrderTextList;

//======================================
#pragma mark
class CMailNewsIdentityMediator : public CPrefsMediator
//======================================
{
	private:
		typedef CPrefsMediator Inherited;
		
	public:

		enum { class_ID = PrefPaneID::eMailNews_Identity };
		CMailNewsIdentityMediator(LStream*);
		virtual	~CMailNewsIdentityMediator() {};

		virtual void	ListenToMessage(MessageT inMessage, void *ioParam);

//		virtual	void	UpdateFromIC();	// don't need this because the IC checkbox is on
										// this pane
		virtual	void	LoadPrefs();
};

#ifdef MOZ_MAIL_NEWS

//======================================
#pragma mark
class CMailNewsMessagesMediator : public CPrefsMediator
//======================================
{
	private:
		typedef CPrefsMediator Inherited;
		
	public:

		enum { class_ID = PrefPaneID::eMailNews_Messages };
		CMailNewsMessagesMediator(LStream*);
		virtual	~CMailNewsMessagesMediator() {}

		virtual	void	LoadMainPane();

		static Boolean WrapLinesValidationFunc(CValidEditField *wrapLines);
};

//======================================
#pragma mark
class CMailNewsOutgoingMediator : public CPrefsMediator
//======================================
{
	private:
		typedef CPrefsMediator Inherited;
		
	public:

		enum { class_ID = PrefPaneID::eMailNews_Outgoing };
		CMailNewsOutgoingMediator(LStream*);
		virtual	~CMailNewsOutgoingMediator();
		virtual	void	LoadPrefs();
		virtual	void	WritePrefs();
		virtual void	ListenToMessage(MessageT inMessage, void* ioParam);
	protected:
#define kNumFolderKinds 4
		char*			mFolderURL[kNumFolderKinds];
}; // class CMailNewsOutgoingMediator

//======================================
#pragma mark
class MServerListMediatorMixin
// Common code for a pane containing a list of servers
//======================================
{
protected:
	MServerListMediatorMixin(CPrefsMediator* inMediatorSelf)
	:	mMediatorSelf(inMediatorSelf)
	,	mServerTable(nil)
	,	mServersLocked(false)
	,	mServersDirty(false)
 	{}

		struct CellContents
		{
			CellContents(const char* inName = nil);
			Str255			description;
		};

				Boolean GetHostFromRow(
							TableIndexT inRow,
							CellContents& outCellData,
							UInt32 inDataSize) const;
				Boolean GetHostFromSelectedRow(
							CellContents& outCellData,
							UInt32 inDataSize) const;
		virtual	void	UpdateButtons();
		virtual	void	AddButton() = 0;
		virtual	void	EditButton() = 0;
		virtual	void	DeleteButton() = 0;
				void	ClearList();
		virtual	void	LoadList() = 0;
		virtual	void	WriteList() = 0;
		virtual Boolean	Listen(MessageT inMessage, void *ioParam);
				void	LoadMainPane();

	CPrefsMediator* 	mMediatorSelf;
	CDragOrderTextList*	mServerTable;
	Boolean				mServersLocked;
	Boolean				mServersDirty;
}; // class MServerListMediatorMixin

//======================================
#pragma mark
class CServerListMediator
//======================================
:	public CPrefsMediator
,	public MServerListMediatorMixin
{
	public:
						CServerListMediator(PaneIDT inMainPaneID);
		virtual void	ListenToMessage(MessageT inMessage, void *ioParam);
		virtual	void	LoadMainPane();
};

//======================================
#pragma mark
class CMailNewsMailServerMediator
//======================================
:	public CServerListMediator
{
	private:
		typedef CServerListMediator Inherited;
		
	public:

		enum { class_ID = PrefPaneID::eMailNews_MailServer };
		CMailNewsMailServerMediator(LStream*);
		virtual	~CMailNewsMailServerMediator();

		virtual	void	UpdateFromIC();
		virtual	void	LoadMainPane();
		virtual	void	LoadPrefs();
		virtual	void	WritePrefs();

		static Boolean NoAtSignValidationFunc(CValidEditField *noAtSign);

	private:
		enum ServerType
		{
			eUnknownServerType = -1,
			ePOPServer = 0,
			eIMAPServer = 1
		};
				MSG_IMAPHost*	FindMSG_Host(const char* inHostName);
				Boolean	UsingPop() const; // Hope this goes away.

	// MServerListMediatorMixin overrides
		virtual	void	AddButton();
		virtual	void	EditButton();
		virtual	void	DeleteButton();
		virtual	void	LoadList();
		virtual	void	WriteList();

	// Data
	protected:
		ServerType		mServerType;
		char*			mPopServerName;
}; // class CMailNewsMailServerMediator

#pragma mark
//======================================
class CMailNewsNewsServerMediator
//======================================
:	public CServerListMediator
{
	private:
		typedef CServerListMediator Inherited;
		
	public:

		enum { class_ID = PrefPaneID::eMailNews_NewsServer };
		CMailNewsNewsServerMediator(LStream*);
		virtual	~CMailNewsNewsServerMediator() {};

		virtual	void	UpdateFromIC();
		virtual	void	LoadPrefs();
		virtual	void	WritePrefs();
		static Boolean	PortNumberValidationFunc(CValidEditField *portNumber);

	// MServerListMediatorMixin overrides
		struct CellContents : public MServerListMediatorMixin::CellContents
		{
			CellContents() {}
			CellContents(const char* inName, MSG_Host* inHost)
			:	MServerListMediatorMixin::CellContents(inName)
			,	serverData(inHost) {}
			MSG_Host*	serverData;
		};
		virtual	void	AddButton();
		virtual	void	EditButton();
		virtual	void	DeleteButton();
		virtual	void	LoadList();
		virtual	void	WriteList();

	enum
	{
		eNNTPStandardPort	= 119,
		eNNTPSecurePort	= 563
	};

	private:
		Boolean	mNewsServerPortLocked;
}; // class CMailNewsNewsServerMediator

struct DIR_Server;
class LTextColumn;
//======================================
#pragma mark
class CMailNewsDirectoryMediator
//======================================
:	public CServerListMediator
{
	private:
		typedef CServerListMediator Inherited;
		
	public:

		enum { class_ID = PrefPaneID::eMailNews_Directory };
		CMailNewsDirectoryMediator(LStream*);
		virtual	~CMailNewsDirectoryMediator() {};

		virtual	void	LoadMainPane();
		virtual	void	LoadPrefs();
		virtual	void	WritePrefs();

		virtual Boolean	ObeyCommand ( CommandT inCommand, void* ioParam );
		
	private:
	// MServerListMediatorMixin overrides
		virtual	void	AddButton();
		virtual	void	EditButton();
		virtual	void	DeleteButton();
		virtual	void	LoadList();
		virtual	void	WriteList();

		virtual	void	UpdateButtons();
				Boolean	IsPABServerSelected(LTextColumn *serverTable = nil);
				Boolean	IsPABServerSelected(DIR_Server *server);
		struct CellContents : public MServerListMediatorMixin::CellContents
		{
			CellContents(const char* inName = nil)
			:	MServerListMediatorMixin::CellContents(inName)
			,	serverData(nil) {}
			DIR_Server	*serverData;
		};
		XP_List	*mDeletedServerList;
}; // class CMailNewsDirectoryMediator

//======================================
#pragma mark
class COfflineNewsMediator : public CPrefsMediator
//======================================
{
	private:
		typedef CPrefsMediator Inherited;
		
	public:

		enum { class_ID = PrefPaneID::eOffline_News };
		COfflineNewsMediator(LStream*);
		virtual	~COfflineNewsMediator() {};

		virtual	void	LoadMainPane();
		virtual	void	WritePrefs();

		static Boolean	SinceDaysValidationFunc(CValidEditField *sinceDays);
}; // class COfflineNewsMediator

//======================================
class CLDAPServerPropDialog : public LGADialogBox
// This class brings up a dialog for editing the LDAP Server properties (duh). This has 
// to be a derivative of LGADialogBox and not just a normal stack-based dialog because
// we want NetHelp to actually get the message to load the help URL while the dialog is up,
// not after you close it, and the stack-based dialogs never returned to the event loop
// to allow the url processing.
//
// The dialog passes the OK/Cancel messages through to the preference pane so the pane can
// handle doing the right thing and close out the dialog. They should be handled in 
// ObeyCommand()
//======================================
{
	typedef LGADialogBox Inherited;

	public:
		
		// PUBLIC CONSTANTS
		
		enum {
			class_ID = 'LDSP',
			eLDAPServerPropertiesDialogResID = 12002,
			cmd_OKButton = -128,
			cmd_CancelButton = -129,
			cmd_HelpButton = 3
		};
		
		enum { eEditServer = false, eNewServer = true } ;

		// PUBLIC METHODS
		
		CLDAPServerPropDialog ( LStream* inStream );
		virtual ~CLDAPServerPropDialog() ;
		
		virtual void FinishCreateSelf();
		virtual void ListenToMessage ( MessageT inMessage, void* ioParam );
		
		void SetUpDialog( DIR_Server* inServer, Boolean inNew, Boolean inIsPAB, 
							Boolean inIsAllLocked );
		DIR_Server* GetServer ( ) { return mServer; } ;
		Boolean IsEditingNewServer ( ) { return mNewServer; } ;
		
	protected:
	
		// Port ID's
		enum {
			eLDAPStandardPort = 389,
			eLDAPSecurePort = 636
		};

		// Pane ID's for dialog
		enum {
			eDescriptionEditField = 10,
			eLDAPServerEditField,
			eSearchRootEditField,
			ePortNumberEditField,
			eMaxHitsEditField,
			eSecureBox = 20,
			eSaveLDAPServerPasswordBox = 21,
			eUpdateButton = 22,
			eDownloadCheckBox = 23
		};
		
		static Boolean MaxHitsValidationFunc(CValidEditField *maxHits) ;
		static Boolean PortNumberValidationFunc(CValidEditField *portNumber) ;

		const char* mHelpTopic;			// help string for NetHelp
		DIR_Server* mServer;		// the LDAP server
		Boolean mNewServer;			// true if a new entry, false if editing existing
		Boolean mIsPAB;				// ???
		
		CTSMEditField* mDescription;		// Items in dialog
		LEditField* mLdapServer;
		LEditField* mSearchRoot;
		CValidEditField* mPortNumber;
		CValidEditField* mMaxHits;
		LGACheckbox* mSecureBox;
		LGACheckbox* mSavePasswordBox;
		LGACheckbox* mDownloadCheckBox;
		
}; // class CLDAPServerPropDialog

#endif // MOZ_MAIL_NEWS
