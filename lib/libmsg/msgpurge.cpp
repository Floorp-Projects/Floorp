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
#include "newspane.h"
#include "msgfinfo.h"
#include "pmsgsrch.h"
#include "newshdr.h"
#include "newsdb.h"
#include "xpgetstr.h"
#include "msgpurge.h"
#include "grpinfo.h"
#include "newshdr.h"
extern "C"
{
	extern int MK_MSG_PURGING_NEWSGROUP_HEADER;
	extern int MK_MSG_PURGING_NEWSGROUP_ARTICLE;
	extern int MK_MSG_PURGING_NEWSGROUP_ETC;
	extern int MK_MSG_PURGING_NEWSGROUP_DONE;
}

const int kPurgeCommitChunk = 200;

MSG_PurgeNewsgroupState::MSG_PurgeNewsgroupState(MSG_Pane *pane, MSG_FolderInfo *folder)
{
	m_purgeCount = 0;
	m_context = pane->GetContext();
	m_folder = folder;
	m_pane = pane;
	m_listContext = NULL;
	m_newsDB = NULL;
	m_headerIndex = 0;
	m_groupInfo = NULL;
	m_headerPurgeInfo.m_purgeBy = MSG_PurgeNone;
}

MSG_PurgeNewsgroupState::~MSG_PurgeNewsgroupState()
{
	if (m_newsDB)
	{
		if (m_listContext)
			m_newsDB->ListDone (m_listContext);
		m_newsDB->Close();
	}
}

MsgERR	MSG_PurgeNewsgroupState::Init()
{
	MessageDB	*newsDB;
	DBFolderInfo	*groupInfo;
	MsgERR err = m_folder->GetDBFolderInfoAndDB(&groupInfo, &newsDB);
	m_newsDB = newsDB->GetNewsDB();

	const char *fmt = XP_GetString(MK_MSG_PURGING_NEWSGROUP_ETC); 
	const char *f = m_folder->GetPrettiestName();
	char *statusString  = PR_smprintf (fmt, f);
	if (statusString)
	{
		FE_Progress(m_context, statusString);
		XP_FREE(statusString);
	}
	MSG_GetHeaderPurgingInfo(m_folder, &m_headerPurgeInfo.m_useDefaults, &m_headerPurgeInfo.m_purgeBy,
		&m_headerPurgeInfo.m_unreadOnly, &m_headerPurgeInfo.m_daysToKeep, &m_headerPurgeInfo.m_numHeadersToKeep);

	MSG_GetArticlePurgingInfo(m_folder, &m_articlePurgeInfo.m_useDefaults, &m_articlePurgeInfo.m_purgeBy,
		&m_articlePurgeInfo.m_daysToKeep);
	m_groupInfo = m_newsDB->GetNewsFolderInfo();
	m_pNextHeader = NULL;
	return err;
}

int	MSG_PurgeNewsgroupState::PurgeSomeMore()
{
	MsgERR	dbErr = eSUCCESS;
	DBMessageHdr	*pHeader = m_pNextHeader;
	XP_Bool		purgedSomething = FALSE;

	if (!m_listContext)
		dbErr = m_newsDB->ListFirst (&m_listContext, &pHeader);
	// advance the iterator so we can delete this message.
	if (eSUCCESS == dbErr && pHeader != NULL)
		dbErr = m_newsDB->ListNext (m_listContext, &m_pNextHeader);

	if (eSUCCESS != dbErr)
		m_pNextHeader = NULL;	// next time through, we'll return MK_CONNECTED.

	if (pHeader == NULL)
	{
		// Do clean up for end-of-scope processing
		if (m_listContext)
			m_newsDB->ListDone (m_listContext);
		m_listContext = NULL;	
		// Let go of the DB when we're done with it so we don't kill the db cache
		if (m_newsDB)
			m_newsDB->Close();
		m_newsDB = NULL;
		return MK_CONNECTED;
	}

	int32 percent = (100L * m_headerIndex) / (uint32) m_groupInfo->GetNumMessages();
	FE_SetProgressBarPercent (m_context, percent);

	if (m_articlePurgeInfo.m_purgeBy == MSG_PurgeByAge)
	{
		time_t now = XP_TIME();
		long matchDay = now - (m_articlePurgeInfo.m_daysToKeep * 60 * 60 * 24);
		if (pHeader->GetDate() < matchDay)
		{
			pHeader->PurgeArticle();
			purgedSomething = TRUE;
		}
	}
	if (m_headerPurgeInfo.m_unreadOnly)
	{
		XP_Bool	isRead;
		if (m_newsDB->IsRead(pHeader->GetMessageKey(), &isRead) == eSUCCESS && isRead)
			PurgeHeader(pHeader);
		else
		{
			delete pHeader;
			m_headerIndex++;
		}
	}
	else
	{
		switch (m_headerPurgeInfo.m_purgeBy)
		{
			case MSG_PurgeByAge:
			{
				time_t now = XP_TIME();
				long matchDay = now - (m_headerPurgeInfo.m_daysToKeep * 60 * 60 * 24);
				if (pHeader->GetDate() < matchDay)
					PurgeHeader(pHeader);
				else
				{
					delete pHeader;
					m_headerIndex++;
				}
				break;
			}
			case MSG_PurgeByNumHeaders:
			{
				int32 numInDB = (m_headerPurgeInfo.m_unreadOnly) ? m_groupInfo->GetNumNewMessages() : m_groupInfo->GetNumMessages();
				if (m_headerIndex < (numInDB -  m_headerPurgeInfo.m_numHeadersToKeep + 1))
					PurgeHeader(pHeader);
				else
				{
					delete pHeader;
					m_headerIndex++;
				}
				break;
			}
			default:
				delete pHeader;
				m_headerIndex++;
				if (!purgedSomething)
				{
					if (m_pNextHeader)
					{
						delete m_pNextHeader;
						m_pNextHeader = NULL;
					}
					return MK_CONNECTED;
				}
		}
	}
	// don't want to purge at MOD of 0, because when we're deleting by number
	// of headers, m_headerIndex will stay at 0 for a while.
	if (m_newsDB && m_headerIndex % kPurgeCommitChunk == 1)
		m_newsDB->Commit();

	return MK_WAITING_FOR_CONNECTION;
}

int	MSG_PurgeNewsgroupState::FinishPurging()
{
	if (m_folder)
		m_folder->ClearRequiresCleanup();

	return MK_CONNECTED;
}

void  MSG_PurgeNewsgroupState::PurgeHeader(DBMessageHdr *hdr)
{
	if (hdr != NULL)
	{
		m_newsDB->DeleteHeader(hdr, NULL, !(m_purgeCount % 200));
		m_purgeCount++;
		char *statusTemplate = XP_GetString (MK_MSG_PURGING_NEWSGROUP_HEADER);
		char *statusString = PR_smprintf (statusTemplate,  m_folder->GetPrettiestName(), m_purgeCount);
		if (statusString)
		{
			FE_Progress (m_context, statusString);
			XP_FREE(statusString);
		}
	}
}

void  MSG_PurgeNewsgroupState::PurgeArticle(DBMessageHdr *hdr)
{
	if (hdr != NULL)
	{
		// OK, we know it's a news hdr becuase it's a newsdb.
		NewsMessageHdr *newsHdr = (NewsMessageHdr *) hdr;
		newsHdr->PurgeArticle();
		m_purgeCount++;
		char *statusTemplate = XP_GetString (MK_MSG_PURGING_NEWSGROUP_ARTICLE);
		char *statusString = PR_smprintf (statusTemplate,  m_folder->GetPrettiestName(), m_purgeCount);
		if (statusString)
		{
			FE_Progress (m_context, statusString);
			XP_FREE(statusString);
		}
	}
}
