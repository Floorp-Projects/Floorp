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

class ListContext;


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
struct MessageHdrStruct
{
	MessageKey   m_threadId; 
	MessageKey	m_messageKey; 	
	nsString	m_subject;
	nsString	m_author;
	nsString	m_messageId;
	nsString	m_references; 
	nsString	m_recipients;
	time_t 		m_date;         // is there some sort of PR type I should use for this?
	PRUint32	m_messageSize;	// lines for news articles, bytes for local mail and imap messages
	PRUint32	m_flags;
	PRInt16		m_numChildren;		// for top-level threads
	PRInt16		m_numNewChildren;	// for top-level threads
	MSG_PRIORITY m_priority;
public:
};

// I don't think this is going to be an interface, actually, since it's just
// a thin layer above MDB that defines the msg db schema.

class nsMsgDatabase 
{
public:
	nsMsgDatabase();
	virtual ~nsMsgDatabase();

	nsrefcnt			AddRef(void);                                       
    nsrefcnt			Release(void);   
	virtual nsresult	OpenMDB(const char *dbName, PRBool create);
	virtual nsresult	CloseMDB(PRBool commit = TRUE);
	// Force closed is evil, and we should see if we can do without it.
	// In 4.x, it was mainly used to remove corrupted databases.
	virtual nsresult	ForceClosed();
	// get a message header for the given key. Caller must release()!
	virtual nsresult	GetMsgHdrForKey(MessageKey messageKey, nsMsgHdr **msgHdr);

	virtual nsresult	CreateNewHdr(PRBool *newThread, nsMsgHdr **newHdr, PRBool notify = FALSE);

	// iterator methods
	// iterate through message headers, in no particular order
	// Caller must unrefer returned nsMsgHdr when done with it.
	// nsMsgEndOfList will be returned when return nsMsgHdr * is NULL.
	// Caller must call ListDone to free up the ListContext.
	nsresult	ListFirst(ListContext **pContext, nsMsgHdr **pResult);
	nsresult	ListNext(ListContext *pContext, nsMsgHdr **pResult);
	nsresult	ListDone(ListContext *pContext);
	static mdbFactory		*GetMDBFactory();

	static nsMsgDatabase* FindInCache(nsFilePath &dbName);
	static void		CleanupCache();
#ifdef DEBUG
	static int		GetNumInCache(void) {return(GetDBCache()->GetSize());}
	static void		DumpCache();
#endif
protected:
	mdbEnv		*m_mdbEnv;	// to be used in all the db calls.
	mdbStore	*m_mdbStore;
	nsFilePath	m_dbName;
	nsrefcnt	mRefCnt;

	static void		AddToCache(nsMsgDatabase* pMessageDB) 
						{GetDBCache()->Add(pMessageDB);}
	static void		RemoveFromCache(nsMsgDatabase* pMessageDB);
	static int		FindInCache(nsMsgDatabase* pMessageDB);
			PRBool	MatchDbName(nsFilePath &dbName);	// returns TRUE if they match
	static nsMsgDatabaseArray*	GetDBCache();
	static nsMsgDatabaseArray	*m_dbCache;
};

#endif
