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

#include "profile.h"
#include "client.h"
#include "UStdDialogs.h"
#include "ufilemgr.h"
#include "uerrmgr.h"
#include "uapp.h"
#include "uprefd.h"
#include "prefwutil.h"
#include "macutil.h"
#include "prefapi.h"
#include "resgui.h"
#include "xp_file_mac.h"
#include "DirectoryCopy.h"
#include <LGARadioButton.h>
#include <LGAPushButton.h>
#include <LGAEditField.h>

// for multi-user profile support in PE
#include "MUC.h"
#include <CodeFragments.h>
#include <LString.h>

#define updateWizardDialog		9800

#define profilePEPane			9801
#define profileIntroPane		9802
#define userNamePane			9803
#define profileNamePane			9804
#define profileFolderPane		9805
#define profileIconsPane		9806
#define profileDonePane			9807

#define profileSelectDialog		9900
#define profileManagerDialog	9901

// NOTE: Magic name must be kept in sync with ns/modules/libreg/src/reg.h
#define MAGIC_PROFILE_NAME		"User1"

const char* kProfileNamePref = "profile.name";

/*****************************************************************************
 * The resource format for storing profile metadata (i.e., username and 
 * email) in the profile database so it can be easily searched.  In order
 * to allow an arbitrary amount of metadata, the resource is divided into
 * a header section, which counts the number of metadata items and the length
 * to each, and a data section, which simply consists of the data in packed
 * format.  The offset to each set of data is the sum of the first offset
 * plus all of the lengths.  Strings are null terminated for convenience.
 * All of this is stored in the profile database in a 'DATA' resource
 * which corresponds to the id of the profile in use.
 ****************************************************************************/
 
/* The version of the metadata for Nav 4.0 */

typedef struct {
	short int		count;
	short int		firstOffset;
	long int		nameLength;
	long int		emailLength;
	/* New data element lengths would go here, along with a corresponding
	   change in the count and firstOffset items */
	/* Data follows here */
} ProfileDataHeader;

MODULE_PRIVATE int PR_CALLBACK ProfilePrefChangedFunc(const char *pref, void *data);

CFragConnectionID CUserProfile::mConfigPluginID;

class LWhiteListBox: public LListBox
{
#if !defined(__MWERKS__) || (__MWERKS__ >= 0x2000)
	typedef LListBox inherited;
#endif

public:
	enum	{ class_ID = 'Lwht' };
	
						LWhiteListBox( LStream* inStream );
	
	virtual Boolean		FocusDraw(LPane* inSubPane = nil);
	virtual void		DrawSelf();
};

LWhiteListBox::LWhiteListBox( LStream* inStream ): LListBox( inStream )
{
}

Boolean LWhiteListBox::FocusDraw(LPane* /*inSubPane*/)
{
	const RGBColor rgbWhite = { 0xFFFF, 0xFFFF, 0xFFFF };
	
	if ( inherited::FocusDraw() )
	{
		::RGBBackColor( &rgbWhite );
		return TRUE;
	}
	return FALSE;
}

void LWhiteListBox::DrawSelf()
{
	Rect	frame;
	
	this->CalcLocalFrameRect( frame );
	::EraseRect( &frame );
	LListBox::DrawSelf();	
}

MODULE_PRIVATE int PR_CALLBACK ProfilePrefChangedFunc(const char *pref, void * /* data */) 
{
	FSSpec profileSpec = CPrefs::GetFilePrototype(CPrefs::UsersFolder);		
	GetIndString(profileSpec.name, 300, userProfiles);

	CUserProfileDB		profile(profileSpec);

	if ((!XP_STRCASECMP(pref,"mail.identity.username")) ||
		(!XP_STRCASECMP(pref,"mail.identity.useremail")))
	{
		profile.SetProfileData( CUserProfile::sCurrentProfileID );			
	}
	else if (!XP_STRCMP(pref, kProfileNamePref))
	{
		char profileName[255];
		int len = 255;
		if ( PREF_GetCharPref(kProfileNamePref, profileName, &len) == PREF_NOERROR )
		{		
			profile.SetProfileName( CUserProfile::sCurrentProfileID, (CStr255) profileName );
		}
	}
	
	return 0;
}

class CProfilePaneMonitor : public LListener
{
	public:
		CProfilePaneMonitor(CDialogWizardHandler*);
		virtual void	ListenToMessage(MessageT inMessage, void *ioParam);
	
	private:
		CDialogWizardHandler*	fWizard;
		Boolean	fCopiedName;
};

/*****************************************************************************
 * class CUserProfile
 *
 * Dialog handlers & wizards for multi-user profile support.
 *
 *****************************************************************************/

short CUserProfile::sCurrentProfileID = CUserProfile::kInvalidProfileID;
Boolean CUserProfile::mHasConfigPlugin = FALSE;
Boolean CUserProfile::mPluginLoaded = FALSE;

void CUserProfile::InitUserProfiles()
{
	mHasConfigPlugin = ( LoadConfigPlugin() != NULL );
	if ( mHasConfigPlugin )
		CloseConfigPlugin();		
}

// Attempts to open "User Profiles" and put up a selection dialog.
// Returns the location of the user's selected Prefs folder.
ProfileErr
CUserProfile::GetUserProfile( const FSSpec& usersFolder, FSSpec& profileFolder,
	Boolean showDialog, short fileType )
{
	ProfileErr		result = eOK;
	Boolean			done = true;
	Boolean			wantsProfileManager = showDialog;
	FSSpec			profileSpec;
	short			numProfiles = 1;
	CStr31			profileName;
	
	// ¥ÊprofileSpec here is "User Profiles"
	profileSpec.vRefNum = usersFolder.vRefNum;
	profileSpec.parID = usersFolder.parID;
	GetIndString( profileSpec.name, 300, userProfiles );
	
	// ¥ÊI have no idea why this is so convoluted
	
	if ( !CFileMgr::FileExists( profileSpec ) )
		return eNeedUpgrade;
	
	if ( DeleteMagicProfile( profileSpec ) )
		return eNeedUpgrade;

	CUserProfileDB		profileDB( profileSpec );
	do
	{
		Try_
		{
			short		newUserID = 0;
			short		lastUserID = profileDB.GetLastProfileID();
			
			// ¥Êonly put up dialog if there's more than one user
			if ( showDialog || profileDB.CountProfiles() > 1 )
			{
				// HACK ALERT!!!!!
				// If we're being asked to open a doc or get a URL just skip the dialog
				// and use the last profile that was selected
				if ( (fileType == FILE_TYPE_ODOC) || (fileType == FILE_TYPE_GETURL) )
				{
					newUserID = lastUserID;
					if ( profileDB.GetProfileAlias( newUserID, profileFolder ) )
						result = eOK;
					else	
						result = eUnknownError;

				}
				else
				// END HACK ALERT!!!!!
					result = HandleProfileDialog(
						profileSpec,
						profileDB,
						profileFolder,
						newUserID,
						lastUserID,
						wantsProfileManager );
					
				if ( result >= 0 && lastUserID != newUserID )
					profileDB.SetLastProfileID( newUserID );
				
				if ( result >= eOK )
					PREF_RegisterCallback("mail.identity",ProfilePrefChangedFunc, NULL);
				done = true;
			}
			else
			{
				newUserID = lastUserID;
				Try_
				{
					if ( profileDB.GetProfileAlias( lastUserID, profileFolder ) )
						result = eOK;
					else	
						result = eUnknownError;
				}
				Catch_ ( inErr )
				{
					CStr255 errStr;
					GetIndString( errStr, kProfileStrings, kReadError );
					ErrorManager::ErrorNotify( inErr, errStr );
				}
				
				if ( result < eOK )
				{
					// ¥ if we failed to load the profile, loop back to the
					// beginning and force Profile Manager to appear
					done = false;
					showDialog = wantsProfileManager = true;
				}
				else
				{
					long		err;
					err = SendMessageToPlugin( kAutoSelectDialConfig, &profileFolder );
					if ( err == errProfileNotFound )
						SendMessageToPlugin( kEditDialConfig, &profileFolder );
												
					PREF_RegisterCallback( "mail.identity",ProfilePrefChangedFunc, NULL );
				}
			}
			
			// ¥ save the previous user ID back to profile db
			if ( result >= eOK )
			{
				sCurrentProfileID = newUserID;
				numProfiles = profileDB.CountProfiles();
			}
		}
		Catch_ ( inErr )
		{
			CStr255		errStr;
			GetIndString( errStr, kProfileStrings, kReadError );
			ErrorManager::ErrorNotify( inErr, errStr );
			result = eUnknownError;
		}
	}
	while ( !done );
	
	if (result != eUserCancelled) {
		// ¥Êreflect path & name into xp preferences
		if (sCurrentProfileID != kInvalidProfileID)
			profileDB.GetProfileName( sCurrentProfileID, profileName );
		ReflectToPreferences(profileName, profileFolder, numProfiles);	
	}
	return result;
}

static void PrefToEditField(const char * prefName, LGAEditField * field);
static void EditFieldToPref(LGAEditField * field, const char * prefName);

#define PREF_STRING_LEN 255
void PrefToEditField(const char * prefName, LGAEditField * field)
{
	int prefStringLen;
	char prefString[PREF_STRING_LEN];
	prefStringLen = PREF_STRING_LEN;
	if ( PREF_GetCharPref(prefName, prefString, &prefStringLen) == 0 )
	{
		c2pstr(prefString);
		field->SetDescriptor((unsigned char *)prefString);
	}
}

void EditFieldToPref(LGAEditField * field, const char * prefName)
{
	Str255 s;
	field->GetDescriptor(s);
	p2cstr(s);
	PREF_SetCharPref(prefName, (char*)s);
}

// Displays the additional data login dialog. Throws are taken care of by
// the caller
void
CUserProfile::DoNetExtendedProfileDialog(LCommander * super)
{
	StDialogHandler	theHandler(9911, super);
	LWindow			*theDialog = theHandler.GetDialog();
	
	LGAEditField *ldapAddressField = (LGAEditField*)theDialog->FindPaneByID('addr');
	LGAEditField *searchBaseField = (LGAEditField*)theDialog->FindPaneByID('sbas');
	LGAEditField *httpAddressField = (LGAEditField*)theDialog->FindPaneByID('hurl');
	LGARadioButton * ldapRadio = (LGARadioButton *)theDialog->FindPaneByID('ldap');
	LGARadioButton * httpRadio = (LGARadioButton *)theDialog->FindPaneByID('http');
	
	ThrowIfNil_(ldapAddressField);
	ThrowIfNil_(searchBaseField);
	ThrowIfNil_(httpAddressField);
	ThrowIfNil_(ldapRadio);
	ThrowIfNil_(httpRadio);

// Initialize the dialog from the preferences
//	PrefToEditField("li.server.ldap.serverName", ldapAddressField );
//	PrefToEditField("li.server.ldap.searchBase", searchBaseField );
//	PrefToEditField("li.server.http.baseURL", httpAddressField);
	ldapRadio->SetValue(1);
	int prefStringLen;
	char prefString[PREF_STRING_LEN];
	prefStringLen = PREF_STRING_LEN;
	if ( PREF_GetCharPref("li.protocol", prefString, &prefStringLen) == 0 )
		if (XP_STRCMP(prefString, "http") == 0)
			httpRadio->SetValue(1);

// Do the dialog
	httpAddressField->SelectAll();
	theDialog->SetLatentSub(httpAddressField);
	theDialog->Show();
	
	while (true) 
	{
		MessageT	hitMessage = theHandler.DoDialog();
		if ( hitMessage == msg_Cancel )
			break;
		else if ( hitMessage == msg_OK )
		{
			
			LStr255 httpAddress;
			httpAddressField->GetDescriptor(httpAddress);
			if ( httpAddress.Length() > 2 )
			{
				if (!httpAddress.BeginsWith(LStr255("http://")))
					httpAddress.Insert(LStr255("http://"), 0);
			}
			EditFieldToPref(ldapAddressField, "li.server.ldap.serverName");
			EditFieldToPref(searchBaseField, "li.server.ldap.searchBase");
			EditFieldToPref(httpAddressField, "li.server.http.baseURL");
		
			if ( ldapRadio->GetValue() > 0 )
				PREF_SetCharPref("li.protocol", "ldap");
			else
				PREF_SetCharPref("li.protocol", "http");

			break;
		}
	}
}

// Displays the modal login dialog
ProfileErr
CUserProfile::DoNetProfileDialog()
{
	ProfileErr perr = eOK;
	try
	{
		StDialogHandler	theHandler(9910, CFrontApp::GetApplication());
		LWindow			*theDialog = theHandler.GetDialog();

		LGAEditField *usernameField = (LGAEditField*)theDialog->FindPaneByID('user');
		LGAEditField *passwordField = (LGAEditField*)theDialog->FindPaneByID('pass');

		ThrowIfNil_(usernameField);
		ThrowIfNil_(passwordField);
		
		usernameField->SelectAll();
		theDialog->SetLatentSub(usernameField);
		theDialog->Show();

		while (true) 
		{
			MessageT	hitMessage = theHandler.DoDialog();
			
			if (hitMessage == msg_Cancel) 
			{
				perr = eUserCancelled;
				break;
			} 
			else if (hitMessage == msg_OK) 
			{
				EditFieldToPref( usernameField, "li.login.name");
				EditFieldToPref( passwordField, "li.login.password");

				PREF_SetBoolPref("li.enabled", true);
				perr = eOK;
				break;
			}
			else if (hitMessage == 'adva')	// Advanced button
			{
				DoNetExtendedProfileDialog(theDialog);
			}
		}
	}
	catch (ExceptionCode err)
	{
		XP_ASSERT(false);
		perr = eUnknownError;
	}
	
	return perr;
}

// Creates a network profile folder
// The folder is located inside the users folder
ProfileErr 
CUserProfile::CreateNetProfile( FSSpec usersFolder, FSSpec& profileFolderSpec )
{	
	ProfileErr perr;
	perr = DoNetProfileDialog();
	
	if (perr == eOK)
	{
	try
	{
			CStr255 profileFolderName("Temporary Profile");
			OSErr err;
			
		// Create the folder spec of the profile directory
			profileFolderSpec = usersFolder;
			LString::CopyPStr(profileFolderName, profileFolderSpec.name, sizeof(profileFolderSpec.name));
			
		// If the folder already exists, delete it
			err = CFileMgr::DeleteFolder( profileFolderSpec );
			XP_ASSERT((err == noErr) || (err == fnfErr));

		// Create the folder
			short dummy; 
			long dummy2;
			err = CFileMgr::CreateFolderInFolder(usersFolder.vRefNum, usersFolder.parID, profileFolderName, 
							&dummy, &dummy2);
			ThrowIfOSErr_(err);
			ReflectToPreferences( CStr255("Network Profile"), profileFolderSpec);
		}
		catch (ExceptionCode err)
		{
			XP_ASSERT(false);
			return eUnknownError;
		}
	}

	return perr;
}

// ¥Êlaunches upgrade wizard for users who have not run 4.0 before
// creates an initial profile folder and User Profiles file.
// if oldNetscapeF is non-null, it points to the user's 3.0
// Netscape Ä folder and the profile "folder" is an alias to it
ProfileErr
CUserProfile::HandleUpgrade( FSSpec& profileFolder, const FSSpec* oldNetscapeF )
{
	ProfileErr		result = eOK;
	CStr31			profileName;
	
	do 
	{
		Try_
		{
			FSSpec profileSpec = CPrefs::GetFilePrototype( CPrefs::UsersFolder );
			
			if ( mHasConfigPlugin && !oldNetscapeF )
			{
				profileFolder.vRefNum = profileSpec.vRefNum;
				profileFolder.parID = profileSpec.parID;
				profileName = MAGIC_PROFILE_NAME;
				LString::CopyPStr( profileName, profileFolder.name, 32 );

				CreateDefaultProfileFolder( profileFolder );
				result = eRunAccountSetup;
			}
			else
			{
				UpgradeEnum upgrading = (oldNetscapeF == nil) ? eNewInstall : eExistingPrefs;
				result = NewUserProfile( profileSpec, profileFolder, profileName,
					 upgrading, oldNetscapeF );
			}
			if ( result == eUserCancelled )
				return eUserCancelled;
			
			if ( profileName.Length() == 0 )
				GetIndString( profileName, kProfileStrings, kDefaultName );

			// ¥Êcreate Profiles file and add the alias
			GetIndString( profileSpec.name, 300, userProfiles );
			CUserProfileDB profileDB( profileSpec, true );
			
			profileDB.AddNewProfile( 0, profileName, profileFolder );
			sCurrentProfileID = 0;
		}
		Catch_( inErr )
		{
			CStr255		errStr;
			GetIndString( errStr, kProfileStrings, kCreateError );
			ErrorManager::ErrorNotify( inErr, errStr );
			result = eUnknownError;
			// ¥Êloop through wizard again if error 
			// don't loop 5/14/97 tgm
			//done = false;
		}
	}
	while ( 0 /*!done*/);
	
	ReflectToPreferences(profileName, profileFolder);
	
	return result;
}

// ¥ puts up user-selection dialog and returns selected user ID
ProfileErr CUserProfile::HandleProfileDialog(
	FSSpec& profileSpec,
	CUserProfileDB& profileDB,
	FSSpec& profileFolder,
	short& newUserID,
	short lastUserID,
	Boolean	wantsProfileManager )
{
	int					dialogID = wantsProfileManager ?
		profileManagerDialog : profileSelectDialog;
	Boolean				success = false;
	LListBox*			listBox;
	LGAPushButton*		okButton;
	LPane*				newButton;
	LPane*				deleteButton;
	LPane*				renameButton;
	LPane*				optionsButton;
	
	ProfileErr			result = eOK;
		
	RegisterClass_( LWhiteListBox);
		
	StBlockingDialogHandler dialog( dialogID, CFrontApp::GetApplication() );
	
	listBox = (LListBox*)dialog.GetDialog()->FindPaneByID( 'user' );
	ThrowIfNil_( listBox );
	ListHandle listHand = listBox->GetMacListH();
	(**listHand).selFlags = lOnlyOne + lNoNilHilite;		// only one selection

	LAddColumn( 1, 0, listHand );
	PopulateListBox( listHand, profileDB, lastUserID );

	listBox->AddListener( &dialog );
	listBox->SwitchTarget( listBox );
	
	okButton = (LGAPushButton*)dialog.GetDialog()->FindPaneByID( 'ok  ' );
	deleteButton = dialog.GetDialog()->FindPaneByID( 2 );
	renameButton = dialog.GetDialog()->FindPaneByID( 3 );
	newButton = dialog.GetDialog()->FindPaneByID( 1 );
	optionsButton = dialog.GetDialog()->FindPaneByID( 'Ebut' );
	
	if ( wantsProfileManager )
		ThrowIfNil_( okButton && deleteButton && renameButton && newButton );
	else
		ThrowIfNil_( okButton );

	if ( !mHasConfigPlugin )
	{
		if ( optionsButton )
			optionsButton->Hide();
	}
	else
	{
		if ( !wantsProfileManager )
			SendMessageToPlugin( kInitListener, (LDialogBox*)dialog.GetDialog() );
	}
	 
	short tempUserID = -1;
	
	/* keep track of the amount of time we've been idly showing this dialog */
	long 	startTime 		= 	TickCount();
	long 	elapsedTime		=	0;
	long 	secsLeft		=	20;  //we can also get this from a resource in the User Profiles file if it exists.		
	Boolean	keepCounting		=	true; // (!wantsProfileManager); /* only countdown in simple profile picker. not manager*/
	const	ResIDT	autoStartResID = 9999;		
				
	// Check to see if our secret keep counting resource exists - if not, use default secsLeft
	StUseResFile resFile( profileDB.GetFile()->GetResourceForkRefNum() );
	StringHandle numSecsStringH = (StringHandle) ::Get1Resource('TEXT', autoStartResID);
	if (keepCounting && numSecsStringH)
	{
		char buffer[4];
		buffer[sizeof(buffer)-1] = 0;
		
		memcpy( (unsigned char *)buffer, (unsigned char *)*numSecsStringH, min(GetHandleSize((Handle)numSecsStringH),(long)sizeof(buffer)-1));
		secsLeft = atoi(buffer);
		if (secsLeft <= 0)
			keepCounting = false;			
	}
	
	
	while ( !success ) 
	{
		//Ê¥ catch any errors inside dialog loop
		Try_
		{
			Cell	cell;
			
			MessageT hit = dialog.DoDialog();
			
			// default to selecting the last selected profile if enough time has elapsed
			if (keepCounting && (secsLeft <= 0))
			{
				okButton->SimulateHotSpotClick(1);
				hit = cmd_SelectProfile;
			}
			else if (keepCounting)
			{
				// update secsLeft
				elapsedTime = TickCount() - startTime;
				if (elapsedTime >= 60)
				{
					secsLeft--;
					startTime = TickCount();
				}
			}
			else if (secsLeft >= 0)
				secsLeft = -1;
			
			if (listBox->GetLastSelectedCell( cell ) )
			{
				newUserID = cell.v;
				okButton->Enable();
				if (wantsProfileManager) {
					renameButton->Enable();
					if ( profileDB.CountProfiles() > 1 )
						// ¥Êdon't allow deleting last profile
						deleteButton->Enable();
					else
						deleteButton->Disable();
				}
				if ( optionsButton )
					optionsButton->Enable();
			}
			else
			{
				newUserID = -1;
				okButton->Disable();
				if (wantsProfileManager) {
					renameButton->Disable();
					deleteButton->Disable();
				}
				if ( optionsButton )
					optionsButton->Disable();
			}
			
			if ( newUserID != tempUserID )
			{
				if (tempUserID > -1)
					keepCounting = false;
				if ( newUserID != -1 && mHasConfigPlugin )
				{
					if ( profileDB.GetProfileAlias( newUserID, profileFolder, false ) )
						SendMessageToPlugin( kNewProfileSelect, &profileFolder );
					else
						SendMessageToPlugin( kClearProfileSelect, NULL );
				}
				tempUserID = newUserID;
			}
			
			switch ( hit )
			{
				case cmd_SelectProfile:
					success = profileDB.GetProfileAlias( newUserID, profileFolder );
					if ( success )
					{
						long		err;
						err = SendMessageToPlugin( kSelectDialConfig, &profileFolder );
						if ( err == errProfileNotFound )
							SendMessageToPlugin( kEditDialConfig, &profileFolder );
					}	
					keepCounting = false;

				break;
				
				case cmd_NewProfile:
					CStr31		profileName;
					result = NewUserProfile( profileSpec, profileFolder, profileName );
					
					if ( result == eUserCancelled )
					{
						result = eOK;
					}
					else
					{
						newUserID = profileDB.CountProfiles();
						profileDB.AddNewProfile( newUserID, profileName, profileFolder );
						
						// redraw the profile list box
						LDelRow(0, 0, listHand);
						PopulateListBox(listHand, profileDB, newUserID);
						listBox->Refresh();
						success = true;

						// ¥Êwe ran the Profile Wizard and now we should
						// open the Edit Settings dialog for the associated
						// dial configuration
						if ( result == eRunMUC )
						{
							SendMessageToPlugin( kEditDialConfig, &profileFolder ); 
							result = eOK;
						}

						// ¥ we ran the Profile Wizard, but we don't want to
						// associate a dialing configuration, so write out
						// an empty "Configuration" file by calling 
						// the plugin
						else if ( result == eSkipMUC )
						{
							SendMessageToPlugin( kAutoSelectDialConfig, &profileFolder );
							result = eOK;
						}
					}
					keepCounting = false;
				break;
					
				case cmd_RenameProfile:
					RenameProfile( newUserID, profileDB, cell, listHand );
					keepCounting = false;
				break;
					
				case cmd_DeleteProfile:
					DeleteProfile( newUserID, profileDB, listHand );
					listBox->Refresh();
					keepCounting = false;
				break;
					
				case cmd_QuitProfile:
					keepCounting = false;
					return eUserCancelled;
				break;	
					
				case cmd_EditDialSettings:
					if ( profileDB.GetProfileAlias( newUserID, profileFolder ) )
					{
						SendMessageToPlugin( kEditDialConfig, &profileFolder );
						tempUserID = -1;
					}
					keepCounting = false;
				break;	
			}
		}
		Catch_ ( inErr )
		{
			CStr255			errStr;
			GetIndString( errStr, kProfileStrings, kReadError );
			ErrorManager::ErrorNotify( inErr, errStr );
		}
	}
	
	return result;
}

Boolean CUserProfile::DeleteMagicProfile( FSSpec& inSpec )
{
	short		nextID = 0;
	CStr31		profileName;
	FSSpec		profileSpec;
	char*		fullURL;
	Boolean		found = FALSE;
	
	profileSpec.vRefNum = inSpec.vRefNum;
	profileSpec.parID = inSpec.parID;
	profileName = MAGIC_PROFILE_NAME;
	LString::CopyPStr( profileName, profileSpec.name, 32 );
	
	CUserProfileDB		db( inSpec );
	
	while ( db.GetProfileName( nextID++, profileName ) )
	{
		if ( profileName == MAGIC_PROFILE_NAME )
		{
			db.DeleteProfile( --nextID );
			if ( db.GetLastProfileID() == nextID )
				db.SetLastProfileID( 0 );
			found = TRUE;
			break;
		}
	}

	fullURL = CFileMgr::EncodedPathNameFromFSSpec( profileSpec, TRUE );
	if ( fullURL && found )
	{
		XP_RemoveDirectoryRecursive( fullURL, xpURL );
		XP_FREE( fullURL );
	}
	if ( found )
		return TRUE;
	return FALSE;
}

void CUserProfile::PopulateListBox(ListHandle& listHand, CUserProfileDB& profileDB, short defaultID)
{
	LSetDrawingMode(false, listHand);

	short nextID = 0;
	Cell cell;
	CStr31 str;
	Boolean gotOne = profileDB.GetProfileName(nextID, str);
	while (gotOne) {
		LAddRow(1, nextID, listHand);
		SetPt(&cell, 0, nextID);
		LSetCell(&str.fStr[1], str.fStr[0], cell, listHand);
		gotOne = profileDB.GetProfileName(++nextID, str);
	}
	// ¥Êselect last user or first cell & scroll to it
	SetPt(&cell, 0, defaultID);
	LSetSelect(true, cell, listHand);
	LAutoScroll(listHand);
	LSetDrawingMode(true, listHand);
}

// Ensure the requested new profile folder does not exist;
// if it does, append a number to make a unique name.
void CUserProfile::GetUniqueFolderName(FSSpec& folder)
{
	if (CFileMgr::FileExists(folder)) {
		int nextIndex = 2;
		CStr31 requestedName = folder.name;
		if (requestedName.Length() > 28)
			requestedName.Length() = 28;
		CStr31 uniqueName;
		do {
			uniqueName = requestedName;
			char suffix[3];
			sprintf(suffix, "-%d", nextIndex++);
			uniqueName += suffix;
			LString::CopyPStr(uniqueName, folder.name, 32);
		}
		while (CFileMgr::FileExists(folder));
	}
}


// Make one desktop icon
static const kIconNameStringsListID = 9800;

static OSErr MakeOneDesktopIcon(const CStr31 &profileName, short templateNameIndex, short iconNameIndex)
{
	OSErr		err = noErr;
	FSSpec 		shortcutSpec = CPrefs::GetFilePrototype(CPrefs::RequiredGutsFolder);
	FSSpec		desktopFolderSpec;
	FSSpec		desktopIconSpec;
	short		vRefNum;
	long		folderID;
	
	GetIndString(shortcutSpec.name, kIconNameStringsListID, templateNameIndex);
	
	//find the source file
	if (!CFileMgr::FileExists(shortcutSpec) || CFileMgr::IsFolder(shortcutSpec))
		ThrowIfOSErr_(fnfErr);
	
	//check the destination
	err = ::FindFolder(kOnSystemDisk, kDesktopFolderType, true,
						&vRefNum, &folderID);
	ThrowIfOSErr_(err);
	
	err = CFileMgr::FolderSpecFromFolderID(vRefNum, folderID, desktopFolderSpec);
	ThrowIfOSErr_(err);
	
	desktopIconSpec.parID = folderID;
	desktopIconSpec.vRefNum = vRefNum;
	GetIndString(desktopIconSpec.name, kIconNameStringsListID, iconNameIndex);
	
	//Append the profile name. We know that it does not contain colons
	CStr63		iconName(desktopIconSpec.name);

	iconName += profileName;
	
	//Truncate the iconName if necessary to 31 chars
	if (iconName.Length() > 30) {
		iconName[0] = 30;
		iconName[30] = 'É';
	}
	
	LString::CopyPStr(iconName, desktopIconSpec.name, 31);
	
	//If there is already an icon with the same name, append the profile name
	long	nextIndex = 1;
	
	while (CFileMgr::FileExists(desktopIconSpec))
	{
		CStr63 	uniqueName(iconName);
		char 	suffix[5];
		short	len;
		
		sprintf(suffix, "-%d", nextIndex++);
		
		len = 30 - strlen(suffix);
		//Truncate the uniqueName if necessary
		if (uniqueName.Length() > len) {
			uniqueName[0] = len;
			uniqueName[len] = 'É';
		}
		
		uniqueName += suffix;
		
		LString::CopyPStr(uniqueName, desktopIconSpec.name, 31);
	}
	
	//phew. now we have a unique name to copy to
	err = CFileMgr::CopyFile(shortcutSpec, desktopFolderSpec, desktopIconSpec.name);
	ThrowIfOSErr_(err);
	
	//make sure the custom icon bit is set
	err = CFileMgr::SetFileFinderFlag(desktopIconSpec, kHasCustomIcon, TRUE);
	ThrowIfOSErr_(err);
	
	//And now copy the profile name into the data fork
	if (! CFileMgr::FileExists(desktopIconSpec))		//if it does not, we're in trouble
		ThrowIfOSErr_(fnfErr);
	
	LFile	scriptFile(desktopIconSpec);

	scriptFile.OpenDataFork(fsRdWrPerm);
	scriptFile.WriteDataFork((const char*)profileName, profileName.Length());
	scriptFile.CloseDataFork();
	
	return err;
}



// Make the desktop icons for the selected profile.
// They are acually applescripts copied from the essential files
// folder, with a property modified for this profile.

OSErr CUserProfile::MakeDesktopIcons(
		const CStr31	&profileName,
		const Boolean	wantsNavigator,
		const Boolean	wantsInbox )
{
	OSErr		err = noErr;
	
	enum {	kNavShortcutName = 1, kInboxShortcutName,
			kNavDesktopName, kInboxDesktopName};			//in profile.cnst

	if (wantsNavigator)
		err = MakeOneDesktopIcon(profileName, kNavShortcutName, kNavDesktopName);
	
	if (wantsInbox)
		err = MakeOneDesktopIcon(profileName, kInboxShortcutName, kInboxDesktopName);
	
	return err;
}


// ¥ prompts for a new user profile
ProfileErr CUserProfile::NewUserProfile(
	const FSSpec& profileSpec,
	FSSpec& profileFolder,
	CStr31& profileName,
	UpgradeEnum upgrading,
	const FSSpec* oldNetscapeF )
{
	Boolean				userChoseFolder = false;
	ProfileErr			result = eOK;
	
	profileFolder.vRefNum = profileSpec.vRefNum;
	profileFolder.parID = profileSpec.parID;

	result = NewProfileWizard( upgrading, profileName, profileSpec, 
		profileFolder, userChoseFolder );
				
	if ( result == eUserCancelled )
		return eUserCancelled;

	// ¥ If the user chose a folder, see if it contains an existing
	// Preferences file.  If so, this folder becomes the new profile
	// folder; otherwise, create a new folder inside it.
	if ( userChoseFolder ) {
		long parentID;
		ThrowIfOSErr_( CFileMgr::GetFolderID(profileFolder, parentID) );
		
		FSSpec prefFile;
		prefFile.vRefNum = profileFolder.vRefNum;
		prefFile.parID = parentID;
		::GetIndString( prefFile.name, 300, prefFileName );
		if (! CFileMgr::FileExists(prefFile)) {
			// want to create a new folder inside the selected one
			userChoseFolder = false;
			profileFolder.parID = parentID;
		}
	}

	// ¥Êcreate a folder and return it
 	if ( !userChoseFolder )
	{
		LString::CopyPStr( profileName, profileFolder.name, 32 );
		GetUniqueFolderName(profileFolder);
		
		if ( oldNetscapeF )
		{
			// ¥Êspecial case: point profile to existing 3.0 Netscape Ä
			CFileMgr::MakeAliasFile( profileFolder, *oldNetscapeF );

			profileFolder = *oldNetscapeF;			
		}
		else
		{	
			CreateDefaultProfileFolder(profileFolder);
		}
	}
	
	return result;
}

ProfileErr CUserProfile::NewProfileWizard(
	UpgradeEnum 	upgrading,
	CStr31			&profileName, 
	const FSSpec	&profileFolder,
	FSSpec			&newProfileFolder,
	Boolean			&userChoseFolder )
{
	// ¥ if we're upgrading 3.0 prefs, we don't allow the user to change
	// the profile folder; just display & disable the default folder.
	// Otherwise, the callback fills in the user name as the profile
	// folder and lets the user edit it. (--this needs work)
	LArray			paneList( sizeof( PaneIDT ) );


#ifdef CAN_MAKE_DESKTOP_ICONS_FOR_PROFILE
	PaneIDT			normalPaneList[] = { profileIntroPane, userNamePane, profileNamePane,
									profileFolderPane, profileIconsPane, profileDonePane, 0 };
	PaneIDT			mucPaneList[] = { profileIntroPane, profilePEPane, userNamePane, profileNamePane,
									profileFolderPane, profileIconsPane, profileDonePane, 0 }; 
#else
	PaneIDT			normalPaneList[] = { profileIntroPane, userNamePane, profileNamePane,
									profileFolderPane, profileDonePane, 0 };
	PaneIDT			mucPaneList[] = { profileIntroPane, profilePEPane, userNamePane, profileNamePane,
									profileFolderPane, profileDonePane, 0 }; 
#endif

	PaneIDT*		tmpP;
	
	if ( !mHasConfigPlugin )
		tmpP = normalPaneList;
	else
		tmpP = mucPaneList;

	while ( *tmpP != 0 )
		paneList.InsertItemsAt( 1, LArray::index_Last, tmpP++ );

	CDialogWizardHandler		wizard( updateWizardDialog, paneList );

	LListener* listener = new CProfilePaneMonitor( &wizard );
	wizard.AddListener( listener );
	
	LWindow* window = wizard.GetDialog();	
	if ( upgrading == eExistingPrefs )
	{
		wizard.SetEditText( 'name', CPrefs::GetString( CPrefs::UserName ) );
		wizard.SetEditText( 'mail', CPrefs::GetString( CPrefs::UserEmail ) );
	}
	if ( upgrading != eNoUpgrade )
	{		
		LStdControl* cancel = (LStdControl*)window->FindPaneByID( 'cncl' );	
		if (cancel)
		{
			CStr31 label;
			GetIndString( label, kProfileStrings, kQuitLabel );
			cancel->SetDescriptor( label );
		}		
	}

	// ¥ hide "detected previous Navigator version" text if appropriate
	if ( upgrading != eExistingPrefs || CPrefs::sPrefFileVersion != 3 )
	{
		LPane* upgradeText = window->FindPaneByID( 40 );
		if ( upgradeText )
			upgradeText->Hide();
		upgradeText = window->FindPaneByID( 41 );
		if ( upgradeText )
			upgradeText->Hide();
	}

	FSSpec		tempFolder;
	tempFolder.vRefNum = profileFolder.vRefNum;
	tempFolder.parID = profileFolder.parID;
	LString::CopyPStr( "\p", tempFolder.name, 32 );
	
	CFilePicker* picker = (CFilePicker*)window->FindPaneByID( 'fold' );
	ThrowIfNil_( picker );
	picker->SetPickType( CFilePicker::Folders );
	picker->SetFSSpec( tempFolder, false );
	// ¥ don't allow different folder when upgrading
	if ( upgrading == eExistingPrefs ) {
		LPane* chooseBtn = picker->FindPaneByID(2);
		LPane* chooseText = window->FindPaneByID(31);
		if (chooseBtn && chooseText) {
			chooseBtn->Hide();
			chooseText->Hide();
		}
	}
	
	// show the profile wizard
	if ( wizard.DoWizard() == false )
	{
		// ¥Êuser cancelled
		delete listener;
		return eUserCancelled;
	}
	
	// copy user name & email to prefs; return profile name
	CStr255	userName;
	wizard.GetEditText( 'name', userName );
	CPrefs::SetString( userName, CPrefs::UserName );
	CStr255	emailAddr;
	wizard.GetEditText( 'mail', emailAddr );
	CPrefs::SetString( emailAddr, CPrefs::UserEmail );
	
	wizard.GetEditText( 'pnam', profileName );
	StripColons(profileName);
	
#ifdef CAN_MAKE_DESKTOP_ICONS_FOR_PROFILE
	// make the desktop icons
	OSErr err = MakeDesktopIcons(profileName, wizard.GetCheckboxValue('navb'), wizard.GetCheckboxValue('inbb'));
	ThrowIfOSErr_(err);		//should we do this?
#endif // CAN_MAKE_DESKTOP_ICONS_FOR_PROFILE
	
	if ( picker->WasSet() )
	{
		userChoseFolder = true;
		CFileMgr::CopyFSSpec( picker->GetFSSpec(), newProfileFolder );
	}
	
	delete listener;

	if ( mHasConfigPlugin )
	{
		LControl*	radioNew = (LControl*)window->FindPaneByID( 'Rnew' );
		LControl*	radioExs = (LControl*)window->FindPaneByID( 'Rexs' );

		if ( ( radioNew && radioNew->GetValue() ) || upgrading != eNoUpgrade )
			return eRunAccountSetup;
		else if ( radioExs && radioExs->GetValue() /* ( upgrading != eNoUpgrade ) */)
			return eRunMUC;
		return eSkipMUC;
	}
	
	return eOK;
}

void CUserProfile::RenameProfile(short selectedID, CUserProfileDB& profileDB,
	Cell& cell, ListHandle& listHand)
{
	CStr31 newName;
	if (profileDB.GetProfileName(selectedID, newName) == false)
		return;

	CStr255 prompt;
	GetIndString(prompt, kProfileStrings, kRenamePrompt);

	Boolean ok = UStdDialogs::AskStandardTextPrompt(nil, prompt, newName,
		NULL, NULL, kStr31Len);

	if (ok && newName.Length() > 0) {
		LSetCell(&newName.fStr[1], newName.Length(), cell, listHand);

		profileDB.SetProfileName(selectedID, newName);
	}
}

void CUserProfile::DeleteProfile(short selectedID, CUserProfileDB& profileDB, ListHandle& listHand)
{
	CStr255 prompt;
	GetIndString(prompt, kProfileStrings, kDeletePrompt);
	if (ErrorManager::PlainConfirm(prompt))
	{
		profileDB.DeleteProfile(selectedID);

		if (--selectedID < 0)
			selectedID = 0;
		
		// redraw the profile list box
		LDelRow(0, 0, listHand);

		PopulateListBox(listHand, profileDB, selectedID);
	}
}

// ¥ Reflects profile name & path into xp preferences for querying by PE;
// also registers a callback so changing the name renames the profile.
// if we don't know how many profiles we have, pass -1 and the numprofiles
// pref will not be set (nasty hack: see CPrefs::InitPrefsFolder())
void
CUserProfile::ReflectToPreferences(const CStr31& profileName,
	const FSSpec& profileFolder, short numProfiles)
{
	char* folderPath = CFileMgr::EncodedPathNameFromFSSpec( profileFolder, true );
	if ( folderPath )
	{
		PREF_SetDefaultCharPref( "profile.directory", folderPath );
		free( folderPath );
	}
	
	PREF_SetDefaultCharPref( "profile.name", profileName );
	
	if (numProfiles > -1)
		PREF_SetDefaultIntPref( "profile.numprofiles", numProfiles );
	
	PREF_RegisterCallback( "profile.name", ProfilePrefChangedFunc, NULL );
}

// ¥ If a Defaults folder exists in Essential Files, copy it into
// Netscape Users and rename it to the selected profile name.
// Otherwise, or in case of a copying error, create a new folder.
void CUserProfile::CreateDefaultProfileFolder(const FSSpec& profileFolder)
{
	OSErr err = noErr;
	Boolean needToCreate = true;
	
	FSSpec usersFolder;
	CFileMgr::FolderSpecFromFolderID(profileFolder.vRefNum,
		profileFolder.parID, usersFolder);
	
	FSSpec templateFolder = CPrefs::GetFilePrototype(CPrefs::RequiredGutsFolder);
	GetIndString( templateFolder.name, 300, profileTemplateDir );

	if (CFileMgr::FileExists(templateFolder))
	{
		err = FSpDirectoryCopy( &templateFolder, &usersFolder,
			nil, 0, true, nil );
		
		if (err == noErr) {
			needToCreate = false;
			
			err = HRename(profileFolder.vRefNum, profileFolder.parID,
				templateFolder.name, profileFolder.name);
		}
	}
	
	if (needToCreate) {
		short		newRefNum;
		long		newParID;
		err = CFileMgr::CreateFolderInFolder(
						profileFolder.vRefNum, profileFolder.parID,
						profileFolder.name, &newRefNum, &newParID );
	}
	
	ThrowIfOSErr_( err ); 
}

// ¥Êfind the network configuration plugin and return a function ptr.
// to it's only entry point if possible
void* CUserProfile::LoadConfigPlugin()
{
		// BULLSHIT ALERT: Get out if I can't call GetSharedLibrary.
		//	Future: do the right thing for 68K.  See bug#56245
	long sSysArchitecture = 0;
	if ( (Gestalt(gestaltSysArchitecture, &sSysArchitecture) != noErr)
		|| (sSysArchitecture != gestaltPowerPC) )
		{
				// Can't determine what we _do_ have, or we determined that it's _not_ PPC...
			return NULL;
		}

	Ptr					main;
	Str255				errName;
	PE_PluginFuncType	pluginFunc;

	OSErr err = ::GetSharedLibrary( "\pMUP", kPowerPCCFragArch, kFindCFrag, &mConfigPluginID,
			&main, errName );
	if ( err != noErr )
	{ 
		err = ::GetSharedLibrary( "\pMUP", kPowerPCCFragArch, kLoadCFrag, &mConfigPluginID,
				&main, errName );
	}
	if ( err == noErr )
	{	
		CFragSymbolClass	dontCare;
		mPluginLoaded = TRUE;
		err = ::FindSymbol( mConfigPluginID, "\pPE_PluginFunc", (Ptr*)&pluginFunc, &dontCare );
	}
	
	if ( err == noErr )
		return pluginFunc;
	return NULL;
}

// ¥Êclose the connection to the configuration plugin
OSErr CUserProfile::CloseConfigPlugin()
{
//	if ( mPluginLoaded )
//		return CloseConnection( &mConfigPluginID );
	return noErr;
}

long CUserProfile::SendMessageToPlugin( long selector, void* pb )
{
	OSErr				err;
	PE_PluginFuncType	pluginFunc;
	
	if ( !mHasConfigPlugin )
		return -1;
		
	pluginFunc = (PE_PluginFuncType)LoadConfigPlugin();			
	if ( pluginFunc )
	{
		union {
		char				buffer[ 1024 ];
		MUCInfo				mucInfo;
		} tmp;
		
		// ¥Êcall the stub to have it switch all the local machine
		//		stuff to use this user's configuration
		err = (*pluginFunc)( selector, pb, tmp.buffer );
		CloseConfigPlugin();

//		if ( err == noErr && selector == kGetDialConfig )
//		{
//			cstring		descString;
//			PrettyPrintConfiguration( mucInfo, descString );
//			inCaption->SetDescriptor( (CStr255)(char*)descString );
//		}
		return noErr;
	}
	return noErr;
}

/*****************************************************************************
*/
CUserProfileDB::CUserProfileDB( FSSpec& spec, Boolean createIt ): fFile( spec )
{
	if ( createIt )
	{
		Try_
		{
			fFile.CreateNewFile( emSignature, 'PRFL' );		// ?? file type?
		}
		Catch_( inErr )
		{
			if ( inErr == dupFNErr )
				;
			else
				Throw_( inErr );
		}
	}
	fFile.OpenResourceFork( fsRdWrPerm );
}

short CUserProfileDB::CountProfiles()
{
	return ::Count1Resources('STR ');
}

short CUserProfileDB::GetNextProfileID()
{
	short nextUserID = CountProfiles();
	
	return nextUserID;
}

short CUserProfileDB::GetProfileIDByUsername(const CString& username)
{
	int					id = kFirstProfileID, returnID = -1;
	Handle				hProfileData = GetDBResource('DATA', id);
	ProfileDataHeader	*pHeader;	
	char				*profileUser;
				
	while (hProfileData && (returnID == -1)) {
		HLock(hProfileData);
		pHeader = (ProfileDataHeader *) *hProfileData;
			
		profileUser = ((char *) pHeader) + pHeader->firstOffset;
		
		if (username == profileUser) {
			returnID = id - kFirstProfileID;
		}
		
		HUnlock(hProfileData);

		hProfileData = GetDBResource('DATA', ++id);
		
	}

	return returnID;
}

short CUserProfileDB::GetProfileIDByEmail(const CString& emailAddr)
{
	int					id = kFirstProfileID, returnID = -1;
	Handle				hProfileData = GetDBResource('DATA', id);
	ProfileDataHeader	*pHeader;	
	char				*profileEmail;
				
	while (hProfileData && (returnID == -1)) {
		HLock(hProfileData);
		pHeader = (ProfileDataHeader *) *hProfileData;
			
		profileEmail = ((char *) pHeader) + pHeader->firstOffset
			+ pHeader->nameLength;
		
		if (emailAddr == profileEmail) {
			returnID = id - kFirstProfileID;
		}
		
		HUnlock(hProfileData);

		hProfileData = GetDBResource('DATA', ++id);
		
	}

	return returnID;
}

short CUserProfileDB::GetLastProfileID()
{
	short lastUserID = kFirstProfileID;
	short nextUserID = kFirstProfileID + GetNextProfileID();

	// ID of the previous user is stored as a resource
	Handle lastUser = GetDBResource('user', kFirstProfileID);
	if (lastUser) {
		lastUserID = **(short **) lastUser;
		if (lastUserID >= nextUserID)
			lastUserID = kFirstProfileID;
		::ReleaseResource(lastUser);
	}
	
	return lastUserID - kFirstProfileID;
}

void CUserProfileDB::SetLastProfileID(short newUserID)
{
	newUserID += kFirstProfileID;
	Handle lastUser = GetDBResource('user', kFirstProfileID);
	if (!lastUser) {
		PtrToHand(&newUserID, &lastUser, sizeof(short));
		::AddResource(lastUser, 'user', kFirstProfileID, nil);
	}
	else {
		BlockMove(&newUserID, *lastUser, sizeof(short));
		::ChangedResource(lastUser);
		::ReleaseResource(lastUser);
	}
}

void CUserProfileDB::AddNewProfile(short id, const CStr31& profileName, const FSSpec& profileFolder)
{
	id += kFirstProfileID;
	// create new STR, alis, and DATA resources
	StringHandle nameHand;
	PtrToHand(profileName, &(Handle) nameHand, profileName.Length() + 1);
	AddResource((Handle) nameHand, 'STR ', id, nil);

	AliasHandle alias;
	OSErr err = NewAlias( nil, &profileFolder, &alias );
	if (err == noErr) {
		AddResource((Handle) alias, 'alis', id, profileFolder.name);
	}

	SetProfileData(id - kFirstProfileID);
}

Boolean CUserProfileDB::GetProfileName(short id, CStr31& name)
{
	// -- use Get1Resource instead of GetString to avoid grabbing 
	// spurious strings from other programs (e.g. Kaleidoscope)
	StringHandle strHand = (StringHandle) GetDBResource('STR ', id + kFirstProfileID);
	if (strHand == nil)
		return false;

	HLock((Handle) strHand);
	int len = *strHand[0] + 1;
	if (len > 32) len = 32;
	BlockMove(*strHand, name.fStr, len);
	name.Length() = len - 1;
	HUnlock((Handle) strHand);
	
	ReleaseResource((Handle) strHand);
	return true;
}

void CUserProfileDB::SetProfileName(short id, const CStr31& name)
{
	StringHandle strHand = (StringHandle) GetDBResource('STR ', id + kFirstProfileID);
	if (strHand) {
		SetString(strHand, name);
		ChangedResource((Handle) strHand);
		ReleaseResource((Handle) strHand);
	}
}

void CUserProfileDB::SetProfileData(short id)
{
	Handle				hProfileData;
	ProfileDataHeader	profileHeader;
	short int			dataLength;
	long int			dataOffset;
	CStr255				userName;
	CStr255				emailName;
	XP_Bool				addResource = false;

	id += kFirstProfileID;		// In order to allow this method to be called from
								// outside the class, we have to allow the value in to
								// be a "raw" profile number (i.e., as returned by
								// GetLastProfile);
	
	emailName = CPrefs::GetString(CPrefs::UserEmail);
	userName = CPrefs::GetString(CPrefs::UserName);
	
	profileHeader.count = 2;
	profileHeader.firstOffset = sizeof(ProfileDataHeader);

	/* (the +1 is for the  null terminator for each string) */
	profileHeader.nameLength = userName.Length() + 1;
	profileHeader.emailLength = emailName.Length() + 1;
	
	/* First, compute the length of the resource */
	dataLength = sizeof(ProfileDataHeader) + profileHeader.nameLength + profileHeader.emailLength;
	
	/* See if we're replacing or adding */
	hProfileData = GetDBResource('DATA', id);
	
	if (hProfileData) {
		SetHandleSize(hProfileData, dataLength);
		if (MemError() == noErr) {
			XP_MEMSET(*hProfileData, '\0', dataLength);
		} else {
			ReleaseResource(hProfileData);
			hProfileData = nil;
		}
	} else {
		hProfileData = NewHandleClear(dataLength);
		addResource = true;
	}
	
	if (hProfileData) {
		dataOffset = 0;
		
		HLock((Handle) hProfileData);
		
		BlockMove(&profileHeader, *hProfileData, sizeof(ProfileDataHeader));
		
		dataOffset = sizeof(ProfileDataHeader);
		BlockMove(&(userName.fStr[1]), ((unsigned char *) *hProfileData) + dataOffset,	
			profileHeader.nameLength-1);
		
		dataOffset += profileHeader.nameLength;
		BlockMove(&(emailName.fStr[1]), ((unsigned char *) *hProfileData) + dataOffset,	
			profileHeader.emailLength-1);

		HUnlock((Handle) hProfileData);

		if (addResource) {
			::AddResource(hProfileData, 'DATA', id, nil);
		} else {
			::ChangedResource(hProfileData);
			::ReleaseResource(hProfileData);
		}
	}	
}

Boolean CUserProfileDB::GetProfileAlias(short id, FSSpec& profileFolder, Boolean allowUserInteraction )
{
	Boolean success = true;
	AliasHandle a;
	a = (AliasHandle) GetDBResource('alis', id + kFirstProfileID);
	ThrowIfNil_(a);
	
	Boolean		changed;
	OSErr		err;
	
	if ( allowUserInteraction )
	{
		err = ResolveAlias( NULL, a, &profileFolder, &changed );

		// If the alias couldn't be resolved, give the user 
		// a chance to locate the profile folder
		if (err < 0) {
			success = false;
			CStr255 errStr;
			CStr31	profileName;
			GetIndString(errStr, CUserProfile::kProfileStrings, CUserProfile::kBadAliasError);
			GetProfileName(id, profileName);
			StringParamText(errStr, profileName);
			
			if (ErrorManager::PlainConfirm(errStr))
			{
				StandardFileReply reply;
				reply.sfFile = CPrefs::GetFilePrototype(CPrefs::UsersFolder);
				if ( CFilePicker::DoCustomGetFile(reply, CFilePicker::Folders, true) )
				{
					CFileMgr::CopyFSSpec(reply.sfFile, profileFolder);
					Boolean changed;
					err = UpdateAlias(nil, &reply.sfFile, a, &changed);
					ThrowIfOSErr_(err);
					ChangedResource((Handle) a);
					success = true;
				}
			}
		}
	}
	else
	{
		short	aliasCount = 1;
		
		err = MatchAlias( NULL, kARMMountVol | kARMNoUI | kARMMultVols | kARMSearch | kARMSearchMore,
			a, &aliasCount, &profileFolder, &changed, NULL, NULL );
		if ( err < 0 )
			success = false;
	}

	ReleaseResource((Handle) a);
	
	return success;
}

// This will change the value of selectedID if it becomes out of bounds
void CUserProfileDB::DeleteProfile(short selectedID)
{
	selectedID += kFirstProfileID;
	// delete profile resources and move the subsequent 
	// resources down to fill the space
	Handle toDelete = GetDBResource('STR ', selectedID);
	if (toDelete)
		RemoveResource(toDelete);
	toDelete = GetDBResource('alis', selectedID);
	if (toDelete)
		RemoveResource(toDelete);
	
	toDelete = GetDBResource('DATA', selectedID);
	if (toDelete)
		RemoveResource(toDelete);
	
	int id = selectedID + 1;
	Handle next = GetDBResource('STR ', id);
	while (next) {
		::SetResInfo(next, id - 1, nil);

		next = GetDBResource('DATA', id);
		if (next) {
			::SetResInfo(next, id - 1, nil);
			ReleaseResource(next);
		}
		
		next = GetDBResource('alis', id);
		if (next) {
			::SetResInfo(next, id - 1, nil);
			ReleaseResource(next);
		}
		// don't need to release strings because Populate uses them
		next = GetDBResource('STR ', ++id);
	}
}

Handle CUserProfileDB::GetDBResource(ResType theType, short theID)
{
	StUseResFile resFile( fFile.GetResourceForkRefNum() );
	
	return ::Get1Resource(theType, theID);
}


/*****************************************************************************
*/
const int cmd_NextPane = 8000;
const int cmd_PrevPane = 8001;

CProfilePaneMonitor::CProfilePaneMonitor( CDialogWizardHandler* wizard )
{
	fWizard = wizard;
	fCopiedName = false;
}

// !! This needs some work.
void CProfilePaneMonitor::ListenToMessage( MessageT inMessage, void* /* ioParam */)
{
	PaneIDT			nameField;
	Boolean			onNameField = false;
	LView*			window;
	
	window = fWizard->GetDialog();

	LStdControl*	next = (LStdControl*)window->FindPaneByID( 'ok  ' );
	LStdControl*	back = (LStdControl*)window->FindPaneByID( 'back' );
	CStr255			buttonTitle;
	
	if (fWizard->CurrentPane() == userNamePane) {
		nameField = 'name';
		onNameField = true;
	}
	else if (fWizard->CurrentPane() == profileNamePane) {
		nameField = 'pnam';
		onNameField = true;
	}

	if ( next && ( inMessage == cmd_NextPane || inMessage == cmd_PrevPane ) )
	{
		if ( fWizard->CurrentPaneNumber() == fWizard->TotalPanes() )
		{
			LControl*	radioNew = (LControl*)window->FindPaneByID( 'Rnew' );
			LControl*	radioExs = (LControl*)window->FindPaneByID( 'Rexs' );
			LPane*		vnorm = (LView*)window->FindPaneByID('Vnor');
			LPane*		vexs = (LView*)window->FindPaneByID('Vexs');
			LPane*		vnew = (LView*)window->FindPaneByID('Vnew');
			
			vnorm->Hide();
			vexs->Hide();
			vnew->Hide();
			if ( radioExs && radioExs->GetValue() )
			{
				GetIndString( buttonTitle, CUserProfile::kProfileStrings, CUserProfile::kCreateProfileLabel );
				vexs->Show();
			}
			else if ( radioNew && radioNew->GetValue() )
			{
				//GetIndString( buttonTitle, CUserProfile::kProfileStrings, CUserProfile::kDoneLabel );
				GetIndString( buttonTitle, CUserProfile::kProfileStrings, CUserProfile::kRunASLabel );
				vnew->Show();
			}
			else
			{
				GetIndString( buttonTitle, CUserProfile::kProfileStrings, CUserProfile::kDoneLabel );
				vnorm->Show();
			}
			next->SetDescriptor( buttonTitle );
		}
		else
		{
			GetIndString( buttonTitle, CUserProfile::kProfileStrings, CUserProfile::kNextLabel );
			next->SetDescriptor( buttonTitle );
		}
	}

	if ( back && ( inMessage == cmd_NextPane || inMessage == cmd_PrevPane ) )
	{
		if ( fWizard->CurrentPaneNumber() == LArray::index_First )
			back->Disable();
		else
			back->Enable();
	}


	// copy profile name to folder name, unless the user has changed
	// the path or we're creating the default profile
	if ( inMessage == cmd_NextPane && fWizard->CurrentPane() == profileFolderPane )
	{
		CStr31 profileName;
		fWizard->GetEditText('pnam', profileName);
		StripColons(profileName);
		CFilePicker* picker = (CFilePicker*) fWizard->GetDialog()->FindPaneByID('fold');
		if (picker && !picker->WasSet()) {
			FSSpec folder = picker->GetFSSpec();
			LString::CopyPStr(profileName, folder.name, 32);
			
			CUserProfile::GetUniqueFolderName(folder);
			picker->SetFSSpec(folder, false);
		}
	}
	// copy user name to profile name (only do this once)
	else if ( inMessage == cmd_NextPane && fWizard->CurrentPane() == profileNamePane 
			  && !fCopiedName)
	{
		CStr31 profileName;
		fWizard->GetEditText('name', profileName);
		StripColons(profileName);
		if (profileName.Length() > kStr31Len)
			profileName.Length() = kStr31Len;
		fWizard->SetEditText('pnam', profileName);
		fCopiedName = true;
	}

	// select Name field
	if (onNameField && (inMessage == cmd_NextPane || inMessage == cmd_PrevPane)) {
		LEditField* field = (LEditField*) fWizard->GetDialog()->FindPaneByID(nameField);
		if (field) {
			field->SwitchTarget(field);
			field->SelectAll();
		}
	}

	// disable Next button if user hasn't entered name
	if (onNameField)
	{
		CStr255 text;
		fWizard->GetEditText( nameField, text );
		if ( text.Length() == 0 )
			fWizard->DisableNextButton();
		else
			fWizard->EnableNextButton();
	}
	else
		fWizard->EnableNextButton();
}

/*****************************************************************************
*/

CDialogWizardHandler::CDialogWizardHandler( ResIDT dlogID, LArray& paneList ):
	fDialog( dlogID, CFrontApp::GetApplication() ), fPaneList( paneList )
{
	ArrayIndexT		index;
	PaneIDT			paneID;
	LView*			pane;
	
	index = LArray::index_First;

	fCurrentPane = LArray::index_First;
	fListener = nil;
	
	LWindow* window = fDialog.GetDialog();
	ThrowIfNil_( window );

	for ( index = LArray::index_First; index <= fPaneList.GetCount(); index++ )
	{
		fPaneList.FetchItemAt( index, &paneID );
		LView::SetDefaultView( window );
		if ( GetResource( 'PPob', paneID ) == nil )
			break;
		pane = (LView*)UReanimator::ReadObjects( 'PPob', paneID );
		if ( pane )
		{
			if ( index != LArray::index_First )
				pane->Hide();
			pane->Enable();
			pane->FinishCreate();
		}
	}
}

PaneIDT CDialogWizardHandler::CurrentPane()
{
	PaneIDT		paneID;
	fPaneList.FetchItemAt( fCurrentPane, &paneID );
	return paneID;
}

ArrayIndexT CDialogWizardHandler::CurrentPaneNumber()
{
	return fCurrentPane;
}

ArrayIndexT CDialogWizardHandler::TotalPanes()
{
	return fPaneList.GetCount();
}

void CDialogWizardHandler::AddListener(LListener* st)
{
	fListener = st;
}

Boolean CDialogWizardHandler::DoWizard()
{
	Boolean cancelled = false;
	LWindow* window = fDialog.GetDialog();
	ShowPane( fCurrentPane, window );
	
	Boolean done = false;
	while ( !done )
	{
		MessageT hit = fDialog.DoDialog();
			
		switch ( hit )
		{
			case cmd_NextPane:
				done = ShowPane( fCurrentPane + 1, window );
			break;
			
			case cmd_PrevPane:
				done = ShowPane( fCurrentPane - 1, window );
			break;
			
			case msg_Cancel:
				cancelled = done = true;
			break;
		}
		if ( !done && fListener )
			fListener->ListenToMessage( hit, nil );
	}
	return !cancelled;
}

Boolean CDialogWizardHandler::ShowPane( ArrayIndexT paneNum, LWindow* window )
{
	PaneIDT		paneID;
	
	if ( paneNum < LArray::index_First )
		return false;
	
	if ( paneNum != fCurrentPane )
	{
		fPaneList.FetchItemAt( fCurrentPane, &paneID );
		LPane* pane = window->FindPaneByID( paneID );
		if ( pane )
			pane->Hide();
	}
		
	fCurrentPane = paneNum;
	if ( paneNum > fPaneList.GetCount() )
		return true;
	fPaneList.FetchItemAt( paneNum, &paneID );
	LPane* pane = window->FindPaneByID( paneID );
	if ( pane )
		pane->Show();

	return false;
}

void CDialogWizardHandler::EnableNextButton()
{
	LWindow*	window = fDialog.GetDialog();
	if ( window )
	{
		LStdControl* next = (LStdControl*)window->FindPaneByID( 'ok  ' );
		if ( next )
			next->Enable();
	}
}
	
void CDialogWizardHandler::DisableNextButton()
{
	LWindow*	window = fDialog.GetDialog();
	if ( window )
	{
		LStdControl* next = (LStdControl*)window->FindPaneByID( 'ok  ' );
		if ( next )
			next->Disable();
	}
}

LWindow* CDialogWizardHandler::GetDialog()
{
	return fDialog.GetDialog();
}

void CDialogWizardHandler::SetEditText(PaneIDT paneID, const CString& text)
{
	LWindow* window = fDialog.GetDialog();
	if (window) {
		LEditField* field = (LEditField*) window->FindPaneByID(paneID);
		if (field) {
			field->SetDescriptor(text);
		}
	}
}

void CDialogWizardHandler::GetEditText(PaneIDT paneID, CString& text)
{
	LWindow* window = fDialog.GetDialog();
	if (window) {
		LEditField* field = (LEditField*) window->FindPaneByID(paneID);
		if (field) {
			field->GetDescriptor(text);
		}
	}
}

void CDialogWizardHandler::SetCheckboxValue(PaneIDT paneID, const Boolean value)
{
	LWindow* window = fDialog.GetDialog();
	if (window) {
		LControl* checkbox = (LControl*) window->FindPaneByID(paneID);
		if (checkbox) {
			checkbox->SetValue((Int32)value);
		}
	}
}

Boolean CDialogWizardHandler::GetCheckboxValue(PaneIDT paneID)
{
	LWindow* window = fDialog.GetDialog();
	if (window) {
		LControl* checkbox = (LControl*) window->FindPaneByID(paneID);
		if (checkbox) {
			return (Boolean)checkbox->GetValue();
		}
	}
	
	return -1;
}

