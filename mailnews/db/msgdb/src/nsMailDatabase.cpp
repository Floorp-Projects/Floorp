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

nsMailDatabase::nsMailDatabase()
{
}

nsMailDatabase::~nsMailDatabase()
{
}


/* static */ nsresult	nsMailDatabase::Open(nsFilePath &dbName, PRBool create, nsMailDatabase** pMessageDB)
{
	nsMailDatabase	*mailDB;
	int				statResult;
	XP_StatStruct	st;
	XP_Bool			newFile = FALSE;

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
		newFile = TRUE;


	mailDB = new nsMailDatabase;

	if (!mailDB)
		return(eOUT_OF_MEMORY);
	
	mailDB->m_folderName = XP_STRDUP(folderName);

	dbName = WH_FileName(folderName, xpMailFolderSummary);
	if (!dbName) return eOUT_OF_MEMORY;
	// stat file before we open the db, because if we've latered
	// any messages, handling latered will change time stamp on
	// folder file.
	statResult = XP_Stat (folderName, &st, xpMailFolder);

	nsresult err = mailDB->OpenMDB(dbName, create);
	XP_FREE(dbName);

	if (NS_SUCCEEDED(err))
	{
//		folderInfo = mailDB->GetDBFolderInfo();
//		if (folderInfo == NULL)
//		{
//			err = eOldSummaryFile;
//		}
//		else
		{
			// if opening existing file, make sure summary file is up to date.
			// if caller is upgrading, don't return eOldSummaryFile so the caller
			// can pull out the transfer info for the new db.
			if (!newFile && !statResult && !upgrading)
			{
				if (folderInfo->m_folderSize != st.st_size ||
						folderInfo->m_folderDate != st.st_mtime || folderInfo->GetNumNewMessages() < 0)
					err = eOldSummaryFile;
			}
			// compare current version of db versus filed out version info.
			if (mailDB->GetCurVersion() != folderInfo->GetDiskVersion())
				err = eOldSummaryFile;
		}
		if (err != eSUCCESS)
		{
			mailDB->Close();
			mailDB = NULL;
		}
	}
	if (err != eSUCCESS || newFile)
	{
		// if we couldn't open file, or we have a blank one, and we're supposed 
		// to upgrade, updgrade it.
		if (newFile && !upgrading)	// caller is upgrading, and we have empty summary file,
		{					// leave db around and open so caller can upgrade it.
			err = eNoSummaryFile;
		}
		else if (err != eSUCCESS)
		{
			*pMessageDB = NULL;
			delete mailDB;
		}
	}
	if (err == eSUCCESS || err == eNoSummaryFile)
	{
		*pMessageDB = mailDB;
		if (m_cacheEnabled)
			GetDBCache()->Add(mailDB);
		if (err == eSUCCESS)
			mailDB->HandleLatered();

	}
	return(err);
	nsresult ret = OpenMDB(dbName, create);
	if (NS_MSG_SUCCEEDED(ret)
	{
	}
	return ret;
}

static  nsresult nsMailDatabase::CloneInvalidDBInfoIntoNewDB(nsFilePath &pathName, nsMailDatabase** pMailDB)
{
}

nsresult nsMailDatabase::OnNewPath (nsFilePath &newPath)
{
}

nsresult nsMailDatabase::DeleteMessages(IDArray &messageKeys, ChangeListener *instigator)
{
}


int	nsMailDatabase::GetCurVersion() {return kMailDBVersion;}

static  nsresult nsMailDatabase::SetFolderInfoValid(nsFilePath &pathname, int num, int numunread)
{
}

nsresult nsMailDatabase::GetFolderName(nsString &folderName)
{
	folderName = m_folderName;
}

nsMailDatabase	*nsMailDatabase::GetMailDB() {return this;}

// The master is needed to find the folder info corresponding to the db.
// Perhaps if we passed in the folder info when we opened the db, 
// we wouldn't need the master. I don't remember why we sometimes need to
// get from the db to the folder info, but it's probably something like
// some poor soul who has a db pointer but no folderInfo.

MSG_Master *nsMailDatabase::GetMaster() {return m_master;}
void nsMailDatabase::SetMaster(MSG_Master *master) {m_master = master;}

MSG_FolderInfo *nsMailDatabase::GetFolderInfo()
{
}
	
	// for offline imap queued operations
	// these are in the base mail class (presumably) because offline moves between online and offline
	// folders can cause these operations to be stored in local mail folders.
nsresult nsMailDatabase::ListAllOfflineOpIds(IDArray &outputIds)
{
}
int nsMailDatabase::ListAllOfflineDeletes(IDArray &outputIds)
{
}
nsresult nsMailDatabase::GetOfflineOpForKey(MessageKey opKey, PRBool create, nsOfflineImapOperation **)
{
}

nsresult nsMailDatabase::AddOfflineOp(nsOfflineImapOperation *op)
{
}

nsresult DeleteOfflineOp(MessageKey opKey)
{
}

nsresult SetSourceMailbox(nsOfflineImapOperation *op, const char *mailbox, MessageKey key)
{
}

nsresult nsMailDatabase::SetSummaryValid(PRBool valid = TRUE)
{
}
	
nsresult nsMailDatabase::GetIdsWithNoBodies (IDArray &bodylessIds)
{
}
