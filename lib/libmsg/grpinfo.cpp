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
#include "msghdr.h"
#include "grpinfo.h"
#include "maildb.h"
#include "newsdb.h"
#include "newsset.h"
#include "csid.h"
#include "msgdbapi.h"

DBFolderInfo::DBFolderInfo(MSG_DBFolderInfoHandle handle)
{
	m_articleNumHighWater = 0;
	m_expiredMark = 0;
	m_numVisibleMessages = 0;
	m_numNewMessages = 0;
	m_numMessages = 0;
	m_folderSize = 0;
	m_folderDate = 0;
	m_parsedThru = 0;
	m_expunged_bytes = 0;
	m_flags = 0;
	m_version = kFolderInfoVersion;
	m_LastMessageUID = 0;
	m_ImapUidValidity = 0;
	m_TotalPendingMessages = 0;
	m_UnreadPendingMessages = 0;
	m_viewType = ViewAllThreads;
	m_sortType = SortByDate;
	m_sortOrder = SortTypeAscending;
	// this is a bogus value. Better to make it always bogus than sometimes.
	m_folderType = FOLDER_CONTAINERONLY;	
	m_csid = CS_DEFAULT;
	m_IMAPHierarchySeparator = kOnlineHierarchySeparatorUnknown;
	m_dbFolderInfoHandle = handle;
}

void		DBFolderInfo::AddReference()
{
}

void		DBFolderInfo::RemoveReference()
{
	MSG_DBFolderInfoHandle_RemoveReference(m_dbFolderInfoHandle);
}

DBFolderInfo::~DBFolderInfo()
{
	MSG_DBFolderInfoHandle_RemoveReference(m_dbFolderInfoHandle);
}

void DBFolderInfo::GetExchangeInfo(MSG_DBFolderInfoExchange &infoExchange)
{
	infoExchange.m_version = m_version;			/* for upgrading...*/
	infoExchange.m_sortType = m_sortType;			/* the last sort type open on this db. */
	infoExchange.m_csid = m_csid;				/* default csid for these messages */
	infoExchange.m_IMAPHierarchySeparator = m_IMAPHierarchySeparator;	/* imap path separator */
	infoExchange.m_sortOrder = m_sortOrder;		/* the last sort order (up or down) */
	infoExchange.m_folderSize = m_folderSize;
	infoExchange.m_folderDate = m_folderDate;
	infoExchange.m_parsedThru = m_parsedThru;		/* how much of the folder have we parsed? Not sure needed in new world order */
	infoExchange.m_expunged_bytes = m_expunged_bytes;	/* sum of size of deleted messages in folder */
	
	infoExchange.m_LastMessageUID = m_LastMessageUID;
	infoExchange.m_ImapUidValidity = m_ImapUidValidity;
	infoExchange.m_TotalPendingMessages = m_TotalPendingMessages;
	infoExchange.m_UnreadPendingMessages = m_UnreadPendingMessages;

	// news only (for now)
	infoExchange.m_articleNumHighWater = m_articleNumHighWater;	/* largest article number whose header we've seen */
	infoExchange.m_expiredMark = m_articleNumHighWater;		/* Highest invalid article number in group - for expiring */
	infoExchange.m_viewType = m_viewType;			/* for news, the last view type open on this db.	*/

	infoExchange.m_numVisibleMessages = m_numVisibleMessages ;	// doesn't include expunged or ignored messages (but does include collapsed).
	infoExchange.m_numNewMessages = m_numNewMessages;
	infoExchange.m_numMessages = m_numMessages;		// includes expunged and ignored messages
	infoExchange.m_flags = m_flags;			// folder specific flags. This holds things like re-use thread pane,
									// configured for off-line use, use default retrieval, purge article/header options
	infoExchange.m_lastMessageLoaded = m_lastMessageLoaded; // set by the FE's to remember the last loaded message

}

void DBFolderInfo::SetExchangeInfo(MSG_DBFolderInfoExchange &infoExchange)
{
	m_version = infoExchange.m_version;		
	m_sortType = infoExchange.m_sortType;	
	m_csid = infoExchange.m_csid ;			
	m_IMAPHierarchySeparator = infoExchange.m_IMAPHierarchySeparator;	
	m_sortOrder = infoExchange.m_sortOrder ;
	m_folderSize = infoExchange.m_folderSize;
	m_folderDate = infoExchange.m_folderDate ;
	m_parsedThru = infoExchange.m_parsedThru;
	m_expunged_bytes = infoExchange.m_expunged_bytes;
	
	m_LastMessageUID = infoExchange.m_LastMessageUID;
	m_ImapUidValidity = infoExchange.m_ImapUidValidity ;
	m_TotalPendingMessages = infoExchange.m_TotalPendingMessages;
	m_UnreadPendingMessages = infoExchange.m_UnreadPendingMessages;

	m_articleNumHighWater = infoExchange.m_articleNumHighWater;	
	m_articleNumHighWater = infoExchange.m_expiredMark ;		
	m_viewType = infoExchange.m_viewType;		

	m_numVisibleMessages = infoExchange.m_numVisibleMessages;	
	m_numNewMessages = infoExchange.m_numNewMessages;
	m_numMessages = infoExchange.m_numMessages ;
	m_flags = infoExchange.m_flags;		
									
	m_lastMessageLoaded = infoExchange.m_lastMessageLoaded;
}

// set highwater mark, if it really is the highest article number we've seen,
// or caller is forcing new highwater mark.
void DBFolderInfo::SetHighWater(MessageKey highWater, Bool force /* = FALSE */)
{
	if (highWater > m_articleNumHighWater || force)
	{
		m_articleNumHighWater = highWater; 
	}
}

// Remember the number of articles we've already expired
void DBFolderInfo::SetExpiredMark(MessageKey expiredKey)
{
	if (expiredKey > m_expiredMark)
	{
		m_expiredMark = expiredKey;
	}
}
void DBFolderInfo::SetViewType(int32 viewType)
{
	if (m_viewType != viewType)
	{
		m_viewType = viewType;
	}
}

void DBFolderInfo::SetSortInfo(SortType sortType, SortOrder sortOrder)
{
	if (m_sortType != sortType || m_sortOrder != sortOrder)
	{
		m_sortType = sortType;
		m_sortOrder = sortOrder;
	}
}

void DBFolderInfo::GetSortInfo(SortType *pSortType, SortOrder *pSortOrder)
{
	*pSortType = (SortType) m_sortType;
	*pSortOrder = (SortOrder) m_sortOrder;
}

int32 DBFolderInfo::GetFlags()
{
	return m_flags;
}

void DBFolderInfo::SetFlags(int32 flags) 
{
	if (m_flags != flags)
	{
		m_flags = flags; 
	}
}

void DBFolderInfo::OrFlags(int32 flags)
{
	m_flags |= flags;
}

void DBFolderInfo::AndFlags(int32 flags)
{
	m_flags &= flags;
}

XP_Bool DBFolderInfo::TestFlag(int32 flags)
{
	return (m_flags & flags) != 0;
}

XP_Bool	DBFolderInfo::AddLaterKey(MessageKey key, time_t until)
{
//	m_lateredKeys.Add(key);
//	m_lateredKeys.Add(until);
//	setDirty();
	return TRUE;
}

int32 DBFolderInfo::GetNumLatered()
{
//	return m_lateredKeys.GetNumElems() / 2;
	return 0;
}

MessageKey DBFolderInfo::GetLateredAt(int32 laterIndex, time_t *pUntil)
{
	MessageKey	retKey = MSG_MESSAGEKEYNONE;

//	if (laterIndex < GetNumLatered())
//	{
//		retKey = m_lateredKeys[laterIndex * 2];
//		if (pUntil)
//			*pUntil = m_lateredKeys[(laterIndex * 2) + 1];
//	}
	return retKey;
}

void DBFolderInfo::RemoveLateredAt(int32 laterIndex)
{
	if (laterIndex < GetNumLatered())
	{
//		m_lateredKeys.RemoveAt(laterIndex, 2);
	}
}

int32 DBFolderInfo::ChangeNumNewMessages(int32 delta)
{
	m_numNewMessages += delta;
	if (m_numNewMessages < 0)
	{
#ifdef DEBUG_bienvenu1
		XP_ASSERT(FALSE);
#endif
		m_numNewMessages = 0;
	}
	return m_numNewMessages;
}

int32 DBFolderInfo::ChangeNumMessages(int32 delta)
{
	m_numMessages += delta;
	if (m_numMessages < 0)
	{
#ifdef DEBUG_bienvenu
		XP_ASSERT(FALSE);
#endif
		m_numMessages = 0;
	}
	return m_numMessages;
}

int32 DBFolderInfo::ChangeNumVisibleMessages(int32 delta)
{
	m_numVisibleMessages += delta;
	if (m_numVisibleMessages < 0)
	{
#ifdef DEBUG_bienvenu
		XP_ASSERT(FALSE);
#endif
		m_numVisibleMessages = 0;
	}
	return m_numVisibleMessages;
}

void DBFolderInfo::SetNewArtsSet(const char *newArts, MSG_DBHandle dbHandle)
{
	MSG_DBFolderInfo_SetNewArtsSet(GetHandle(), newArts, dbHandle);
}

void DBFolderInfo::GetNewArtsSet(XPStringObj &newArts)
{
	char *newArtsStr;
	MSG_DBFolderInfo_GetNewArtsSet(GetHandle(), &newArtsStr);
	newArts.SetStrPtr(newArtsStr);
}

void DBFolderInfo::SetMailboxName(const char *newBoxName)
{
	MSG_DBFolderInfo_SetMailboxName(GetHandle(), newBoxName);
}

void DBFolderInfo::GetMailboxName(XPStringObj &boxName)
{
	char	*mailboxNameStr;
	MSG_DBFolderInfo_GetMailboxName(GetHandle(), &mailboxNameStr);
	boxName.SetStrPtr(mailboxNameStr);
}

void	DBFolderInfo::SetCachedPassword(const char *password, MSG_DBHandle db)
{
	MSG_DBFolderInfo_SetCachedPassword(GetHandle(), password, db);
}

void	DBFolderInfo::GetCachedPassword(XPStringObj &password, MSG_DBHandle db)
{
	char *passwordStr;
	MSG_DBFolderInfo_GetCachedPassword(GetHandle(), &passwordStr, db);
	password.SetStrPtr(passwordStr);
}

NewsFolderInfo::NewsFolderInfo(MSG_DBFolderInfoHandle handle) : DBFolderInfo(handle)
{
	m_newsRCLineDate = 0;
	m_sortType = SortByThread;
	m_newsDB = NULL;
	XP_BZERO(&m_purgeHdrInfo, sizeof(m_purgeHdrInfo));
	m_purgeHdrInfo.m_useDefaults = TRUE;
	m_purgeArtInfo.m_useDefaults = TRUE;
	XP_BZERO(&m_purgeArtInfo, sizeof(m_purgeArtInfo));
	XP_BZERO(&m_retrieveArtInfo, sizeof(m_retrieveArtInfo));
	m_retrieveArtInfo.m_useDefaults = TRUE;
}

NewsFolderInfo::~NewsFolderInfo()
{
}

void NewsFolderInfo::SetKnownArtsSet(const char *knownArts)
{
	MSG_NewsDBFolderInfoHandle_SetKnownArts(GetHandle(), knownArts);
}

void NewsFolderInfo::GetKnownArtsSet(XPStringObj &knownArts)
{
	char	*knownArtsStr;
	MSG_NewsDBFolderInfoHandle_GetKnownArts(GetHandle(), &knownArtsStr);
	knownArts.SetStrPtr(knownArtsStr);
}

void NewsFolderInfo::SetPurgeHeaderInfo(MSG_PurgeInfo *info)
{
	m_purgeHdrInfo = *info;
}

void NewsFolderInfo::SetPurgeArticleInfo(MSG_PurgeInfo *info)
{
	m_purgeArtInfo = *info;
}


void NewsFolderInfo::ClearOfflineRetrievalTerms()
{
}

void NewsFolderInfo::ClearPurgeHeaderTerms()
{
}

void NewsFolderInfo::ClearPurgeArticleTerms()
{
}

MsgERR NewsFolderInfo::GetOfflineRetrievalInfo(MSG_RetrieveArtInfo *retrieveArtInfo)
{
	*retrieveArtInfo = m_retrieveArtInfo;
	return eSUCCESS;
}

void NewsFolderInfo::SetOfflineRetrievalInfo(MSG_RetrieveArtInfo *retrieveArtInfo)
{
	m_retrieveArtInfo = *retrieveArtInfo;
}

MsgERR NewsFolderInfo::GetPurgeHeaderInfo(MSG_PurgeInfo *info)
{
	*info = m_purgeHdrInfo;
	return eSUCCESS;
}

MsgERR NewsFolderInfo::GetPurgeArticleInfo(MSG_PurgeInfo *info)
{
	*info = m_purgeArtInfo;
	return eSUCCESS;
}

extern "C" XP_Bool
msg_IsSummaryValid(const char* pathname, XP_StatStruct* folderst)
{
	XP_Bool result = FALSE;
	MailDB	*pMessageDB;
	char *summary_file_name = WH_FileName((char*) pathname, xpMailFolderSummary);

	if (summary_file_name) 
	{
		if (MailDB::Open(pathname, FALSE, &pMessageDB) != eSUCCESS)
		{
			result = FALSE ;
		}
		else
		{
			if (pMessageDB->m_dbFolderInfo->m_folderSize == folderst->st_size
				&& pMessageDB->m_dbFolderInfo->m_folderDate == folderst->st_mtime)
				result = TRUE;
			pMessageDB->Close();
		}
		FREEIF(summary_file_name);
	}
	return result;
}



// a silly place for this, but it's a silly function.
extern "C" void
msg_SetSummaryValid(const char* pathname, int num, int numunread)
{
  XP_StatStruct folderst;

#ifdef DEBUG
//  XP_ASSERT(lastquery_result && XP_FILENAMECMP(pathname, lastquery_path) == 0);
#endif

  if (!XP_Stat((char*) pathname, &folderst, xpMailFolder)) 
  {
	char *summary_file_name = WH_FileName((char*) pathname, xpMailFolderSummary);
	if (summary_file_name) 
	{
		MailDB::SetFolderInfoValid(pathname, num, numunread);
		FREEIF(summary_file_name);
	}
  }
}


TDBFolderInfoTransfer::TDBFolderInfoTransfer(DBFolderInfo &sourceInfo)
{
	sourceInfo.GetMailboxName(m_mailboxName);
	m_IMAPHierarchySeparator = sourceInfo.GetIMAPHierarchySeparator();
	m_viewType = sourceInfo.GetViewType();
	sourceInfo.GetSortInfo(&m_sortType, &m_sortOrder);
	m_csid = sourceInfo.GetCSID();
	m_flags = sourceInfo.GetFlags();
}

	
void	TDBFolderInfoTransfer::TransferFolderInfo(DBFolderInfo &destinationInfo)
{
	destinationInfo.SetMailboxName(m_mailboxName);
	destinationInfo.SetIMAPHierarchySeparator(m_IMAPHierarchySeparator);
	destinationInfo.SetViewType(m_viewType);
	destinationInfo.SetCSID(m_csid);
	destinationInfo.SetSortInfo(m_sortType, m_sortOrder);
	destinationInfo.SetFlags(m_flags);
}


