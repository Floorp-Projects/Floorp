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

#include "nsIMsgDatabase.h"
#include "nsMsgHdr.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsFileSpec.h"
#include "nsIDBChangeListener.h"
#include "nsMsgMessageFlags.h"
#include "nsISupportsArray.h"
#include "nsDBFolderInfo.h"

class ListContext;
class nsMsgKeyArray;
class nsNewsSet;
class nsMsgThread;
class nsIMsgThread;
class nsIDBFolderInfo;
class nsIMsgRFC822Parser;

class nsMsgDatabase : public nsIMsgDatabase 
{
public:
  NS_DECL_ISUPPORTS

  //////////////////////////////////////////////////////////////////////////////
  // nsIDBChangeAnnouncer methods:
  NS_IMETHOD AddListener(nsIDBChangeListener *listener);
  NS_IMETHOD RemoveListener(nsIDBChangeListener *listener);

  NS_IMETHOD NotifyKeyChangeAll(nsMsgKey keyChanged, PRInt32 flags, 
                                nsIDBChangeListener *instigator);
  NS_IMETHOD NotifyKeyAddedAll(nsMsgKey keyAdded, PRInt32 flags, 
                               nsIDBChangeListener *instigator);
  NS_IMETHOD NotifyKeyDeletedAll(nsMsgKey keyDeleted, PRInt32 flags, 
                                 nsIDBChangeListener *instigator);
  NS_IMETHOD NotifyAnnouncerGoingAway(void);

  //////////////////////////////////////////////////////////////////////////////
  // nsIMsgDatabase methods:
  NS_IMETHOD Open(nsFileSpec &folderName, PRBool create, nsIMsgDatabase** pMessageDB, PRBool upgrading);
  NS_IMETHOD Close(PRBool forceCommit);
  NS_IMETHOD OpenMDB(const char *dbName, PRBool create);
  NS_IMETHOD CloseMDB(PRBool commit);
  NS_IMETHOD Commit(nsMsgDBCommitType commitType);
  // Force closed is evil, and we should see if we can do without it.
  // In 4.x, it was mainly used to remove corrupted databases.
  NS_IMETHOD ForceClosed(void);
  // get a message header for the given key. Caller must release()!
  NS_IMETHOD GetMsgHdrForKey(nsMsgKey key, nsIMessage **msg);
  // create a new message header from a hdrStruct. Caller must release resulting header,
  // after adding any extra properties they want.
  NS_IMETHOD CreateNewHdrAndAddToDB(PRBool *newThread,
                                    nsMsgHdrStruct *hdrStruct,
                                    nsIMessage **newHdr,
                                    PRBool notify);
  // Must call AddNewHdrToDB after creating. The idea is that you create
  // a new header, fill in its properties, and then call AddNewHdrToDB.
  // AddNewHdrToDB will send notifications to any listeners.
  NS_IMETHOD CreateNewHdr(nsMsgKey key, nsIMessage **newHdr);

  virtual nsresult CreateMsgHdr(nsIMdbRow* hdrRow, nsFileSpec& path, nsMsgKey key, nsIMessage **result, PRBool createKeyFromHeader) = 0;

  NS_IMETHOD CopyHdrFromExistingHdr(nsMsgKey key, nsIMessage *existingHdr, nsIMessage **newHdr);
  NS_IMETHOD AddNewHdrToDB(nsIMessage *newHdr, PRBool notify);
  // extract info from an nsIMessage into a nsMsgHdrStruct
  NS_IMETHOD GetMsgHdrStructFromnsMsgHdr(nsIMessage *msgHdr, 
                                         nsMsgHdrStruct *hdrStruct);

#if HAVE_INT_ENUMERATORS
  NS_IMETHOD EnumerateKeys(nsIEnumerator* *outputKeys);
#else
  NS_IMETHOD ListAllKeys(nsMsgKeyArray &outputKeys);
#endif
  NS_IMETHOD EnumerateMessages(nsIEnumerator* *result);
  NS_IMETHOD EnumerateUnreadMessages(nsIEnumerator* *result);
  NS_IMETHOD EnumerateThreads(nsIEnumerator* *result);

  // this might just be for debugging - we'll see.
  nsresult ListAllThreads(nsMsgKeyArray *threadIds);

  // helpers for user command functions like delete, mark read, etc.

  NS_IMETHOD MarkHdrRead(nsIMessage *msgHdr, PRBool bRead,
                         nsIDBChangeListener *instigator);

  // MDN support
  NS_IMETHOD MarkMDNNeeded(nsMsgKey key, PRBool bNeeded,
                           nsIDBChangeListener *instigator);

  // MarkMDNneeded only used when mail server is a POP3 server
  // or when the IMAP server does not support user defined
  // PERMANENTFLAGS
  NS_IMETHOD IsMDNNeeded(nsMsgKey key, PRBool *isNeeded);

  NS_IMETHOD MarkMDNSent(nsMsgKey key, PRBool bNeeded,
                         nsIDBChangeListener *instigator);
  NS_IMETHOD IsMDNSent(nsMsgKey key, PRBool *isSent);

// methods to get and set docsets for ids.
  NS_IMETHOD MarkRead(nsMsgKey key, PRBool bRead, 
                      nsIDBChangeListener *instigator);

  NS_IMETHOD MarkReplied(nsMsgKey key, PRBool bReplied, 
                         nsIDBChangeListener *instigator);

  NS_IMETHOD MarkForwarded(nsMsgKey key, PRBool bForwarded, 
                           nsIDBChangeListener *instigator);

  NS_IMETHOD MarkHasAttachments(nsMsgKey key, PRBool bHasAttachments, 
                                nsIDBChangeListener *instigator);

  NS_IMETHOD MarkThreadIgnored(nsThreadMessageHdr *thread, nsMsgKey threadKey, PRBool bIgnored,
                               nsIDBChangeListener *instigator);
  NS_IMETHOD MarkThreadWatched(nsThreadMessageHdr *thread, nsMsgKey threadKey, PRBool bWatched,
                               nsIDBChangeListener *instigator);

  NS_IMETHOD IsRead(nsMsgKey key, PRBool *pRead);
  NS_IMETHOD IsIgnored(nsMsgKey key, PRBool *pIgnored);
  NS_IMETHOD IsMarked(nsMsgKey key, PRBool *pMarked);
  NS_IMETHOD HasAttachments(nsMsgKey key, PRBool *pHasThem);

  NS_IMETHOD MarkAllRead(nsMsgKeyArray *thoseMarked);
  NS_IMETHOD MarkReadByDate (time_t te, time_t endDate, nsMsgKeyArray *markedIds);

  NS_IMETHOD DeleteMessages(nsMsgKeyArray* nsMsgKeys, nsIDBChangeListener *instigator);
  NS_IMETHOD DeleteMessage(nsMsgKey key, 
                           nsIDBChangeListener *instigator,
                           PRBool commit);
  NS_IMETHOD DeleteHeader(nsIMessage *msgHdr, nsIDBChangeListener *instigator,
                          PRBool commit, PRBool notify);

  NS_IMETHOD UndoDelete(nsIMessage *msgHdr);

  NS_IMETHOD MarkLater(nsMsgKey key, time_t *until);
  NS_IMETHOD MarkMarked(nsMsgKey key, PRBool mark,
                        nsIDBChangeListener *instigator);
  NS_IMETHOD MarkOffline(nsMsgKey key, PRBool offline,
                         nsIDBChangeListener *instigator);

  // returns NS_OK on success, NS_COMFALSE on failure
  NS_IMETHOD  AllMsgKeysImapDeleted(const nsMsgKeyArray *keys);

  NS_IMETHOD MarkImapDeleted(nsMsgKey key, PRBool deleted,
                             nsIDBChangeListener *instigator);

  NS_IMETHOD GetFirstNew(nsMsgKey *result);
  NS_IMETHOD HasNew(void);  // returns NS_OK if true, NS_COMFALSE if false
  NS_IMETHOD ClearNewList(PRBool notify);
  NS_IMETHOD AddToNewList(nsMsgKey key);

  // used mainly to force the timestamp of a local mail folder db to
  // match the time stamp of the corresponding berkeley mail folder,
  // but also useful to tell the summary to mark itself invalid
  NS_IMETHOD SetSummaryValid(PRBool valid);

  // offline operations
  NS_IMETHOD ListAllOfflineOpIds(nsMsgKeyArray *offlineOpIds);
  NS_IMETHOD ListAllOfflineDeletes(nsMsgKeyArray *offlineDeletes);

  NS_IMETHOD GetThreadForMsgKey(nsMsgKey msgKey, nsIMsgThread **result);
  NS_IMETHOD GetThreadContainingMsgHdr(nsIMessage *msgHdr, nsIMsgThread **result) ;
  //////////////////////////////////////////////////////////////////////////////
  // nsMsgDatabase methods:
	nsMsgDatabase();
	virtual ~nsMsgDatabase();

    NS_IMETHOD IsHeaderRead(nsIMessage *hdr, PRBool *pRead);

	static nsIMdbFactory	*GetMDBFactory();
	NS_IMETHOD				GetDBFolderInfo(nsIDBFolderInfo **result);
	nsIMdbEnv				*GetEnv() {return m_mdbEnv;}
	nsIMdbStore				*GetStore() {return m_mdbStore;}
	virtual PRUint32		GetCurVersion();
	nsIMsgRFC822Parser		*GetRFC822Parser();

	static nsMsgDatabase* FindInCache(nsFileSpec &dbName);

	//helper function to fill in nsStrings from hdr row cell contents.
	nsresult				RowCellColumnTonsString(nsIMdbRow *row, mdb_token columnToken, nsString &resultStr);
	nsresult				RowCellColumnToUInt32(nsIMdbRow *row, mdb_token columnToken, PRUint32 *uint32Result);
	nsresult				RowCellColumnToUInt32(nsIMdbRow *row, mdb_token columnToken, PRUint32 &uint32Result);
	nsresult				RowCellColumnToMime2EncodedString(nsIMdbRow *row, mdb_token columnToken, nsString &resultStr);
	nsresult				RowCellColumnToCollationKey(nsIMdbRow *row, mdb_token columnToken, nsString &resultStr);

	// helper functions to put values in cells for the passed-in row
	nsresult				UInt32ToRowCellColumn(nsIMdbRow *row, mdb_token columnToken, PRUint32 value);
	// helper functions to copy an nsString to a yarn, int32 to yarn, and vice versa.
	static	struct mdbYarn *nsStringToYarn(struct mdbYarn *yarn, nsString *str);
	static	struct mdbYarn *UInt32ToYarn(struct mdbYarn *yarn, PRUint32 i);
	static	void			YarnTonsString(struct mdbYarn *yarn, nsString *str);
	static	void			YarnToUInt32(struct mdbYarn *yarn, PRUint32 *i);

	static void		CleanupCache();
#ifdef DEBUG
	static int		GetNumInCache(void) {return(GetDBCache()->Count());}
	static void		DumpCache();
    virtual nsresult DumpContents();
			nsresult DumpThread(nsMsgKey threadId);
	nsresult DumpMsgChildren(nsIMessage *msgHdr);
#endif

	friend class nsMsgHdr;	// use this to get access to cached tokens for hdr fields
	friend class nsMsgThread;	// use this to get access to cached tokens for hdr fields
    friend class nsMsgDBEnumerator;
	friend class nsMsgDBThreadEnumerator;
protected:
	// prefs stuff - in future, we might want to cache the prefs interface
	nsresult GetBoolPref(const char *prefName, PRBool *result);
	// retrieval methods
	nsMsgThread *	GetThreadForReference(nsString2 &msgID);
	nsMsgThread *	GetThreadForSubject(const char * subject);
	nsMsgThread *	GetThreadForThreadId(nsMsgKey threadId);
	nsMsgHdr	*	GetMsgHdrForReference(nsString2 &reference);
	nsMsgHdr	*	GetMsgHdrForMessageID(nsString2 &msgID);
	nsMsgThread *	GetThreadContainingMsgHdr(nsMsgHdr *msgHdr);
	// threading interfaces
	virtual nsresult CreateNewThread(nsMsgKey key, nsMsgThread **newThread);
	virtual PRBool	ThreadBySubjectWithoutRe();
	virtual nsresult ThreadNewHdr(nsMsgHdr* hdr, PRBool &newThread);
	virtual nsresult AddNewThread(nsMsgHdr *msgHdr);
	virtual nsresult AddToThread(nsMsgHdr *newHdr, nsMsgThread *thread, PRBool threadInThread);


	// open db cache
    static void		AddToCache(nsMsgDatabase* pMessageDB) 
						{GetDBCache()->AppendElement(pMessageDB);}
	static void		RemoveFromCache(nsMsgDatabase* pMessageDB);
	static int		FindInCache(nsMsgDatabase* pMessageDB);
			PRBool	MatchDbName(nsFileSpec &dbName);	// returns TRUE if they match

#if defined(XP_PC) || defined(XP_MAC)	// this should go away when we can provide our own file stream to MDB/Mork
	static void		UnixToNative(char*& ioPath);
#endif
#if defined(XP_MAC)
	static void		NativeToUnix(char*& ioPath);
#endif

	// Flag handling routines
	virtual nsresult SetKeyFlag(nsMsgKey key, PRBool set, PRInt32 flag,
							  nsIDBChangeListener *instigator = NULL);
	virtual PRBool	SetHdrFlag(nsIMessage *, PRBool bSet, MsgFlags flag);
	virtual PRUint32 GetStatusFlags(nsIMessage *msgHdr);
	// helper function which doesn't involve thread object
    NS_IMETHOD MarkHdrReadInDB(nsIMessage *msgHdr, PRBool bRead,
                               nsIDBChangeListener *instigator);

	virtual nsresult		RemoveHeaderFromDB(nsMsgHdr *msgHdr);


	static nsISupportsArray/*<nsMsgDatabase>*/* GetDBCache();
	static nsISupportsArray/*<nsMsgDatabase>*/* m_dbCache;

	// mdb bookkeeping stuff
	nsresult			InitExistingDB();
	nsresult			InitNewDB();
	nsresult			InitMDBInfo();

	nsDBFolderInfo      *m_dbFolderInfo;
	nsIMdbEnv		    *m_mdbEnv;	// to be used in all the db calls.
	nsIMdbStore	 	    *m_mdbStore;
	nsIMdbTable		    *m_mdbAllMsgHeadersTable;
	nsFileSpec		    m_dbName;
	nsNewsSet		    *m_newSet;		// new messages since last open.
	PRBool				m_mdbTokensInitialized;
    nsISupportsArray/*<nsIDBChangeListener>*/ *m_ChangeListeners;
	mdb_token			m_hdrRowScopeToken;
	mdb_token			m_hdrTableKindToken;
	mdb_token			m_threadTableKindToken;
	mdb_token			m_subjectColumnToken;
	mdb_token			m_senderColumnToken;
	mdb_token			m_messageIdColumnToken;
	mdb_token			m_referencesColumnToken;
	mdb_token			m_recipientsColumnToken;
	mdb_token			m_dateColumnToken;
	mdb_token			m_messageSizeColumnToken;
	mdb_token			m_flagsColumnToken;
	mdb_token			m_priorityColumnToken;
	mdb_token			m_statusOffsetColumnToken;
	mdb_token			m_numLinesColumnToken;
	mdb_token			m_ccListColumnToken;
	mdb_token			m_threadFlagsColumnToken;
	mdb_token			m_threadIdColumnToken;
	mdb_token			m_threadChildrenColumnToken;
	mdb_token			m_threadUnreadChildrenColumnToken;
	mdb_token			m_messageThreadIdColumnToken;
	mdb_token			m_numReferencesColumnToken;
	nsIMsgRFC822Parser	*m_rfc822Parser;
};

#endif
