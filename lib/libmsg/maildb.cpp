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
#include "msg.h"
#include "xp.h"

#include "maildb.h"
#include "dberror.h"

#include "msgdbvw.h"
#include "mailhdr.h"
#include "thrhead.h"
#include "grpinfo.h"
#include "imap.h"
#include "msgmast.h"
#include "msgprefs.h"
#include "msgimap.h"
#include "imapoff.h"
#include "prefapi.h"
#include "msgdbapi.h"

extern "C"
{
	extern int MK_MSG_CANT_OPEN;
}

MailDB::MailDB() 
{
	m_folderName = NULL;
	m_reparse = FALSE;
	m_folderFile = NULL;
	m_master = NULL;
}

MailDB::~MailDB()
{
	FREEIF(m_folderName);
}

MsgERR MailDB::OnNewPath (const char *newPath)
{
	FREEIF(m_folderName);
	m_folderName = XP_STRDUP (newPath);
	return eSUCCESS;
}

// static method. This routine works in very different ways depending on the passed flags.
// If create is true, we will create a database if none was there.
// If upgrading is TRUE, the caller is upgrading and does not care that the database is 
// gone, or out of sync with the mail folder. The caller will upgrade in the background
// and just wants a db open to upgrade into.
MsgERR	MailDB::Open(const char * folderName, XP_Bool create, 
					 MailDB** pMessageDB,  
					 XP_Bool upgrading /*=FALSE*/)
{
	MailDB	*mailDB;
	int				statResult;
	XP_StatStruct	st;
	DBFolderInfo	*folderInfo = NULL;
	XP_Bool			newFile = FALSE;
	char *dbName;

	*pMessageDB = NULL;

	if (m_cacheEnabled)
	{
		dbName = WH_FileName(folderName, xpMailFolderSummary);
		if (!dbName) return eOUT_OF_MEMORY;
		mailDB = (MailDB *) FindInCache(dbName);
		XP_FREE(dbName);
		if (mailDB)
		{
			*pMessageDB = mailDB;
			// make this behave like the non-cache case. Is global dB set? DMB TODO
			++(mailDB->m_useCount);
			return(eSUCCESS);
		}
	}
	// if the old summary doesn't exist, we're creating a new one.
	if (XP_Stat (folderName, &st, xpMailFolderSummary) && create)
		newFile = TRUE;


	mailDB = new MailDB;

	if (!mailDB)
		return(eOUT_OF_MEMORY);
	
	mailDB->m_folderName = XP_STRDUP(folderName);

	dbName = WH_FileName(folderName, xpMailFolderSummary);
	if (!dbName) return eOUT_OF_MEMORY;
	// stat file before we open the db, because if we've latered
	// any messages, handling latered will change time stamp on
	// folder file.
	statResult = XP_Stat (folderName, &st, xpMailFolder);

	MsgERR err = mailDB->MessageDBOpen(dbName, create);
	XP_FREE(dbName);

	if (err == eSUCCESS)
	{
		folderInfo = mailDB->GetDBFolderInfo();
		if (folderInfo == NULL)
		{
			err = eOldSummaryFile;
		}
		else
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
}

// This routine opens an invalid db, pulls out the information worth saving, (like sort order),
// blows away the invalid db, creates a new empty one, and restores the information worth saving.
/* static */
MsgERR	MailDB::CloneInvalidDBInfoIntoNewDB(const char * pathname, MailDB** pMailDB)
{
	XP_StatStruct folderst;
	MailDB	*mailDB;
	TDBFolderInfoTransfer *originalInfo = NULL;

	int status = MailDB::Open(pathname, TRUE, &mailDB, TRUE);

	if (status == eSUCCESS && mailDB != NULL)
	{
		originalInfo = new TDBFolderInfoTransfer(*mailDB->GetDBFolderInfo());
		mailDB->Close();
	}

	XP_Trace("blowing away old summary file\n");
	if (!XP_Stat(pathname, &folderst, xpMailFolderSummary) && XP_FileRemove(pathname, xpMailFolderSummary) != 0)
	{
		status = MK_MSG_CANT_OPEN;
	}
	else
	{
		status = MailDB::Open(pathname, TRUE, &mailDB, TRUE);
		if (originalInfo)
		{
			if (status == eSUCCESS && mailDB != NULL)
				originalInfo->TransferFolderInfo(*mailDB->GetDBFolderInfo());
			delete originalInfo;
		}
	}

	*pMailDB = mailDB;
	return status;
}

MSG_FolderInfo *MailDB::GetFolderInfo()
{
	if (!m_folderInfo)
	{
		m_folderInfo = m_master->FindMailFolder(m_folderName, FALSE);
	}
	return m_folderInfo;
}


// caller needs to unrefer when finished.
MailMessageHdr *MailDB::GetMailHdrForKey(MessageKey messageKey)
{
	MailMessageHdr *headerObject = NULL;
	MSG_HeaderHandle headerHandle = MSG_DBHandle_GetHandleForKey(m_dbHandle, messageKey);
	if (headerHandle)
		headerObject = new MailMessageHdr(headerHandle);

	return headerObject;
}

MsgERR MailDB::DeleteMessages(IDArray &messageKeys, ChangeListener *instigator)
{
	m_folderFile = XP_FileOpen(m_folderName, xpMailFolder,
								 XP_FILE_UPDATE_BIN);
	MsgERR	err = MessageDB::DeleteMessages(messageKeys, instigator);
	if (m_folderFile)
		XP_FileClose(m_folderFile);
	m_folderFile = NULL;
	SetFolderInfoValid(m_folderName, 0, 0);
	return err;
}

// Helper routine - lowest level of flag setting
void MailDB::SetHdrFlag(DBMessageHdr *msgHdr, XP_Bool bSet, MsgFlags flag)
{
	XP_File fid = NULL;

	MessageDB::SetHdrFlag(msgHdr, bSet, flag);
	// update summary flag
	if (msgHdr->GetFlags() & kDirty)
	{
		// ### Hack - we know it's a mail message hdr 'cuz it's in a maildb
		// ImapMailDB::UpdateFolderFlag does nothing
		UpdateFolderFlag((MailMessageHdr *) msgHdr, bSet, flag, &fid);
		if (fid != NULL)
		{
			XP_FileClose(fid);
			SetFolderInfoValid(m_folderName, 0, 0);
		}
	}
}

// We let the caller close the file in case he's updating a lot of flags
// and we don't want to open and close the file every time through.
// As an experiment, try caching the fid in the db as m_folderFile.
// If this is set, use it but don't return *pFid.
void MailDB::UpdateFolderFlag(MailMessageHdr *mailHdr, XP_Bool /*bSet*/, 
							  MsgFlags /*flag*/, XP_File *pFid)
{
	static char buf[30];
	XP_File fid = (m_folderFile) ? m_folderFile : *pFid;
	if (mailHdr->GetFlags() & kDirty)
	{
		if (mailHdr->GetStatusOffset() > 0) 
		{
			if (fid == NULL) 
			{
				fid = XP_FileOpen(m_folderName, xpMailFolder,
								 XP_FILE_UPDATE_BIN);
			}
			if (fid) 
			{
				uint32 position =
				mailHdr->GetStatusOffset() + mailHdr->GetMessageOffset();
				XP_ASSERT(mailHdr->GetStatusOffset() < 10000);
				XP_FileSeek(fid, position, SEEK_SET);
				buf[0] = '\0';
				if (XP_FileReadLine(buf, sizeof(buf), fid)) 
				{
					if (strncmp(buf, X_MOZILLA_STATUS, X_MOZILLA_STATUS_LEN) == 0 &&
						strncmp(buf + X_MOZILLA_STATUS_LEN, ": ", 2) == 0 &&
						strlen(buf) > X_MOZILLA_STATUS_LEN + 6) 
					{
						XP_FileSeek(fid, position, SEEK_SET);
						// We are filing out old Cheddar flags here
						XP_FilePrintf(fid, X_MOZILLA_STATUS_FORMAT,
									(mailHdr->GetMozillaStatusFlags() & ~MSG_FLAG_RUNTIME_ONLY));
						// time to upate x-mozilla-status2
						position = XP_FileTell(fid);
						XP_FileSeek(fid, position + LINEBREAK_LEN, SEEK_SET);
						if (XP_FileReadLine(buf, sizeof(buf), fid)) 
						{
							if (strncmp(buf, X_MOZILLA_STATUS2, X_MOZILLA_STATUS2_LEN) == 0 &&
								strncmp(buf + X_MOZILLA_STATUS2_LEN, ": ", 2) == 0 &&
								strlen(buf) > X_MOZILLA_STATUS2_LEN + 10) 
							{
								uint32 dbFlags = mailHdr->GetFlags();
								MessageDB::ConvertDBFlagsToPublicFlags(&dbFlags);
								dbFlags &= (MSG_FLAG_MDN_REPORT_NEEDED | MSG_FLAG_MDN_REPORT_SENT | MSG_FLAG_TEMPLATE);
								XP_FileSeek(fid, position + LINEBREAK_LEN, SEEK_SET);
								XP_FilePrintf(fid, X_MOZILLA_STATUS2_FORMAT, dbFlags);
							}
						}
					} else 
					{
						PRINTF(("Didn't find %s where expected at position %ld\n"
							  "instead, found %s.\n",
							  X_MOZILLA_STATUS, (long) position, buf));
						SetReparse(TRUE);
					}			
				} 
				else 
				{
					PRINTF(("Couldn't read old status line at all at position %ld\n",
							(long) position));
					SetReparse(TRUE);
				}
			}
			else
			{
				PRINTF(("Couldn't open mail folder for update%s!\n", m_folderName));
				XP_ASSERT(FALSE);
			}
		}
		if (!m_folderFile)
			*pFid = fid;
		mailHdr->AndFlags(~kDirty);	
	}
}

// Use this function if you have the correct counts, but just want to protect 
// from a crash. In theory, this Commit should be quick, since there's only
// one or two dirty objects.
MsgERR MailDB::SetSummaryValid(XP_Bool valid /*= TRUE*/)
{
	XP_StatStruct folderst;

	if (XP_Stat((char*) m_folderName, &folderst, xpMailFolder)) 
		return eDBExistsNot;
	if (valid)
	{
		m_dbFolderInfo->m_folderSize = folderst.st_size;
		m_dbFolderInfo->m_folderDate = folderst.st_mtime;
	}
	else
	{
		m_dbFolderInfo->m_folderDate = 0;	// that ought to do the trick.
	}
//	m_dbFolderInfo->setDirty();	DMB TODO
	Commit();
	return eSUCCESS;
}

MsgERR MailDB::GetIdsWithNoBodies (IDArray &bodylessIds)
{
	DBMessageHdr	*currentHeader;
	ListContext		*listContext = NULL;
	MsgERR			dbErr = ListFirst (&listContext, &currentHeader);
	int32			maxMsgSize = 0x7fffffff;
	XP_Bool			respectMsgSizeLimit = FALSE;

	PREF_GetBoolPref("mail.limit_message_size", &respectMsgSizeLimit);

	if (respectMsgSizeLimit && PREF_OK == PREF_GetIntPref("mail.max_size", &maxMsgSize))
		maxMsgSize *= 1024;
	
	while (dbErr == eSUCCESS)
	{
		if (0 == ((MailMessageHdr *)currentHeader)->GetOfflineMessageLength(GetDB()) && currentHeader->GetByteLength() < (uint32) maxMsgSize)
			bodylessIds.Add(currentHeader->GetMessageKey());
		delete currentHeader;
		dbErr = ListNext(listContext, &currentHeader);
	}
	
	if (dbErr == eDBEndOfList)
	{
		dbErr = eSUCCESS;
		ListDone(listContext);
	}

	return dbErr;
}

/* static */
MsgERR	MailDB::SetFolderInfoValid(const char* pathname, int num, int numunread)
{
	XP_StatStruct folderst;
	MsgERR		err;

	if (XP_Stat((char*) pathname, &folderst, xpMailFolder)) 
		return eDBExistsNot;

	char *dbName = WH_FileName(pathname, xpMailFolderSummary);
	if (!dbName) return eOUT_OF_MEMORY;
	MessageDB *pMessageDB = MessageDB::FindInCache(dbName);
	if (pMessageDB == NULL)
	{
		pMessageDB = new MailDB;
		// ### this does later stuff (marks latered messages unread), which may be a problem
		err = pMessageDB->MessageDBOpen(dbName, FALSE);
		if (err != eSUCCESS)
		{
			delete pMessageDB;
			pMessageDB = NULL;
		}
	}
	else
		pMessageDB->AddUseCount();

	XP_FREE(dbName);

	if (pMessageDB == NULL)
	{
		XP_Trace("Exception opening summary file\n");
		return eOldSummaryFile;
	}
	
	pMessageDB->m_dbFolderInfo->m_folderSize = folderst.st_size;
	pMessageDB->m_dbFolderInfo->m_folderDate = folderst.st_mtime;
	pMessageDB->m_dbFolderInfo->ChangeNumVisibleMessages(num);
	pMessageDB->m_dbFolderInfo->ChangeNumNewMessages(numunread);
	pMessageDB->m_dbFolderInfo->ChangeNumMessages(num);
	pMessageDB->Close();
	return eSUCCESS;
}


// This is used to remember that the db is out of sync with the mail folder
// and needs to be regenerated.
void MailDB::SetReparse(XP_Bool reparse)
{
	m_reparse = reparse;
}

ImapMailDB::ImapMailDB() : MailDB()
{
}

ImapMailDB::~ImapMailDB()
{}

MsgERR ImapMailDB::Open(const char * folderName, XP_Bool create, 
						     MailDB** pMessageDB, 
						     MSG_Master* mailMaster,
						     XP_Bool *dbWasCreated)
{
	ImapMailDB	*mailDB=NULL;
	XP_StatStruct	st;
	DBFolderInfo	*folderInfo = NULL;
	XP_Bool			newFile = FALSE;
	char* dbName;
	MsgERR err=eUNKNOWN;
	XP_Bool dbFoundInCache=FALSE;

	XP_ASSERT(mailMaster);

	*dbWasCreated = FALSE;
	
	// code below can't call XP_File routines, or dbName will be crunched.

	if (m_cacheEnabled)
	{
		dbName = WH_FileName(folderName, xpMailFolderSummary);
		if (!dbName) return eOUT_OF_MEMORY;
		mailDB = (ImapMailDB *) FindInCache(dbName);
		XP_FREE(dbName);
		if (mailDB)
		{
			*pMessageDB = mailDB;
			++(mailDB->m_useCount);
			// make this behave like the non-cache case. Is global DB set? DMB TODO
			err = eSUCCESS;
			dbFoundInCache = TRUE;
		}
	}
	
	if (!dbFoundInCache)
	{
		// if the old summary doesn't exist, we're creating a new one.
		if (XP_Stat (folderName, &st, xpMailFolderSummary) && create)
		{
			newFile = TRUE;
			*dbWasCreated = TRUE;
		}

		mailDB = new ImapMailDB;
		if (!mailDB)
		{
			*pMessageDB = NULL;
			return(eOUT_OF_MEMORY);
		}			
		mailDB->m_folderName = XP_STRDUP(folderName);

		dbName = WH_FileName(folderName, xpMailFolderSummary);
		if (!dbName) {
			err = eOUT_OF_MEMORY;
		} else {
			err = mailDB->MessageDBOpen(dbName, create);
			XP_FREE(dbName);
		}
		if (err == eSUCCESS)
		{			
			folderInfo = mailDB->GetDBFolderInfo();
			// if opening existing file, make sure summary file is up to date.
			if (!newFile && !XP_Stat (folderName, &st, xpMailFolder))
			{
				if (folderInfo->m_folderSize != st.st_size)
					err = eOldSummaryFile;
			}
			if (mailDB->GetCurVersion() != folderInfo->GetDiskVersion())
				err = eOldSummaryFile;
			if (err != eSUCCESS)
			{
				mailDB->Close();
				mailDB = NULL;
			}
		}
		if (err == eSUCCESS)
		{
			*pMessageDB = mailDB;
			if (m_cacheEnabled)
				GetDBCache()->Add(mailDB);
		}
	}
	
	if (mailDB)
	{
		mailDB->SetMaster(mailMaster);
		if (mailMaster)
		{
			// this causes an infinite recursion, so don't do it.
//			MSG_FolderInfoMail *folderInfo = mailMaster->FindImapMailFolder(folderName);
//			mailDB->SetFolderInfo(folderInfo);
		}
	}
	return(err);
}
  
void ImapMailDB::UpdateFolderFlag(MailMessageHdr *, XP_Bool , 
								  MsgFlags , XP_File *fid)
{
	fid = NULL;
}

MSG_FolderInfo *ImapMailDB::GetFolderInfo()
{
	if (!m_folderInfo)
	{
		XPStringObj mailBoxName;

		m_dbFolderInfo->GetMailboxName(mailBoxName);
		m_folderInfo = m_master->FindImapMailFolderFromPath(m_folderName);
	}
	return m_folderInfo;
}


// caller needs to unrefer when finished.
DBOfflineImapOperation *MailDB::GetOfflineOpForKey(MessageKey messageKey, XP_Bool create)
{
	DBOfflineImapOperation *offlineObject = NULL;
	MSG_OfflineIMAPOperationHandle offlineOpHandle = MSG_DBHandle_GetOfflineOp(m_dbHandle, messageKey);
	if (offlineOpHandle)
	{
		offlineObject = new DBOfflineImapOperation;
		if (offlineObject)
		{
			offlineObject->SetHandle(offlineOpHandle);
			offlineObject->SetDBHandle(m_dbHandle);
		}
	}
	
	if (!offlineObject && create)
	{
		offlineObject = new DBOfflineImapOperation;
		if (offlineObject)
		{
			offlineObject->SetHandle(GetOfflineIMAPOperation());

			offlineObject->SetMessageKey(messageKey);
			offlineObject->SetDBHandle(m_dbHandle);
			
			// if there is a corresponding msg, use its flags
			MailMessageHdr	*msgHdr = GetMailHdrForKey(messageKey);
			if (msgHdr)
			{
				imapMessageFlagsType imapFlags = kNoImapMsgFlag;
				if (msgHdr->GetFlags() & MSG_FLAG_READ)
					imapFlags |= kImapMsgSeenFlag;
				if (msgHdr->GetFlags() & MSG_FLAG_REPLIED)
					imapFlags |= kImapMsgAnsweredFlag;
				if (msgHdr->GetFlags() & MSG_FLAG_MARKED)
					imapFlags |= kImapMsgFlaggedFlag;
				if (msgHdr->GetFlags() & MSG_FLAG_FORWARDED)
					imapFlags |= kImapMsgForwardedFlag;
				offlineObject->SetInitialImapFlags(imapFlags);
				delete msgHdr;
			}
			
			AddOfflineOp(offlineObject);
		}
	}
		
	return offlineObject;
}

MsgERR	MailDB::SetSourceMailbox(DBOfflineImapOperation *op, const char *mailbox, MessageKey key)
{
	MsgERR err = MSG_OfflineIMAPOperationHandle_SetSourceMailbox(op->GetHandle(), m_dbHandle, mailbox, key);
	if (err == eCorruptDB)
		SetSummaryValid(FALSE);
	return err;
}

MsgERR MailDB::ListAllOfflineOpIds(IDArray &outputIds)
{
	MessageKey *resultKeys;
	int32		numKeys;
	MsgERR err = MSG_DBHandle_ListAllOflineOperationKeys(m_dbHandle, &resultKeys, &numKeys);
	outputIds.SetArray(resultKeys, numKeys, numKeys);
	return err;
}

// List the ids of messages that are going to be deleted via offline playback,
// because we don't need to refetch those headers.
int MailDB::ListAllOfflineDeletes(IDArray &outputIds)
{
	MessageKey *deletedKeys;
	int32		numKeys;
	MsgERR err = MSG_DBHandle_ListAllOfflineDeletedMessageKeys(m_dbHandle, &deletedKeys, &numKeys);
	outputIds.Add(deletedKeys, numKeys);
	XP_FREE(deletedKeys);
	return numKeys;
}

MsgERR MailDB::AddOfflineOp(DBOfflineImapOperation *op)
{
	MSG_DBHandle_AddOfflineOperation(m_dbHandle, op->GetHandle());
	MSG_FolderInfo *folderInfo = GetFolderInfo();
	XP_ASSERT(folderInfo);	// it would be bad if we couldn't get the folder info.
	if (folderInfo)
	{
		folderInfo->SetFolderPrefFlags(folderInfo->GetFolderPrefFlags() | MSG_FOLDER_PREF_OFFLINEEVENTS);
		if (folderInfo->GetType() == FOLDER_IMAPMAIL)
		{
			MSG_IMAPFolderInfoMail *imapFolder = (MSG_IMAPFolderInfoMail *) folderInfo;
			imapFolder->SetHasOfflineEvents(TRUE);
			// where should we clear this flag? At the end of the process events url?
		}
	}
	return eSUCCESS;
}

MsgERR MailDB::DeleteOfflineOp(MessageKey opKey)
{
	DBOfflineImapOperation *doomedOp = GetOfflineOpForKey(opKey, FALSE);
	if (doomedOp)
	{
		MSG_DBHandle_RemoveOfflineOperation(m_dbHandle, doomedOp->GetHandle());
		delete doomedOp;
	}
	return eSUCCESS;
}

MsgERR	ImapMailDB::SetSummaryValid(XP_Bool valid /* = TRUE */)
{
	return MessageDB::SetSummaryValid(valid);
}
