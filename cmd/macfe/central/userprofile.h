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

#include <Files.h>
#include "PascalString.h"
#include "StBlockingDialogHandler.h"

const MessageT cmd_SelectProfile = 4000;
const MessageT cmd_NewProfile = 4001;
const MessageT cmd_DeleteProfile = 4002;
const MessageT cmd_RenameProfile = 4003;
const MessageT cmd_QuitProfile = 4004;
const MessageT cmd_EditDialSettings = 4010;
const MessageT cmd_LocationPopup = 4011;

enum ProfileErr {
	eUserCancelled = -2,
	eUnknownError = -1,
	eNeedUpgrade = 0,
	eOK = 1,
	eRunAccountSetup = 2,
	eRunMUC = 3,
	eSkipMUC = 4
};


/*****************************************************************************
 * class CUserProfileDB
 *
 * Wrapper for multi-user profile database file.
 *
 *****************************************************************************/
class CUserProfileDB
{

public:

						CUserProfileDB(FSSpec& spec, Boolean createIt = false);
						
	short				CountProfiles();
	short				GetNextProfileID();

	short				GetProfileIDByUsername(const CString& userName);
	short				GetProfileIDByEmail(const CString& emailAddr);
	
	short				GetLastProfileID();
	void				SetLastProfileID(short newUserID);
	
	void				AddNewProfile(short id, const CStr31& profileName,
							const FSSpec& profileFolder);
	
	Boolean				GetProfileName(short id, CStr31& name);
	void				SetProfileName(short id, const CStr31& name);
	
	void				SetProfileData(short id);

	Boolean				GetProfileAlias(short id, FSSpec& profileFolder, Boolean allowUserInteraction = true);							
	void				DeleteProfile(short selectedID);
	
	LFile *				GetFile() {return &fFile;}


private:
	LFile				fFile;
	Handle				GetDBResource(ResType theType, short theID);
	
	enum			{	kFirstProfileID = 128	};

};


/*****************************************************************************
 * class CDialogWizardHandler
 *
 * A generic dialog wizard handler.
 *
 *****************************************************************************/
class CDialogWizardHandler
{
public:
							CDialogWizardHandler( ResIDT dlogID, LArray& paneList );
	void					AddListener(LListener* st);
	
	Boolean					DoWizard();
	LWindow*				GetDialog();
	
	void					GetEditText( PaneIDT paneID, CString& text );
	void					SetEditText( PaneIDT paneID, const CString& text );
	
	void 					SetCheckboxValue(PaneIDT paneID, const Boolean value);
	Boolean					GetCheckboxValue(PaneIDT paneID);

	PaneIDT					CurrentPane();
	ArrayIndexT				CurrentPaneNumber();
	ArrayIndexT				TotalPanes();
	void					EnableNextButton();
	void					DisableNextButton();
	
protected:
	Boolean					ShowPane( ArrayIndexT paneNum, LWindow* window );
	
	StBlockingDialogHandler	fDialog;
	LArray					fPaneList;
	ArrayIndexT				fCurrentPane;
	LListener*				fListener;
};

/*****************************************************************************
 * class CUserProfile
 *
 * Launches wizards and file operations for multi-user profile support.
 *
 *****************************************************************************/
class CUserProfile
{

public:
	static void			InitUserProfiles();
	
	// Opens the User Profiles registry and puts up a profile-selection
	// dialog if there is more than one profile (or showDialog is true).
	// Returns kNeedUpgrade if User Profiles does not exist (i.e. we need 
	// to call HandleUpgrade); else returns path of selected profile.
	
	static ProfileErr	GetUserProfile( const FSSpec& usersFolder,
							FSSpec& profileFolder, Boolean showDialog, short fileType );
	
	// Creates a new network profile in the user's folder
	static ProfileErr	CreateNetProfile( FSSpec usersFolder, FSSpec& profileFolder );
	
private:
	static ProfileErr	DoNetProfileDialog();
	static void			DoNetExtendedProfileDialog(LCommander * super);

public:
	// Launches upgrade wizard for users who have not run 4.0 before.
	// Creates an initial profile folder and User Profiles file.
	// If oldNetscapeF is non-null, it points to the user's 3.0
	// Netscape Ä folder and the profile "folder" is an alias to it.
	// Returns error code if user cancelled; else returns profile path.
	
	static ProfileErr	HandleUpgrade( FSSpec& profileFolder,
							const FSSpec* oldNetscapeF = nil );

	// Creates a unique profile folder name if necessary
	static void			GetUniqueFolderName(FSSpec& folder);

	static short		sCurrentProfileID;
	
	enum  			{	kRenamePrompt = 1,
						kDeletePrompt,
						kReadError,
						kCreateError,
						kDefaultName,
						kBadAliasError,
						kQuitLabel,
						kDoneLabel,
						kNextLabel,
						kConfigFileError,
						kInvalidConfigFile,
						kRunASLabel,
						kCreateProfileLabel,
						kConfigurationFileName	};
	enum			{	kProfileStrings = 900	};
							
private:
	static ProfileErr	HandleProfileDialog( FSSpec& profileSpec, CUserProfileDB& profileDB,
							FSSpec& profileFolder, short& newUserID, short lastUserID,
							Boolean wantsProfileManager );
	static void			PopulateListBox( ListHandle& listHand, CUserProfileDB& profileDB,
							short defaultID );
	
	enum UpgradeEnum {	eNoUpgrade,			// an additional profile is being created
						eExistingPrefs,		// first profile, existing Netscape Prefs file
						eNewInstall		};	// first profile, fresh install

	static ProfileErr	NewUserProfile( const FSSpec& profileSpec, FSSpec& profileFolder,
							CStr31& profileName, UpgradeEnum upgrading = eNoUpgrade,
							const FSSpec* oldNetscapeF = nil );
	static ProfileErr	NewProfileWizard( UpgradeEnum upgrading, CStr31& profileName,
							const FSSpec& profileFolder, FSSpec& newProfileFolder,
							Boolean& userChoseFolder );
							
	static void			RenameProfile( short selectedID, CUserProfileDB& profileDB,
							Cell& cell, ListHandle& listHand );
	static void			DeleteProfile( short selectedID, CUserProfileDB& profileDB,
							ListHandle& listHand );
	
	static void			ReflectToPreferences(const CStr31& profileName,
							const FSSpec& profileFolder, short numProfiles = 1);
	static void			CreateDefaultProfileFolder(const FSSpec& profileFolder);
	
	static OSErr	 	MakeDesktopIcons(const CStr31& profileName,
							const Boolean wantsNavigator, const Boolean wantsInbox);

	enum			{	kInvalidProfileID = -1 };
	
protected:
						// ¥ÊinPrefsFolder is the FSSpec of the users Preferences
						//		folderÉ we read a file directly below that
	static long			SendMessageToPlugin( long inMessage, void* pb = NULL );
	
	static void*		LoadConfigPlugin();		// really returns PE_PluginFuncType
	static OSErr		CloseConfigPlugin();
	
	static Boolean		DeleteMagicProfile( FSSpec& inSpec );
	
	static CFragConnectionID	mConfigPluginID;
	static Boolean		mHasConfigPlugin;
	static Boolean		mPluginLoaded;
};

