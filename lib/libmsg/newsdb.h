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
#ifndef _NewsGroupDB_H_
#define _NewsGroupDB_H_

#include "msgdb.h"
#include "msgdbvw.h"
#include "errcode.h"
#include "msghdr.h"
#include "msgpurge.h"

class DBThreadMessageHdr;
class NewsMessageHdr;
struct MSG_FilterList;

class msg_NewsArtSet;
class MSG_FolderInfoNews;
class NewsFolderInfo;
class MSG_SearchTermArray;
struct MSG_PurgeInfo;
struct MSG_RetrieveArtInfo;

const int kNewsDBVersion = 13;

// news group database

class NewsGroupDB : public MessageDB
{
public:
			NewsGroupDB();
	virtual ~NewsGroupDB();
	virtual  MsgERR MessageDBOpenUsingURL(const char * groupURL);
	char *GetGroupURL() { return m_groupURL; }
	static MsgERR	Open(const char * groupURL, MSG_Master *master,
						 NewsGroupDB** pMessageDB);
	virtual MsgERR		Close();
	virtual MsgERR		ForceClosed();
	virtual MsgERR		Commit(XP_Bool compress = FALSE);

	virtual int		GetCurVersion();

// methods to get and set docsets for ids.
	virtual MsgERR		MarkHdrRead(DBMessageHdr *msgHdr, XP_Bool bRead,
								ChangeListener *instigator = NULL);
	virtual MsgERR		IsRead(MessageKey messageKey, XP_Bool *pRead);
	virtual XP_Bool		IsArticleOffline(MessageKey messageKey);
	virtual MsgERR		MarkAllRead(MWContext *context, IDArray *thoseMarked = NULL);
	virtual MsgERR		AddHdrFromXOver(const char * line,  MessageKey *msgId);
	virtual MsgERR		AddHdrToDB(DBMessageHdr *newHdr, XP_Bool *newThread, XP_Bool notify = FALSE);

	virtual MsgERR			ListNextUnread(ListContext **pContext, DBMessageHdr **pResult);
	virtual NewsMessageHdr	*GetNewsHdrForKey(MessageKey messageKey);
	// return highest article number we've seen.
	virtual MessageKey		GetHighwaterArticleNum(); 
	virtual MessageKey		GetLowWaterArticleNum();

	virtual MsgERR			ExpireUpTo(MessageKey expireKey, MWContext *context);
	virtual MsgERR			ExpireRange(MessageKey startRange, MessageKey endRange, MWContext *context);

	msg_NewsArtSet			*GetNewsArtSet() {return m_set;}
	virtual NewsGroupDB		*GetNewsDB() ;

	virtual XP_Bool			PurgeNeeded(MSG_PurgeInfo *hdrPurgeInfo, MSG_PurgeInfo *artPurgeInfo);
	XP_Bool					IsCategory();
	NewsFolderInfo			*GetNewsFolderInfo() {return (NewsFolderInfo *) m_dbFolderInfo;}
	MsgERR					SetOfflineRetrievalInfo(MSG_RetrieveArtInfo *);
	MsgERR					SetPurgeHeaderInfo(MSG_PurgeInfo *purgeInfo);
	MsgERR					SetPurgeArticleInfo(MSG_PurgeInfo *purgeInfo);
	MsgERR					GetOfflineRetrievalInfo(MSG_RetrieveArtInfo *info);
	MsgERR					GetPurgeHeaderInfo(MSG_PurgeInfo *purgeInfo);
	MsgERR					GetPurgeArticleInfo(MSG_PurgeInfo *purgeInfo);

	MSG_FolderInfoNews		*GetFolderInfoNews() {return m_info;}
	// caller needs to free
	static char				*GetGroupNameFromURL(const char *url);
protected:

	virtual					DBFolderInfo *CreateFolderInfo(MSG_DBFolderInfoHandle handle);

	char*				m_groupURL;
	MSG_FilterList*		m_filterList;

	uint32				m_headerIndex;		// index of unthreaded headers
												// at a specified entry.
	MSG_Master		*m_master;

	msg_NewsArtSet *m_set;

	MSG_FolderInfoNews* m_info;
};

#endif
