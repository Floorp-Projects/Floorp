/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsMsgDatabase_H_
#define _nsMsgDatabase_H_

#define USE_PLD_HASHTABLE

#include "nsIMsgDatabase.h"
#include "nsMsgHdr.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsFileSpec.h"
#include "nsIDBChangeListener.h"
#include "nsIDBChangeAnnouncer.h"
#include "nsMsgMessageFlags.h"
#include "nsISupportsArray.h"
#include "nsDBFolderInfo.h"
#include "nsICollation.h"
#include "nsIMimeConverter.h"
#include "nsCOMPtr.h"
#include "nsHashtable.h"
#ifdef USE_PLD_HASHTABLE
#include "pldhash.h"
#endif
class ListContext;
class nsMsgKeyArray;
class nsMsgKeySet;
class nsMsgThread;
class nsIMsgThread;
class nsIDBFolderInfo;
class nsIMsgHeaderParser;

class nsMsgDatabase : public nsIMsgDatabase 
{
public:
  NS_DECL_ISUPPORTS

  //////////////////////////////////////////////////////////////////////////////
  // nsIDBChangeAnnouncer methods:
  NS_IMETHOD AddListener(nsIDBChangeListener *listener);
  NS_IMETHOD RemoveListener(nsIDBChangeListener *listener);

  NS_IMETHOD NotifyKeyChangeAll(nsMsgKey keyChanged, PRUint32 aOldFlags, PRUint32 aNewFlags, 
                                nsIDBChangeListener *instigator);
  NS_IMETHOD NotifyKeyAddedAll(nsMsgKey keyAdded, nsMsgKey parentKey, PRInt32 flags, 
                               nsIDBChangeListener *instigator);
  NS_IMETHOD NotifyKeyDeletedAll(nsMsgKey keyDeleted, nsMsgKey parentKey, PRInt32 flags, 
                                 nsIDBChangeListener *instigator);
  NS_IMETHOD NotifyParentChangedAll(nsMsgKey keyReparented, nsMsgKey oldParent, nsMsgKey newParent,
								nsIDBChangeListener *instigator);

  NS_IMETHOD NotifyReadChanged(nsIDBChangeListener *instigator);
  NS_IMETHOD NotifyAnnouncerGoingAway(void);

  //////////////////////////////////////////////////////////////////////////////
  // nsIMsgDatabase methods:
  NS_IMETHOD Open(nsIFileSpec *folderName, PRBool create, PRBool upgrading, nsIMsgDatabase** pMessageDB);
  NS_IMETHOD Close(PRBool forceCommit);

  // argh, these two shouldn't be Interface methods, but I can't diddle the interfaces
  // until the idl works on windows. grumble grumble.
  NS_IMETHOD OpenMDB(const char *dbName, PRBool create);
  NS_IMETHOD CloseMDB(PRBool commit);

  NS_IMETHOD Commit(nsMsgDBCommit commitType);
  // Force closed is evil, and we should see if we can do without it.
  // In 4.x, it was mainly used to remove corrupted databases.
  NS_IMETHOD ForceClosed(void);
  // get a message header for the given key. Caller must release()!
  NS_IMETHOD GetMsgHdrForKey(nsMsgKey key, nsIMsgDBHdr **msg);

 //Returns whether or not this database contains the given key
  NS_IMETHOD ContainsKey(nsMsgKey key, PRBool *containsKey);

  // Must call AddNewHdrToDB after creating. The idea is that you create
  // a new header, fill in its properties, and then call AddNewHdrToDB.
  // AddNewHdrToDB will send notifications to any listeners.
  NS_IMETHOD CreateNewHdr(nsMsgKey key, nsIMsgDBHdr **newHdr);
  virtual nsresult  CreateMsgHdr(nsIMdbRow* hdrRow, nsMsgKey key, nsIMsgDBHdr **result);

  NS_IMETHOD CopyHdrFromExistingHdr(nsMsgKey key, nsIMsgDBHdr *existingHdr, nsIMsgDBHdr **newHdr);
  NS_IMETHOD AddNewHdrToDB(nsIMsgDBHdr *newHdr, PRBool notify);

  NS_IMETHOD ListAllKeys(nsMsgKeyArray &outputKeys);
  NS_IMETHOD EnumerateMessages(nsISimpleEnumerator* *result);
  NS_IMETHOD EnumerateUnreadMessages(nsISimpleEnumerator* *result);
  NS_IMETHOD EnumerateThreads(nsISimpleEnumerator* *result);

  // this might just be for debugging - we'll see.
  nsresult ListAllThreads(nsMsgKeyArray *threadIds);

  // helpers for user command functions like delete, mark read, etc.

  NS_IMETHOD MarkHdrRead(nsIMsgDBHdr *msgHdr, PRBool bRead,
                         nsIDBChangeListener *instigator);

  NS_IMETHOD MarkHdrReplied(nsIMsgDBHdr *msgHdr, PRBool bReplied,
                         nsIDBChangeListener *instigator);

  NS_IMETHOD MarkHdrMarked(nsIMsgDBHdr *msgHdr, PRBool mark,
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

  NS_IMETHOD MarkThreadRead(nsIMsgThread *thread, nsIDBChangeListener *instigator, nsMsgKeyArray *thoseMarked);
  NS_IMETHOD MarkThreadIgnored(nsIMsgThread *thread, nsMsgKey threadKey, PRBool bIgnored,
                               nsIDBChangeListener *instigator);
  NS_IMETHOD MarkThreadWatched(nsIMsgThread *thread, nsMsgKey threadKey, PRBool bWatched,
                               nsIDBChangeListener *instigator);

  NS_IMETHOD IsRead(nsMsgKey key, PRBool *pRead);
  NS_IMETHOD IsIgnored(nsMsgKey key, PRBool *pIgnored);
  NS_IMETHOD IsMarked(nsMsgKey key, PRBool *pMarked);
  NS_IMETHOD HasAttachments(nsMsgKey key, PRBool *pHasThem);

  NS_IMETHOD MarkAllRead(nsMsgKeyArray *thoseMarked);
  NS_IMETHOD MarkReadByDate (PRTime te, PRTime endDate, nsMsgKeyArray *markedIds);

  NS_IMETHOD DeleteMessages(nsMsgKeyArray* nsMsgKeys, nsIDBChangeListener *instigator);
  NS_IMETHOD DeleteMessage(nsMsgKey key, 
                           nsIDBChangeListener *instigator,
                           PRBool commit);
  NS_IMETHOD DeleteHeader(nsIMsgDBHdr *msgHdr, nsIDBChangeListener *instigator,
                          PRBool commit, PRBool notify);

  NS_IMETHOD UndoDelete(nsIMsgDBHdr *msgHdr);

  NS_IMETHOD MarkLater(nsMsgKey key, PRTime until);
  NS_IMETHOD MarkMarked(nsMsgKey key, PRBool mark,
                        nsIDBChangeListener *instigator);
  NS_IMETHOD MarkOffline(nsMsgKey key, PRBool offline,
                         nsIDBChangeListener *instigator);

  NS_IMETHOD  AllMsgKeysImapDeleted(nsMsgKeyArray *keys, PRBool *allKeysDeleted);

  NS_IMETHOD MarkImapDeleted(nsMsgKey key, PRBool deleted,
                             nsIDBChangeListener *instigator);

  NS_IMETHOD GetFirstNew(nsMsgKey *result);
  NS_IMETHOD HasNew(PRBool *_retval);  
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
  NS_IMETHOD GetThreadContainingMsgHdr(nsIMsgDBHdr *msgHdr, nsIMsgThread **result) ;

  NS_IMETHOD                GetHighWaterArticleNum(nsMsgKey *key);
  NS_IMETHOD                GetLowWaterArticleNum(nsMsgKey *key);
  //////////////////////////////////////////////////////////////////////////////
  // nsMsgDatabase methods:
	nsMsgDatabase();
	virtual ~nsMsgDatabase();

    NS_IMETHOD IsHeaderRead(nsIMsgDBHdr *hdr, PRBool *pRead);

	static nsIMdbFactory	*GetMDBFactory();
	NS_IMETHOD				GetDBFolderInfo(nsIDBFolderInfo **result);
	nsIMdbEnv				*GetEnv() {return m_mdbEnv;}
	nsIMdbStore				*GetStore() {return m_mdbStore;}
	virtual PRUint32		GetCurVersion();
	nsIMsgHeaderParser		*GetHeaderParser();
	nsresult				GetCollationKeyGenerator();

	static nsMsgDatabase* FindInCache(nsFileSpec &dbName);

	//helper function to fill in nsStrings from hdr row cell contents.
	nsresult				RowCellColumnTonsString(nsIMdbRow *row, mdb_token columnToken, nsString &resultStr);
	nsresult				RowCellColumnTonsCString(nsIMdbRow *row, mdb_token columnToken, nsCString &resultStr);
	nsresult				RowCellColumnToUInt32(nsIMdbRow *row, mdb_token columnToken, PRUint32 *uint32Result, PRUint32 defaultValue = 0);
	nsresult				RowCellColumnToUInt32(nsIMdbRow *row, mdb_token columnToken, PRUint32 &uint32Result, PRUint32 defaultValue = 0);
	nsresult				RowCellColumnToMime2DecodedString(nsIMdbRow *row, mdb_token columnToken, PRUnichar **);
	nsresult				RowCellColumnToCollationKey(nsIMdbRow *row, mdb_token columnToken, PRUnichar**);

	// helper functions to put values in cells for the passed-in row
	nsresult				UInt32ToRowCellColumn(nsIMdbRow *row, mdb_token columnToken, PRUint32 value);
	nsresult				CharPtrToRowCellColumn(nsIMdbRow *row, mdb_token columnToken, const char *charPtr);
	nsresult				RowCellColumnToCharPtr(nsIMdbRow *row, mdb_token columnToken, char **result);

	nsresult				CreateCollationKey(const PRUnichar* sourceString, PRUnichar **resultString);

	// helper functions to copy an nsString to a yarn, int32 to yarn, and vice versa.
	static	struct mdbYarn *nsStringToYarn(struct mdbYarn *yarn, nsString *str);
	static	struct mdbYarn *UInt32ToYarn(struct mdbYarn *yarn, PRUint32 i);
	static	void			YarnTonsString(struct mdbYarn *yarn, nsString *str);
	static	void			YarnTonsCString(struct mdbYarn *yarn, nsCString *str);
	static	void			YarnToUInt32(struct mdbYarn *yarn, PRUint32 *i);

	
	// helper functions to convert a 64bits PRTime into a 32bits value (compatible time_t) and vice versa.
	static void				PRTime2Seconds(PRTime prTime, PRUint32 *seconds);
	static void				Seconds2PRTime(PRUint32 seconds, PRTime *prTime);

	static void		CleanupCache();
#ifdef DEBUG
	static int		GetNumInCache(void) {return(GetDBCache()->Count());}
	static void		DumpCache();
    virtual nsresult DumpContents();
			nsresult DumpThread(nsMsgKey threadId);
	nsresult DumpMsgChildren(nsIMsgDBHdr *msgHdr);
#endif

	friend class nsMsgHdr;	// use this to get access to cached tokens for hdr fields
	friend class nsMsgThread;	// use this to get access to cached tokens for hdr fields
    friend class nsMsgDBEnumerator;
	friend class nsMsgDBThreadEnumerator;
protected:
	// prefs stuff - in future, we might want to cache the prefs interface
	nsresult GetBoolPref(const char *prefName, PRBool *result);
	// retrieval methods
	nsIMsgThread *	GetThreadForReference(nsCString &msgID, nsIMsgDBHdr **pMsgHdr);
	nsIMsgThread *	GetThreadForSubject(nsCString &subject);
	nsIMsgThread *	GetThreadForThreadId(nsMsgKey threadId);
	nsMsgHdr	*	GetMsgHdrForReference(nsCString &reference);
	nsIMsgDBHdr	*	GetMsgHdrForMessageID(nsCString &msgID);
	nsIMsgDBHdr	*	GetMsgHdrForSubject(nsCString &msgID);
	// threading interfaces
	virtual nsresult CreateNewThread(nsMsgKey key, const char *subject, nsMsgThread **newThread);
	virtual PRBool	ThreadBySubjectWithoutRe();
	virtual nsresult ThreadNewHdr(nsMsgHdr* hdr, PRBool &newThread);
	virtual nsresult AddNewThread(nsMsgHdr *msgHdr);
	virtual nsresult AddToThread(nsMsgHdr *newHdr, nsIMsgThread *thread, nsIMsgDBHdr *pMsgHdr, PRBool threadInThread);


	// open db cache
    static void		AddToCache(nsMsgDatabase* pMessageDB) 
						{GetDBCache()->AppendElement(pMessageDB);}
	static void		RemoveFromCache(nsMsgDatabase* pMessageDB);
	static int		FindInCache(nsMsgDatabase* pMessageDB);
			PRBool	MatchDbName(nsFileSpec &dbName);	// returns TRUE if they match

#if defined(XP_PC) || defined(XP_MAC)	// this should go away when we can provide our own file stream to MDB/Mork
	static void		UnixToNative(char*& ioPath);
#endif

	// Flag handling routines
	virtual nsresult SetKeyFlag(nsMsgKey key, PRBool set, PRUint32 flag,
							  nsIDBChangeListener *instigator = NULL);
	virtual nsresult SetMsgHdrFlag(nsIMsgDBHdr *msgHdr, PRBool set, PRUint32 flag, 
								nsIDBChangeListener *instigator);

	virtual PRBool	SetHdrFlag(nsIMsgDBHdr *, PRBool bSet, MsgFlags flag);
    virtual PRBool  SetHdrReadFlag(nsIMsgDBHdr *, PRBool pRead);
	virtual PRUint32 GetStatusFlags(nsIMsgDBHdr *msgHdr, PRUint32 origFlags);
	// helper function which doesn't involve thread object
    NS_IMETHOD MarkHdrReadInDB(nsIMsgDBHdr *msgHdr, PRBool bRead,
                               nsIDBChangeListener *instigator);

	virtual nsresult		RemoveHeaderFromDB(nsMsgHdr *msgHdr);
  virtual nsresult    RemoveHeaderFromThread(nsMsgHdr *msgHdr);


	static nsVoidArray/*<nsMsgDatabase>*/* GetDBCache();
	static nsVoidArray/*<nsMsgDatabase>*/* m_dbCache;

	nsCOMPtr <nsICollation> m_collationKeyGenerator;
	nsCOMPtr <nsIMimeConverter> m_mimeConverter;
	// mdb bookkeeping stuff
	nsresult			InitExistingDB();
	nsresult			InitNewDB();
	nsresult			InitMDBInfo();

	nsDBFolderInfo      *m_dbFolderInfo;
	nsIMdbEnv		    *m_mdbEnv;	// to be used in all the db calls.
	nsIMdbStore	 	    *m_mdbStore;
	nsIMdbTable		    *m_mdbAllMsgHeadersTable;
	nsFileSpec		    m_dbName;
	nsMsgKeySet		    *m_newSet;	// new messages since last open.
	PRBool				m_mdbTokensInitialized;
    nsVoidArray/*<nsIDBChangeListener>*/ *m_ChangeListeners;
	mdb_token			m_hdrRowScopeToken;
	mdb_token			m_threadRowScopeToken;
	mdb_token			m_hdrTableKindToken;
	mdb_token			m_threadTableKindToken;
	mdb_token			m_allThreadsTableKindToken;
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
	mdb_token			m_threadSubjectColumnToken;
	mdb_token			m_numReferencesColumnToken;
	mdb_token			m_messageCharSetColumnToken;
	mdb_token			m_threadParentColumnToken;
	mdb_token			m_threadRootKeyColumnToken;
	nsIMsgHeaderParser	*m_HeaderParser;

	// header caching stuff - MRU headers, keeps them around in memory
	nsresult			GetHdrFromCache(nsMsgKey key, nsIMsgDBHdr* *result);
	nsresult			AddHdrToCache(nsIMsgDBHdr *hdr, nsMsgKey key);
	nsresult			ClearHdrCache();
	nsresult			RemoveHdrFromCache(nsIMsgDBHdr *hdr, nsMsgKey key);
	// all headers currently instantiated, doesn't hold refs
	// these get added when msg hdrs get constructed, and removed when they get destroyed.
	nsresult			GetHdrFromUseCache(nsMsgKey key, nsIMsgDBHdr* *result);
	nsresult			AddHdrToUseCache(nsIMsgDBHdr *hdr, nsMsgKey key); 
	nsresult			ClearUseHdrCache();
	nsresult			RemoveHdrFromUseCache(nsIMsgDBHdr *hdr, nsMsgKey key);

	// all instantiated headers, but doesn't hold refs. 
#ifdef USE_PLD_HASHTABLE
  PLDHashTable  *m_headersInUse;
  static const void* CRT_CALL GetKey(PLDHashTable* aTable, PLDHashEntryHdr* aEntry);
  static PLDHashNumber CRT_CALL HashKey(PLDHashTable* aTable, const void* aKey);
  static PRBool CRT_CALL MatchEntry(PLDHashTable* aTable, const PLDHashEntryHdr* aEntry, const void* aKey);
  static void CRT_CALL MoveEntry(PLDHashTable* aTable, const PLDHashEntryHdr* aFrom, PLDHashEntryHdr* aTo);
  static void CRT_CALL ClearEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry);
  static PLDHashOperator CRT_CALL HeaderEnumerator (PLDHashTable *table, PLDHashEntryHdr *hdr,
                               PRUint32 number, void *arg);
  static PLDHashTableOps gMsgDBHashTableOps;
  struct MsgHdrHashElement {
    PLDHashEntryHdr mHeader;
    nsMsgKey       mKey;
    nsIMsgDBHdr     *mHdr;
  };
  PLDHashTable  *m_cachedHeaders;
#else
	nsSupportsHashtable	*m_cachedHeaders; // keeps MRU headers around
	nsHashtable			*m_headersInUse;  
#endif // USE_PLD_HASHTABLE
	PRBool				m_bCacheHeaders;

};

#endif
