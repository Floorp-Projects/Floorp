/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsEudoraWin32_h__
#define nsEudoraWin32_h__

#include "nscore.h"
#include "nsString.h"
#include "nsIFileSpec.h"
#include "nsISupportsArray.h"
#include "nsEudoraMailbox.h"
#include "nsEudoraAddress.h"

#include <windows.h>

class nsIImportService;
class nsIMsgAccountManager;
class nsIMsgAccount;


class nsEudoraWin32 : public nsEudoraMailbox, public nsEudoraAddress {
public:
	nsEudoraWin32();
	~nsEudoraWin32();

		// retrieve the mail folder
	virtual PRBool		FindMailFolder( nsIFileSpec *pFolder);
		// get the list of mailboxes
	virtual nsresult	FindMailboxes( nsIFileSpec *pRoot, nsISupportsArray **ppArray);
		// get a TOC file from a mailbox file
	virtual nsresult	FindTOCFile( nsIFileSpec *pMailFile, nsIFileSpec **pTOCFile, PRBool *pDeleteToc);

	virtual nsresult	GetAttachmentInfo( const char *pFileName, nsIFileSpec *pSpec, nsCString& mimeType);

	// Things that must be overridden because they are platform specific.
		// retrieve the address book folder
	virtual PRBool		FindAddressFolder( nsIFileSpec *pFolder);
		// get the list of address books
	virtual nsresult	FindAddressBooks( nsIFileSpec *pRoot, nsISupportsArray **ppArray);

		// import settings from Win32 ini file
	static PRBool	ImportSettings( nsIFileSpec *pIniFile, nsIMsgAccount **localMailAccount);
	static PRBool	FindSettingsFile( nsIFileSpec *pIniFile) { return( FindEudoraLocation( pIniFile, PR_TRUE));}

private:
	nsresult	ScanMailDir( nsIFileSpec *pFolder, nsISupportsArray *pArray, nsIImportService *pImport);
	nsresult	IterateMailDir( nsIFileSpec *pFolder, nsISupportsArray *pArray, nsIImportService *pImport);
	nsresult	ScanDescmap( nsIFileSpec *pFolder, nsISupportsArray *pArray, nsIImportService *pImport, const char *pData, PRInt32 len);
	nsresult	FoundMailFolder( nsIFileSpec *mailFolder, const char *pName, nsISupportsArray *pArray, nsIImportService *pImport);
	nsresult	FoundMailbox( nsIFileSpec *mailFile, const char *pName, nsISupportsArray *pArray, nsIImportService *pImport);
	PRBool		FindMimeIniFile( nsIFileSpec *pSpec);
	void		GetMimeTypeFromExtension( nsCString& ext, nsCString& mimeType);
	nsresult	FoundAddressBook( nsIFileSpec *spec, const PRUnichar *pName, nsISupportsArray *pArray, nsIImportService *impSvc);
	nsresult	ScanAddressDir( nsIFileSpec *pDir, nsISupportsArray *pArray, nsIImportService *impSvc);


	static PRBool		FindEudoraLocation( nsIFileSpec *pFolder, PRBool findIni = PR_FALSE);

		// Settings support
	static PRBool	BuildPOPAccount( nsIMsgAccountManager *accMgr, const char *pSection, const char *pIni, nsIMsgAccount **ppAccount);
	static PRBool	BuildIMAPAccount( nsIMsgAccountManager *accMgr, const char *pSection, const char *pIni, nsIMsgAccount **ppAccount);
	static void		GetServerAndUserName( const char *pSection, const char *pIni, nsCString& serverName, nsCString& userName, char *pBuff);
	static void		GetAccountName( const char *pSection, nsString& str);
	static void		SetIdentities( nsIMsgAccountManager *accMgr, nsIMsgAccount *acc, const char *pSection, const char *pIniFile, const char *userName, const char *serverName, char *pBuff);
	static void		SetSmtpServer( nsIMsgAccountManager *pMgr, nsIMsgAccount *pAcc, const char *pServer, const char *pUser);



	static BYTE *	GetValueBytes( HKEY hKey, const char *pValueName);
	static void		ConvertPath( nsCString& str);

private:
	PRUint32		m_depth;
	nsIFileSpec *	m_mailImportLocation;
	nsIFileSpec *	m_addressImportFolder;
	char *			m_pMimeSection;
};


#endif /* nsEudoraWin32_h__ */

