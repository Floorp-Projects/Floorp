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

#include "nsMailDatabase.h"
#include "nsDBFolderInfo.h"

nsMailDatabase::nsMailDatabase()
{
}

nsMailDatabase::~nsMailDatabase()
{
}


/* static */ nsresult	nsMailDatabase::Open(nsFilePath &dbName, PRBool create, nsMailDatabase** pMessageDB,
					 PRBool upgrading /*=PR_FALSE*/)
{
	nsMailDatabase	*mailDB;
	int				statResult;
	XP_StatStruct	st;
	PRBool			newFile = PR_FALSE;
	nsDBFolderInfo	*folderInfo = NULL;

// OK, dbName is probably folder name, since I can't figure out how nsFilePath interacts
// with xpFileTypes and its related routines.
	const char *folderName = dbName;

	*pMessageDB = NULL;

	mailDB = (nsMailDatabase *) FindInCache(dbName);
	if (mailDB)
	{
		*pMessageDB = mailDB;
		mailDB->AddRef();
		return(NS_OK);
	}
	// if the old summary doesn't exist, we're creating a new one.
	if (XP_Stat (folderName, &st, xpMailFolderSummary) && create)
		newFile = PR_TRUE;


	mailDB = new nsMailDatabase;

	if (!mailDB)
		return NS_ERROR_OUT_OF_MEMORY;
	
	mailDB->m_folderName = PL_strdup(folderName);

	dbName = WH_FileName(folderName, xpMailFolderSummary);
	if (!dbName) 
		return NS_ERROR_OUT_OF_MEMORY;
	// stat file before we open the db, because if we've latered
	// any messages, handling latered will change time stamp on
	// folder file.
	statResult = XP_Stat (folderName, &st, xpMailFolder);

	nsresult err = mailDB->OpenMDB(dbName, create);
	PR_Free(dbName);

	if (NS_SUCCEEDED(err))
	{
		folderInfo = mailDB->GetDBFolderInfo();
		if (folderInfo == NULL)
		{
			err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
		}
		else
		{
			// if opening existing file, make sure summary file is up to date.
			// if caller is upgrading, don't return NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE so the caller
			// can pull out the transfer info for the new db.
			if (!newFile && !statResult && !upgrading)
			{
				if (folderInfo->m_folderSize != st.st_size ||
						folderInfo->m_folderDate != st.st_mtime || folderInfo->GetNumNewMessages() < 0)
					err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
			}
			// compare current version of db versus filed out version info.
			if (mailDB->GetCurVersion() != folderInfo->GetDiskVersion())
				err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
		}
		if (err != NS_OK)
		{
			mailDB->Close();
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
		}
	}
	if (err == NS_OK || err == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING)
	{
		*pMessageDB = mailDB;
		GetDBCache()->AppendElement(mailDB);
//		if (err == NS_OK)
//			mailDB->HandleLatered();

	}
	return err;
}

/* static */ nsresult nsMailDatabase::CloneInvalidDBInfoIntoNewDB(nsFilePath &pathName, nsMailDatabase** pMailDB)
{
	nsresult ret = NS_OK;
	return ret;
}

nsresult nsMailDatabase::OnNewPath (nsFilePath &newPath)
{
	nsresult ret = NS_OK;
	return ret;
}

nsresult nsMailDatabase::DeleteMessages(nsMsgKeyArray &messageKeys, nsIDBChangeListener *instigator)
{
	nsresult ret = NS_OK;
	m_folderFile = PR_Open(m_dbName, PR_RDWR, 0);
	ret = nsMsgDatabase::DeleteMessages(messageKeys, instigator);
	if (m_folderFile)
		PR_Close(m_folderFile);
	m_folderFile = NULL;
	SetFolderInfoValid(m_dbName, 0, 0);
	return ret;
}


/* static */  nsresult nsMailDatabase::SetSummaryValid(PRBool valid)
{
	nsresult ret = NS_OK;
	struct stat st;

	if (stat(m_dbName, &st)) 
		return NS_MSG_ERROR_FOLDER_MISSING;

	if (valid)
	{
		m_dbFolderInfo->SetFolderSize(st.st_size);
		m_dbFolderInfo->SetFolderDate(st.st_mtime);
	}
	else
	{
		m_dbFolderInfo->SetFolderDate(0);	// that ought to do the trick.
	}
	return ret;
}

nsresult nsMailDatabase::GetFolderName(nsString &folderName)
{
	folderName = m_folderName;
	return NS_OK;
}


// The master is needed to find the folder info corresponding to the db.
// Perhaps if we passed in the folder info when we opened the db, 
// we wouldn't need the master. I don't remember why we sometimes need to
// get from the db to the folder info, but it's probably something like
// some poor soul who has a db pointer but no folderInfo.


MSG_FolderInfo *nsMailDatabase::GetFolderInfo()
{
	PR_ASSERT(PR_FALSE);
	return NULL;
}
	
	// for offline imap queued operations
	// these are in the base mail class (presumably) because offline moves between online and offline
	// folders can cause these operations to be stored in local mail folders.
nsresult nsMailDatabase::ListAllOfflineOpIds(nsMsgKeyArray &outputIds)
{
	nsresult ret = NS_OK;
	return ret;
}

int nsMailDatabase::ListAllOfflineDeletes(nsMsgKeyArray &outputIds)
{
	nsresult ret = NS_OK;
	return ret;
}
nsresult nsMailDatabase::GetOfflineOpForKey(MessageKey opKey, PRBool create, nsOfflineImapOperation **)
{
	nsresult ret = NS_OK;
	return ret;
}

nsresult nsMailDatabase::AddOfflineOp(nsOfflineImapOperation *op)
{
	nsresult ret = NS_OK;
	return ret;
}

nsresult DeleteOfflineOp(MessageKey opKey)
{
	nsresult ret = NS_OK;
	return ret;
}

nsresult SetSourceMailbox(nsOfflineImapOperation *op, const char *mailbox, MessageKey key)
{
	nsresult ret = NS_OK;
	return ret;
}

	
nsresult nsMailDatabase::GetIdsWithNoBodies (nsMsgKeyArray &bodylessIds)
{
	nsresult ret = NS_OK;
	return ret;
}
