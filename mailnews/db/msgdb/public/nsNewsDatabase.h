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
#ifndef _nsNewsDatabase_H_
#define _nsNewsDatabase_H_

#include "nsMsgDatabase.h"
// news group database

class nsNewsDatabase : public nsMsgDatabase
{
public:
			nsNewsDatabase();
	virtual ~nsNewsDatabase();
	virtual  nsresult MessageDBOpenUsingURL(const char * groupURL);
	char *GetGroupURL() { return m_groupURL; }
	static nsresult	Open(const char * groupURL, MSG_Master *master,
						 nsNewsDatabase** pMessageDB);
	virtual nsresult		Close(PRBool forceCommit = PR_TRUE);
	virtual nsresult		ForceClosed();
	virtual nsresult		Commit(PRBool compress = PR_FALSE);

	virtual int		GetCurVersion();

// methods to get and set docsets for ids.
	virtual nsresult		MarkHdrRead(nsMsgHdr *msgHdr, PRBool bRead,
								ChangeListener *instigator = NULL);
	virtual nsresult		IsRead(MessageKey messageKey, PRBool *pRead);
	virtual PRBool		IsArticleOffline(MessageKey messageKey);
	virtual nsresult		MarkAllRead(MWContext *context, IDArray *thoseMarked = NULL);
	virtual nsresult		AddHdrFromXOver(const char * line,  MessageKey *msgId);
	virtual nsresult		AddHdrToDB(nsMsgHdr *newHdr, PRBool *newThread, PRBool notify = PR_FALSE);

	virtual nsresult		ListNextUnread(ListContext **pContext, nsMsgHdr **pResult);
	// return highest article number we've seen.
	virtual MessageKey		GetHighwaterArticleNum(); 
	virtual MessageKey		GetLowWaterArticleNum();

	virtual nsresult		ExpireUpTo(MessageKey expireKey);
	virtual nsresult		ExpireRange(MessageKey startRange, MessageKey endRange);

	msg_NewsArtSet			*GetNewsArtSet() {return m_set;}
	virtual nsNewsDatabase	*GetNewsDB() ;

	virtual PRBool			PurgeNeeded(MSG_PurgeInfo *hdrPurgeInfo, MSG_PurgeInfo *artPurgeInfo);
	PRBool					IsCategory();
	NewsFolderInfo			*GetNewsFolderInfo() {return (NewsFolderInfo *) m_FolderInfo;}
	nsresult				SetOfflineRetrievalInfo(MSG_RetrieveArtInfo *);
	nsresult				SetPurgeHeaderInfo(MSG_PurgeInfo *purgeInfo);
	nsresult				SetPurgeArticleInfo(MSG_PurgeInfo *purgeInfo);
	nsresult				GetOfflineRetrievalInfo(MSG_RetrieveArtInfo *info);
	nsresult				GetPurgeHeaderInfo(MSG_PurgeInfo *purgeInfo);
	nsresult				GetPurgeArticleInfo(MSG_PurgeInfo *purgeInfo);

	MSG_FolderInfoNews		*GetFolderInfoNews() {return m_info;}
	// used to handle filters editing on open news groups.
	static void				NotifyOpenDBsOfFilterChange(MSG_FolderInfo *folder);
	void					ClearFilterList();	// filter was changed by user.
	void					OpenFilterList();
	void					OnFolderFilterListChanged(MSG_FolderInfo *folder);
	// caller needs to free
	static char				*GetGroupNameFromURL(const char *url);
protected:

	char*				m_groupURL;
	MSG_FilterList*		m_filterList;

	PRUint32				m_headerIndex;		// index of unthreaded headers
												// at a specified entry.
	MSG_Master		*m_master;

	msg_NewsArtSet *m_set;

	MSG_FolderInfoNews* m_info;
};

#endif
