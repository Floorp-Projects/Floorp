/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* This class encapsulates the global information about a folder stored in the
	summary file.
*/
#ifndef _nsDBFolderInfo_H
#define _nsDBFolderInfo_H

#include "nsString.h"
#include "MailNewsTypes.h"
#include "xp.h"
#include "mdb.h"
#include "nsMsgKeyArray.h"
#include "nsIDBFolderInfo.h"

class nsMsgDatabase;

// again, this could inherit from nsISupports, but I don't see the need as of yet.
// I'm not sure it needs to be ref-counted (but I think it does).

// I think these getters and setters really need to go through mdb and not rely on the object
// caching the values. If this somehow turns out to be prohibitively expensive, we can invent
// some sort of dirty mechanism, but I think it turns out that these values will be cached by 
// the MSG_FolderInfo's anyway.
class nsDBFolderInfo : public nsIDBFolderInfo
{
public:
	friend class nsMsgDatabase;

	nsDBFolderInfo(nsMsgDatabase *mdb);
	virtual ~nsDBFolderInfo();

    NS_DECL_ISUPPORTS
	// interface methods.
	NS_IMETHOD			GetFlags(PRInt32 *result);
	NS_IMETHOD			SetFlags(PRInt32 flags);
	NS_IMETHOD			OrFlags(PRInt32 flags, PRInt32 *result);
	NS_IMETHOD			AndFlags(PRInt32 flags, PRInt32 *result);
	NS_IMETHOD			SetHighWater(nsMsgKey highWater, PRBool force = FALSE) ;
	NS_IMETHOD			GetHighWater(nsMsgKey *result) ;
	NS_IMETHOD			SetExpiredMark(nsMsgKey expiredKey);
	NS_IMETHOD			ChangeNumNewMessages(PRInt32 delta);
	NS_IMETHOD			ChangeNumMessages(PRInt32 delta);
	NS_IMETHOD			ChangeNumVisibleMessages(PRInt32 delta);
	NS_IMETHOD			GetNumNewMessages(PRInt32 *result) ;
	NS_IMETHOD			GetNumMessages(PRInt32 *result) ;
	NS_IMETHOD			GetNumVisibleMessages(PRInt32 *result) ;
	NS_IMETHOD			GetImapUidValidity(PRInt32 *result) ;
	NS_IMETHOD			SetImapUidValidity(PRInt32 uidValidity) ;
	NS_IMETHOD			GetImapTotalPendingMessages(PRInt32 *result) ;
	NS_IMETHOD			GetImapUnreadPendingMessages(PRInt32 *result) ;
	NS_IMETHOD			GetCSID(PRInt16 *result) ;

	NS_IMETHOD			SetVersion(PRUint32 version) ;
	NS_IMETHOD			GetVersion(PRUint32 *result);

	NS_IMETHOD			GetLastMessageLoaded(nsMsgKey *result);
	NS_IMETHOD			SetLastMessageLoaded(nsMsgKey lastLoaded);

	NS_IMETHOD			GetFolderSize(PRUint32 *size);
	NS_IMETHOD			SetFolderSize(PRUint32 size);
	NS_IMETHOD			GetFolderDate(time_t *date);
	NS_IMETHOD			SetFolderDate(time_t date);

    NS_IMETHOD			GetDiskVersion(int *version);

    NS_IMETHOD			ChangeExpungedBytes(PRInt32 delta);
  
	NS_IMETHOD			GetProperty(const char *propertyName, nsString &resultProperty);
	NS_IMETHOD			SetProperty(const char *propertyName, nsString &propertyStr);
	NS_IMETHOD			SetUint32Property(const char *propertyName, PRUint32 propertyValue);
	NS_IMETHOD			GetUint32Property(const char *propertyName, PRUint32 &propertyValue);
	// create the appropriate table and row in a new db.
	nsresult			AddToNewMDB();
	// accessor methods.

	PRBool				AddLaterKey(nsMsgKey key, time_t *until);
	PRInt32				GetNumLatered();
	nsMsgKey			GetLateredAt(PRInt32 laterIndex, time_t *pUntil);
	void				RemoveLateredAt(PRInt32 laterIndex);

	virtual void		SetMailboxName(nsString &newBoxName);
	virtual void		GetMailboxName(nsString &boxName);

	void				SetViewType(PRInt32 viewType);
	PRInt32				GetViewType();
	// we would like to just store the property name we're sorted by
	void				SetSortInfo(nsMsgSortType, nsMsgSortOrder);
	void				GetSortInfo(nsMsgSortType *, nsMsgSortOrder *);
	PRBool				TestFlag(PRInt32 flags);
	void				SetCSID(PRInt16 csid) ;
	PRInt16				GetIMAPHierarchySeparator() ;
	void				SetIMAPHierarchySeparator(PRInt16 hierarchySeparator) ;
	void				ChangeImapTotalPendingMessages(PRInt32 delta);
	void				ChangeImapUnreadPendingMessages(PRInt32 delta) ;
	

	virtual void		SetKnownArtsSet(nsString &newsArtSet);
	virtual void		GetKnownArtsSet(nsString &newsArtSet);

	// get and set arbitrary property, aka row cell value.
	nsresult	SetPropertyWithToken(mdb_token aProperty, nsString &propertyStr);
	nsresult	SetUint32PropertyWithToken(mdb_token aProperty, PRUint32 propertyValue);
	nsresult	SetInt32PropertyWithToken(mdb_token aProperty, PRInt32 propertyValue);
	nsresult	GetPropertyWithToken(mdb_token aProperty, nsString &resultProperty);
	nsresult	GetUint32PropertyWithToken(mdb_token aProperty, PRUint32 &propertyValue);
	nsresult	GetInt32PropertyWithToken(mdb_token aProperty, PRInt32 &propertyValue);

	PRUint16	m_version;			// for upgrading...
	PRInt32		m_sortType;			// the last sort type open on this db.
	PRInt16		m_csid;				// default csid for these messages
	PRInt16		m_IMAPHierarchySeparator;	// imap path separator
	PRInt8		m_sortOrder;		// the last sort order (up or down
	// mail only (for now)
	PRInt32		m_folderSize;
	PRInt32		m_expungedBytes;	// sum of size of deleted messages in folder
	time_t		m_folderDate;
	nsMsgKey  m_highWaterMessageKey;	// largest news article number or imap uid whose header we've seen
	
	// IMAP only
	PRInt32		m_ImapUidValidity;
	PRInt32		m_totalPendingMessages;
	PRInt32		m_unreadPendingMessages;

	// news only (for now)
	nsMsgKey	m_expiredMark;		// Highest invalid article number in group - for expiring
	PRInt32		m_viewType;			// for news, the last view type open on this db.	

	nsMsgKeyArray m_lateredKeys;		// list of latered messages

protected:
	
	// initialize from appropriate table and row in existing db.
	nsresult			InitFromExistingDB();
	nsresult			InitMDBInfo();
	nsresult			LoadMemberVariables();

	PRInt32		m_numVisibleMessages;	// doesn't include expunged or ignored messages (but does include collapsed).
	PRInt32		m_numNewMessages;
	PRInt32		m_numMessages;		// includes expunged and ignored messages
	PRInt32		m_flags;			// folder specific flags. This holds things like re-use thread pane,
									// configured for off-line use, use default retrieval, purge article/header options
	nsMsgKey	m_lastMessageLoaded; // set by the FE's to remember the last loaded message

// the db folder info will have to know what db and row it belongs to, since it is really
// just a wrapper around the singleton folder info row in the mdb. 
	nsMsgDatabase		*m_mdb;
	nsIMdbTable			*m_mdbTable;	// singleton table in db
	nsIMdbRow			*m_mdbRow;	// singleton row in table;

	PRBool				m_mdbTokensInitialized;

	mdb_token			m_rowScopeToken;
	mdb_token			m_tableKindToken;
	// tokens for the pre-set columns - we cache these for speed, which may be silly
	mdb_token			m_mailboxNameColumnToken;
	mdb_token			m_numVisibleMessagesColumnToken;
	mdb_token			m_numMessagesColumnToken;
	mdb_token			m_numNewMessagesColumnToken;
	mdb_token			m_flagsColumnToken;
	mdb_token			m_lastMessageLoadedColumnToken;
	mdb_token			m_folderSizeColumnToken;
	mdb_token			m_expungedBytesColumnToken;
	mdb_token			m_folderDateColumnToken;
	mdb_token			m_highWaterMessageKeyColumnToken;

	mdb_token			m_imapUidValidityColumnToken;
	mdb_token			m_totalPendingMessagesColumnToken;
	mdb_token			m_unreadPendingMessagesColumnToken;
	mdb_token			m_expiredMarkColumnToken;
	mdb_token			m_versionColumnToken;
};

#endif
