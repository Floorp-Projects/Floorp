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

#include "stdafx.h"

#include "property.h"
#include "styles.h"

#include "helper.h"
#include "display.h"
#include "dialog.h"

#include "secnav.h"
#include "custom.h"
#include "cxabstra.h"
#include "setupwiz.h"
#include "logindg.h"
#include "prefapi.h"

#include "profile.h"
#include <ctype.h>

#define BUFSZ MAX_PATH+1

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

CUserProfileDB::CUserProfileDB( )
{
#ifdef XP_WIN
	m_hUser.Empty();
#else
#endif

	m_csUserAddr.Empty();
	m_csProfileDirectory.Empty();

	OpenUserProfileDB();
}

CUserProfileDB::~CUserProfileDB( )
{
	// m_cslProfiles->RemoveAll();

	CloseUserProfileDB();
}

HPROFILE CUserProfileDB::OpenUserProfileDB( /* Handle Mac Parms somehow */ )
{
//============================================================================
#ifdef XP_WIN32
	CString csTmp = "SOFTWARE\\Netscape\\Netscape Navigator\\Users\\";

	int result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, csTmp, NULL,
			      KEY_READ | KEY_WRITE, &m_hProfileDB);
	if ( result != ERROR_SUCCESS )
		m_hProfileDB = NULL;

//============================================================================
#else // XP_WIN16
	login_GetIniFilePath(m_hProfileDB);
#endif

	return m_hProfileDB;
}

int CUserProfileDB::CloseUserProfileDB( )
{
	int ret = TRUE;

//============================================================================
#ifdef XP_WIN32
	if (m_hProfileDB)
		ret = RegCloseKey(m_hProfileDB);
	m_hProfileDB = NULL;

//============================================================================
#else
	m_hProfileDB = "";
#endif

	return ret;
}

void CUserProfileDB::GetUserProfilesList( CStringList *cslProfiles )
{
	int  idx = 0;
	char lpBuf[MAX_PATH + 1];

	// Loop and delete entries?
	cslProfiles->RemoveAll();

	if (!m_hProfileDB)
		OpenUserProfileDB();

//============================================================================
#ifdef XP_WIN32
	while(RegEnumKey(m_hProfileDB,idx,lpBuf,MAX_PATH+1) == ERROR_SUCCESS) {
		cslProfiles->AddHead(lpBuf);
		idx++;
	}

//============================================================================
#else	
	auto char ca_iniBuff[_MAX_PATH];

	// load the user list from the ini file
	::GetPrivateProfileString("Users", NULL, "", ca_iniBuff, _MAX_PATH, m_hProfileDB);

	while (ca_iniBuff[idx]) {
		XP_STRCPY(lpBuf, &ca_iniBuff[idx]);
		cslProfiles->AddHead(lpBuf);
		idx += XP_STRLEN(lpBuf) + 1; // skip the single null and the prof we just read
	}
#endif

}

//-----------------------------------------------
// Returns string allocated with XP_ALLOC or NULL
//
LPSTR CUserProfileDB::GetUserProfileValue( 
	HUSER   hUser, 
	CString csName )
{
//============================================================================
#ifdef XP_WIN32
	HKEY hKeyRet = NULL;
	CString csSub = "SOFTWARE\\Netscape\\Netscape Navigator\\Users\\" + hUser;
	DWORD type, size = 0;
        char *pString = NULL;
	int result;

	// Unfortunately, this open is _necessary_!
	result = RegOpenKeyEx(  HKEY_LOCAL_MACHINE,
				csSub,
				NULL,
				KEY_QUERY_VALUE,
				&hKeyRet);

	if(result != ERROR_SUCCESS)
		goto Fail;

	result = RegQueryValueEx(hKeyRet, (const char *) csName, 
				NULL, &type, NULL, &size);
	if ( result != ERROR_SUCCESS || size == 0 )  
		goto Fail;

        // allocate space to hold the string
        pString = (char *) XP_ALLOC((size + 1) * sizeof(char));

        // actually load the string now that we have the space
        result = RegQueryValueEx(hKeyRet, (const char *) csName, NULL, 
				 &type, (LPBYTE) pString, &size);
        if ( result != ERROR_SUCCESS || size == 0 )
	{
		XP_FREE(pString);
		pString = NULL;
		goto Fail;
	}

	pString[size] = '\0';

Fail:
	if (hKeyRet) RegCloseKey(hKeyRet);

//============================================================================
#else // XP_WIN16
	CString csSection;
	CString csResource;
	char *  pString = NULL;
	auto char ca_iniBuff[_MAX_PATH];

	if ( csName == "DirRoot" )
	{
		// This one comes from the base [Users] section, <user>=value
		csSection  = "Users";
		csResource = hUser;
	}
	else
	{
		// These come from the [<username>] section, name=value
		csSection  = hUser;
		csResource = csName;
	}

	if (::GetPrivateProfileString( csSection, csResource, "", 
				       ca_iniBuff, _MAX_PATH, 
				       m_hProfileDB) )
	{
		// allocate space to hold the string
		pString = XP_STRDUP(ca_iniBuff);
	}
#endif

	return pString;	
}


int CUserProfileDB::SetUserProfileValue( 
	HUSER   hUser, 
	CString csName,
	CString csValue )
{
	int result;

//============================================================================
#ifdef XP_WIN32
	HKEY hKeyRet = NULL;
	CString csSub = "SOFTWARE\\Netscape\\Netscape Navigator\\Users\\" + hUser;
	const char *pValue = (const char *) csValue;

	// Unfortunately, this open is _necessary_!
	result = RegOpenKeyEx(  HKEY_LOCAL_MACHINE,
				csSub,
				NULL,
				KEY_QUERY_VALUE,
				&hKeyRet);

	if(result == ERROR_SUCCESS)
		result = RegSetValueEx( m_hProfileDB, csName, NULL, 
					REG_SZ, 
					(const BYTE *) pValue, 
					strlen(pValue) );

	if (hKeyRet) RegCloseKey(hKeyRet);

//============================================================================
#else // XP_WIN16
	CString csSection;
	CString csResource;

	if ( csName == "DirRoot" )
	{
		// This one comes from the base [Users] section, <user>=value
		csSection  = "Users";
		csResource = hUser;
	}
	else
	{
		// These come from the [<username>] section, name=value
		csSection  = hUser;
		csResource = csName;
	}

	result = ::WritePrivateProfileString( csSection, csResource, 
					      (const char *) csValue, 
					      m_hProfileDB);

#endif

	return result;
}


void CUserProfileDB::GetUserProfile( HUSER hUser )
{
	char *pString;

//============================================================================
#ifdef XP_WIN
	m_hUser = hUser;

	/*
	** munge user name to profile name 
	*/
	int iAtSign = m_hUser.Find('@');
	if (iAtSign != -1) 
		m_hUser = m_hUser.Left(iAtSign);
#endif

	// Find the profile directory
	pString = GetUserProfileValue( m_hUser, "DirRoot" );
	if (pString)
	{
		m_csProfileDirectory = pString;
		XP_FREE(pString);
	}
	else
		m_csProfileDirectory.Empty();

	// Find the EMail Address
	pString = GetUserProfileValue( m_hUser, "EmailAddr" );
	if (pString)
	{
		m_csUserAddr = pString;
		XP_FREE(pString);
	}
	else
		m_csUserAddr.Empty();

	// Add filling in any new profile members here...


	return ;
}


int CUserProfileDB::BuildDirectoryPath( const char *path )
{
	int ret;
	XP_StatStruct statinfo; 

	ret = _stat((char *) path, &statinfo);
	if(ret == -1) {
		// see if we can just create it
		char * slash = strchr(path,'\\');
		while (slash) {
			slash[0] = NULL;
			ret = _mkdir(path);
			slash[0] = '\\';
			if (slash+1) slash = strchr(slash+1,'\\');
		}
		ret = _mkdir(path);
		if ( ret == -1 ) return FALSE;
		// ERROR: Create Directory failed
	}

	// If directory exists already, then what?
	return TRUE;
}

int CUserProfileDB::AddNewProfile( 
	HUSER   hUser,
	CString csProfileDirectory, 
	int     iUpgradeOption )
{
	int ret;

	// Using _mkdir for win16/win32 compatibility
	// Do we need new error constants if this gets incorporated into the setupwiz dialogs?

	m_hUser = hUser;
	m_csProfileDirectory = csProfileDirectory;

	/*
	** Create the directory if necessary 
	*/
	BuildDirectoryPath(  (const char *) csProfileDirectory );

	/*
	** Handle upgrades.  The Move operation moves data and leaves links behind.
	** The Copy operation copies the data and changes are not reflected to old 
	** space.  Ignore leaves old space alone and creates new profile.
	*/
	if (iUpgradeOption == UPGRADE_MOVE || iUpgradeOption == UPGRADE_COPY) {
		ret = login_UpdateFilesToNewLocation( csProfileDirectory, NULL, 
			(iUpgradeOption == UPGRADE_COPY) );

		// ret value is hosed, doesn't actually do any checking!
		if ( !ret ) return NULL;              
		// ERROR: Couldn't Move/Copy old profile

		ret = login_UpdatePreferencesToJavaScript( csProfileDirectory );
		// ret value is hosed, doesn't actually do any checking!
		if ( !ret ) return NULL;              
		// ERROR: Couldn't update old preferences to JS

	} else {
		// just create the directories --
		login_CreateEmptyProfileDir( csProfileDirectory, NULL, FALSE);
	}

	/*
	** Create the user and add the profile directory to the user's registry entry 
	** !! Don't do this until all other operations have succeeded!
	*/
	ret = login_CreateNewUserKey( hUser, csProfileDirectory ); 
	if ( ret != ERROR_SUCCESS ) return NULL;
	// ERROR: Create User Entry failed


	/*
	** Now that we've done all this work, let's fill the profile up with stuff
	**/
	PREF_SavePrefFile();                          
	// ERROR: ?

	return 1;
}



int CUserProfileDB::DeleteUserProfile( HUSER hUser )
{
	char *pString;


	pString = GetUserProfileValue( hUser, "DirRoot" );
	if (pString)
	{
		XP_RemoveDirectoryRecursive(pString, xpURL);
		XP_FREE(pString);
	}

	if (m_hUser && hUser == m_hUser)
		m_csProfileDirectory.Empty();

	return login_DeleteUserKey( hUser );
}

// Given a profile name, return a profile directory path in Program Files
BOOL CUserProfileDB::AssignProfileDirectoryName(HUSER hUser, CString &csUserDirectory)
{
	CString install("");
	CString userDir = "\\Users\\";

	// first chance of default is based on any existing user dir

	char *pExistingUser = NULL;
	pExistingUser = login_GetCurrentUser();
	if (pExistingUser) {
		char *pDir = login_GetUserProfileDir();
		if (pDir && *pDir) {
			install = pDir;
			
			int iLastSlash = -1;
			iLastSlash = install.ReverseFind('\\');
			if (iLastSlash > 2) {
				int iPos = install.GetLength()-iLastSlash;
				install= install.Left(install.GetLength()-iPos+1);
			}
			// already have the user dir as part of the path so empty out the string
			userDir.Empty();
			XP_FREE(pDir);
		}
	}

	if (install.IsEmpty())
	{
		// Try to find the official install directory value in the registry
#ifdef XP_WIN32
		CString csProduct;
		csProduct.LoadString(IDS_NETHELP_REGISTRY);

		csProduct = FEU_GetCurrentRegistry(csProduct);
		if (!csProduct.IsEmpty())
			install = FEU_GetInstallationDirectory(csProduct, szLoadString(IDS_INSTALL_DIRECTORY));
#else 
		
		install = FEU_GetInstallationDirectory(szLoadString(IDS_NETSCAPE_REGISTRY), szLoadString(IDS_INSTALL_DIRECTORY));
        if (!install.IsEmpty())
        {
                // Assuming we got a name back, remove the Program part - which is only there for 16 bit, not for 32 bit.
                int iLastSlash = install.ReverseFind('\\');

                if (iLastSlash != -1)
                {
                        CString subdir = install.Mid(iLastSlash+1);
                        if (!subdir.CompareNoCase("Program"))
                                install = install.Left(iLastSlash);
                }
        }
#endif
	}

	// If we can't get it from the registry, try the program name
	if (install.IsEmpty())
	{
		char aPath[_MAX_PATH];

		if (::GetModuleFileName(theApp.m_hInstance, aPath, sizeof(aPath)))
		{
			// WIN16 default: c:\Netscape\Comm\Program\netscape.exe
			// WIN32 default: c:\Program Files\Netscape\Communicator\Program\netscape.exe

			// Remove the trailing netscape.exe stuff
			char *pSlash = ::strrchr(aPath, '\\');
			if(pSlash)
				*pSlash = '\0';

			// Remove the Program part
			pSlash = ::strrchr(aPath, '\\');
			if (pSlash)
				*pSlash = '\0';

			install = aPath;
		}

	}
	
	// If we still can't get it, use these defaults
	if (install.IsEmpty())
	{
#ifdef XP_WIN32
			install = "C:\\Program Files\\Netscape";
#else
			install = "C:\\Netscape";
#endif
	}
	else
	{
		// Assuming we got a name back somehow, remove the Comm[unicator] part
		int iLastSlash = install.ReverseFind('\\');

		if (iLastSlash != -1)
		{
			CString subdir = install.Mid(iLastSlash+1);
#ifdef XP_WIN32
			if ((!subdir.CompareNoCase("Communicator")) || (!subdir.CompareNoCase("Commun~1")))
#else 
			if (!subdir.CompareNoCase("Comm"))
#endif
				install = install.Left(iLastSlash);
		}
	}
	//we need to make sure that the hUser is valid characters
	int idx =0;
	for (idx =0; idx < hUser.GetLength(); idx++) {
		if (!__iscsym(hUser.GetAt(idx)))
			hUser.SetAt(idx,'_');
	}
#ifdef XP_WIN32
	csUserDirectory = install + userDir + hUser;
#else
	csUserDirectory = install + userDir + hUser.Left(8);
#endif

	return TRUE;
}
