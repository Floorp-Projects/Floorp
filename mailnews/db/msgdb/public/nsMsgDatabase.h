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
#ifndef _nsMsgDatabase_H_
#define _nsMsgDatabase_H_


#include "nsMsgHdr.h"
#include "nsMsgPtrArray.h"	// for XPPtrArray
#include "nsString.h"
#include "nsFileSpec.h"
#include "nsIDBChangeListener.h"

class ListContext;
class nsDBFolderInfo;
class nsMsgKeyArray;
class ChangeListener;

class nsDBChangeAnnouncer : public XPPtrArray  // array of ChangeListeners
{
public:
	nsDBChangeAnnouncer();
	virtual ~nsDBChangeAnnouncer();
	virtual PRBool AddListener(nsIDBChangeListener *listener);
	virtual PRBool RemoveListener(nsIDBChangeListener *listener);

	// convenience routines to notify all our ChangeListeners
	void NotifyKeyChangeAll(MessageKey keyChanged, PRInt32 flags, 
		nsIDBChangeListener *instigator);
	void NotifyAnnouncerGoingAway(nsDBChangeAnnouncer *instigator);
};


// used to cache open db's.
class nsMsgDatabaseArray : public XPPtrArray
{
public:
	nsMsgDatabaseArray();
	
	// overrides with proper types to avoid casting
	nsMsgDatabase* GetAt(int nIndex) const {return((nsMsgDatabase*)XPPtrArray::GetAt(nIndex));}
	void* operator[](int nIndex) const {return((nsMsgDatabase*)XPPtrArray::operator[](nIndex));}
};

// This is to be used as an interchange object, to make creating nsMsgHeaders easier.
typedef struct _MessageHdrStruct
{
	MessageKey   m_threadId; 
	MessageKey	m_messageKey; 	
	nsString	m_subject;		// should be nsCString when it's impl
	nsString	m_author;		// should be nsCString when it's impl
	nsString	m_messageId;	// should be nsCString when it's impl
	nsString	m_references;	// should be nsCString when it's impl
	nsString	m_recipients;	// should be nsCString when it's impl
	time_t 		m_date;         // is there some sort of PR type I should use for this?
	PRUint32	m_messageSize;	// lines for news articles, bytes for local mail and imap messages
	PRUint32	m_flags;
	PRInt16		m_numChildren;		// for top-level threads
	PRInt16		m_numNewChildren;	// for top-level threads
	MSG_PRIORITY m_priority;
} MessageHdrStruct;

// I don't think this is going to be an interface, actually, since it's just
// a thin layer above MDB that defines the msg db schema.

class nsMsgDatabase : nsDBChangeAnnouncer
{
public:
	nsMsgDatabase();
	virtual ~nsMsgDatabase();

	nsrefcnt			AddRef(void);                                       
    nsrefcnt			Release(void);   
	virtual nsresult	Close(PRBool forceCommit = TRUE);
	virtual nsresult	OpenMDB(const char *dbName, PRBool create);
	virtual nsresult	CloseMDB(PRBool commit = TRUE);
	// Force closed is evil, and we should see if we can do without it.
	// In 4.x, it was mainly used to remove corrupted databases.
	virtual nsresult	ForceClosed();
	// get a message header for the given key. Caller must release()!
	virtual nsresult	GetMsgHdrForKey(MessageKey messageKey, nsMsgHdr **msgHdr);
	// create a new message header from a hdrStruct. Caller must release resulting header,
	// after adding any extra properties they want.
	virtual nsresult	CreateNewHdr(PRBool *newThread, MessageHdrStruct *hdrStruct, nsMsgHdr **newHdr, PRBool notify = FALSE);
	// extract info from an nsMsgHdr into a MessageHdrStruct
	virtual nsresult	GetMsgHdrStructFromnsMsgHdr(nsMsgHdr *msgHdr, MessageHdrStruct &hdrStruct);

	nsresult			ListAllKeys(nsMsgKeyArray &outputKeys);

	// iterator methods
	// iterate through message headers, in no particular order
	// Caller must unrefer returned nsMsgHdr when done with it.
	// nsMsgHdr will be NULL when the end of the list is reached.
	// Caller must call ListDone to free up the ListContext.
	nsresult	ListFirst(ListContext **pContext, nsMsgHdr **pResult);
	nsresult	ListNext(ListContext *pContext, nsMsgHdr **pResult);
	nsresult	ListDone(ListContext *pContext);

	// helpers for user command functions like delete, mark read, etc.

	virtual nsresult	DeleteMessages(nsMsgKeyArray &messageKeys, ChangeListener *instigator);

	static mdbFactory		*GetMDBFactory();
	nsDBFolderInfo *GetDBFolderInfo() {return m_dbFolderInfo;}
	mdbEnv		*GetEnv() {return m_mdbEnv;}
	mdbStore	*GetStore() {return m_mdbStore;}

	static nsMsgDatabase* FindInCache(nsFilePath &dbName);

	//helper function to fill in nsStrings from hdr row cell contents.
	nsresult				HdrCellColumnTonsString(nsMsgHdr *msgHdr, mdb_token columnToken, nsString &resultStr);
	nsresult				HdrCellColumnToUInt32(nsMsgHdr *msgHdr, mdb_token columnToken, PRUint32 *uint32Result);

	// helper functions to copy an nsString to a yarn, int32 to yarn, and vice versa.
	static	struct mdbYarn *nsStringToYarn(struct mdbYarn *yarn, nsString *str);
	static	struct mdbYarn *UInt32ToYarn(struct mdbYarn *yarn, PRUint32 i);
	static	void			YarnTonsString(struct mdbYarn *yarn, nsString *str);
	static	void			YarnToUInt32(struct mdbYarn *yarn, PRUint32 *i);

	static void		CleanupCache();
#ifdef DEBUG
	static int		GetNumInCache(void) {return(GetDBCache()->GetSize());}
	static void		DumpCache();
#endif
protected:
	nsDBFolderInfo 	*m_dbFolderInfo;
	mdbEnv			*m_mdbEnv;	// to be used in all the db calls.
	mdbStore		*m_mdbStore;
	mdbTable		*m_mdbAllMsgHeadersTable;
	nsFilePath		m_dbName;

	nsrefcnt		mRefCnt;

	static void		AddToCache(nsMsgDatabase* pMessageDB) 
						{GetDBCache()->Add(pMessageDB);}
	static void		RemoveFromCache(nsMsgDatabase* pMessageDB);
	static int		FindInCache(nsMsgDatabase* pMessageDB);
			PRBool	MatchDbName(nsFilePath &dbName);	// returns TRUE if they match
	static nsMsgDatabaseArray*	GetDBCache();
	static nsMsgDatabaseArray	*m_dbCache;

	// mdb bookkeeping stuff
	nsresult			InitExistingDB();
	nsresult			InitNewDB();
	nsresult			InitMDBInfo();
	PRBool				m_mdbTokensInitialized;

	mdb_token			m_hdrRowScopeToken;
	mdb_token			m_hdrTableKindToken;
	mdb_token			m_subjectColumnToken;
	mdb_token			m_senderColumnToken;
	mdb_token			m_messageIdColumnToken;
	mdb_token			m_referencesColumnToken;
	mdb_token			m_recipientsColumnToken;
	mdb_token			m_dateColumnToken;
	mdb_token			m_messageSizeColumnToken;
	mdb_token			m_flagsColumnToken;
	mdb_token			m_priorityColumnToken;

};

#endif
