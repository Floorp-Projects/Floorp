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


typedef struct MSG_Pane MSG_Pane;
typedef struct DIR_Server DIR_Server;
class CValidEditField;
class LEditField;
class LGACheckbox;
class LGADialogBox;
class CTSMEditField;


//------------------------------------------------------------------------------
//	¥	CLDAPPropertyDialogManager
//------------------------------------------------------------------------------

class CLDAPPropertyDialogManager
{
public:
	 CLDAPPropertyDialogManager( DIR_Server* ioServer, MSG_Pane* inPane );
private:
	Boolean UpdateDirServerToUI();
	void Setup( LGADialogBox* inDialog , DIR_Server *inServer );


public:
	// PUBLIC CONSTANTS
	enum {
		class_ID = 'LDSP',
		eLDAPServerPropertiesDialogResID = 12002,
		cmd_HelpButton = 3
	};
	
	// Port ID's
	enum {
		eLDAPStandardPort = 389,
		eLDAPGarbledPort = 636
	};

	// Pane ID's for dialog
	enum {
		eDescriptionEditField = 10,
		eLDAPServerEditField,
		eSearchRootEditField,
		ePortNumberEditField,
		eMaxHitsEditField,
		eGarbledBox = 20,
		eSaveLDAPServerPasswordBox = 21,
		eUpdateButton = 22,
		eDownloadCheckBox = 23
	};
	
	static Boolean MaxHitsValidationFunc(CValidEditField *maxHits) ;
	static Boolean PortNumberValidationFunc(CValidEditField *portNumber) ;
private:
	const char* mHelpTopic;			// help string for NetHelp
	DIR_Server* mServer;		// the LDAP server
	
	CTSMEditField* mDescription;		// Items in dialog
	LEditField* mLdapServer;
	LEditField* mSearchRoot;
	CValidEditField* mPortNumber;
	CValidEditField* mMaxHits;
	LGACheckbox* mGarbledBox;
	LGACheckbox* mSavePasswordBox;
	LGACheckbox* mDownloadCheckBox;
};

//------------------------------------------------------------------------------
//	¥	CPABPropertyDialogManager
//------------------------------------------------------------------------------

class CPABPropertyDialogManager
{
	enum { eNamePaneID = 'NmEd' };
	enum { ePABPropertyWindowID = 8995 };
public:
	CPABPropertyDialogManager(  DIR_Server *inServer, MSG_Pane* inPane  );
};