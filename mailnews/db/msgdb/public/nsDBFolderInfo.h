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

class nsMsgDatabase;

// again, this could inherit from nsISupports, but I don't see the need as of yet.
// I'm not sure it needs to be ref-counted (but I think it does).

// I think these getters and setters really need to go through mdb and not rely on the object
// caching the values. If this somehow turns out to be prohibitively expensive, we can invent
// some sort of dirty mechanism, but I think it turns out that these values will be cached by 
// the MSG_FolderInfo's anyway.
class nsDBFolderInfo
{
public:
	nsDBFolderInfo(nsMsgDatabase *mdb);
	virtual ~nsDBFolderInfo();
	nsrefcnt			AddRef(void);                                       
    nsrefcnt			Release(void);

	// create the appropriate table and row in a new db.
	nsresult			AddToNewMDB();
	// accessor methods.
	void				SetHighWater(nsMsgKey highWater, PRBool force = FALSE) ;
	nsMsgKey			GetHighWater() ;
	void				SetExpiredMark(nsMsgKey expiredKey);
	int					GetDiskVersion();

	PRBool				AddLaterKey(nsMsgKey key, time_t *until);
	PRInt32				GetNumLatered();
	nsMsgKey			GetLateredAt(PRInt32 laterIndex, time_t *pUntil);
	void				RemoveLateredAt(PRInt32 laterIndex);

	virtual void		SetMailboxName(const char *newBoxName);
	virtual void		GetMailboxName(nsString &boxName);

	void				SetViewType(PRInt32 viewType);
	PRInt32				GetViewType();
	// we would like to just store the property name we're sorted by
	void				SetSortInfo(nsMsgSortType, nsMsgSortOrder);
	void				GetSortInfo(nsMsgSortType *, nsMsgSortOrder *);
	PRInt32				ChangeNumNewMessages(PRInt32 delta);
	PRInt32				ChangeNumMessages(PRInt32 delta);
	PRInt32				ChangeNumVisibleMessages(PRInt32 delta);
	PRInt32				GetNumNewMessages() ;
	PRInt32				GetNumMessages() ;
	PRInt32				GetNumVisibleMessages() ;
	PRInt32				GetFlags();
	void				SetFlags(PRInt32 flags);
	void				OrFlags(PRInt32 flags);
	void				AndFlags(PRInt32 flags);
	PRBool				TestFlag(PRInt32 flags);
	PRInt16				GetCSID() ;
	void				SetCSID(PRInt16 csid) ;
	PRInt16				GetIMAPHierarchySeparator() ;
	void				SetIMAPHierarchySeparator(PRInt16 hierarchySeparator) ;
	PRInt32				GetImapTotalPendingMessages() ;
	void				ChangeImapTotalPendingMessages(PRInt32 delta);
	PRInt32				GetImapUnreadPendingMessages() ;
	void				ChangeImapUnreadPendingMessages(PRInt32 delta) ;
	
	PRInt32				GetImapUidValidity() ;
	void				SetImapUidValidity(PRInt32 uidValidity) ;

	void				SetVersion(PRUint16 version) {m_version = version;}

	nsMsgKey			GetLastMessageLoaded();
	void				SetLastMessageLoaded(nsMsgKey lastLoaded);

	void				SetFolderSize(PRUint32 size);
	void				SetFolderDate(time_t date);

	// get and set arbitrary property, aka row cell value.
	nsresult	GetProperty(const char *propertyName, nsString &resultProperty);
	nsresult	SetProperty(const char *propertyName, nsString &propertyStr);
	nsresult	SetUint32Property(const char *propertyName, PRUint32 propertyValue);

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
	PRInt32		m_TotalPendingMessages;
	PRInt32		m_UnreadPendingMessages;

	// news only (for now)
	nsMsgKey	m_expiredMark;		// Highest invalid article number in group - for expiring
	PRInt32		m_viewType;			// for news, the last view type open on this db.	

	nsMsgKeyArray m_lateredKeys;		// list of latered messages

protected:
	nsrefcnt mRefCnt;                                                         
	
	// initialize from appropriate table and row in existing db.
	nsresult			InitFromExistingDB();
	nsresult			InitMDBInfo();

	nsString	m_mailboxName;		// name presented to the user, will match imap server name
	PRInt32		m_numVisibleMessages;	// doesn't include expunged or ignored messages (but does include collapsed).
	PRInt32		m_numNewMessages;
	PRInt32		m_numMessages;		// includes expunged and ignored messages
	PRInt32		m_flags;			// folder specific flags. This holds things like re-use thread pane,
									// configured for off-line use, use default retrieval, purge article/header options
	nsMsgKey	m_lastMessageLoaded; // set by the FE's to remember the last loaded message

// the db folder info will have to know what db and row it belongs to, since it is really
// just a wrapper around the singleton folder info row in the mdb. 
	nsMsgDatabase		*m_mdb;
	mdbTable			*m_mdbTable;	// singleton table in db
	mdbRow				*m_mdbRow;	// singleton row in table;

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

};

#endif
