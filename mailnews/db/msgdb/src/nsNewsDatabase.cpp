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
#include "nsNewsDatabase.h"

nsNewsDatabase::nsNewsDatabase()
{
}

nsNewsDatabase::~nsNewsDatabase()
{
}

nsresult nsNewsDatabase::MessageDBOpenUsingURL(const char * groupURL)
{
	return 0;
}

/* static */ 
nsresult	nsNewsDatabase::Open(const char * groupURL, MSG_Master *master,
						 nsNewsDatabase** pMessageDB)
{
	return 0;
}
nsresult		nsNewsDatabase::Close(PRBool forceCommit)
{
	return nsMsgDatabase::Close(forceCommit);
}

nsresult		nsNewsDatabase::ForceClosed()
{
	return nsMsgDatabase::ForceClosed();
}

nsresult		nsNewsDatabase::Commit(nsMsgDBCommitType commitType)
{
	return nsMsgDatabase::Commit(commitType);
}


PRUint32		nsNewsDatabase::GetCurVersion()
{
	return 1;
}

// methods to get and set docsets for ids.
NS_IMETHODIMP nsNewsDatabase::MarkHdrRead(nsIMessage *msgHdr, PRBool bRead,
								nsIDBChangeListener *instigator)
{
	nsresult		err;
#if 0
	nsMsgKey messageKey = msgHdr->GetMessageKey();

	if (bRead)
		err = m_set->Add(messageKey);
	else
		err = m_set->Remove(messageKey);
#endif
	// give parent class chance to update data structures
	nsMsgDatabase::MarkHdrRead(msgHdr, bRead, instigator);

//	return (err >= 0) ? 0 : NS_ERROR_OUT_OF_MEMORY;
	return 0;
}

NS_IMETHODIMP nsNewsDatabase::IsRead(nsMsgKey key, PRBool *pRead)
{
	XP_ASSERT(pRead != NULL);
	if (pRead == NULL) 
		return NS_ERROR_NULL_POINTER;
#if 0
	PRBool isRead = m_set->IsMember(messageKey);
	*pRead = isRead;
#endif
	return 0;
}

PRBool nsNewsDatabase::IsArticleOffline(nsMsgKey key)
{
	return 0;
}

nsresult		nsNewsDatabase::MarkAllRead(nsMsgKeyArray *thoseMarked)
{
	return 0;
}
nsresult		nsNewsDatabase::AddHdrFromXOver(const char * line,  nsMsgKey *msgId)
{
	return 0;
}

NS_IMETHODIMP		nsNewsDatabase::AddHdrToDB(nsMsgHdr *newHdr, PRBool *newThread, PRBool notify)
{
	return 0;
}

NS_IMETHODIMP		nsNewsDatabase::ListNextUnread(ListContext **pContext, nsMsgHdr **pResult)
{
	return 0;
}

	// return highest article number we've seen.
	nsMsgKey		nsNewsDatabase::GetHighwaterArticleNum()
	{
		return 0;
	}
	nsMsgKey		nsNewsDatabase::GetLowWaterArticleNum()
	{
		return 0;
	}

	nsresult		nsNewsDatabase::ExpireUpTo(nsMsgKey expireKey)
	{
		return 0;
	}
	nsresult		nsNewsDatabase::ExpireRange(nsMsgKey startRange, nsMsgKey endRange)
	{
		return 0;
	}

nsNewsSet				*nsNewsDatabase::GetNewsArtSet() 
{
	return m_set;
}

nsNewsDatabase	*nsNewsDatabase::GetNewsDB() 
{
	return this;
}

PRBool	nsNewsDatabase::PurgeNeeded(MSG_PurgeInfo *hdrPurgeInfo, MSG_PurgeInfo *artPurgeInfo) { return PR_FALSE; };
PRBool	nsNewsDatabase::IsCategory();
nsresult nsNewsDatabase::SetOfflineRetrievalInfo(MSG_RetrieveArtInfo *);
nsresult nsNewsDatabase::SetPurgeHeaderInfo(MSG_PurgeInfo *purgeInfo);
nsresult nsNewsDatabase::SetPurgeArticleInfo(MSG_PurgeInfo *purgeInfo);
nsresult nsNewsDatabase::GetOfflineRetrievalInfo(MSG_RetrieveArtInfo *info);
nsresult nsNewsDatabase::GetPurgeHeaderInfo(MSG_PurgeInfo *purgeInfo);
nsresult nsNewsDatabase::GetPurgeArticleInfo(MSG_PurgeInfo *purgeInfo);

	// used to handle filters editing on open news groups.
//	static void				NotifyOpenDBsOfFilterChange(MSG_FolderInfo *folder);
	void					nsNewsDatabase::ClearFilterList();	// filter was changed by user.
	void					nsNewsDatabase::OpenFilterList();
//	void					OnFolderFilterListChanged(MSG_FolderInfo *folder);
	// caller needs to free
/* static */ char				*nsNewsDatabase::GetGroupNameFromURL(const char *url);

