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

#ifndef MSGDBTYP_H
#define MSGDBTYP_H

typedef void *MSG_DBHandle; 
typedef void *MSG_HeaderHandle; 
typedef void *MSG_MailHeaderHandle; 
typedef void *MSG_NewsHeaderHandle; 

typedef void *MSG_DBFolderInfoHandle; 
typedef void *MSG_MailDBFolderInfoHandle; 
typedef void *MSG_NewsDBFolderInfoHandle; 

typedef void *MSG_ThreadHandle; 

typedef void *MSG_OfflineIMAPOperationHandle;
typedef void *MSG_OfflineMsgDocumentHandle;
typedef void *MSG_IteratorHandle;

const int kNumUnused = 8;
typedef uint32 MsgERR ;

enum SortOrder
{
	SortTypeNone,
	SortTypeAscending,
	SortTypeDescending
};
enum SortType
{
	SortByDate = 0x12,
	SortBySubject = 0x13,
	SortByAuthor = 0x14,
	SortById = 0x15,
	SortByThread = 0x16,
	SortByPriority = 0x17,
	SortByStatus = 0x18,
	SortBySize = 0x19,
	SortByFlagged = 0x1a,
	SortByUnread = 0x1b,
	SortByRelevance,
	SortByRecipient
};

enum ViewType
{
	ViewAny,				// this view type matches any other view type, 
							// for the purpose of matching cached views.
							// Else, it's equivalent to ViewAllThreads
	ViewAllThreads,			// default view, no killed threads
	ViewOnlyThreadsWithNew,
	ViewKilledThreads,			// obsolete!!! Only remembered because it's in old db's.
	ViewOnlyNewHeaders,		
	ViewWatchedThreadsWithNew,
	ViewCustom,					// client will insert id's by hand
	ViewCacheless				// probably obsoletes ViewCustom	
};


/* This struct is used to get and set the basic info about a folder */
typedef struct
{
	uint16		m_version;			/* for upgrading...*/
	int32		m_sortType;			/* the last sort type open on this db. */
	int16		m_csid;				/* default csid for these messages */
	int16		m_IMAPHierarchySeparator;	/* imap path separator */
	int8		m_sortOrder;		/* the last sort order (up or down) */
	/* mail only (for now) */
	int32		m_folderSize;
	time_t		m_folderDate;
	int32		m_parsedThru;		/* how much of the folder have we parsed? Not sure needed in new world order */
	int32		m_expunged_bytes;	/* sum of size of deleted messages in folder */
	
	// IMAP only
	int32		m_LastMessageUID;
	int32		m_ImapUidValidity;
	int32		m_TotalPendingMessages;
	int32		m_UnreadPendingMessages;

	// news only (for now)
	MessageKey   m_articleNumHighWater;	/* largest article number whose header we've seen */
	MessageKey	m_expiredMark;		/* Highest invalid article number in group - for expiring */
	int32		m_viewType;			/* for news, the last view type open on this db.	*/

	int32		m_numVisibleMessages;	// doesn't include expunged or ignored messages (but does include collapsed).
	int32		m_numNewMessages;
	int32		m_numMessages;		// includes expunged and ignored messages
	int32		m_flags;			// folder specific flags. This holds things like re-use thread pane,
									// configured for off-line use, use default retrieval, purge article/header options
	MessageKey	m_lastMessageLoaded; // set by the FE's to remember the last loaded message
	int32		m_unused[kNumUnused];
} MSG_DBFolderInfoExchange;

typedef struct
{
	MessageKey   m_threadId; 
	MessageKey	m_messageKey; 	//news: article number, mail mbox offset
	time_t 		m_date;                         
	uint32		m_messageSize;	// lines for news articles, bytes for mail messages
	uint32		m_flags;
	char		m_level;
} MSG_DBHeaderExchange;

typedef struct
{
	uint16		m_numChildren;		
	uint16		m_numNewChildren;	
	uint32		m_flags;
	MessageKey	m_threadKey;
} MSG_DBThreadExchange;

typedef struct MSG_PurgeInfo
{
	MSG_PurgeByPreferences	m_purgeBy;
	XP_Bool			m_useDefaults;
	XP_Bool			m_unreadOnly;
	int32			m_daysToKeep;
	int32			m_numHeadersToKeep;
} MSG_PurgeInfo;

typedef struct MSG_RetrieveArtInfo
{
	XP_Bool		m_useDefaults;
	XP_Bool		m_byReadness;
	XP_Bool		m_unreadOnly;
	XP_Bool		m_byDate;
	int32		m_daysOld;
} MSG_RetrieveArtInfo;


#endif

