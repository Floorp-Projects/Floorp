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

#ifndef _MsgDB_H_
#define _MsgDB_H_

#include "msgcom.h"
#include "ptrarray.h"
#include "dberror.h"
#include "chngntfy.h"
#include "msgdbtyp.h"

class MessageDB;
class MsgDocument;
class DBFolderInfo;
class MailDB;
class NewsGroupDB;
class XPByteArray;
class msg_NewsArtSet;
class XPStringObj;

const int kMaxSubject = 160;
const int kMaxAuthor = 160;
const int kMaxRecipient = 80;
const int kMaxMsgIdLen = 80;
const int kMaxReferenceLen = 10 * kMaxMsgIdLen;

const MessageKey kIdNone = 0xffffffff;
const MessageKey kIdPending = 0xfffffffe;
const MessageKey kIdStartOfFake = 0xffffff80; // start of the fake message id

#include "idarray.h"	// need MessageKey to be defined before including...

struct ListContext
{
	MessageDB			*m_pMessageDB;
	int					m_curMessageNum;
	MSG_IteratorHandle	m_iterator;			
};

struct MessageHdrStruct
{
	MessageKey   m_threadId; 
	MessageKey	m_messageKey; 	
	char		m_subject[kMaxSubject];
	char		m_author[kMaxAuthor];
	char		m_messageId[kMaxMsgIdLen];
	char		m_references[kMaxReferenceLen]; 
	char		m_recipients[kMaxRecipient];
	time_t 		m_date;                         
	uint32		m_messageSize;	// lines for news articles, bytes for mail messages
	uint32		m_flags;
	uint16		m_numChildren;		// for top-level threads
	uint16		m_numNewChildren;	// for top-level threads
	char		m_level;		// indentation level
	MSG_PRIORITY m_priority;
public:
	void SetSubject(const char * subject);
	void SetAuthor(const char * author);
	void SetMessageID(const char * msgID);
	void SetReferences(const char * referencesStr);
	void SetDate(const char * date);
	void SetLines(uint32 lines);
	void SetSize(uint32 size);
	const char *GetReference(const char *nextRef, char *reference);
	static void StripMessageId(const char *msgID, char *outMsgId, int msgIdLen);
};

typedef int32 MsgFlags;	// try to keep flags view is going to care about
						// in the first byte, so we can copy the flags over directly

const int32	kIsRead			= 0x1;	// same as MSG_FLAG_READ
const int32	kReplied		= 0x2;	// same as MSG_FLAG_REPLIED
const int32	kMsgMarked		= 0x4;	// same as MSG_FLAG_MARKED - (kMarked collides with IMAP)
const int32	kHasChildren	= 0x8;  // no equivalent (shares space with FLAG_EXPUNGED)
const int32	kIsThread		= 0x10; // !!shares space with MSG_FLAG_HAS_RE
const int32	kElided			= 0x20; // same as MSG_FLAG_ELIDED
const int32	kExpired		= 0x40; // same as MSG_FLAG_EXPIRED
const int32	kOffline		= 0x80; // this message has been downloaded
const int32	kWatched		= 0x100;
const int32	kSenderAuthed	= 0x200; // same as MSG_FLAG_SENDER_AUTHED
const int32	kExpunged		= 0x400;  // NOT same value as MSG_FLAG_EXPUNGED
const int32	kHasRe			= 0x800;  // Not same values as MSG_FLAG_HAS_RE
const int32	kForwarded		= 0x1000; // this message has been forward (mail only)
const int32	kIgnored		= 0x2000; // this message is ignored

const int32	kMDNNeeded		= 0x4000; // this message needs MDN report
const int32 kMDNSent		= 0x8000; // MDN report has been Sent

const int32 kNew			= 0x10000; // used for status, never stored in db.
const int32	kAdded			= 0x20000; // Just added to db - used in 
							  // notifications, never set in msgHdr.

const int32 kTemplate       = 0x40000; // this message is a template

const int32 kDirty			= 0x80000;
const int32 kPartial		= 0x100000;	// NOT same value as MSG_FLAG_PARTIAL
const int32 kIMAPdeleted	= 0x200000;	// same value as MSG_FLAG_IMAP_DELETED
const int32 kHasAttachment	= 0x10000000;	// message has attachments

const int32	kSameAsMSG_FLAG  = kHasAttachment|kIsRead|kMsgMarked|kExpired|kElided|kSenderAuthed|kReplied|kOffline|kForwarded|kWatched|kIMAPdeleted;
const int32	kMozillaSameAsMSG_FLAG  = kIsRead|kMsgMarked|kExpired|kElided|kSenderAuthed|kReplied|kOffline|kForwarded|kWatched|kIMAPdeleted;
const int32	kExtraFlags		= 0xFF;


class MessageDBArray : public XPPtrArray
{
public:
	MessageDBArray();
	
	// overrides with proper types to avoid casting
	MessageDB* GetAt(int nIndex) const {return((MessageDB*)XPPtrArray::GetAt(nIndex));}
	void* operator[](int nIndex) const {return((MessageDB*)XPPtrArray::operator[](nIndex));}
};


#include "msghdr.h"

class DBThreadMessageHdr;
class DBMessageHdr;

typedef XP_Bool
HdrCompareFunc (DBMessageHdr *hdr);

// the DB doing threading, partly because we want it to be persistent,
// to avoid rethreading everytime a group is opened.  
class MessageDB : public ChangeAnnouncer
{
	friend class MessageDBView;
	friend class DBThreadMessageHdr;
public:
	MessageDB();
	virtual ~MessageDB();
	virtual	MsgERR	OpenDB(const char *dbFileName, XP_Bool create);
	virtual MsgERR 	MessageDBOpen(const char * dbName, XP_Bool create);

	virtual MsgERR		Close();
	virtual MsgERR		ForceClosed();
	virtual MsgERR		CloseDB();
	virtual MsgERR		OnNewPath (const char * /*path*/) ;
	static  MsgERR		RenameDB(const char *sourceName, const char *destName);
	virtual MsgERR		SetSummaryValid(XP_Bool valid = TRUE);

	virtual MsgERR		Commit(XP_Bool compress = FALSE);

	virtual MsgERR		GetMessageHdr(MessageKey messageNum, MessageHdrStruct *);
	virtual MsgERR		GetShortMessageHdr(MessageKey messageNum, MSG_MessageLine *msgHdr);

	virtual XP_Bool		KeyToAddExists(MessageKey messageKey);

	virtual MsgERR		AddHeaderToArray(DBMessageHdr *header);
	virtual MsgERR		AddHdr(DBMessageHdr *hdr);
	virtual MsgERR		AddHdrToDB(DBMessageHdr *newHdr, XP_Bool *newThread, 
									XP_Bool notify = FALSE);
	virtual MsgERR		AddThread(DBMessageHdr *msgHdr);
	virtual DBThreadMessageHdr *GetDBThreadHdrForSubject(DBMessageHdr *msgHdr);
	virtual DBThreadMessageHdr *GetDBMsgHdrForReference(const char * msgID);
	virtual DBMessageHdr *		GetDBMessageHdrForID(const char * msgID);
	virtual MsgERR		GetHeaderFromHandle(MSG_HeaderHandle headerHandle, DBMessageHdr **pResult);
			DBThreadMessageHdr *GetThreadHeaderFromHandle(MSG_ThreadHandle handle);

	virtual MessageKey	GetMessageKeyForID(const char *msgID);
	virtual DBThreadMessageHdr *GetDBThreadHdrForMsgID(MessageKey messageKey);
	virtual MessageKey	GetThreadIdForMsgId(MessageKey messageKey);
	virtual MsgERR		AddToThread(DBMessageHdr *reply, DBThreadMessageHdr *threadHdr, XP_Bool threadInThread);
	// we've finished receiving new headers - clean up any temporary info.
	virtual MsgERR		FinishAddingHeaders();
	// given an array of thread ids, return list header information for each thread
	MsgERR		ListHeaders(MessageKey *pMessageNums, int numToList, MessageHdrStruct *pOutput, int *pNumListed);
	MsgERR		ListHeadersShort(MessageKey *pMessageNums, int numToList, MSG_MessageLine *pOutput, int *pNumListed);
	// list the headers of the top-level thread ids 
	virtual MsgERR		ListThreadIds(MessageKey *startMsg, XP_Bool unreadOnly, MessageKey *pOutput, int32 *pFlags, 
										char *pLevels, int numToList, int *numListed, MessageDBView *view, int32 *pTotalHeaders);
	virtual MsgERR		ListThreadIds(ListContext *context, MessageKey *pOutput, int numToList, int *numListed);
	// Info about a given thread
	virtual MsgERR		GetThreadCount(MessageKey messageKey, int32 *pThreadCount);
	virtual MsgERR		GetThreadCount(DBThreadMessageHdr *threadHdr, int32 *pThreadCount);
	virtual MsgERR		GetDBThreadListContext(MessageKey threadId, MessageKey startMsg, DBThreadMessageHdr **pThreadHdr, uint16 *pThreadIndex);
	virtual MsgERR		GetDBThreadListContext(DBMessageHdr *msgHdr, MessageKey startMsg, DBThreadMessageHdr **pThreadHdr, uint16 *pThreadIndex);
	virtual DBThreadMessageHdr * GetDBThreadHdrForThreadID(MessageKey messageID);
	virtual DBMessageHdr		*GetDBHdrForKey(MessageKey messageKey);
	virtual DBThreadMessageHdr *GetDBThreadHdrForMsgHdr(DBMessageHdr *msgHdr);
	virtual MessageKey	GetKeyOfFirstMsgInThread(MessageKey key);

// return a flat list of ids (no threading)
	MsgERR		ListAllIds(IDArray &outputIds);
	MsgERR		ListAllIds(IDArray *outputIds);
	// iterate through message headers, in no particular order (currently, id order).
	// Caller must unrefer return DBMessageHdr when done with it.
	// eDBEndOfList will be returned when return DBMessageHdr * is NULL.
	// Caller must call ListDone to free up the context.
	// ListLast will produce a reverse iterator that starts at the end and moves towards the start
	MsgERR				ListFirst(ListContext **pContext, DBMessageHdr **pResult);
	MsgERR				ListLast(ListContext **pContext, DBMessageHdr **pResult);
	MsgERR				ListNext(ListContext *pContext, DBMessageHdr **pResult);
	MsgERR				ListDone(ListContext *pContext);

	MessageKey			ListHighwaterMark();	// returns the biggest key, kIdNone if none found.
	// return the list header information for the documents in a thread.
	virtual MsgERR		ListIdsInThread(MessageKey threadId, MessageKey *startMsg, int numToList, MessageKey *pOutput, char *pFlags, char *pLevels, int *pNumListed);
	virtual MsgERR		ListIdsInThread(DBMessageHdr *msgHdr, MessageKey *startMsg, int numToList, MessageKey *pOutput, char *pFlags, char *pLevels, int *pNumListed);
	virtual MsgERR		ListIdsInThread(DBThreadMessageHdr *threadHdr, uint16	threadIndex, MessageKey *startMsg, int numToList, MessageKey *pOutput, char *pFlags, char *pLevels, int *pNumListed);
	virtual MsgERR		ListUnreadIdsInThread(MessageKey threadId, MessageKey *startMsg, XPByteArray &levelStack, int numToList, MessageKey *pOutput, char *pFlags, char *pLevels, int *pNumListed);
	virtual MsgERR		ListNextUnread(ListContext **pContext, DBMessageHdr **pResult);

	static XP_Bool		MatchFlaggedNotOffline(DBMessageHdr *hdr);
	virtual void		ListMatchingKeys(HdrCompareFunc *compareFunc, IDArray &matchingKeys);
	MsgERR		Purge();

	// MDN support
	virtual MsgERR		MarkMDNNeeded(MessageKey messageKey, XP_Bool bNeeded,
									  ChangeListener *instigator = NULL);
						// MarkMDNneeded only used when mail server is a POP3 server
						// or when the IMAP server does not support user defined
						// PERMANENTFLAGS
	virtual MsgERR		IsMDNNeeded(MessageKey messageKey, XP_Bool *isNeeded);

	virtual MsgERR		MarkMDNSent(MessageKey messageKey, XP_Bool bNeeded,
									ChangeListener *instigator = NULL);
	virtual MsgERR		IsMDNSent(MessageKey messageKey, XP_Bool *isSent);

// methods to get and set docsets for ids.
	virtual MsgERR		MarkRead(MessageKey messageKey, XP_Bool bRead, 
								ChangeListener *instigator = NULL);

	virtual MsgERR		MarkReplied(MessageKey messageKey, XP_Bool bReplied, 
								ChangeListener *instigator = NULL);

	virtual MsgERR		MarkForwarded(MessageKey messageKey, XP_Bool bForwarded, 
								ChangeListener *instigator = NULL);

	virtual MsgERR		MarkHasAttachments(MessageKey messageKey, XP_Bool bHasAttachments, 
								ChangeListener *instigator = NULL);

	virtual MsgERR		MarkThreadIgnored(DBThreadMessageHdr *thread, MessageKey threadKey, XP_Bool bIgnored,
									ChangeListener *instigator = NULL);
	virtual MsgERR		MarkThreadWatched(DBThreadMessageHdr *thread, MessageKey threadKey, XP_Bool bWatched,
									ChangeListener *instigator = NULL);
	virtual MsgERR		IsRead(MessageKey messageKey, XP_Bool *pRead);
	virtual MsgERR		IsIgnored(MessageKey messageKey, XP_Bool *pIgnored);
	virtual MsgERR		IsMarked(MessageKey messageKey, XP_Bool *pMarked);
	virtual MsgERR		HasAttachments(MessageKey messageKey, XP_Bool *pHasThem);
	virtual MsgERR		DeleteMessages(IDArray &messageKeys, ChangeListener *instigator);
	virtual MsgERR		DeleteMessage(MessageKey messageKey, 
										ChangeListener *instigator = NULL,
										XP_Bool commit = TRUE);
	virtual MsgERR		DeleteHeader(DBMessageHdr *msgHdr, ChangeListener *instigator, XP_Bool commit, XP_Bool onlyRemoveFromThread = FALSE);
	virtual MsgERR		UndoDelete(DBMessageHdr *msgHdr);
	virtual MsgERR		MarkLater(MessageKey messageKey, time_t until);
	virtual MsgERR		MarkMarked(MessageKey messageKey, XP_Bool mark,
										ChangeListener *instigator = NULL);
	virtual MsgERR		MarkOffline(MessageKey messageKey, XP_Bool offline,
										ChangeListener *instigator);
	virtual XP_Bool		AllMessageKeysImapDeleted(const IDArray &keys);
	virtual MsgERR		MarkImapDeleted(MessageKey messageKey, XP_Bool deleted,
										ChangeListener *instigator);

	virtual void		RemoveHeaderFromDB(DBMessageHdr *msgHdr);

	virtual MsgERR		GetUnreadKeyInThread(MessageKey threadId, 
											MessageKey *resultKey,
											MessageKey *resultThreadId);

	virtual MsgERR		MarkReadByDate (time_t startDate, time_t endDate, MWContext *context, IDArray *markedIds);
	virtual MsgERR		MarkAllRead(MWContext *context, IDArray *thoseMarked = NULL);
	virtual XP_Bool		SetPriority(MessageKey messageKey, MSG_PRIORITY priority);
	virtual XP_Bool		SetPriority(DBMessageHdr *msgHdr, MSG_PRIORITY priority);

	void				SetSortInfo(SortType, SortOrder);
	MsgERR				GetSortInfo(SortType *, SortOrder *);

	virtual void		HandleLatered();	// mark latered documents unread

	// methods to handle new message support
	MessageKey			GetFirstNew();
	void				AddToNewList(MessageKey key);
	XP_Bool				HasNew();
	void				ClearNewList(XP_Bool notify = FALSE);

	MessageKey			GetUnusedFakeId();

	DBFolderInfo 	*m_dbFolderInfo;
	MSG_DBHandle	GetDB() {return m_dbHandle;}

	// typesafe downcasts for db's
	virtual MailDB		*GetMailDB();	// returns NULL if not a mail db
	virtual NewsGroupDB	*GetNewsDB();	// returns NULL if not a news db

	virtual MSG_FolderInfo *GetFolderInfo();
	void			SetFolderInfo(MSG_FolderInfo *info) {m_folderInfo = info;}

	virtual int		GetCurVersion() {return 0;}
	DBFolderInfo *GetDBFolderInfo() {return m_dbFolderInfo;}
	int				AddUseCount() {return ++m_useCount;}

	static void		ConvertDBFlagsToPublicFlags(uint32 *flags);
	static void		ConvertPublicFlagsToDBFlags(uint32 *flags);

	static MessageDB* FindInCache(const char * pDBName);
	static void 	CopyFullHdrToShortHdr(MSG_MessageLine *msgHdr, 
										  MessageHdrStruct *fullHdr);
	static void		CleanupCache();
#ifdef DEBUG
	static int		GetNumInCache(void) {return(GetDBCache()->GetSize());}
	static void		DumpCache();
#endif
	ViewType		GetViewType();
	void			SetViewType(ViewType viewType);
	uint32			GetStatusFlags(DBMessageHdr *msgHdr);
#ifdef DEBUG_bienvenu
	void			Verify();
#endif
	virtual MsgERR	MarkHdrRead(DBMessageHdr *msgHdr, XP_Bool bRead, 
						   ChangeListener *instigator);

	virtual MsgERR	GetCachedPassword(XPStringObj &cachedPassword);
	virtual MsgERR	SetCachedPassword(const char *password);
	virtual XP_Bool HasCachedPassword();
protected:
	virtual MsgERR	SetKeyFlag(MessageKey messageKey, XP_Bool set, int32 flag,
							  ChangeListener *instigator /* = NULL */);
	virtual void	SetHdrFlag(DBMessageHdr *, XP_Bool bSet, MsgFlags flag);
	virtual void	MarkHdrReadInDB(DBMessageHdr *msgHdr, XP_Bool bRead,
									ChangeListener *instigator);

	virtual			DBFolderInfo	*CreateFolderInfo(MSG_DBFolderInfoHandle handle);
	virtual			MsgERR			IsHeaderRead(DBMessageHdr *hdr, XP_Bool *pRead);

	XP_Bool			MatchDbName(const char * dbName);	// returns TRUE if they match
	DBFolderInfo	*AddNewFolderInfoToDB();
	// helper function for creating iterators;
	MsgERR			CreateListIterator(XP_Bool forward, ListContext **pContext, DBMessageHdr **pResult);
	MsgERR			GetMessageIndexInThread(DBThreadMessageHdr *threadHdr, MessageKey startMsg, uint16 *pThreadIndex);
protected:
	MSG_DBHandle	m_dbHandle;
	MSG_DBFolderInfoHandle	m_dbFolderInfoHandle;
	MSG_IteratorHandle	m_threadIterator;			
	char			*m_dbName;
	int				m_useCount;
	MSG_FolderInfo	*m_folderInfo;
	XPPtrArray		m_newHeaders;	// array of DBMessageHdr *
	int				m_headerIndex;	// index of last header added to m_newHeaders
	int32			m_addCount;		// how many added since last commit
	int32			m_commitChunk;  // how many to add before committing

	IDArray			m_lockedKeys;
	msg_NewsArtSet *m_newSet;		// new messages since last open.
// static message db cache functions - perhaps overlaps with URL caching
// since we can open message dbs without opening a UI on the message db,
// or have two ui widgets open the same db.
	static MessageDBArray	*m_dbCache;	// array of messageDB's
	static XP_Bool			m_cacheEnabled;

	static void		AddToCache(MessageDB* pMessageDB) 
						{GetDBCache()->Add(pMessageDB);}
	static void		RemoveFromCache(MessageDB* pMessageDB);
	static int		FindInCache(MessageDB* pMessageDB);
	static XP_Bool  EnableCache(XP_Bool enable);
	static XP_Bool	IsCacheEnabled(void) {return(m_cacheEnabled);}
	static MessageDBArray*	GetDBCache();
};

#endif
