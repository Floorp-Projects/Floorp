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

// profile.h : Profile DataBase
//

#ifndef _PROFILE_H
#define _PROFILE_H

// NOTE: magic name must be kept in sync with ns/modules/libreg/src/reg.h
#define ASW_MAGIC_PROFILE_NAME "User1"
// Obsoleted Directory constants due to runtime directory assignment

#ifdef XP_WIN32
	#define HPROFILE HKEY
	#define HUSER    CString
#else
#ifdef XP_MAC
	#define HPROFILE FSSpec&   // Or would the LFile be the right one?
	#define HUSER    short
#else
	#define HPROFILE CString
	#define HUSER    CString
#endif
#endif

#define UPGRADE_MOVE   0
#define UPGRADE_COPY   1
#define UPGRADE_IGNORE 2

class CUserProfileDB
{

public:
			CUserProfileDB();
			~CUserProfileDB();

	int		AddNewProfile( 
				HUSER hUser, 
				CString  csProfileDirectory, 
				int      iUpgradeOption );

	void		GetUserProfile( HUSER hUser );

	int 		DeleteUserProfile( HUSER hUser );

	char * 		GetUserProfileValue( 
				HUSER hUser, CString csName );

	int 		SetUserProfileValue( 
				HUSER hUser, CString csName,
				CString csValue );

	static BOOL 		AssignProfileDirectoryName(HUSER hUser, CString &csUserDirectory);

	void 		GetUserProfilesList( CStringList *cslProfiles );

// Macintosh interfaces don't make sense on Windows
//	short		CountProfiles();
//	short		GetNextProfileID();
//	short		GetLastProfileID();
//	void		SetLastProfileID( HUSER hUser );
//	BOOL		GetProfileName( HUSER hUser, CString *name );
//	void		SetProfileName( HUSER hUser, const CString *name );
//	BOOL		GetProfileAlias( HUSER hUser );

private:
	int 		BuildDirectoryPath( const char *path );

	HPROFILE	OpenUserProfileDB();
	int		CloseUserProfileDB();

	HPROFILE        m_hProfileDB;         // handle to the profile DB
	CStringList *   m_cslProfiles;        // list of all user profiles in the DB

	HUSER  		m_hUser;              // handle to the current profile
	CString		m_csUserAddr;         // Current profile email name
	CString		m_csProfileDirectory; // Current profile directory

};

#endif
