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
#include "pldhash.h"
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
  NS_DECL_NSIDBCHANGEANNOUNCER
  NS_DECL_NSIMSGDATABASE
  virtual nsresult IsHeaderRead(nsIMsgDBHdr *hdr, PRBool *pRead);
  virtual nsresult MarkHdrReadInDB(nsIMsgDBHdr *msgHdr, PRBool bRead,
                               nsIDBChangeListener *instigator);
  virtual nsresult OpenMDB(const char *dbName, PRBool create);
  virtual nsresult CloseMDB(PRBool commit);
  virtual nsresult CreateMsgHdr(nsIMdbRow* hdrRow, nsMsgKey key, nsIMsgDBHdr **result);
  virtual nsresult GetThreadForMsgKey(nsMsgKey msgKey, nsIMsgThread **result);
  virtual nsresult EnumerateUnreadMessages(nsISimpleEnumerator* *result);
  // this might just be for debugging - we'll see.
  nsresult ListAllThreads(nsMsgKeyArray *threadIds);
  //////////////////////////////////////////////////////////////////////////////
  // nsMsgDatabase methods:
	nsMsgDatabase();
	virtual ~nsMsgDatabase();


	static nsIMdbFactory	*GetMDBFactory();
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

	virtual nsresult		RemoveHeaderFromDB(nsMsgHdr *msgHdr);
  virtual nsresult    RemoveHeaderFromThread(nsMsgHdr *msgHdr);


	static nsVoidArray/*<nsMsgDatabase>*/* GetDBCache();
	static nsVoidArray/*<nsMsgDatabase>*/* m_dbCache;

	nsCOMPtr <nsICollation> m_collationKeyGenerator;
	nsCOMPtr <nsIMimeConverter> m_mimeConverter;
  nsCOMPtr <nsIMsgRetentionSettings> m_retentionSettings;

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
	mdb_token			m_offlineMsgOffsetColumnToken;
	mdb_token			m_offlineMessageSizeColumnToken;
	nsIMsgHeaderParser	*m_HeaderParser;

	// header caching stuff - MRU headers, keeps them around in memory
	nsresult			GetHdrFromCache(nsMsgKey key, nsIMsgDBHdr* *result);
	nsresult			AddHdrToCache(nsIMsgDBHdr *hdr, nsMsgKey key);
	nsresult			ClearHdrCache(PRBool reInit);
	nsresult			RemoveHdrFromCache(nsIMsgDBHdr *hdr, nsMsgKey key);
	// all headers currently instantiated, doesn't hold refs
	// these get added when msg hdrs get constructed, and removed when they get destroyed.
	nsresult			GetHdrFromUseCache(nsMsgKey key, nsIMsgDBHdr* *result);
	nsresult			AddHdrToUseCache(nsIMsgDBHdr *hdr, nsMsgKey key); 
	nsresult			ClearUseHdrCache();
	nsresult			RemoveHdrFromUseCache(nsIMsgDBHdr *hdr, nsMsgKey key);

	// all instantiated headers, but doesn't hold refs. 
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
	PRBool				m_bCacheHeaders;

};

class nsMsgRetentionSettings : public nsIMsgRetentionSettings
{
public:
  nsMsgRetentionSettings();
  virtual ~nsMsgRetentionSettings();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGRETENTIONSETTINGS
protected:
  nsMsgRetainByPreference m_retainByPreference;
  PRUint32                m_daysToKeepHdrs;
  PRUint32                m_numHeadersToKeep;
  PRUint32                m_keepUnreadMessagesProp;
  PRBool                  m_keepUnreadMessagesOnly;
  PRUint32                m_daysToKeepBodies;
};

#endif
