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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"
#include "nsImapMailDatabase.h"
#include "nsDBFolderInfo.h"
#include "nsLocalFolderSummarySpec.h"

nsImapMailDatabase::nsImapMailDatabase()
{
}

nsImapMailDatabase::~nsImapMailDatabase()
{
}

NS_IMETHODIMP nsImapMailDatabase::Open(nsFileSpec &folderName, PRBool create, nsIMsgDatabase** pMessageDB, PRBool upgrading /*=PR_FALSE*/)
{
	nsImapMailDatabase	*mailDB;
	PRBool			summaryFileExists;
	struct stat		st;
	PRBool			newFile = PR_FALSE;
	nsLocalFolderSummarySpec	summarySpec(folderName);

#ifdef DEBUG
    printf("nsImapMailDatabase::Open(%s, %s, %p, %s) -> %s\n",
           (const char*)folderName, create ? "TRUE":"FALSE",
           pMessageDB, upgrading ? "TRUE":"FALSE", (const char*)folderName);
#endif
	nsIDBFolderInfo	*folderInfo = NULL;

	*pMessageDB = NULL;

	nsFileSpec dbPath(summarySpec);

	mailDB = (nsImapMailDatabase *) FindInCache(dbPath);
	if (mailDB)
	{
		*pMessageDB = mailDB;
		mailDB->AddRef();
		return(NS_OK);
	}

	// if the old summary doesn't exist, we're creating a new one.
	if (!summarySpec.Exists() && create)
		newFile = PR_TRUE;

	mailDB = new nsImapMailDatabase;

	if (!mailDB)
		return NS_ERROR_OUT_OF_MEMORY;
	mailDB->m_folderSpec = new nsFileSpec(folderName);
	mailDB->AddRef();
	// stat file before we open the db, because if we've latered
	// any messages, handling latered will change time stamp on
	// folder file.
	summaryFileExists = summarySpec.Exists();

	char	*nativeFolderName = nsCRT::strdup((const char *) folderName);

#if defined(XP_PC) || defined(XP_MAC)
	UnixToNative(nativeFolderName);
#endif
	stat (nativeFolderName, &st);
	PR_FREEIF(nativeFolderName);

	nsresult err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
	
	err = mailDB->OpenMDB((const char *) summarySpec, create);

	if (NS_SUCCEEDED(err))
	{
		mailDB->GetDBFolderInfo(&folderInfo);
		if (folderInfo == NULL)
		{
			err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
		}
		else
		{
			// compare current version of db versus filed out version info.
            int version;
            folderInfo->GetDiskVersion(&version);
			if (mailDB->GetCurVersion() != version)
				err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
			NS_RELEASE(folderInfo);
		}
		if (err != NS_OK)
		{
			// this will make the db folder info release its ref to the mail db...
			NS_IF_RELEASE(mailDB->m_dbFolderInfo);
			mailDB->Close(PR_TRUE);
			if (err == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
				summarySpec.Delete(PR_FALSE);

			mailDB = NULL;
		}
	}
	if (err != NS_OK || newFile)
	{
		// if we couldn't open file, or we have a blank one, and we're supposed 
		// to upgrade, updgrade it.
		if (newFile && !upgrading)	// caller is upgrading, and we have empty summary file,
		{					// leave db around and open so caller can upgrade it.
			err = NS_MSG_ERROR_FOLDER_SUMMARY_MISSING;
		}
		else if (err != NS_OK)
		{
			*pMessageDB = NULL;
			delete mailDB;
			mailDB = NULL;
		}
	}
	if (err == NS_OK || err == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING)
	{
		*pMessageDB = mailDB;
		if (mailDB)
			GetDBCache()->AppendElement(mailDB);
//		if (err == NS_OK)
//			mailDB->HandleLatered();

	}
	return err;
}
	
NS_IMETHODIMP	nsImapMailDatabase::SetSummaryValid(PRBool /* valid */)
{
	return NS_OK;
}
	
// IMAP does not set local file flags, override does nothing
void	nsImapMailDatabase::UpdateFolderFlag(nsMsgHdr * /* msgHdr */, PRBool /* bSet */, 
									 MsgFlags /* flag */, PRFileDesc * /* fid */)
{
}
