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
#include "newsdb.h"
#include "dberror.h"
#include "newshdr.h"
#include "msgdbvw.h"
#include "msghdr.h"
#include "thrhead.h"
#include "grpinfo.h"
#include "msgmast.h"
#include "newshost.h"
#include "msgfinfo.h"
#include "newsset.h"
#include "xpgetstr.h"
#include "pmsgfilt.h"
#include "jsmsg.h"
#include "msgdbapi.h"

extern "C" 
{
	extern int MK_MSG_EXPIRE_COUNT;
	extern int MK_MSG_DONE_EXPIRE_COUNT;
}

NewsGroupDB::NewsGroupDB() 
{
	m_groupURL = NULL;
	m_filterList = NULL;
	m_headerIndex = 0;
	m_master = NULL;
}

NewsGroupDB::~NewsGroupDB()
{
	if (m_filterList != NULL)
		MSG_CancelFilterList(m_filterList);
	FREEIF(m_groupURL);
	JSFilter_cleanup();
}
// inline virtuals put in .cpp file to help some compilers
int				NewsGroupDB::GetCurVersion() {return kNewsDBVersion;}
NewsGroupDB		*NewsGroupDB::GetNewsDB() {return this;}

// groupURL is just the url constructed by msg_MakeNewsURL, e.g.,
// "alt.fan.h-h-hippeaux//newshost:20/". 
MsgERR NewsGroupDB::MessageDBOpenUsingURL(const char * groupURL)
{
	MsgERR err;
	char *groupName;

	m_groupURL = XP_STRDUP(groupURL);

	char *hostName = NET_ParseURL (groupURL, GET_HOST_PART);
	MSG_NewsHost* host = m_master->FindHost(hostName, *groupURL == 's', -1);
	FREEIF(hostName);

	if (!host) return eUNKNOWN;	// ###tw

	groupName = GetGroupNameFromURL(groupURL);
	m_info = host->FindGroup(groupName);
	FREEIF(groupName);

	if (!m_info) return eUNKNOWN; // ###tw

	// OK, break java script news filters if the user has defined filters for this group.
	MSG_FilterError filterErr = MSG_FilterList::Open(m_master, filterNews, NULL, m_info, &m_filterList);
	int32 filterCount = 0;
	if (filterErr != FilterError_Success || (m_filterList->GetFilterCount(&filterCount) == FilterError_Success && filterCount == 0))
	{
		if (m_filterList)
			MSG_CancelFilterList(m_filterList);

		if (MSG_FilterList::Open(m_master, filterInbox, &m_filterList) != FilterError_Success)
			m_filterList = NULL;	
	}

	// get root category if we have a category container...
	m_info = m_info->GetNewsFolderInfo();
	m_set = m_info->GetSet();
	const char* dbFileName = m_info->GetDBFileName();

	err = MessageDB::MessageDBOpen(dbFileName, TRUE);
//	XP_FREE(dbFileName);
	return err;
}

MsgERR	NewsGroupDB::Open(const char * groupURL, MSG_Master *master,
						  NewsGroupDB** pMessageDB)
{
	NewsGroupDB	*newsDB;

	XP_ASSERT(master != NULL);
	if (master == NULL) return eUNKNOWN;

	char *hostName = NET_ParseURL (groupURL, GET_HOST_PART);

	MSG_NewsHost* host = master->FindHost(hostName, *groupURL == 's', -1);

	if (!host) {
		// ###tw  This should probably be allowed.  If someone types a news:
		// URL by hand, then we should go create the host and stuff here.
		// Not right now, though.
		return eUNKNOWN;
	}
	

	FREEIF(hostName);

	if (m_cacheEnabled)
	{
		char* groupname = GetGroupNameFromURL(groupURL);
		if (!groupname) return eUNKNOWN; // Bad URL?

		MSG_FolderInfoNews* info = host->FindGroup(groupname);
		FREEIF(groupname);
		if (!info) return eUNKNOWN;	// New newsgroup?  Probably should add...
									// ###tw
		const char *groupFile = info->GetDBFileName();
		XP_ASSERT(groupFile);
		if (!groupFile) return eUNKNOWN;
		newsDB = (NewsGroupDB *) FindInCache(groupFile);
		if (newsDB)
		{
			*pMessageDB = newsDB;
			// make this behave like the non-cache case.
			++newsDB->m_useCount;
			return(eSUCCESS);
		}
	}
	newsDB = new NewsGroupDB;
	if (!newsDB)
	{
		*pMessageDB = NULL;
		return(eOUT_OF_MEMORY);
	}			

	newsDB->m_master = master;

	MsgERR err = newsDB->MessageDBOpenUsingURL(groupURL);
	if (err == eSUCCESS)
	{
		*pMessageDB = newsDB;
		if (m_cacheEnabled)
			GetDBCache()->Add(newsDB);

		newsDB->HandleLatered();

		if (newsDB->m_dbFolderInfo != NULL)
			newsDB->GetNewsFolderInfo()->SetNewsDB(newsDB);
	}
	else
	{
		*pMessageDB = NULL;
		delete newsDB;
	}
	return(err);
}

MsgERR NewsGroupDB::Commit(XP_Bool compress /* = FALSE */)
{
	return MessageDB::Commit(compress);
}

// caller needs to RemoveReference when finished.
NewsMessageHdr *NewsGroupDB::GetNewsHdrForKey(MessageKey messageKey)
{
	NewsMessageHdr *headerObject = NULL;
	MSG_HeaderHandle headerHandle = MSG_DBHandle_GetHandleForKey(m_dbHandle, messageKey);
	if (headerHandle)
	{
		headerObject = new NewsMessageHdr;
		headerObject->SetHandle(headerHandle);
	}
	return headerObject;
}

// Expire up to but not including expireKey.
MsgERR NewsGroupDB::ExpireUpTo(MessageKey expireKey, MWContext *context)
{
	MsgERR	err;
	XP_ASSERT(m_dbFolderInfo != NULL);
	if (m_dbFolderInfo == NULL)
		return eSUCCESS;
	// pass in 0 in case expiredMark is wrong - should'nt cost much since there
	// probably aren't any articles with id's below expiredMark.
		if ((err = ExpireRange(0 /*m_dbFolderInfo->m_expiredMark*/, --expireKey, context)) == eSUCCESS)
			m_dbFolderInfo->SetExpiredMark(expireKey);

	if (err == eCorruptDB)
		SetSummaryValid(FALSE);

	return err;
}

MsgERR NewsGroupDB::ExpireRange(MessageKey startRange, MessageKey endRange, MWContext *context)
{
	MsgERR ret = MSG_DBHandle_ExpireRange(m_dbHandle, startRange, endRange);

	m_set->AddRange(startRange, endRange);

	char			msgBuf[100];
	const char *	msgTemplate = XP_GetString(MK_MSG_EXPIRE_COUNT);
//DMB TODO - what about removing these from the view, and giving progress?
//		if (removeCount % 10 == 0)
//		{
//			PR_snprintf (msgBuf, sizeof(msgBuf), msgTemplate, removeCount);
//			FE_Progress (context, msgBuf);
//		}
//	}

//	msgTemplate = XP_GetString(MK_MSG_DONE_EXPIRE_COUNT);
//	PR_snprintf (msgBuf, sizeof(msgBuf), msgTemplate, removeCount);
//	FE_Progress (context, msgBuf);
	return eSUCCESS;
}

// return highest article number we've seen. Returns 0 in case of errors.
MessageKey	NewsGroupDB::GetHighwaterArticleNum()
{
	XP_ASSERT(m_dbFolderInfo != NULL);
	if (m_dbFolderInfo == NULL)
		return 0;
	return m_dbFolderInfo->GetHighWater();
}

// return the key of the first article number we know about.
// Since the iterator iterates in id order, we can just grab the
// messagekey of the first header it returns.
// ### dmb
// This will not deal with the situation where we get holes in 
// the headers we know about. Need to figure out how and when
// to solve that. This could happen if a transfer is interrupted.
// Do we need to keep track of known arts permanently?
MessageKey NewsGroupDB::GetLowWaterArticleNum()
{
	ListContext	*pContext;
	DBMessageHdr	*pFirstHdr;
	MessageKey		lowWater = 0;

	if (ListFirst(&pContext, &pFirstHdr) == eSUCCESS)
	{
		lowWater = pFirstHdr->GetMessageKey();
		delete pFirstHdr;
		ListDone(pContext);
	}
	return lowWater;
}

// This function merely parses the XOver header into a DBMessageHdr
// and stores that in m_headers. From there, it will be threaded
// and added to the database in a separate pass.
MsgERR NewsGroupDB::AddHdrFromXOver(const char * line,  MessageKey *msgId)
{
	MsgERR	err = eSUCCESS;

	char    copyHeader[1024];
	DBMessageHdr *msgHdr = new NewsMessageHdr;
	XP_STRNCPY_SAFE(copyHeader, line, sizeof(copyHeader));
	if (!msgHdr->InitFromXOverLine(copyHeader, m_dbHandle))
	{
//		XP_TRACE("Failed to parse XOver line %s\n", line);
		return eXOverParseError;
	}
	if (msgId)
		*msgId = msgHdr->GetMessageKey();

	m_newHeaders.SetAtGrow(m_headerIndex++, msgHdr);
	if (m_headerIndex > 200)
	{
		for (int i = 0; i < m_newHeaders.GetSize(); i++)
		{
			XP_Bool		isNewThread;

			DBMessageHdr *dbMsgHdr = (DBMessageHdr *) m_newHeaders[i];
			err = AddHdrToDB(dbMsgHdr, &isNewThread);
			delete dbMsgHdr;
			// ### dmb - should we really ignore the rest of the headers?
			if (err != eSUCCESS)
				break;
		}
		m_newHeaders.RemoveAll();
		m_headerIndex = 0;
	}
	return err;
}

MsgERR NewsGroupDB::AddHdrToDB(DBMessageHdr *newHdr, XP_Bool *newThread, XP_Bool notify /* = FALSE */)
{
	MsgERR		err = eSUCCESS;
	XP_Bool		bRead;
	XPStringObj	msgId;
	XP_Bool		addToDB = TRUE;

	MessageKey highWaterMark = newHdr->m_messageKey;

	IsRead(newHdr->m_messageKey, &bRead);
	if (bRead)		
		newHdr->OrFlags(kIsRead);
	else
		newHdr->OrFlags(kNew);

#if 0	// DMB TODO - we could have an api that checks if duplicate header exists
		DBMessageHdr	*dbHeader = GetDBMessageHdrForHashMessageID(newHdr->m_messageIdID);
		{
		if (dbHeader != NULL)
		{
			if (dbHeader->GetLineCount() == newHdr->GetLineCount())
			{
				XPStringObj newSubject, dbHdrSubject;
				if (newHdr->GetSubject(newSubject, TRUE, GetDB()) && dbHeader->GetSubject(dbHdrSubject, TRUE, GetDB()))
				{
					addToDB = XP_STRCMP(newSubject, dbHdrSubject);
					if (!addToDB)
					{
						MarkHdrRead(newHdr, TRUE, NULL); 
#ifdef DEBUG_bienvenu
						XP_Trace("got duplicate message-id marking read and ignoring\n");
#endif
						delete newHdr;	// forget about this header
					}
				}
			}
			delete dbHeader;
		}
	}
#endif
	// kill files should be here.
	if (addToDB)
	{
		if (!bRead)
		{
			int32 filterCount = 0;
			MSG_Filter *filter;
			
			if (m_filterList != NULL)
				m_filterList->GetFilterCount(&filterCount);
			
			for (MSG_FilterIndex filterIndex = 0; filterIndex < filterCount; filterIndex++)
			{
				if (m_filterList->GetFilterAt(filterIndex, &filter) == FilterError_Success)
				{
					if (!filter->GetEnabled())
						continue;
					
					if (filter->GetType() == filterNewsJavaScript)
					{
						if (JSNewsFilter_execute(filter, newHdr, this) == FilterError_Success)
							break;
					}
					else if (filter->GetType() == filterNewsRule)
					{
						MSG_Rule	*rule;
						MSG_SearchError matchTermStatus;

						if (filter->GetRule(&rule) == FilterError_Success)
						{
							{	// put this in its own block so scope will get destroyed
								// before we apply the actions, so folder file will get closed.
								MSG_ScopeTerm scope (NULL, scopeOfflineNewsgroup, m_info);

								matchTermStatus = msg_SearchOfflineMail::MatchTermsForSearch(newHdr, 
															rule->GetTermList(), &scope, this);
							}
							if (matchTermStatus  == SearchError_Success)
							{
								MSG_RuleActionType actionType;
								void				*value = NULL;

								if (rule->GetAction(&actionType, &value) == FilterError_Success)
								{
									switch (actionType)
									{
									case acDelete:
										addToDB = FALSE;
										break;
									case acMarkRead:
										MarkHdrRead(newHdr, TRUE, NULL);
										break;
									case acKillThread:
										// for ignore and watch, we will need the db
										// to check for the flags in msgHdr's that
										// get added, because only then will we know
										// the thread they're getting added to.
										newHdr->OrFlags(kIgnored);
										break;
									case acWatchThread:
										newHdr->OrFlags(kWatched);
										break;
									case acChangePriority:
										SetPriority(newHdr,  * ((MSG_PRIORITY *) &value));
										break;
									default:
										break;
									}
								}
								break;
							}
						}
					}
				}
			}
		}
	}

	if (addToDB)
		err = MessageDB::AddHdrToDB(newHdr, newThread, notify);
	// what if we just followed a link to an article number??? ### dmb TODO
	m_dbFolderInfo->SetHighWater(highWaterMark);
	return err;
}


// force the database to close - this'll flush out anybody holding onto
// a database without having a listener!
MsgERR NewsGroupDB::ForceClosed()
{
	m_info = NULL; // if we're forcing closed at shutdown, m_info might be already deleted.
	return MessageDB::ForceClosed();
}

MsgERR NewsGroupDB::Close()
{
	// if we're really closing, make sure any changes to the newsrc file get
	// saved.
	if (m_useCount <= 1 && m_info)
	{
		m_info->GetHost()->WriteIfDirty();
	}
	MessageDB::Close();
	return eSUCCESS;
}

DBFolderInfo *NewsGroupDB::CreateFolderInfo(MSG_DBFolderInfoHandle handle)
{
	return new NewsFolderInfo((handle) ? handle : MSG_CreateNewsDBFolderInfo());
}

MsgERR NewsGroupDB::MarkHdrRead(DBMessageHdr *msgHdr, XP_Bool bRead, 
						   ChangeListener *instigator)
{
	MessageKey messageKey = msgHdr->GetMessageKey();
	int		err;

	if (bRead)
		err = m_set->Add(messageKey);
	else
		err = m_set->Remove(messageKey);

	// give parent class chance to update data structures
	MessageDB::MarkHdrRead(msgHdr, bRead, instigator);

	return (err >= 0) ? eSUCCESS : eOUT_OF_MEMORY;
}

MsgERR NewsGroupDB::IsRead(MessageKey messageKey, XP_Bool *pRead)
{
	XP_ASSERT(pRead != NULL);
	if (pRead == NULL) return eUNKNOWN;

	XP_Bool isRead = m_set->IsMember(messageKey);
	*pRead = isRead;
	return eSUCCESS;
}

MsgERR		NewsGroupDB::MarkAllRead(MWContext *context, IDArray * /*thoseMarked*/)
{
	if (GetLowWaterArticleNum() > 2)
		m_set->AddRange(1, GetLowWaterArticleNum() - 1);
	MsgERR err = MessageDB::MarkAllRead(context);
	if (err == eSUCCESS && 1 <= GetHighwaterArticleNum())
		m_set->AddRange(1, GetHighwaterArticleNum());	// mark everything read in newsrc.

	return err;
}

// caller needs to free return char *
char *NewsGroupDB::GetGroupNameFromURL(const char *url)
{
	char *groupPart;
	char *groupName;

	groupPart = groupName = NET_ParseURL (url, GET_PATH_PART);

	if (!groupPart)
		return NULL;
	// ### dmb (tw) - needs someone who knows how to parse newsgroup urls
	if (groupName[0] == '/')
	{
		groupName = XP_STRDUP(groupPart + 1);
		FREEIF(groupPart);
	}
	return groupName;
}

MsgERR NewsGroupDB::ListNextUnread(ListContext **pContext, DBMessageHdr **pResult)
{
	MsgERR			dbErr = eSUCCESS;
	XP_Bool			lastWasRead = TRUE;
	MessageKey		nextKey = 0;
	*pResult = NULL;

	while (TRUE)
	{
		if (*pContext == NULL)
		{
			*pContext = new ListContext;

			if (!*pContext)
				return eOUT_OF_MEMORY;
			(*pContext)->m_pMessageDB = this;
			(*pContext)->m_curMessageNum = nextKey = GetLowWaterArticleNum();
		}
		else
		{
			// this works if we are marking things read as we go.
			// We really need a NextNonMember function which takes
			// the place to start.
			nextKey = MAX(m_set->FirstNonMember(), nextKey + 1);
		}
		if (dbErr == eDBEndOfList || nextKey > GetHighwaterArticleNum())
		{
			delete *pContext;
			break;
		}

		// this currently doesn't happen since ListNext doesn't return errors
		// other than eDBEndOfList.
		else if (dbErr != eSUCCESS)	 
			break;
		if (IsRead(nextKey, &lastWasRead) == eSUCCESS && !lastWasRead)
		{
			DBMessageHdr *msgHdr = GetNewsHdrForKey(nextKey);
			if (msgHdr)
			{
				*pResult = msgHdr;
				(*pContext)->m_curMessageNum = nextKey;
				break;
			}
			else
			{
				m_set->Add(nextKey);	// mark read in newsrc file, so FirstNonMember will work
			}
		}
		else if (lastWasRead)	// first non-member seems horked - try adding this key to set
			m_set->Add(nextKey);
	}
	return dbErr;
}

XP_Bool		NewsGroupDB::IsArticleOffline(MessageKey messageKey)
{
	XP_Bool ret = FALSE;
	NewsMessageHdr *newsHdr = GetNewsHdrForKey(messageKey);
	if (newsHdr != NULL)
	{
		ret = (newsHdr->GetFlags() & kOffline);
		delete newsHdr;
	}
	return ret;
}

XP_Bool	NewsGroupDB::IsCategory()
{
	return m_info && m_info->TestFlag(MSG_FOLDER_FLAG_CATEGORY);
}

XP_Bool	NewsGroupDB::PurgeNeeded(MSG_PurgeInfo *purgeHdrPref, MSG_PurgeInfo *purgeArtPref)
{
	MSG_PurgeInfo curHdrPurgeInfo;
	MSG_PurgeInfo curBodyPurgeInfo;

	GetPurgeHeaderInfo(&curHdrPurgeInfo);
	GetPurgeArticleInfo(&curBodyPurgeInfo);

	if (curHdrPurgeInfo.m_useDefaults)
		curHdrPurgeInfo = *purgeHdrPref;

	if (curBodyPurgeInfo.m_useDefaults)
		curBodyPurgeInfo = *purgeArtPref;

	if (curHdrPurgeInfo.m_purgeBy == MSG_PurgeNone && curBodyPurgeInfo.m_purgeBy == MSG_PurgeNone)
		return FALSE;

	if (curHdrPurgeInfo.m_purgeBy == MSG_PurgeByNumHeaders && m_dbFolderInfo->GetNumMessages() > curHdrPurgeInfo.m_numHeadersToKeep)
		return TRUE;

	long daysToKeep = 0;
	if (curHdrPurgeInfo.m_purgeBy == MSG_PurgeByAge)
		daysToKeep = curHdrPurgeInfo.m_daysToKeep;
	if (curBodyPurgeInfo.m_purgeBy == MSG_PurgeByAge)
		daysToKeep = (daysToKeep == 0) ? curBodyPurgeInfo.m_daysToKeep : MIN(daysToKeep, curBodyPurgeInfo.m_daysToKeep);
	
	if (curHdrPurgeInfo.m_purgeBy == MSG_PurgeByAge || curBodyPurgeInfo.m_purgeBy == MSG_PurgeByAge)
	{
		// need to add a use default setting for per newsgroup purging preferences.
		ListContext		*listContext = NULL;

		while (TRUE)
		{
			MsgERR	dbErr;

			DBMessageHdr	*pHeader = NULL;
			if (listContext == NULL)
				dbErr = ListFirst (&listContext, &pHeader);
			else
				dbErr = ListNext(listContext, &pHeader);
			if (dbErr != eSUCCESS)
				dbErr = eDBEndOfList;

			if (dbErr != eSUCCESS /* eDBEndOfList */)
			{
				dbErr = eSUCCESS;
				ListDone(listContext);
				break;
			}
			time_t now = XP_TIME();
			long matchDay = now - (daysToKeep * 60 * 60 * 24);
			long msgDate = pHeader->GetDate();
			delete pHeader;
			if (msgDate < matchDay)
			{
				ListDone(listContext);
				return TRUE;
			}
		}
	}
	return FALSE;
}

MsgERR NewsGroupDB::SetOfflineRetrievalInfo(MSG_RetrieveArtInfo *info)
{
	XP_ASSERT(m_dbFolderInfo != NULL);
	if (m_dbFolderInfo == NULL)
		return eDBNotOpen;
	GetNewsFolderInfo()->SetOfflineRetrievalInfo(info);
	return eSUCCESS;
}

MsgERR NewsGroupDB::SetPurgeHeaderInfo(MSG_PurgeInfo *purgeInfo)
{
	XP_ASSERT(m_dbFolderInfo != NULL);
	if (m_dbFolderInfo == NULL)
		return eDBNotOpen;
	GetNewsFolderInfo()->SetPurgeHeaderInfo(purgeInfo);
	return eSUCCESS;
}

MsgERR NewsGroupDB::SetPurgeArticleInfo(MSG_PurgeInfo *purgeInfo)
{
	XP_ASSERT(m_dbFolderInfo != NULL);
	if (m_dbFolderInfo == NULL)
		return eDBNotOpen;
	GetNewsFolderInfo()->SetPurgeArticleInfo(purgeInfo);
	return eSUCCESS;
}

MsgERR NewsGroupDB::GetOfflineRetrievalInfo(MSG_RetrieveArtInfo *info)
{
	XP_ASSERT(m_dbFolderInfo != NULL);
	if (m_dbFolderInfo == NULL)
		return eDBNotOpen;
	 GetNewsFolderInfo()->GetOfflineRetrievalInfo(info);
	 return eSUCCESS;
}

MsgERR NewsGroupDB::GetPurgeHeaderInfo(MSG_PurgeInfo *purgeInfo)
{
	XP_ASSERT(m_dbFolderInfo != NULL);
	if (m_dbFolderInfo == NULL)
		return eDBNotOpen;
	return GetNewsFolderInfo()->GetPurgeHeaderInfo(purgeInfo);
}

MsgERR NewsGroupDB::GetPurgeArticleInfo(MSG_PurgeInfo *purgeInfo)
{
	XP_ASSERT(m_dbFolderInfo != NULL);
	if (m_dbFolderInfo == NULL)
		return eDBNotOpen;
	return GetNewsFolderInfo()->GetPurgeArticleInfo(purgeInfo);
}
