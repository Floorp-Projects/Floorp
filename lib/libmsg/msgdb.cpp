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
#include "xp_time.h"
#include "msgdb.h"
#include "msgdbvw.h"
#include "dberror.h"
#include "grpinfo.h"
#include "thrhead.h"
#include "xpgetstr.h"
#include "newsset.h"
#include "msgdbapi.h"

extern "C" 
{
	extern int MK_MSG_MARKREAD_COUNT;
	extern int MK_MSG_DONE_MARKREAD_COUNT;
}

XP_Bool MessageDB::m_cacheEnabled = TRUE;

MessageDBArray /*NEAR*/	*MessageDB::m_dbCache = NULL;

MessageDBArray::MessageDBArray()
{
}

MessageDB::MessageDB()
{
	m_useCount = 0;
	m_dbHandle = NULL;
	m_headerIndex = 0;
	m_addCount = 0;
	m_commitChunk = 200;
	m_dbName = NULL;
	m_dbFolderInfo = NULL;
	m_newSet = NULL;
	m_folderInfo = NULL;
}
//----------------------------------------------------------------------
// GetDBCache
//----------------------------------------------------------------------
MessageDBArray *
MessageDB::GetDBCache()
{
	if (!m_dbCache)
	{
		m_dbCache = new MessageDBArray();
	}
	return m_dbCache;
	
}

void
MessageDB::CleanupCache()
{
	if (m_dbCache) // clean up memory leak
	{
		for (int i = 0; i < GetDBCache()->GetSize(); i++)
		{
			MessageDB* pMessageDB = GetDBCache()->GetAt(i);
			if (pMessageDB)
			{
#ifdef DEBUG_bienvenu
				XP_Trace("closing %s\n", pMessageDB->m_dbName);
#endif
				pMessageDB->ForceClosed();
				i--;	// back up array index, since closing removes db from cache.
			}
		}
		XP_ASSERT(GetNumInCache() == 0);	// better not be any open db's.
		delete m_dbCache;
	}
	m_dbCache = NULL; // Need to reset to NULL since it's a
			  // static global ptr and maybe referenced 
			  // again in other places.
}

MessageDB::~MessageDB()
{
	NotifyAnnouncerGoingAway(NULL);
	Purge();
	if (m_dbName)
		XP_FREE(m_dbName);
	if (m_newSet)
		delete m_newSet;
}

MsgERR MessageDB::MessageDBOpen(const char * dbName, XP_Bool create)
{
	MsgERR err = OpenDB(dbName, create);
	if (err == eSUCCESS)
		m_useCount++;

	return err;
}

MsgERR		MessageDB::Close()
{
	--m_useCount;
	XP_ASSERT(m_useCount >= 0);
	if (m_useCount == 0)
	{
		Purge();
#ifdef DEBUG_bienvenu1
		Verify();
#endif
		CloseDB();
		RemoveFromCache(this);
#ifdef DEBUG_bienvenu
		if (GetNumInCache() != 0)
		{
			XP_Trace("closing %s\n", m_dbName);
			DumpCache();
		}
#endif
		// if this terrifies you, we can make it a static method
		delete this;	
		return(eSUCCESS);
	}
	else
	{
		return(eSUCCESS);
	}
}

// virtual inlines moved to .cpp file to help compiler charity cases.
MsgERR		MessageDB::OnNewPath (const char * /*path*/) { return eSUCCESS; }

// force the database to close - this'll flush out anybody holding onto
// a database without having a listener!
MsgERR MessageDB::ForceClosed()
{
	MsgERR	err = eSUCCESS;

	while (m_useCount > 0 && err == eSUCCESS)
	{
		int32 saveUseCount = m_useCount;
		err = Close();
		if (saveUseCount == 1)
			break;
	}
	return err;
}

// this routine should not leave the database open if it returns an error.
MsgERR MessageDB::OpenDB(const char *dbFileName, XP_Bool create)
{
	MsgERR err = MSG_OpenDB(dbFileName, create, &m_dbHandle, &m_dbFolderInfoHandle);

	XPStringObj newSet;

	if (err == eSUCCESS)
	{
		MSG_DBFolderInfoExchange exchangeInfo;

		if (!m_dbFolderInfoHandle)
			AddNewFolderInfoToDB();

		if (m_dbName)
			XP_FREE(m_dbName);
		m_dbName = XP_STRDUP( dbFileName );

		m_dbFolderInfo =  CreateFolderInfo(m_dbFolderInfoHandle);
		m_dbFolderInfo->SetHandle(m_dbFolderInfoHandle);
		MSG_DBFolderInfo_GetFolderInfo(m_dbFolderInfoHandle, &exchangeInfo);
		m_dbFolderInfo->SetExchangeInfo(exchangeInfo);

		// compare db version filed out to current db version
		if (m_dbFolderInfoHandle != NULL && GetCurVersion() != m_dbFolderInfo->GetDiskVersion())
		{
			CloseDB();
			err = eOldSummaryFile;
		}
		else
		{
			m_dbFolderInfo->GetNewArtsSet(newSet);
			m_newSet = msg_NewsArtSet::Create(newSet);
		}
	}
	return err;
}

MsgERR MessageDB::CloseDB()
{
	if (m_dbHandle != NULL)
	{
		Commit();
		if (m_dbFolderInfo != NULL)
		{
			delete m_dbFolderInfo;
			m_dbFolderInfo = NULL;
		}
		MSG_CloseDB(m_dbHandle);
	}
	return eSUCCESS;
}

DBFolderInfo *MessageDB::AddNewFolderInfoToDB()
{
	m_dbFolderInfo = CreateFolderInfo(0);
//	m_dbFolderInfo->fID = 1;	// one and only newsgroup info
	m_dbFolderInfo->SetHighWater(0);
	m_dbFolderInfo->m_version = GetCurVersion();
//	m_dbFolderInfo->setDirty();
	MSG_AddDBFolderInfo(m_dbHandle, m_dbFolderInfo->GetHandle());
	m_dbFolderInfoHandle = m_dbFolderInfo->GetHandle();
	Commit();
	return m_dbFolderInfo;
}

// Renames the destdb as sourceDB. DestDB could be open, in which case we need to rename it under
// the covers. SourceDB better not be open, since it's going away.
/*static*/  MsgERR		MessageDB::RenameDB(const char *sourceName, const char *destName)
{
	MsgERR	err = eSUCCESS;

#ifdef DEBUG
	{
		char* filename = WH_FileName(sourceName, xpMailFolderSummary);
		XP_ASSERT(filename);
		XP_ASSERT(MessageDB::FindInCache(filename) == NULL);
		FREEIF(filename);
	}
#endif
	MessageDB *destDB;
	{
		char* filename = WH_FileName(destName, xpMailFolderSummary);
		if (!filename) return eOUT_OF_MEMORY;
		destDB =  MessageDB::FindInCache(filename);
		XP_FREE(filename);
	}
	if (destDB)
		err = destDB->CloseDB();

	if (XP_FileRename(sourceName, xpMailFolderSummary, destName, xpMailFolderSummary) == 0)
	{
		if (destDB) {
			char* filename = WH_FileName (destName, xpMailFolderSummary);
			if (!filename) return eOUT_OF_MEMORY;
			err = destDB->OpenDB (filename, FALSE /*create?*/);
			XP_FREE(filename);
		}
	}
	else 
		err = eFAILURE;	// ### dmb - need to come up with better error.
	return err;
}

MsgERR	MessageDB::SetSummaryValid(XP_Bool valid /* = TRUE */)
{
	if (!valid)
	{
		if (m_dbFolderInfo)
		{
			m_dbFolderInfo->m_version = 0;
// DMB TODO			m_dbFolderInfo->setDirty();
			Commit();
		}
		else
			return eFAILURE;	// can't do this w/o a folder info...
	}
	// for default db (and news), there's no nothing to set to make it it valid
	return eSUCCESS;
}

// returns NULL if not a mail db
MailDB	*MessageDB::GetMailDB()
{
	return NULL;
}

// returns NULL if not a news db
NewsGroupDB *MessageDB::GetNewsDB()
{
	return NULL;
}

DBFolderInfo *MessageDB::CreateFolderInfo(MSG_DBFolderInfoHandle handle)
{
	return new DBFolderInfo((handle) ? handle : MSG_CreateMailDBFolderInfo());
}

 
MSG_FolderInfo *MessageDB::GetFolderInfo()
{
	return m_folderInfo;
}

MsgERR MessageDB::GetHeaderFromHandle(MSG_HeaderHandle headerHandle, DBMessageHdr **pResult)
{
	DBMessageHdr *header = new DBMessageHdr(headerHandle);
	*pResult = header;
	return eSUCCESS;
}

DBThreadMessageHdr *MessageDB::GetThreadHeaderFromHandle(MSG_ThreadHandle handle)
{
	return (handle) ? new DBThreadMessageHdr(handle) : 0;
}

MsgERR MessageDB::Commit(XP_Bool compress /* = FALSE */)
{
	MSG_DBFolderInfoExchange exchangeInfo;
	XP_ASSERT(m_dbHandle != NULL);
	char	*outputLine = (m_newSet) ? m_newSet->Output() : 0;
	if (outputLine)
	{
		GetDBFolderInfo()->SetNewArtsSet(outputLine, m_dbHandle);
		delete [] outputLine;
	}
	else
		GetDBFolderInfo()->SetNewArtsSet("", m_dbHandle);

	m_dbFolderInfo->GetExchangeInfo(exchangeInfo);
	MSG_DBFolderInfo_SetFolderInfo(m_dbFolderInfoHandle, &exchangeInfo, m_dbHandle);
	MsgERR err = MSG_CommitDB(m_dbHandle, compress);
	if (err != eSUCCESS)
		return eWRITE_ERROR;

	return eSUCCESS;
}

// Fill in messageHdr from database. Don't change msgHdr if we don't find messageNum.
MsgERR	MessageDB::GetMessageHdr(MessageKey messageKey, MessageHdrStruct *msgHdr)
{
	DBMessageHdr	*headerObject = GetDBHdrForKey(messageKey);
	if (headerObject == NULL)
		return 0;
	else
	{
		XP_Bool	isRead = FALSE;

		headerObject->CopyToMessageHdr(msgHdr, GetDB());
		// unless/until marking read/unread goes through view layer, we need to get unreadness from newsrc
		IsRead(messageKey, &isRead);
		if (isRead)
			msgHdr->m_flags |= kIsRead;

		delete headerObject;
		return eSUCCESS;
	}
}

MsgERR	MessageDB::GetShortMessageHdr(MessageKey messageNum, MSG_MessageLine *msgHdr)
{
	MessageHdrStruct messageHdr;

	XP_MEMSET(&messageHdr, 0, sizeof(messageHdr));
	DBMessageHdr	*headerObject = GetDBHdrForKey(messageNum);
	if (headerObject == NULL)
	{
		return eID_NOT_FOUND;
	}
	else
	{
		XP_Bool	isRead = FALSE;

		headerObject->CopyToShortMessageHdr(msgHdr, GetDB());

		// unless/until marking read/unread goes through view layer, we need to get unreadness from newsrc
		IsRead(messageNum, &isRead);
		if (isRead)
			msgHdr->flags |= kIsRead;

		if (m_newSet && m_newSet->IsMember(headerObject->GetMessageKey()))
			msgHdr->flags |= kNew;

		delete headerObject;
		return eSUCCESS;
	}
}

// This function merely takes a DBMessageHdr
// and stores that in m_newHeaders. From there, it will be threaded
// and added to the database in a separate pass.
MsgERR	MessageDB::AddHeaderToArray(DBMessageHdr *dbMsgHdr)
{
	m_newHeaders.SetAtGrow(m_headerIndex++, dbMsgHdr);
	return eSUCCESS;
}

// given an array of message ids, return list header information for each header
MsgERR		MessageDB::ListHeaders(MessageKey *pMessageNums,
										   int numToList,
										   MessageHdrStruct *pOutput,
										   int *pNumListed)
{
	MsgERR	err = eSUCCESS;
	*pNumListed = 0;

	for (int i = 0; i < numToList; i++)
	{
		err = GetMessageHdr(pMessageNums[i], &pOutput[i]);
		if (err != eSUCCESS)
			break;
		(*pNumListed)++;
	}

	return err;
}
MsgERR		MessageDB::ListHeadersShort(MessageKey *pMessageNums, int numToList, MSG_MessageLine *pOutput, int *pNumListed)
{
	MsgERR	err = eSUCCESS;
	*pNumListed = 0;

	for (int i = 0; i < numToList; i++)
	{
		err = GetShortMessageHdr(pMessageNums[i], &pOutput[i]);
		if (err != eSUCCESS)
			break;
		(*pNumListed)++;
	}

	return err;
}

MsgERR MessageDB::ListNextUnread(ListContext **pContext, DBMessageHdr **pResult)
{
	DBMessageHdr	*pHeader;
	MsgERR			dbErr = eSUCCESS;
	XP_Bool			lastWasRead = TRUE;
	*pResult = NULL;

	while (TRUE)
	{
		if (*pContext == NULL)
			dbErr = ListFirst (pContext, &pHeader);
		else
			dbErr = ListNext(*pContext, &pHeader);

		if (dbErr != eSUCCESS)
		{
			ListDone(*pContext);
			break;
		}

		// this currently doesn't happen since ListNext doesn't return errors
		// other than eDBEndOfList.
		else if (dbErr != eSUCCESS)	 
			break;
		if (IsHeaderRead(pHeader, &lastWasRead) == eSUCCESS && !lastWasRead)
			break;
		else
			delete pHeader;
	}
	if (!lastWasRead)
		*pResult = pHeader;
	return dbErr;
}

MsgERR	MessageDB::ListAllIds(IDArray *outputIds)
{
	MessageKey *resultKeys;
	int32		numKeys;
	MsgERR err = MSG_DBHandle_ListAllKeys(m_dbHandle, &resultKeys, &numKeys);
	outputIds->SetArray(resultKeys, numKeys, numKeys);
	return err;
}

MsgERR	MessageDB::ListAllIds(IDArray &outputIds)
{
	return ListAllIds(&outputIds);
}

MsgERR MessageDB::ListFirst(ListContext **pContext, DBMessageHdr **pResult)
{
	MsgERR	err;

	err = CreateListIterator(TRUE, pContext, pResult);
	if (err == eCorruptDB)
	{
		err = eEXCEPTION;
		SetSummaryValid(FALSE);
	}
	return err;

}

MsgERR MessageDB::ListNext(ListContext *pContext, DBMessageHdr **pResult)
{
	DBMessageHdr	*msgHdr = NULL;
	MsgERR			err;
	MSG_HeaderHandle	headerHandle;

	err = MSG_IteratorHandle_GetNextHeader(pContext->m_iterator, m_dbHandle, &headerHandle);
	if (err == eSUCCESS)
		err = GetHeaderFromHandle(headerHandle, pResult);
	else if (err == eCorruptDB)
		SetSummaryValid(FALSE);

	return err;
}

MsgERR MessageDB::ListLast(ListContext **pContext, DBMessageHdr **pResult)
{
	return CreateListIterator(FALSE, pContext, pResult);
}

// returns the biggest key, kIdNone if none found.
MessageKey	MessageDB::ListHighwaterMark()
{
	return MSG_DBHandle_GetHighwaterMark(m_dbHandle);

}

// Create an iterator that either starts at the beginning and goes towards the end
// (if forward == TRUE), or starts at the end and goes towards the beginning.
MsgERR MessageDB::CreateListIterator(XP_Bool forward, ListContext **pContext, DBMessageHdr **pResult)
{
	MSG_HeaderHandle headerHandle;
	ListContext		*listContext = NULL;

	listContext = new ListContext;
	MsgERR err = MSG_DBHandle_CreateHdrListIterator(m_dbHandle, forward, &listContext->m_iterator, &headerHandle);
	if (err == eSUCCESS)
		err = GetHeaderFromHandle(headerHandle, pResult);
	*pContext = listContext;
	return err;
}

MsgERR MessageDB::ListDone(ListContext *pContext)
{
	if (pContext != NULL)
	{
		if (pContext->m_iterator)
			MSG_IteratorHandle_DestroyIterator(pContext->m_iterator);

		delete pContext;
	}
	return eSUCCESS;
}

/* static */ XP_Bool		MessageDB::MatchFlaggedNotOffline(DBMessageHdr *hdr)
{
	return (hdr->GetFlags() & kMsgMarked) && !(hdr->GetFlags() & kOffline);
}

void MessageDB::ListMatchingKeys(HdrCompareFunc *compareFunc, IDArray &matchingKeys)
{
	MsgERR			dbErr;
	DBMessageHdr	*pHeader;
	ListContext		*listContext = NULL;

	while (TRUE)
	{
		if (listContext == NULL)
			dbErr = ListFirst (&listContext, &pHeader);
		else
			dbErr = ListNext(listContext, &pHeader);

		if (dbErr == eDBEndOfList)
		{
			dbErr = eSUCCESS;
			ListDone(listContext);
			break;
		}
		// this currently doesn't happen since ListNext doesn't return errors
		// other than eDBEndOfList.
		else if (dbErr != eSUCCESS)	 
			break;
		if ((*compareFunc)(pHeader))
			matchingKeys.Add(pHeader->GetMessageKey());
		delete pHeader;
	}
}

// list the ids of the top-level thread ids starting at id == startMsg. This actually returns
// the ids of the first message in each thread.
MsgERR		MessageDB::ListThreadIds(MessageKey *startMsg, XP_Bool unreadOnly, MessageKey *pOutput, int32 *pFlags, char *pLevels, 
									 int numToList, int *pNumListed, MessageDBView *view, int32 *pTotalHeaders)
{
	MsgERR			err = eSUCCESS;
	DBMessageHdr	*msgHdr;
	MSG_ThreadHandle	threadHandle = NULL;
	DBThreadMessageHdr * threadHdr = NULL;
	// N.B..don't ret before assigning numListed to *pNumListed
	int				numListed = 0;

	if (*startMsg > 0)
	{
		XP_ASSERT(m_threadIterator != NULL);	// for now, we'll just have to rely on the caller leaving
									// the iterator in the right place.
		err = MSG_IteratorHandle_GetNextThread(m_threadIterator, m_dbHandle, &threadHandle);
	}
	else
	{
		MSG_DBHandle_CreateThreadListIterator(m_dbHandle, TRUE, &m_threadIterator, &threadHandle);
	}

	if (!threadHandle)
	{
		if (*startMsg > 0)
			err = eID_NOT_FOUND;
	}
	else
	{

		threadHdr =  new DBThreadMessageHdr(threadHandle);
		int32	threadCount;
		int32	threadsRemoved = 0;
		for (int i = 0; i < numToList && threadHdr != NULL; i++)
		{
			MSG_ThreadHandle nextThreadHandle = NULL;
			DBThreadMessageHdr *nextThreadHdr = NULL;
			err = MSG_IteratorHandle_GetNextThread(m_threadIterator, m_dbHandle, &nextThreadHandle);
			if (err == eCorruptDB)
				break;
			else if (err == eDBEndOfList)
				err = eSUCCESS;
			if (nextThreadHandle)
				nextThreadHdr = new DBThreadMessageHdr(nextThreadHandle);
			else
				nextThreadHdr = NULL;

			if (threadHdr->GetNumChildren() != 0)	// not empty thread
			{
				if (pTotalHeaders)
					*pTotalHeaders += threadHdr->GetNumChildren();
				if (unreadOnly)
					msgHdr = threadHdr->GetFirstUnreadChild(this);
				else
					msgHdr = threadHdr->GetChildHdrAt(0);
				uint32 threadFlags = threadHdr->GetFlags();

				if (msgHdr != NULL && (!view || view->WantsThisThread(threadHdr)))
				{
					pOutput[numListed] = msgHdr->GetMessageKey();
					pLevels[numListed] = 0 /* msgHdr->GetLevel() */;
					// DMB TODO - This will do for now...Until we decide how to
					// handle thread flags vs. message flags, if we do decide
					// to make them different.
					msgHdr->OrFlags(threadFlags & (kWatched | kIgnored));
					XP_Bool	isRead = FALSE;

					// make sure DB agrees with newsrc, if we're news.
					IsRead(msgHdr->GetMessageKey(), &isRead);
					MarkHdrRead(msgHdr, isRead, NULL);
					// try adding in kIsThread flag for unreadonly view.
					if (GetThreadCount(threadHdr, &threadCount) == eSUCCESS && threadCount > 1)
						pFlags[numListed] |= kHasChildren;
					pFlags[numListed] = msgHdr->m_flags | kIsThread | threadFlags;

					numListed++;
				}
				delete msgHdr;
			}
			else if (threadsRemoved < 10 && !(threadHdr->GetFlags() & (kWatched | kIgnored)))
			{
				MSG_DBHandle_RemoveThread(m_dbHandle, threadHandle);
				threadsRemoved++;	// don't want to remove all empty threads first time
									// around as it will choke preformance for upgrade.
#ifdef DEBUG_bienvenu
				XP_Trace("removing empty non-ignored non-watched thread\n");
#endif
			}
			delete threadHdr;
			threadHdr = nextThreadHdr;
		}
	}	
	if (threadHdr != NULL)
	{
		*startMsg = threadHdr->GetThreadID();
		delete threadHdr;
	}
	else
	{
		*startMsg = kIdNone;
		MSG_IteratorHandle_DestroyIterator(m_threadIterator);
		m_threadIterator = NULL;
	}
	*pNumListed = numListed;
	return err;
}

// helper function to get the thread list context from a thread id and start msg.
// If successful, pThreadHdr will be non null on return
MsgERR		MessageDB::GetDBThreadListContext(MessageKey threadId, MessageKey startMsg, DBThreadMessageHdr **pThreadHdr, uint16 *pThreadIndex)
{
	*pThreadIndex = 0;
	*pThreadHdr = GetDBThreadHdrForThreadID(threadId);
	if (*pThreadHdr == NULL)
		return eID_NOT_FOUND;

	return GetMessageIndexInThread(*pThreadHdr, startMsg, pThreadIndex);
}

MsgERR	MessageDB::GetMessageIndexInThread(DBThreadMessageHdr *threadHdr, MessageKey startMsg, uint16 *pThreadIndex)
{
	XP_Bool	foundStartMsg = FALSE;

	if (startMsg != kIdNone && startMsg != 0)
	{
		while (*pThreadIndex < threadHdr->GetNumChildren())
		{
			if (threadHdr->GetChildAt(*pThreadIndex) == startMsg)
			{
				foundStartMsg = TRUE;
				break;
			}
			(*pThreadIndex)++;
		}
	}
	else
	{
		*pThreadIndex = 0;
		foundStartMsg = TRUE;
	}
	return (foundStartMsg) ? eSUCCESS : eID_NOT_FOUND;
}

// overloaded helper function to get the thread list context from a msgHdr and start msg.
// If successful, pThreadHdr will be non null on return
MsgERR		MessageDB::GetDBThreadListContext(DBMessageHdr *msgHdr, MessageKey startMsg, DBThreadMessageHdr **pThreadHdr, uint16 *pThreadIndex)
{
	*pThreadIndex = 0;
	*pThreadHdr = GetDBThreadHdrForMsgHdr(msgHdr);
	if (*pThreadHdr == NULL)
		return eID_NOT_FOUND;

	return GetMessageIndexInThread(*pThreadHdr, startMsg, pThreadIndex);
}



// Overloaded ListThreadIds which takes a ListContext
MsgERR		MessageDB::ListThreadIds(ListContext * /*context*/,
											 MessageKey * /*pOutput*/,
											 int /*numToList*/,
											 int * /*numListed*/)
{
	return eNYI;
}

MsgERR	MessageDB::ListUnreadIdsInThread(MessageKey threadId, MessageKey *startMsg, XPByteArray &levelStack, int numToList, MessageKey *pOutput, char *pFlags, char *pLevels, int *pNumListed)
{
	MsgERR	err;
	uint16	threadIndex = 0;
	DBThreadMessageHdr	*threadHdr;

	*pNumListed = 0;

	err = GetDBThreadListContext(threadId, *startMsg, &threadHdr, &threadIndex);
	if (err != eSUCCESS)
		return err;
	// these children ids are in thread order.
	int i;
	int startNumListed = *pNumListed;
	for (i = 0; i + threadIndex < threadHdr->GetNumChildren() && (*pNumListed - startNumListed) < numToList; i++)
	{
		DBMessageHdr *msgHdr = threadHdr->GetChildHdrAt(i + threadIndex);
		if (msgHdr != NULL)
		{
			// if the current header's level is <= to the top of the level stack,
			// pop off the top of the stack.
			// ### dmb unreadonly - The level stack needs to work across calls
			// to this routine, in the case that we have more than 200 unread
			// messages in a thread.
			while (levelStack.GetSize() > 1 && 
						msgHdr->GetLevel()  <= levelStack.GetAt(levelStack.GetSize() - 1))
			{
				levelStack.RemoveAt(levelStack.GetSize() - 1);
			}

			if (! (msgHdr->GetFlags() & kExpunged))
			{
				XP_Bool isRead = FALSE;
				IsRead(msgHdr->GetMessageKey(), &isRead);
				if (!isRead)
				{
					uint8 levelToAdd;
					// just make sure flag is right in db.
					MarkHdrRead(msgHdr, FALSE, NULL);
					*pOutput++ = msgHdr->GetMessageKey();	// was fId DMB TODO
					*pFlags = 0;
//					pLevels[i] = msgHdr->GetLevel();
					if (levelStack.GetSize() == 0)
						levelToAdd = 0;
					else
						levelToAdd = levelStack.GetAt(levelStack.GetSize() - 1) + 1;
					*pLevels++ = levelToAdd;
#ifdef DEBUG_bienvenu
//					XP_Trace("added at level %d\n", levelToAdd);
#endif
					levelStack.Add(levelToAdd);
					MessageDBView::CopyDBFlagsToExtraFlags(msgHdr->m_flags, pFlags);
					pFlags++;
					(*pNumListed)++;
				}
			}
			delete msgHdr;
		}
	}
	if ((i + threadIndex) < threadHdr->GetNumChildren())
		*startMsg = threadHdr->GetChildAt(i + threadIndex);
	else
		*startMsg = kIdNone;

	delete threadHdr;
	return eSUCCESS;
}

MsgERR		MessageDB::ListIdsInThread(DBMessageHdr *msgHdr, MessageKey *startMsg, int numToList, MessageKey *pOutput, char *pFlags, char *pLevels, int *pNumListed)
{
	uint16	threadIndex = 0;
	DBThreadMessageHdr	*threadHdr;
	MsgERR	err;

	err = GetDBThreadListContext(msgHdr, *startMsg, &threadHdr, &threadIndex);
	if (err != eSUCCESS)
		return err;
	return ListIdsInThread(threadHdr, threadIndex, startMsg, numToList, pOutput, pFlags, pLevels, pNumListed);
}

MsgERR		MessageDB::ListIdsInThread(MessageKey threadId, MessageKey *startMsg, int numToList, MessageKey *pOutput, char *pFlags, char *pLevels, int *pNumListed)
{
	uint16	threadIndex = 0;
	DBThreadMessageHdr	*threadHdr;
	MsgERR	err;

	*pNumListed = 0;

	err = GetDBThreadListContext(threadId, *startMsg, &threadHdr, &threadIndex);
	if (err != eSUCCESS)
		return err;

	return ListIdsInThread(threadHdr, threadIndex, startMsg, numToList, pOutput, pFlags, pLevels, pNumListed);
}

MsgERR	MessageDB::ListIdsInThread(DBThreadMessageHdr *threadHdr, uint16 threadIndex, MessageKey *startMsg, int numToList, MessageKey *pOutput, char *pFlags, char *pLevels, int *pNumListed)
{
	// these children ids should be in thread order.
	int i;

	*pNumListed = 0;

	for (i = 0; i + threadIndex < threadHdr->GetNumChildren() && i < numToList; i++)
	{
		DBMessageHdr *msgHdr = threadHdr->GetChildHdrAt(i + threadIndex);
		if (msgHdr != NULL)
		{
			if (! (msgHdr->GetFlags() & kExpunged))
			{
				XP_Bool isRead = FALSE;
				IsRead(msgHdr->GetMessageKey(), &isRead);
				// just make sure flag is right in db.
				MarkHdrRead(msgHdr, isRead, NULL);
//				if (isRead)
//					msgHdr->m_flags |= kIsRead;
//				else
//					msgHdr->m_flags &= ~kIsRead;
				*pOutput++ = msgHdr->GetMessageKey();
				pFlags[i] = 0;
				pLevels[i] = msgHdr->GetLevel();
				// turn off thread or elided bit if they got turned on (maybe from new only view?)
				if (i > 0)	
					msgHdr->AndFlags(~(kIsThread|kElided));
				MessageDBView::CopyDBFlagsToExtraFlags(msgHdr->m_flags, &pFlags[i]);
				(*pNumListed)++;
			}
			else
			{
				XP_ASSERT(FALSE);	// shouldn't happen - expunging should remove
			}
			delete msgHdr;
		}
	}
	if ((i + threadIndex) < threadHdr->GetNumChildren())
		*startMsg = threadHdr->GetChildAt(i + threadIndex);
	else
		*startMsg = kIdNone;

	delete threadHdr;
	return eSUCCESS;
}

MsgERR		MessageDB::GetThreadCount(MessageKey messageKey, int32 *pThreadCount)
{
	MsgERR	ret = eID_NOT_FOUND;
	DBThreadMessageHdr *threadHdr = GetDBThreadHdrForMsgID(messageKey);
	if (threadHdr != NULL)
	{
		ret = GetThreadCount(threadHdr, pThreadCount);
		delete threadHdr;
	}
	return ret;
}

MsgERR MessageDB::GetThreadCount(DBThreadMessageHdr *threadHdr, int32 *pThreadCount)
{
	*pThreadCount = threadHdr->GetNumChildren();
	return eSUCCESS;
}

MessageKey MessageDB::GetKeyOfFirstMsgInThread(MessageKey key)
{
	DBThreadMessageHdr *threadHdr = GetDBThreadHdrForMsgID(key);
	MessageKey	firstKeyInThread = kIdNone;

	if (threadHdr == NULL)
	{
		//XP_ASSERT(FALSE); (rb) message not found, deleted already but delete key hit too fast for us
		return firstKeyInThread;
	}
	// ### dmb UnreadOnly - this is wrong.
	firstKeyInThread = threadHdr->GetChildAt(0);
	delete threadHdr;
	return firstKeyInThread;
}

XP_Bool MessageDB::SetPriority(MessageKey key, MSG_PRIORITY priority)
{
	XP_Bool	ret;

	DBMessageHdr *msgHdr = GetDBHdrForKey(key);
	if (msgHdr == NULL)
		return FALSE;
	ret = SetPriority(msgHdr, priority);
	delete msgHdr;
	return ret;
}

XP_Bool MessageDB::SetPriority(DBMessageHdr *msgHdr, MSG_PRIORITY priority)
{
	if ((GetMailDB() != NULL))
	{
		msgHdr->SetPriority(priority);
		// ###dmb calling SetHdrFlag (on mailDB) this way will basically flush the new mozilla 
		// status which is all we want. Should invent new method. Also, we should only
		// call this on maildbs, because other db's will leave dirty flag set.
		SetHdrFlag(msgHdr, TRUE, (MsgFlags) 0);
		// ###tw When we decide what our priority header and format will be, 
		// this code should also fix the header in the mail msg, 
		// just to be extra paranoid.
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


// Helper routine - lowest level of flag setting
void MessageDB::SetHdrFlag(DBMessageHdr *msgHdr, XP_Bool bSet, MsgFlags flag)
{
	XP_ASSERT(! (flag & kDirty));	// this won't do the right thing so don't.

	if (bSet && (!(msgHdr->GetFlags() & flag)))
	{
		msgHdr->OrFlags(flag | kDirty);
	}
	else if (!bSet && (msgHdr->GetFlags() & flag))
	{
		msgHdr->AndFlags(~flag);
		msgHdr->OrFlags(kDirty);
	}
}

void MessageDB::MarkHdrReadInDB(DBMessageHdr *msgHdr, XP_Bool bRead,
								ChangeListener *instigator)
{
	SetHdrFlag(msgHdr, bRead, kIsRead);
	if (m_newSet)
		m_newSet->Remove(msgHdr->GetMessageKey());
	if (m_dbFolderInfo != NULL)
	{
		if (bRead)
			m_dbFolderInfo->ChangeNumNewMessages(-1);
		else
			m_dbFolderInfo->ChangeNumNewMessages(1);
// DMB TODO		m_dbFolderInfo->setDirty();
	}

	NotifyKeyChangeAll(msgHdr->GetMessageKey(), msgHdr->GetFlags(), instigator);
}

MsgERR MessageDB::MarkRead(MessageKey messageKey, XP_Bool bRead, 
						   ChangeListener *instigator)
{
	MsgERR			err;
	DBMessageHdr *msgHdr = GetDBHdrForKey(messageKey);
	if (msgHdr == NULL)
		return eID_NOT_FOUND;

	err = MarkHdrRead(msgHdr, bRead, instigator);
	delete msgHdr;
	return err;
}

MsgERR MessageDB::MarkReplied(MessageKey messageKey, XP_Bool bReplied, 
								ChangeListener *instigator /* = NULL */)
{
	return SetKeyFlag(messageKey, bReplied, kReplied, instigator);
}

MsgERR MessageDB::MarkForwarded(MessageKey messageKey, XP_Bool bForwarded, 
								ChangeListener *instigator /* = NULL */) 
{
	return SetKeyFlag(messageKey, bForwarded, kForwarded, instigator);
}

MsgERR MessageDB::MarkHasAttachments(MessageKey messageKey, XP_Bool bHasAttachments, 
								ChangeListener *instigator)
{
	return SetKeyFlag(messageKey, bHasAttachments, kHasAttachment, instigator);
}

MsgERR		MessageDB::MarkMarked(MessageKey messageKey, XP_Bool mark,
										ChangeListener *instigator)
{
	return SetKeyFlag(messageKey, mark, kMsgMarked, instigator);
}

MsgERR		MessageDB::MarkOffline(MessageKey messageKey, XP_Bool offline,
										ChangeListener *instigator)
{
	return SetKeyFlag(messageKey, offline, kOffline, instigator);
}


MsgERR		MessageDB::MarkImapDeleted(MessageKey messageKey, XP_Bool deleted,
										ChangeListener *instigator)
{
	return SetKeyFlag(messageKey, deleted, kIMAPdeleted, instigator);
}

MsgERR MessageDB::MarkMDNNeeded(MessageKey messageKey, XP_Bool bNeeded, 
								ChangeListener *instigator /* = NULL */)
{
	return SetKeyFlag(messageKey, bNeeded, kMDNNeeded, instigator);
}

MsgERR MessageDB::IsMDNNeeded(MessageKey messageKey, XP_Bool *pNeeded)
{
	MsgERR err = eSUCCESS;
	DBMessageHdr *msgHdr = GetDBHdrForKey(messageKey);
	if (msgHdr != NULL && pNeeded)
	{
		*pNeeded = ((msgHdr->GetFlags() & kMDNNeeded) == kMDNNeeded);
		delete msgHdr;
		return err;
	}
	else
	{
		return eID_NOT_FOUND;
	}
}


MsgERR MessageDB::MarkMDNSent(MessageKey messageKey, XP_Bool bSent, 
							  ChangeListener *instigator /* = NULL */)
{
	return SetKeyFlag(messageKey, bSent, kMDNSent, instigator);
}


MsgERR MessageDB::IsMDNSent(MessageKey messageKey, XP_Bool *pSent)
{
	MsgERR err = eSUCCESS;
	DBMessageHdr *msgHdr = GetDBHdrForKey(messageKey);
	if (msgHdr != NULL && pSent)
	{
		*pSent = msgHdr->GetFlags() & kMDNSent;
		delete msgHdr;
		return err;
	}
	else
	{
		return eID_NOT_FOUND;
	}
}


MsgERR	MessageDB::SetKeyFlag(MessageKey messageKey, XP_Bool set, int32 flag,
							  ChangeListener *instigator)
{
	MsgERR			err = eSUCCESS;
	DBMessageHdr *msgHdr = GetDBHdrForKey(messageKey);
	if (msgHdr == NULL)
		return eID_NOT_FOUND;

	SetHdrFlag(msgHdr, set, flag);

	NotifyKeyChangeAll(msgHdr->GetMessageKey(), msgHdr->GetFlags(), instigator);

	delete msgHdr;
	return err;
}

MsgERR MessageDB::MarkHdrRead(DBMessageHdr *msgHdr, XP_Bool bRead, 
						   ChangeListener *instigator)
{
	XP_Bool	isRead;
	IsHeaderRead(msgHdr, &isRead);
	// if the flag is already correct in the db, don't change it
	if (!!isRead != !!bRead)
	{
		DBThreadMessageHdr *threadHdr = GetDBThreadHdrForMsgID(msgHdr->GetMessageKey());
		if (threadHdr != NULL)
		{
			threadHdr->MarkChildRead(bRead, m_dbHandle);
			delete threadHdr;
		}
		MarkHdrReadInDB(msgHdr, bRead, instigator);
	}
	return eSUCCESS;
}

MsgERR MessageDB::MarkAllRead(MWContext *context, IDArray *thoseMarked)
{
	MsgERR			dbErr;
	DBMessageHdr	*pHeader;
	ListContext		*listContext = NULL;
	int32			numChanged = 0;
	char			msgBuf[100];
	const char *	msgTemplate = XP_GetString(MK_MSG_MARKREAD_COUNT);

	while (TRUE)
	{
		dbErr = ListNextUnread(&listContext, &pHeader);
		if (dbErr == eDBEndOfList)
		{
			dbErr = eSUCCESS;
			break;
		}
		// this currently doesn't happen since ListNext doesn't return errors
		// other than eDBEndOfList.
		else if (dbErr != eSUCCESS || !pHeader)	 
			break;
		
		if (numChanged % 10 == 0)
		{
			PR_snprintf (msgBuf, sizeof(msgBuf), msgTemplate, numChanged);
			FE_Progress (context, msgBuf);
		}
		if (thoseMarked)
			thoseMarked->Add(pHeader->GetMessageKey());
		dbErr = MarkHdrRead(pHeader, TRUE, NULL); 	// ### dmb - blow off error?
		delete pHeader;
		if (numChanged++ % 200 == 0)	// commit every once in a while
			Commit();
	}
	// force num new to 0.
	m_dbFolderInfo->ChangeNumNewMessages(-m_dbFolderInfo->GetNumNewMessages());
//	DMB TODO m_dbFolderInfo->setDirty();	
	msgTemplate = XP_GetString(MK_MSG_DONE_MARKREAD_COUNT);
	PR_snprintf (msgBuf, sizeof(msgBuf), msgTemplate, numChanged);
	FE_Progress (context, msgBuf);
	return dbErr;
}

MsgERR MessageDB::MarkReadByDate (time_t startDate, time_t endDate, MWContext *context, IDArray *markedIds)
{
	MsgERR			dbErr;
	DBMessageHdr	*pHeader;
	ListContext		*listContext = NULL;
	int32			numChanged = 0;
	char			msgBuf[100];
	const char *	msgTemplate = XP_GetString(MK_MSG_MARKREAD_COUNT);

	while (TRUE)
	{
		if (listContext == NULL)
			dbErr = ListFirst (&listContext, &pHeader);
		else
			dbErr = ListNext(listContext, &pHeader);

		if (dbErr == eDBEndOfList)
		{
			dbErr = eSUCCESS;
			ListDone(listContext);
			break;
		}
		// this currently doesn't happen since ListNext doesn't return errors
		// other than eDBEndOfList.
		else if (dbErr != eSUCCESS)	 
			break;
		time_t headerDate = pHeader->GetDate();
		if (headerDate > startDate && headerDate <= endDate)
		{
			XP_Bool isRead;
			IsRead(pHeader->GetMessageKey(), &isRead);
			if (!isRead)
			{
				if (markedIds)
					markedIds->Add(pHeader->GetMessageKey());
				MarkHdrRead(pHeader, TRUE, NULL);	// ### dmb - blow off error?
				if (numChanged % 10 == 0)
				{
					PR_snprintf (msgBuf, sizeof(msgBuf), msgTemplate, numChanged);
					FE_Progress (context, msgBuf);
				}
				if (numChanged++ % 1000 == 0)	// commit every once in a while
					Commit();
			}
		}
		delete pHeader;
	}
	msgTemplate = XP_GetString(MK_MSG_DONE_MARKREAD_COUNT);
	PR_snprintf (msgBuf, sizeof(msgBuf), msgTemplate, numChanged);
	FE_Progress (context, msgBuf);
	return dbErr;
}

MsgERR MessageDB::MarkLater(MessageKey messageKey, time_t until)
{
	XP_ASSERT(m_dbFolderInfo);
	if (m_dbFolderInfo != NULL)
	{
		m_dbFolderInfo->AddLaterKey(messageKey, until);
	}
	return eSUCCESS;
}

void MessageDB::ClearNewList(XP_Bool notify /* = FALSE */)
{
	if (m_newSet)
	{
		if (notify)	// need to update view
		{
			int32 firstMember;
			while ((firstMember = m_newSet->GetFirstMember()) != 0)
			{
				m_newSet->Remove(firstMember);	// this bites, since this will cause us to regen new list many times.
				DBMessageHdr *msgHdr = GetDBHdrForKey(firstMember);
				if (msgHdr != NULL)
				{
					NotifyKeyChangeAll(msgHdr->GetMessageKey(), msgHdr->GetFlags(), NULL);
					delete msgHdr;
				}
			}
		}
		delete m_newSet;
		m_newSet = NULL;
	}
}

XP_Bool		MessageDB::HasNew()
{
	return m_newSet && m_newSet->getLength() > 0;
}

MessageKey	MessageDB::GetFirstNew()
{
	// even though getLength is supposedly for debugging only, it's the only
	// way I can tell if the set is empty (as opposed to having a member 0.
	if (HasNew())
		return m_newSet->GetFirstMember();
	else
		return MSG_MESSAGEKEYNONE;
}

MessageKey	MessageDB::GetUnusedFakeId()
{
	ListContext *listContext = NULL;
	DBMessageHdr *highHdr = NULL;
	MessageKey fakeMsgKey = kIdStartOfFake;

	if (ListLast(&listContext, &highHdr) == eSUCCESS)
	{
		MessageKey curKey = highHdr->GetMessageKey();

		while (curKey == fakeMsgKey || curKey == kIdNone || curKey == kIdPending)
		{
			if (curKey == fakeMsgKey) fakeMsgKey--;
			delete highHdr;
			highHdr = NULL;
			if (ListNext(listContext, &highHdr) == eSUCCESS)
				curKey = highHdr->GetMessageKey();
			else
				break;
		}
		if (highHdr) 
			delete highHdr;
		ListDone(listContext);
	}

	return fakeMsgKey;
}

MsgERR MessageDB::GetUnreadKeyInThread(MessageKey threadId, MessageKey *resultKey,
									   MessageKey *resultThreadId)
{
	MsgERR err = eSUCCESS;
	DBThreadMessageHdr *threadHdr = GetDBThreadHdrForMsgID(threadId);
	if (threadHdr == NULL)
	{
#ifdef DEBUG_bienvenu
		XP_ASSERT(FALSE);
#endif
		return eID_NOT_FOUND;
	}
	if (threadHdr->GetNumNewChildren() > 0)
	{
		MessageKey	startMsg = kIdNone;
		int			numListed;
		do
		{
			const int listChunk = 200;
			MessageKey	listIDs[listChunk];
			char		listFlags[listChunk];
			char		listLevels[listChunk];

			err = ListIdsInThread(threadHdr->GetThreadID(),  &startMsg, listChunk, 
												listIDs, listFlags, listLevels, &numListed);

			// start at 1, because id 0 is the thread header itself.
			for (int i = 1; i < numListed; i++)
			{
				if (!(listFlags[i] & kIsRead))
				{
					*resultKey = listIDs[i];
					if (resultThreadId)
						*resultThreadId = threadId;
					break;
				}
			}
			if (numListed < listChunk || startMsg == kIdNone)
				break;
		}
		while (err == eSUCCESS && (*resultKey == kIdNone));
	}
	delete threadHdr;
	return err;
}

MsgERR MessageDB::DeleteMessages(IDArray &messageKeys, ChangeListener *instigator)
{
	MsgERR	err = eSUCCESS;
	for (uint index = 0; index < messageKeys.GetSize(); index++)
	{
		MessageKey messageKey = messageKeys.GetAt(index);
		DBMessageHdr *msgHdr = GetDBHdrForKey(messageKey);
		if (msgHdr == NULL)
		{
			err = eID_NOT_FOUND;
			break;
		}
		err = DeleteHeader(msgHdr, instigator, index % 300 == 0);
		delete msgHdr;
		if (err != eSUCCESS)
			break;
	}
	Commit();
	return err;
}


XP_Bool MessageDB::AllMessageKeysImapDeleted(const IDArray &messageKeys)
{
	XP_Bool allDeleted = TRUE;
	for (uint index = 0; allDeleted && (index < messageKeys.GetSize()); index++)
	{
		DBMessageHdr *msgHdr = GetDBHdrForKey(messageKeys.GetAt(index));
		allDeleted = msgHdr && ((msgHdr->GetFlags() & kIMAPdeleted) != 0);
		delete msgHdr;
	}
	
	return allDeleted;
}

MsgERR  MessageDB::DeleteMessage(MessageKey messageKey, ChangeListener *instigator, XP_Bool commit)
{
	DBMessageHdr *msgHdr = GetDBHdrForKey(messageKey);
	if (msgHdr == NULL)
		return eID_NOT_FOUND;

	MsgERR ret = DeleteHeader(msgHdr, instigator, commit);
	delete msgHdr;
	return ret;
}

MsgERR MessageDB::DeleteHeader(DBMessageHdr *msgHdr, ChangeListener *instigator, XP_Bool commit, XP_Bool /* onlyRemoveFromThread */)
{
	MessageKey	messageKey = msgHdr->GetMessageKey();
	// only need to do this for mail - will this speed up news expiration? Is dirtying objects
	// we're about to delete slow for ? But are we short circuiting some
	// notifications that we need?
//	if (GetMailDB())
		SetHdrFlag(msgHdr, TRUE, kExpunged);	// tell mailbox (mail)

	if (m_newSet)	// if it's in the new set, better get rid of it.
		m_newSet->Remove(msgHdr->GetMessageKey());

	if (m_dbFolderInfo != NULL)
	{
		XP_Bool isRead;
		m_dbFolderInfo->ChangeNumMessages(-1);
		m_dbFolderInfo->ChangeNumVisibleMessages(-1);
		IsRead(msgHdr->GetMessageKey(), &isRead);
		if (!isRead)
			m_dbFolderInfo->ChangeNumNewMessages(-1);

		m_dbFolderInfo->m_expunged_bytes += msgHdr->GetByteLength();
// DMB TODO		m_dbFolderInfo->setDirty();

	}	
	NotifyKeyChangeAll(messageKey, msgHdr->GetFlags(), instigator); // tell listeners

//	if (!onlyRemoveFromThread)	// to speed up expiration, try this. But really need to do this in RemoveHeaderFromDB
		RemoveHeaderFromDB(msgHdr);
	if (commit)
		Commit();			// ### dmb is this a good time to commit?
	return eSUCCESS;
}

MsgERR MessageDB::UndoDelete(DBMessageHdr *msgHdr)
{
	MsgERR msgErr = AddHdrToDB(msgHdr, NULL, TRUE);
	// make sure message is undeleted from source mail folder.
	// Need to pretend that it's deleted first to reverse it.
	msgHdr->OrFlags(kExpunged);
	SetHdrFlag(msgHdr, FALSE, kExpunged);

	if (m_dbFolderInfo)
	{
		m_dbFolderInfo->m_expunged_bytes -= msgHdr->GetByteLength();
// DMB TODO		m_dbFolderInfo->setDirty();
	}

	return msgErr;
}

// This is a lower level routine which doesn't send notifcations or
// update folder info. One use is when a rule fires moving a header
// from one db to another, to remove it from the first db.

void MessageDB::RemoveHeaderFromDB(DBMessageHdr *msgHdr)
{
	// DMB TODO
//	if (msgHdr->fMark == 0)	// msghdr is not in DB!
//		return;
	DBThreadMessageHdr *threadHdr = GetDBThreadHdrForMsgID(msgHdr->GetMessageKey());
	if (threadHdr != NULL)
	{
		threadHdr->RemoveChild(msgHdr->GetMessageKey(), m_dbHandle);
		// remove empty thread object if it isn't watched or ignored
		if (threadHdr->GetNumChildren() == 0 && !(threadHdr->GetFlags() & (kWatched | kIgnored)))
			MSG_DBHandle_RemoveThread(m_dbHandle, threadHdr->GetHandle());

		delete threadHdr;
	}
	MSG_DBHandle_RemoveHeader(m_dbHandle, msgHdr->GetHandle());
}

MsgERR MessageDB::MarkThreadIgnored(DBThreadMessageHdr *threadHdr, MessageKey messageKey, XP_Bool bIgnored,
							  ChangeListener *instigator)
{
	if (bIgnored)
	{
		threadHdr->OrFlags(kIgnored);
		threadHdr->AndFlags(~kWatched);	// ignore is implicit un-watch
	}
	else
		threadHdr->AndFlags(~kIgnored);
	NotifyKeyChangeAll(messageKey, threadHdr->GetFlags(), instigator);
	return eSUCCESS;
}

MsgERR MessageDB::MarkThreadWatched(DBThreadMessageHdr *threadHdr, MessageKey messageKey, XP_Bool bWatched, 
							  ChangeListener *instigator)
{
	if (bWatched)
	{
		threadHdr->AndFlags(~kIgnored);
		threadHdr->OrFlags(kWatched);
	}
	else
		threadHdr->AndFlags(~kWatched);
	NotifyKeyChangeAll(messageKey, threadHdr->GetFlags(), instigator);
	return eSUCCESS;
}

MsgERR MessageDB::IsMarked(MessageKey messageKey, XP_Bool *pMarked)
{
	DBMessageHdr *msgHdr = GetDBHdrForKey(messageKey);
	if (msgHdr != NULL)
	{
		*pMarked = (msgHdr->GetFlags() & kMsgMarked) != 0;
		delete msgHdr;
		return eSUCCESS;
	}
	else
	{
		return eID_NOT_FOUND;
	}
}


MsgERR MessageDB::IsRead(MessageKey messageKey, XP_Bool *pRead)
{
	MsgERR err = eSUCCESS;
	DBMessageHdr *msgHdr = GetDBHdrForKey(messageKey);
	if (msgHdr != NULL)
	{
		err = IsHeaderRead(msgHdr, pRead);
		delete msgHdr;
		return err;
	}
	else
	{
		return eID_NOT_FOUND;
	}
}

uint32	MessageDB::GetStatusFlags(DBMessageHdr *msgHdr)
{
	uint32	statusFlags = msgHdr->GetFlags();
	XP_Bool	isRead;

	if (m_newSet && m_newSet->IsMember(msgHdr->GetMessageKey()))
		statusFlags |= kNew;
	if (IsRead(msgHdr->GetMessageKey(), &isRead) == eSUCCESS && isRead)
		statusFlags |= kIsRead;
	return statusFlags;
}

MsgERR MessageDB::IsHeaderRead(DBMessageHdr *hdr, XP_Bool *pRead)
{
	if (!hdr)
		return eID_NOT_FOUND;

	*pRead = (hdr->GetFlags() & kIsRead) != 0;
	return eSUCCESS;
}

MsgERR MessageDB::IsIgnored(MessageKey messageKey, XP_Bool *pIgnored)
{
	XP_ASSERT(pIgnored != NULL);
	if (!pIgnored)
		return eBAD_PARAMETER;

	DBThreadMessageHdr *threadHdr = GetDBThreadHdrForMsgID(messageKey);
	// This should be very surprising, but we leave that up to the caller
	// to determine for now.
	if (threadHdr == NULL)
		return eID_NOT_FOUND;
	*pIgnored = (threadHdr->GetFlags() & kIgnored) ? TRUE : FALSE;
	delete threadHdr;
	return eSUCCESS;
}

MsgERR MessageDB::HasAttachments(MessageKey messageKey, XP_Bool *pHasThem)
{
	XP_ASSERT(pHasThem != NULL);
	if (!pHasThem)
		return eBAD_PARAMETER;

	DBThreadMessageHdr *threadHdr = GetDBThreadHdrForMsgID(messageKey);
	// This should be very surprising, but we leave that up to the caller
	// to determine for now.
	if (threadHdr == NULL)
		return eID_NOT_FOUND;
	*pHasThem = (threadHdr->GetFlags() & kHasAttachment) ? TRUE : FALSE;
	delete threadHdr;
	return eSUCCESS;
}


// This function goes through the list of latered documents and marks them
// unread if the current date/time is > than the latered "until" setting.
// Since that is currently always 0, this routine should mark everything 
// in the latered list unread.
void MessageDB::HandleLatered()
{
	time_t	curTime = XP_TIME();

	if (!m_dbFolderInfo)
		return;

	for (int32 laterIndex = 0; laterIndex < m_dbFolderInfo->GetNumLatered(); )
	{
		time_t until;

		MessageKey laterKey = m_dbFolderInfo->GetLateredAt(laterIndex, &until);
		if (curTime > until)
		{
			MarkRead(laterKey, FALSE, NULL);
			m_dbFolderInfo->RemoveLateredAt(laterIndex);
		}
		else
		{
			laterIndex++;
		}
	}
}

void MessageDB::SetSortInfo(SortType sortType, SortOrder sortOrder)
{
	if (m_dbFolderInfo)
		m_dbFolderInfo->SetSortInfo(sortType, sortOrder);
}

MsgERR MessageDB::GetSortInfo(SortType *pSortType, SortOrder *pSortOrder)
{
	if (!(pSortType && pSortOrder && m_dbFolderInfo))
	{
		XP_ASSERT(FALSE);
		return eBAD_PARAMETER;
	}
	m_dbFolderInfo->GetSortInfo(pSortType, pSortOrder);
	return eSUCCESS;
}
// Get a handle for a document given its message number. Because a subclass
// of MsgDocument may be returned, we need to return a pointer to an allocated object.

// For some reason, the UI has decided to force a purge of the database.
MsgERR		MessageDB::Purge()
{
	FinishAddingHeaders();	// this will add m_newHeaders to the db
	return eSUCCESS;
}

//-----------------------------------------------------------------------------
// EnableCache
//-----------------------------------------------------------------------------
XP_Bool MessageDB::EnableCache(XP_Bool enable)
{
	XP_Bool oldVal = m_cacheEnabled;
	m_cacheEnabled = enable;
	return(oldVal);
}


//----------------------------------------------------------------------
// FindInCache
//----------------------------------------------------------------------
MessageDB* MessageDB::FindInCache(const char * pDbName)
{
	for (int i = 0; i < GetDBCache()->GetSize(); i++)
	{
		MessageDB* pMessageDB = GetDBCache()->GetAt(i);
		if (pMessageDB->MatchDbName(pDbName))
		{
			return(pMessageDB);
		}
	}
	return(NULL);
}

//----------------------------------------------------------------------
// FindInCache
//----------------------------------------------------------------------
int MessageDB::FindInCache(MessageDB* pMessageDB)
{
	for (int i = 0; i < GetDBCache()->GetSize(); i++)
	{
		if (GetDBCache()->GetAt(i) == pMessageDB)
		{
			return(i);
		}
	}
	return(-1);
}

//----------------------------------------------------------------------
// RemoveFromCache
//----------------------------------------------------------------------
void MessageDB::RemoveFromCache(MessageDB* pMessageDB)
{
	int i = FindInCache(pMessageDB);
	if (i != -1)
	{
		GetDBCache()->RemoveAt(i);
	}
}


#ifdef DEBUG
void MessageDB::DumpCache()
{
	for (int i = 0; i < GetDBCache()->GetSize(); i++)
	{
#ifdef DEBUG_bienvenu
		MessageDB* pMessageDB = 
#endif
        GetDBCache()->GetAt(i);
#ifdef DEBUG_bienvenu
		XP_Trace("db %s in cache use count = %d\n", pMessageDB->m_dbName, pMessageDB->m_useCount);
#endif
	}
}
#endif

XP_Bool MessageDB::MatchDbName(const char * dbName)	// returns TRUE if they match
{
	XP_ASSERT(m_dbName);
	return !XP_FILENAMECMP(dbName, m_dbName); 
}

MsgERR		MessageDB::FinishAddingHeaders()
{
	MsgERR err = eSUCCESS;
	XP_Bool		isNewThread;

	// go through the new headers adding them to the db
	// The idea here is that m_headers is just the new headers
	for (int i = 0; i < m_newHeaders.GetSize(); i++)
	{

		DBMessageHdr *dbMsgHdr = (DBMessageHdr *) m_newHeaders[i];
		err = AddHdrToDB(dbMsgHdr, &isNewThread);
		delete dbMsgHdr;
	}
	m_headerIndex = 0;
	m_newHeaders.RemoveAll();
	return err;

//	m_headerIndex = 0;
//	return eSUCCESS;
}

DBThreadMessageHdr *MessageDB::GetDBThreadHdrForSubject(DBMessageHdr *msgHdr)
{
	MSG_ThreadHandle threadHandle = MSG_DBHandle_GetThreadHandleForMsgHdrSubject(m_dbHandle, msgHdr->GetHandle());
	return GetThreadHeaderFromHandle(threadHandle);
}

// This functions takes a string message id and returns the 
// corresponding message hdr
DBThreadMessageHdr *MessageDB::GetDBMsgHdrForReference(const char * msgID)
{
	DBMessageHdr	*headerObject = GetDBMessageHdrForID(msgID);  
	DBThreadMessageHdr *thread = NULL;

	if (headerObject != NULL)
	{
		// find thread header for header whose message id we matched.
		thread = GetDBThreadHdrForMsgID(headerObject->GetMessageKey());
		delete headerObject;
	}
	return thread;
}

// make the passed in header a thread header
MsgERR MessageDB::AddThread(DBMessageHdr *msgHdr)
{
	//TRACE("entering AddThread\n");
	MSG_ThreadHandle threadHandle = MSG_DBHandle_AddThreadFromMsgHandle(m_dbHandle, msgHdr->GetHandle());
	DBThreadMessageHdr *threadHdr = new DBThreadMessageHdr(threadHandle);
	AddToThread(msgHdr, threadHdr, FALSE);

	XP_ASSERT(threadHdr->GetThreadID() == msgHdr->GetThreadId());

	delete threadHdr;
	// If this header has references, we might want to create an expired
	// header for this thread, instead of promoting this header to thread status.
	// In particular, the real thread header might arrive later, in which case
	// we would just turn off the expired bit on the dummy. Of course, it
	// could be anyone of the parents, and is more likely to be an immediate ancestor...
	// which would argue for making the message-id be the last reference, not the first.
	// But for now, just make it a top-level thread - we can always rearrange things
	// if a better top-level thread header comes in. Or if we decide to have a dummy
	// header...
//	AddHdr(msgHdr);
	//TRACE("adding thread %s\n", (const char *) dummyHdr->m_subject);

#ifdef _DEBUG1	// check that we can pull it out of the database.
	DBThreadMessageHdr	*newHdr;
	newHdr = GetDBMsgHdrForReference(dummyHdr->m_messageId);
	ASSERT(newHdr != NULL);
	delete newHdr;
#endif
	//TRACE("leaving AddThread\n");
	return eSUCCESS;
}

// really add it to DB.
MsgERR MessageDB::AddHdr(DBMessageHdr *hdr)
{
	// TODO - need to do exception handling.
	MSG_DBHandle_AddHeader(m_dbHandle, hdr->GetHandle());

	return eSUCCESS;
}

void MessageDB::AddToNewList(MessageKey key)
{
	if (m_newSet == NULL)
		m_newSet = msg_NewsArtSet::Create();
	if (m_newSet)
		m_newSet->Add(key);
}


// add a header to the database, and thread it. 
// For now, it's OK if newThread or resultHdr are NULL
MsgERR MessageDB::AddHdrToDB(DBMessageHdr *newHdr, XP_Bool *newThread, 
							 XP_Bool notify /* = FALSE */)
{
	MsgERR err = eSUCCESS;
	DBThreadMessageHdr *refHdr = NULL;

	if (m_addCount >= m_commitChunk)
	{
		Commit();
		m_addCount = 0;
	}

	if (newHdr == NULL)
		return err;		

#define SUBJ_THREADING 1// try reference threading first
	for (int32 i = 0; i < newHdr->GetNumReferences(); i++)
	{
		MSG_ThreadHandle refHdrThreadHandle = MSG_HeaderHandle_GetThreadForReference(newHdr->GetHandle(), i, m_dbHandle);
		if (refHdrThreadHandle)
		{
			refHdr = GetThreadHeaderFromHandle(refHdrThreadHandle);
			if (refHdr)
			{
				newHdr->SetThreadId(refHdr->GetThreadID());
				err = AddToThread(newHdr, refHdr, TRUE);
			}
			break;
		}
	}
#ifdef SUBJ_THREADING
	// try subject threading if we couldn't find a reference and the subject starts with Re:
	if ((newHdr->GetFlags() & kHasRe) && refHdr == NULL && (refHdr = GetDBThreadHdrForSubject(newHdr)) != NULL)
	{
		newHdr->SetThreadId(refHdr->GetThreadID());
		//TRACE("threading based on subject %s\n", (const char *) msgHdr->m_subject);
//			AddHdr(newHdr);
		// if we move this and do subject threading after, ref threading, 
		// don't thread within children, since we know it won't work. But for now, pass TRUE.
		err = AddToThread(newHdr, refHdr, TRUE);	
	}
#endif // SUBJ_THREADING

	XP_ASSERT( newHdr != NULL);
	if (refHdr == NULL)
	{
		// couldn't find any parent articles - msgHdr is top-level thread, for now
		err = AddThread(newHdr);
		if (newThread)
			*newThread = TRUE;
	}
	else
	{
		if (newThread)
			*newThread = FALSE;
	}
	delete refHdr;

	// update 
	if (err == eSUCCESS)
	{
		if ((newHdr->GetFlags() & kNew))
		{
			newHdr->AndFlags(~kNew);	// make sure not filed out
			AddToNewList(newHdr->GetMessageKey());
		}
		if (m_dbFolderInfo != NULL)
		{
			m_dbFolderInfo->ChangeNumMessages(1);
			m_dbFolderInfo->ChangeNumVisibleMessages(1);
			if (! (newHdr->GetFlags() & kIsRead))
				m_dbFolderInfo->ChangeNumNewMessages(1);
// dmb todo			m_dbFolderInfo->setDirty();
		}
		if (notify)
			NotifyKeyChangeAll(newHdr->GetMessageKey(), newHdr->GetFlags() | kAdded, NULL);
	}
	m_addCount++;
	return err;
}

MsgERR MessageDB::AddToThread(DBMessageHdr *reply, DBThreadMessageHdr *threadHdr, XP_Bool threadInThread)
{
	reply->SetLevel(0);	// for now, until we get threading within a thread.
	reply->SetThreadId(threadHdr->GetThreadID());
	// determine where to add to thread.
	threadHdr->AddChild(reply, this, threadInThread);
	return eSUCCESS;
}


// Get the header for the passed in message-id. Could return a DBMailMessageHdr
// because we do a deep find. Caller must RemoveReference DBMessageHdr when done with it.
DBMessageHdr *MessageDB::GetDBMessageHdrForID(const char * msgID)
{
	DBMessageHdr *headerObject = NULL;  
	MSG_HeaderHandle headerHandle = MSG_DBHandle_GetHandleForMessageID(m_dbHandle, msgID);
	if (headerHandle)
		GetHeaderFromHandle(headerHandle, &headerObject);
	return headerObject;
}

MessageKey	MessageDB::GetMessageKeyForID(const char *msgID)
{
	MessageKey		retKey = kIdNone;
	DBMessageHdr *msgHdr = GetDBMessageHdrForID(msgID);
	if (msgHdr)
	{
		retKey = msgHdr->GetMessageKey();
		delete msgHdr;
	}
	return retKey;
}

// caller needs to RemoveReference when finished.
DBThreadMessageHdr *MessageDB::GetDBThreadHdrForThreadID(MessageKey messageKey)
{
	MSG_ThreadHandle threadHandle = MSG_DBHandle_GetThreadHeaderForThreadID(m_dbHandle, messageKey);
	return GetThreadHeaderFromHandle(threadHandle);
}

DBThreadMessageHdr *MessageDB::GetDBThreadHdrForMsgHdr(DBMessageHdr *msgHdr)
{
	DBThreadMessageHdr *threadHdr = GetDBThreadHdrForThreadID(msgHdr->GetThreadId());
	return threadHdr;
}

// Given the id of a message, find the thread header for the message's thread
// Returns NULL if we can't find the message hdr, or its thread.
// Caller needs to RemoveReference thread header
DBThreadMessageHdr *MessageDB::GetDBThreadHdrForMsgID(MessageKey messageKey)
{
	DBMessageHdr *msgHdr = GetDBHdrForKey(messageKey);
	if (msgHdr == NULL)
		return NULL;
	DBThreadMessageHdr *threadHdr = GetDBThreadHdrForMsgHdr(msgHdr);

	delete msgHdr;
	return threadHdr;
}

// Given a MessageKey, return the threadId of its thread, or kIdNone
// if we can't find the given MessageKey.
MessageKey	MessageDB::GetThreadIdForMsgId(MessageKey messageKey)
{
	DBMessageHdr *msgHdr = GetDBHdrForKey(messageKey);
	if (msgHdr != NULL)
	{
		MessageKey threadId = msgHdr->GetThreadId();
		delete msgHdr;
		return threadId;
	}
	else
	{
		return kIdNone;
	}
}

// caller needs to delete when finished.
DBMessageHdr *MessageDB::GetDBHdrForKey(MessageKey messageKey)
{
	MSG_DBHandle dbHandle = MSG_DBHandle_GetHandleForKey(m_dbHandle, messageKey);
	DBMessageHdr *headerObject = NULL;
	
	if (dbHandle)
		headerObject = new DBMessageHdr(dbHandle);

	return headerObject;
}

// Test if the key we're about to add already exists (in which case
// the caller shouldn't add. This can happen in news for various reasons
// and should be handled).
XP_Bool MessageDB::KeyToAddExists(MessageKey messageKey)
{
	// this relies on GetMessageHdr not touching msgHdr if it doesn't find key in db 
	DBMessageHdr *msgHdr = GetDBHdrForKey(messageKey);
	if (msgHdr != NULL)
	{
		// this handles the bizarre case of the the db having all the
		// headers but not having the highwater mark set, in which
		// case it will always retrieve all headers.
		if (m_dbFolderInfo != NULL)
			m_dbFolderInfo->SetHighWater(messageKey);
		delete msgHdr;
		return TRUE;
	}
	return FALSE;
}

///////////////////// MessageHdrStruct methods
void MessageHdrStruct::SetSubject(const char * subject)
{
  if (msg_StripRE(&subject, NULL))
  {
	m_flags |= kHasRe;
  }
  else
  {
  	m_flags &= ~kHasRe;
  }
  XP_STRNCPY_SAFE(m_subject, subject, sizeof(m_subject));
}

void MessageHdrStruct::SetAuthor(const char * author)
{
  XP_STRNCPY_SAFE(m_author, author, sizeof(m_author));
}

// Set message id, stripping off leading '<' and trailing '>', if any
void MessageHdrStruct::SetMessageID(const char * msgID)
{
  if (msgID)
	StripMessageId(msgID, m_messageId, sizeof(m_messageId));
}

/* static */void MessageHdrStruct::StripMessageId(const char *msgID, char *outMsgId, int msgIdLen)
{
  if (*msgID == '<')
	  msgID++;
  XP_STRNCPY_SAFE(outMsgId, msgID, msgIdLen);
  char * lastChar = outMsgId + strlen(outMsgId) -1;
  if (*lastChar == '>')
	  *lastChar = '\0';
}

void MessageHdrStruct::SetReferences(const char * referencesStr)
{
	if (referencesStr)
		XP_STRNCPY_SAFE(m_references, referencesStr, sizeof(m_references));
}

void MessageHdrStruct::SetDate(const char * date)
{
	m_date = XP_ParseTimeString (date, FALSE);
}
void MessageHdrStruct::SetLines(uint32 lines)
{
	  m_messageSize = lines;
}

void MessageHdrStruct::SetSize(uint32 size)
{
	m_messageSize = size;
}

// get the next <> delimited reference from nextRef and copy it into reference,
// which is a pointer to a buffer at least kMaxMsgIdLen long.
const char * MessageHdrStruct::GetReference(const char *nextRef, char *reference)
{
	const char *ptr = nextRef;

	while ((*ptr == '<' || *ptr == ' ') && *ptr)
		ptr++;

	for (int i = 0; *ptr && *ptr != '>' && i < kMaxMsgIdLen; i++)
		*reference++ = *ptr++;

	if (*ptr == '>')
		ptr++;
	*reference = '\0';
	return ptr;
}

// Copy the corresponding fields from a full message header into a short message hdr.
void MessageDB::CopyFullHdrToShortHdr(MSG_MessageLine *msgHdr, MessageHdrStruct *fullHdr)
{
	msgHdr->threadId = fullHdr->m_threadId;
	msgHdr->messageKey = fullHdr->m_messageKey; 	//for threads, same as threadId
	XP_STRNCPY_SAFE(msgHdr->subject, fullHdr->m_subject, sizeof(msgHdr->subject));
	XP_STRNCPY_SAFE(msgHdr->author, fullHdr->m_author, sizeof(msgHdr->author));
	msgHdr->date = fullHdr->m_date;
	msgHdr->messageLines = fullHdr->m_messageSize;	// lines for news articles,
													// bytes for mail messages
													// ###tw  Is the above true
													// yet?
	msgHdr->priority = fullHdr->m_priority;
	msgHdr->flags = fullHdr->m_flags;
	msgHdr->level = fullHdr->m_level;			// indentation level
	msgHdr->numChildren = fullHdr->m_numChildren;		// for top-level threads
	msgHdr->numNewChildren = fullHdr->m_numNewChildren;	// for top-level threads
}

// static helper functions to convert between kFlags and MSG_FLAG_*
void MessageDB::ConvertDBFlagsToPublicFlags(uint32 *flags)
{
	uint32 publicFlags = 0;
	publicFlags = (kSameAsMSG_FLAG & *flags); 
	if (*flags & kExpunged)	// is this needed?
		publicFlags |= MSG_FLAG_EXPUNGED;
	if (*flags & kHasRe)
		publicFlags |= MSG_FLAG_HAS_RE;
	if (*flags & kIgnored)
		publicFlags |= MSG_FLAG_IGNORED;
	if (*flags & kPartial)
		publicFlags |= MSG_FLAG_PARTIAL;
	if (*flags & kMDNNeeded)
		publicFlags |= MSG_FLAG_MDN_REPORT_NEEDED;
	if (*flags & kMDNSent)
		publicFlags |= MSG_FLAG_MDN_REPORT_SENT;
	if (*flags & kTemplate)
		publicFlags |= MSG_FLAG_TEMPLATE;
	*flags = publicFlags;
}

void MessageDB::ConvertPublicFlagsToDBFlags(uint32 *flags)
{
	uint32 dbFlags = 0;
	dbFlags = (kSameAsMSG_FLAG & *flags); 
	if (*flags & MSG_FLAG_EXPUNGED)	// is this needed?
		dbFlags |= kExpunged ;
	if (*flags & MSG_FLAG_HAS_RE)
		dbFlags |= kHasRe ;
	if (*flags & MSG_FLAG_IGNORED)
		dbFlags |= kIgnored;
	if (*flags & MSG_FLAG_PARTIAL)
		dbFlags |= kPartial;
	if (*flags & MSG_FLAG_MDN_REPORT_NEEDED)
		dbFlags |= kMDNNeeded;
	if (*flags & MSG_FLAG_MDN_REPORT_SENT)
		dbFlags |= kMDNSent;
	if (*flags & MSG_FLAG_TEMPLATE)
		dbFlags |= kTemplate;
	*flags = dbFlags;
}
ViewType	 MessageDB::GetViewType()
{
	ViewType retViewType = ViewAllThreads;
	if (m_dbFolderInfo)
	{
		retViewType = (ViewType) m_dbFolderInfo->GetViewType();
		if (retViewType == ViewKilledThreads)
		{
			retViewType = ViewAllThreads;
			m_dbFolderInfo->SetFlags(m_dbFolderInfo->GetFlags() | MSG_FOLDER_PREF_SHOWIGNORED);
		}
	}
	return retViewType;
}

void MessageDB::SetViewType(ViewType viewType)
{
	if (m_dbFolderInfo)
		m_dbFolderInfo->SetViewType(viewType);
	else
		XP_ASSERT(FALSE);
}

MsgERR	MessageDB::GetCachedPassword(XPStringObj &cachedPassword)
{
	m_dbFolderInfo->GetCachedPassword(cachedPassword, m_dbHandle);
	return eSUCCESS;
}

MsgERR	MessageDB::SetCachedPassword(const char *password)
{
	m_dbFolderInfo->SetCachedPassword(password, m_dbHandle);
	return eSUCCESS;
}

XP_Bool MessageDB::HasCachedPassword()
{
	XPStringObj password;
	m_dbFolderInfo->GetCachedPassword(password, m_dbHandle);
	return (XP_STRLEN(password) > 0);
}

