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
// mail/news folder info - Currently, DBFolderInfo is used for mail, and its subclass,
//  DBFolderInfoNews is used by news.
#ifndef _GRPINFO_H
#define _GRPINFO_H

#include "msgfinfo.h"
#include "msgpurge.h"
#include "nwsartst.h"
#include "msgstrob.h"

class NewsGroupDB;

const int kFolderInfoVersion = 12;

class DBFolderInfo 
{
public:
						/** Instance Methods **/
						DBFolderInfo(MSG_DBFolderInfoHandle handle);
						~DBFolderInfo();
	void				SetHighWater(MessageKey highWater, Bool force = FALSE) ;
	MessageKey			GetHighWater() {return m_articleNumHighWater;}
	void				SetExpiredMark(MessageKey expiredKey);
	int					GetDiskVersion() {return m_version;}
	void				GetExchangeInfo(MSG_DBFolderInfoExchange &infoExchange);
	void				SetExchangeInfo(MSG_DBFolderInfoExchange &infoExchange);

	XP_Bool				AddLaterKey(MessageKey key, time_t until);
	int32				GetNumLatered();
	MessageKey			GetLateredAt(int32 laterIndex, time_t *pUntil);
	void				RemoveLateredAt(int32 laterIndex);

	virtual void		SetNewArtsSet(const char *newArtsSet, MSG_DBHandle dbHandle);
	virtual void		GetNewArtsSet(XPStringObj &newArtsSet);

	virtual void		SetMailboxName(const char *newBoxName);
	virtual void		GetMailboxName(XPStringObj &boxName);

	void				SetViewType(int32 viewType);
	int32				GetViewType() {return m_viewType;}
	void				SetSortInfo(SortType, SortOrder);
	void				GetSortInfo(SortType *, SortOrder *);
	int32				ChangeNumNewMessages(int32 delta);
	int32				ChangeNumMessages(int32 delta);
	int32				ChangeNumVisibleMessages(int32 delta);
	int32				GetNumNewMessages() {return m_numNewMessages;}
	int32				GetNumMessages() {return m_numMessages;}
	int32				GetNumVisibleMessages() {return m_numVisibleMessages;}
	int32				GetFlags();
	void				SetFlags(int32 flags);
	void				OrFlags(int32 flags);
	void				AndFlags(int32 flags);
	XP_Bool				TestFlag(int32 flags);
	int16				GetCSID() {return m_csid;}
	void				SetCSID(int16 csid) {m_csid = csid; }
	int16				GetIMAPHierarchySeparator() {return m_IMAPHierarchySeparator;}
	void				SetIMAPHierarchySeparator(int16 hierarchySeparator) {m_IMAPHierarchySeparator = hierarchySeparator;}
	int32				GetImapTotalPendingMessages() {return m_TotalPendingMessages;}
	void				ChangeImapTotalPendingMessages(int32 delta) {m_TotalPendingMessages+=delta;}
	int32				GetImapUnreadPendingMessages() {return m_UnreadPendingMessages;}
	void				ChangeImapUnreadPendingMessages(int32 delta) {m_UnreadPendingMessages+=delta;}
	
	int32				GetImapUidValidity() {return m_ImapUidValidity;}
	void				SetImapUidValidity(int32 uidValidity) {m_ImapUidValidity=uidValidity;}

	MessageKey			GetLastMessageLoaded() {return m_lastMessageLoaded;}
	void				SetLastMessageLoaded(MessageKey lastLoaded) {m_lastMessageLoaded=lastLoaded;}

	void				SetCachedPassword(const char *password, MSG_DBHandle db);
	void				GetCachedPassword(XPStringObj &password, MSG_DBHandle db);
	virtual void		AddReference();
	virtual void		RemoveReference();

	// mail and news
	uint16		m_version;			// for upgrading...
	int32		m_sortType;			// the last sort type open on this db.
	int16		m_csid;				// default csid for these messages
	int16		m_IMAPHierarchySeparator;	// imap path separator
	int8		m_sortOrder;		// the last sort order (up or down
	// mail only (for now)
	int32		m_folderSize;
	time_t		m_folderDate;
	int32		m_parsedThru;		// how much of the folder have we parsed? Not sure needed in new world order
	int32		m_expunged_bytes;	// sum of size of deleted messages in folde
	
	// IMAP only
	int32		m_LastMessageUID;
	int32		m_ImapUidValidity;
	int32		m_TotalPendingMessages;
	int32		m_UnreadPendingMessages;

	// news only (for now)
	MessageKey   m_articleNumHighWater;	// largest article number whose header we've seen
	MessageKey	m_expiredMark;		// Highest invalid article number in group - for expiring
	int32		m_viewType;			// for news, the last view type open on this db.	
	MSG_DBFolderInfoHandle	GetHandle() {return m_dbFolderInfoHandle;}
	void		SetHandle(MSG_DBFolderInfoHandle handle) {m_dbFolderInfoHandle = handle;}

	// db object on disk contains kNumUnused more uint32's for future expansion
protected:

	MSG_DBFolderInfoHandle	m_dbFolderInfoHandle;
	int32		m_numVisibleMessages;	// doesn't include expunged or ignored messages (but does include collapsed).
	int32		m_numNewMessages;
	int32		m_numMessages;		// includes expunged and ignored messages
	FolderType	m_folderType;		// not currently filed out
	int32		m_flags;			// folder specific flags. This holds things like re-use thread pane,
									// configured for off-line use, use default retrieval, purge article/header options
	MessageKey	m_lastMessageLoaded; // set by the FE's to remember the last loaded message
};


class NewsFolderInfo : public DBFolderInfo
{
public:
	NewsFolderInfo(MSG_DBFolderInfoHandle handle);
	~NewsFolderInfo();
	virtual void		SetKnownArtsSet(const char *newsArtSet);
	virtual void		GetKnownArtsSet(XPStringObj &newsArtSet);
	// offline retrieval and purge control settings - these override the users'
	// default settings, which should be stored in the xp preferences, when that's done.
	virtual void		SetOfflineRetrievalInfo(MSG_RetrieveArtInfo *retrieveArtInfo);
	virtual void		SetPurgeHeaderInfo(MSG_PurgeInfo *info);
	virtual void		SetPurgeArticleInfo(MSG_PurgeInfo *info);
	virtual void		ClearOfflineRetrievalTerms();
	virtual void		ClearPurgeHeaderTerms();
	virtual void		ClearPurgeArticleTerms();
	virtual MsgERR		GetOfflineRetrievalInfo(MSG_RetrieveArtInfo *retrieveArtInfo);
	virtual MsgERR		GetPurgeHeaderInfo(MSG_PurgeInfo *info);
	virtual MsgERR		GetPurgeArticleInfo(MSG_PurgeInfo *info);

			void		SetNewsDB(NewsGroupDB *newsDB) {m_newsDB = newsDB;}

protected:
	NewsGroupDB		*m_newsDB;
	time_t			m_newsRCLineDate;	// the time/date  we remembered this line.
	// These are streamed search terms, preceded by the count of terms.

	MSG_PurgeInfo m_purgeHdrInfo;		// news hdr purge info.
	MSG_PurgeInfo m_purgeArtInfo;		// news art purge info. (only uses age, currently)
	MSG_RetrieveArtInfo m_retrieveArtInfo;

};

// kmcentee
// Use one of these guys to transfer info when duplicating a message db.
// Initially this happens when emptying trash (pop and imap) and compressing
// pop folders.

// The reason that this is a separate object rather than some methods on
// DBFolderInfo is that it is handy to delete the source db before you
// create the new one.
class TDBFolderInfoTransfer {
public:
	TDBFolderInfoTransfer(DBFolderInfo &sourceInfo);
	~TDBFolderInfoTransfer() {}
	
	void	TransferFolderInfo(DBFolderInfo &destinationInfo);
private:
	// add transfer fields as needed.
	XPStringObj  m_mailboxName;		// name presented to the user, will match imap server name
	int16		m_IMAPHierarchySeparator;	// imap path separator
	int32		m_viewType;
	SortType	m_sortType;			// the last sort type open on this db.
	int16		m_csid;				// default csid for these messages
	SortOrder	m_sortOrder;		// the last sort order (up or down
	int32		m_flags;			// folder specific flags. This holds things like re-use thread pane,
									// configured for off-line use, use default retrieval, purge article/header options
};
	

XP_BEGIN_PROTOS

XP_Bool msg_IsSummaryValid(const char* pathname, XP_StatStruct* folderst);
void msg_SetSummaryValid(const char* pathname, int num, int numunread);

XP_END_PROTOS

#endif

