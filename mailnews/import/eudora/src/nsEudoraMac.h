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

#ifndef nsEudoraMac_h__
#define nsEudoraMac_h__

#include "nscore.h"
#include "nsString.h"
#include "nsIFileSpec.h"
#include "nsISupportsArray.h"
#include "nsEudoraMailbox.h"
#include "nsEudoraAddress.h"

class nsIImportService;
class nsIMsgAccountManager;
class nsIMsgAccount;


class nsEudoraMac : public nsEudoraMailbox, public nsEudoraAddress {
public:
	nsEudoraMac();
	~nsEudoraMac();

		// retrieve the mail folder
	virtual PRBool		FindMailFolder( nsIFileSpec *pFolder);
		// get the list of mailboxes
	virtual nsresult	FindMailboxes( nsIFileSpec *pRoot, nsISupportsArray **ppArray);
		// get a TOC file from a mailbox file
	virtual nsresult	FindTOCFile( nsIFileSpec *pMailFile, nsIFileSpec **pTOCFile, PRBool *pDeleteToc);

	virtual nsresult	GetAttachmentInfo( const char *pFileName, nsIFileSpec *pSpec, nsCString& mimeType);

		// Address book stuff
	virtual PRBool		FindAddressFolder( nsIFileSpec *pFolder);
		// get the list of mailboxes
	virtual nsresult	FindAddressBooks( nsIFileSpec *pRoot, nsISupportsArray **ppArray);

		// import settings
	static PRBool	ImportSettings( nsIFileSpec *pIniFile, nsIMsgAccount **localMailAccount);
	static PRBool	FindSettingsFile( nsIFileSpec *pIniFile) { return( FindEudoraLocation( pIniFile, PR_TRUE));}

private:
	static PRBool		FindEudoraLocation( nsIFileSpec *pFolder, PRBool findIni = PR_FALSE, nsIFileSpec *pLookIn = nsnull);


	nsresult	ScanMailDir( nsIFileSpec *pFolder, nsISupportsArray *pArray, nsIImportService *pImport);
	nsresult	IterateMailDir( nsIFileSpec *pFolder, nsISupportsArray *pArray, nsIImportService *pImport);
	nsresult	FoundMailFolder( nsIFileSpec *mailFolder, const char *pName, nsISupportsArray *pArray, nsIImportService *pImport);
	nsresult	FoundMailbox( nsIFileSpec *mailFile, const char *pName, nsISupportsArray *pArray, nsIImportService *pImport);

	PRBool 		IsValidMailFolderName( nsCString& name);
	PRBool 		IsValidMailboxName( nsCString& fName);
	PRBool 		IsValidMailboxFile( nsIFileSpec *pFile);

	PRBool		CreateTocFromResource( nsIFileSpec *pMail, nsIFileSpec *pToc);

	

		// Settings support
	static PRBool	BuildPOPAccount( nsIMsgAccountManager *accMgr, nsCString **pStrs, nsIMsgAccount **ppAccount, nsString& accName);
	static PRBool	BuildIMAPAccount( nsIMsgAccountManager *accMgr, nsCString **pStrs, nsIMsgAccount **ppAccount, nsString& accName);
	static void		SetIdentities( nsIMsgAccountManager *accMgr, nsIMsgAccount *acc, const char *userName, const char *serverName, nsCString **pStrs);
	static void		SetSmtpServer( nsIMsgAccountManager *pMgr, nsIMsgAccount *pAcc, const char *pServer, const char *pUser);
	static PRBool 	GetSettingsFromResource( nsIFileSpec *pSettings, short resId, nsCString **pStrs, PRBool *pIMAP);


private:
	PRUint32		m_depth;
	nsIFileSpec *	m_mailImportLocation;
        PRBool HasResourceFork(FSSpec *fsSpec);
};


#endif /* nsEudoraMac_h__ */

