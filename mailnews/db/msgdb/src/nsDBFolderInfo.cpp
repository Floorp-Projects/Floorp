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

#include "msgCore.h"
#include "nsDBFolderInfo.h"
#include "nsMsgDatabase.h"

static const char *kDBFolderInfoScope = "ns:msg:db:row:scope:dbfolderinfo:all";
static const char *kDBFolderInfoTableKind = "ns:msg:db:table:kind:dbfolderinfo";

struct mdbOid gDBFolderInfoOID;

static const char *	kNumVisibleMessagesColumnName = "numVisMsgs";
static const char *	kNumMessagesColumnName ="numMsgs";
static const char *	kNumNewMessagesColumnName = "numNewMsgs";
static const char *	kFlagsColumnName = "flags";
static const char *	kLastMessageLoadedColumnName = "lastMsgLoaded";
static const char *	kFolderSizeColumnName = "folderSize";
static const char *	kExpungedBytesColumnName = "expungedBytes";
static const char *	kFolderDateColumnName = "folderDate";
static const char *	kHighWaterMessageKeyColumnName = "highWaterKey";

static const char *	kImapUidValidityColumnName = "UIDValidity";
static const char *	kTotalPendingMessagesColumnName = "totPendingMsgs";
static const char *	kUnreadPendingMessagesColumnName = "unreadPendingMsgs";
static const char * kMailboxNameColumnName = "mailboxName";
static const char * kKnownArtsSetColumnName = "knownArts";
static const char * kExpiredMarkColumnName = "expiredMark";
static const char * kVersionColumnName = "version";

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMPL_ADDREF(nsDBFolderInfo)
NS_IMPL_RELEASE(nsDBFolderInfo)

NS_IMETHODIMP
nsDBFolderInfo::QueryInterface(REFNSIID iid, void** result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
    if(iid.Equals(nsIDBFolderInfo::GetIID()) ||
       iid.Equals(kISupportsIID)) {
		*result = NS_STATIC_CAST(nsIDBFolderInfo*, this);
		AddRef();
		return NS_OK;
	}
    return NS_NOINTERFACE;
}


nsDBFolderInfo::nsDBFolderInfo(nsMsgDatabase *mdb)
  : m_expiredMarkColumnToken(0),
    m_expiredMark(0),
    m_viewType(0),
    m_numVisibleMessagesColumnToken(0),
    m_flags(0),
    m_lastMessageLoaded(0)
{
    NS_INIT_REFCNT();
	m_mdbTable = NULL;
	m_mdbRow = NULL;
	m_version = 1;			// for upgrading...
	m_sortType = 0;			// the last sort type open on this db.
	m_csid = 0;				// default csid for these messages
	m_IMAPHierarchySeparator = 0;	// imap path separator
	m_sortOrder = 0;		// the last sort order (up or down
	// mail only (for now)
	m_folderSize = 0;
	m_folderDate = 0;
	m_expungedBytes = 0;	// sum of size of deleted messages in folder
	m_highWaterMessageKey = 0;

	m_numNewMessages = 0;
	m_numMessages = 0;
	m_numVisibleMessages = 0;
	// IMAP only
	m_ImapUidValidity = 0;
	m_totalPendingMessages =0;
	m_unreadPendingMessages = 0;

	m_mdbTokensInitialized = FALSE;

	if (mdb)
	{
		mdb_err err;

		mdb->AddRef();
		m_mdb = mdb;
		err = m_mdb->GetStore()->StringToToken(mdb->GetEnv(), kDBFolderInfoScope, &m_rowScopeToken); 
		if (err == NS_OK)
		{
			err = m_mdb->GetStore()->StringToToken(mdb->GetEnv(), kDBFolderInfoTableKind, &m_tableKindToken); 
			if (err == NS_OK)
			{
				gDBFolderInfoOID.mOid_Scope = m_rowScopeToken;
				gDBFolderInfoOID.mOid_Id = 1;
			}
		}
		InitMDBInfo();
	}
}

nsDBFolderInfo::~nsDBFolderInfo()
{
	if (m_mdb)
	{
		if (m_mdbTable)
			m_mdbTable->CutStrongRef(m_mdb->GetEnv());
		if (m_mdbRow)
			m_mdbRow->CutStrongRef(m_mdb->GetEnv());
		m_mdb->Release();
	}
}


// this routine sets up a new db to know about the dbFolderInfo stuff...
nsresult nsDBFolderInfo::AddToNewMDB()
{
	nsresult ret = NS_OK;
	if (m_mdb && m_mdb->GetStore())
	{
		nsIMdbStore *store = m_mdb->GetStore();
		// create the unique table for the dbFolderInfo.
		mdb_err err = store->NewTable(m_mdb->GetEnv(), m_rowScopeToken, 
			m_tableKindToken, PR_TRUE, &m_mdbTable);

		// make sure the oid of the table is 1.
		struct mdbOid folderInfoTableOID;
		folderInfoTableOID.mOid_Id = 1;
		folderInfoTableOID.mOid_Scope = m_rowScopeToken;

//		m_mdbTable->BecomeContent(m_mdb->GetEnv(), &folderInfoTableOID);

		// create the singleton row for the dbFolderInfo.
		err  = store->NewRowWithOid(m_mdb->GetEnv(),
			&gDBFolderInfoOID, &m_mdbRow);

		// add the row to the singleton table.
		if (m_mdbRow && NS_SUCCEEDED(err))
		{
			err = m_mdbTable->AddRow(m_mdb->GetEnv(), m_mdbRow);
		}

		ret = err;	// what are we going to do about mdb_err's?
	}
	return ret;
}

nsresult nsDBFolderInfo::InitFromExistingDB()
{
	nsresult ret = NS_OK;
	if (m_mdb && m_mdb->GetStore())
	{
		nsIMdbStore *store = m_mdb->GetStore();
		if (store)
		{
			mdb_pos		rowPos;
			mdb_count outTableCount; // current number of such tables
			mdb_bool mustBeUnique; // whether port can hold only one of these
			ret = store->GetTableKind(m_mdb->GetEnv(), m_rowScopeToken, m_tableKindToken, &outTableCount, 
				&mustBeUnique, &m_mdbTable);
//			NS_ASSERTION(mustBeUnique && outTableCount == 1, "only one global db info allowed");

			if (m_mdbTable)
			{
				// find singleton row for global info.
				ret = m_mdbTable->HasOid(m_mdb->GetEnv(), &gDBFolderInfoOID, &rowPos);
				if (ret == NS_OK)
				{
					nsIMdbTableRowCursor *rowCursor;
					rowPos = -1;
					ret= m_mdbTable->GetTableRowCursor(m_mdb->GetEnv(), rowPos, &rowCursor);
					if (ret == NS_OK)
					{
						ret = rowCursor->NextRow(m_mdb->GetEnv(), &m_mdbRow, &rowPos);
						rowCursor->Release();
						if (ret == NS_OK && m_mdbRow)
						{
							LoadMemberVariables();
						}
					}
				}
			}
		}
	}
	return ret;
}

nsresult nsDBFolderInfo::InitMDBInfo()
{
	nsresult ret = NS_OK;
	if (!m_mdbTokensInitialized && m_mdb && m_mdb->GetStore())
	{
		nsIMdbStore *store = m_mdb->GetStore();
		nsIMdbEnv	*env = m_mdb->GetEnv();

		store->StringToToken(env,  kNumVisibleMessagesColumnName, &m_numVisibleMessagesColumnToken);
		store->StringToToken(env,  kNumMessagesColumnName, &m_numMessagesColumnToken);
		store->StringToToken(env,  kNumNewMessagesColumnName, &m_numNewMessagesColumnToken);
		store->StringToToken(env,  kFlagsColumnName, &m_flagsColumnToken);
		store->StringToToken(env,  kLastMessageLoadedColumnName, &m_lastMessageLoadedColumnToken);
		store->StringToToken(env,  kFolderSizeColumnName, &m_folderSizeColumnToken);
		store->StringToToken(env,  kExpungedBytesColumnName, &m_expungedBytesColumnToken);
		store->StringToToken(env,  kFolderDateColumnName, &m_folderDateColumnToken);

		store->StringToToken(env,  kHighWaterMessageKeyColumnName, &m_highWaterMessageKeyColumnToken);
		store->StringToToken(env,  kMailboxNameColumnName, &m_mailboxNameColumnToken);

		store->StringToToken(env,  kImapUidValidityColumnName, &m_imapUidValidityColumnToken);
		store->StringToToken(env,  kTotalPendingMessagesColumnName, &m_totalPendingMessagesColumnToken);
		store->StringToToken(env,  kUnreadPendingMessagesColumnName, &m_unreadPendingMessagesColumnToken);
		store->StringToToken(env,  kExpiredMarkColumnName, &m_expiredMarkColumnToken);
		store->StringToToken(env,  kVersionColumnName, &m_versionColumnToken);
		store->StringToToken(env,  kNumVisibleMessagesColumnName, &m_numVisibleMessagesColumnToken);
		m_mdbTokensInitialized  = PR_TRUE;
	}
	return ret;
}

nsresult nsDBFolderInfo::LoadMemberVariables()
{
	nsresult ret = NS_OK;
	// it's really not an error for these properties to not exist...
	GetInt32PropertyWithToken(m_numVisibleMessagesColumnToken, m_numVisibleMessages);
	GetInt32PropertyWithToken(m_numMessagesColumnToken, m_numMessages);
	GetInt32PropertyWithToken(m_numNewMessagesColumnToken, m_numNewMessages);
	GetInt32PropertyWithToken(m_flagsColumnToken, m_flags);
	GetInt32PropertyWithToken(m_folderSizeColumnToken, m_folderSize);
	GetInt32PropertyWithToken(m_folderDateColumnToken, (PRInt32 &) m_folderDate);
	GetInt32PropertyWithToken(m_folderDateColumnToken, (PRInt32 &) m_folderDate);
	return ret;
}

NS_IMETHODIMP nsDBFolderInfo::SetVersion(PRUint32 version)
{
	m_version = version; 
	return SetUint32PropertyWithToken(m_versionColumnToken, (PRUint32) m_version);
}

NS_IMETHODIMP nsDBFolderInfo::GetVersion(PRUint32 *version)
{
	*version = m_version; 
	return NS_OK;
}


NS_IMETHODIMP nsDBFolderInfo::SetHighWater(nsMsgKey highWater, PRBool force /* = FALSE */)
{
	if (force || m_highWaterMessageKey < highWater)
		m_highWaterMessageKey = highWater;

	return NS_OK;
}

NS_IMETHODIMP	nsDBFolderInfo::SetFolderSize(PRUint32 size)
{
	m_folderSize = size;
	return SetUint32PropertyWithToken(m_folderSizeColumnToken, m_folderSize);
}

NS_IMETHODIMP	nsDBFolderInfo::SetFolderDate(time_t folderDate)
{
	m_folderDate = folderDate;
	return SetUint32PropertyWithToken(m_folderDateColumnToken, folderDate);
}


NS_IMETHODIMP	nsDBFolderInfo::GetHighWater(nsMsgKey *result) 
{
	*result = m_highWaterMessageKey;
	return NS_OK;
}

NS_IMETHODIMP nsDBFolderInfo::SetExpiredMark(nsMsgKey expiredKey)
{
	m_expiredMark = expiredKey;
	return SetUint32PropertyWithToken(m_expiredMarkColumnToken, expiredKey);
}

int	nsDBFolderInfo::GetDiskVersion() 
{
	return m_version;
}

PRBool nsDBFolderInfo::AddLaterKey(nsMsgKey key, time_t *until)
{
	return PR_FALSE;
}

PRInt32	nsDBFolderInfo::GetNumLatered()
{
	return 0;
}

nsMsgKey	nsDBFolderInfo::GetLateredAt(PRInt32 laterIndex, time_t *pUntil)
{
	return nsMsgKey_None;
}

void nsDBFolderInfo::RemoveLateredAt(PRInt32 laterIndex)
{
}

void nsDBFolderInfo::SetMailboxName(nsString &newBoxName)
{
	SetPropertyWithToken(m_mailboxNameColumnToken, newBoxName);
}

void nsDBFolderInfo::GetMailboxName(nsString &boxName)
{
	GetPropertyWithToken(m_mailboxNameColumnToken, boxName);
}

void nsDBFolderInfo::SetViewType(PRInt32 viewType)
{
  m_viewType = viewType;
}

PRInt32	nsDBFolderInfo::GetViewType() 
{
	return m_viewType;
}

void nsDBFolderInfo::SetSortInfo(nsMsgSortType type, nsMsgSortOrder order)
{
}

void nsDBFolderInfo::GetSortInfo(nsMsgSortType *type, nsMsgSortOrder *orde)
{
}

NS_IMETHODIMP nsDBFolderInfo::ChangeNumNewMessages(PRInt32 delta)
{
	m_numNewMessages += delta;
	if (m_numNewMessages < 0)
	{
#ifdef DEBUG_bienvenu1
		XP_ASSERT(FALSE);
#endif
		m_numNewMessages = 0;
	}
	return SetUint32PropertyWithToken(m_numNewMessagesColumnToken, m_numNewMessages);
}

NS_IMETHODIMP nsDBFolderInfo::ChangeNumMessages(PRInt32 delta)
{
	m_numMessages += delta;
	if (m_numMessages < 0)
	{
#ifdef DEBUG_bienvenu
		NS_ASSERTION(FALSE, "num messages can't be < 0");
#endif
		m_numMessages = 0;
	}
	return SetUint32PropertyWithToken(m_numMessagesColumnToken, m_numMessages);
}

NS_IMETHODIMP nsDBFolderInfo::ChangeNumVisibleMessages(PRInt32 delta)
{
	m_numVisibleMessages += delta;
	if (m_numVisibleMessages < 0)
	{
#ifdef DEBUG_bienvenu
		NS_ASSERTION(FALSE, "num visible messages can't be < 0");
#endif
		m_numVisibleMessages = 0;
	}
	return SetUint32PropertyWithToken(m_numVisibleMessagesColumnToken, m_numVisibleMessages);
}

NS_IMETHODIMP nsDBFolderInfo::GetNumNewMessages(PRInt32 *result) 
{
	*result = m_numNewMessages;
	return NS_OK;
}

NS_IMETHODIMP	nsDBFolderInfo::GetNumMessages(PRInt32 *result) 
{
	*result = m_numMessages;
	return NS_OK;;
}

NS_IMETHODIMP	nsDBFolderInfo::GetNumVisibleMessages(PRInt32 *result) 
{
	*result = m_numVisibleMessages;
	return NS_OK;
}

NS_IMETHODIMP	nsDBFolderInfo::GetFlags(PRInt32 *result)
{
	*result = m_flags;
	return NS_OK;
}

NS_IMETHODIMP nsDBFolderInfo::SetFlags(PRInt32 flags)
{
	nsresult ret = NS_OK;

	if (m_flags != flags)
	{
		m_flags = flags; 
		ret = SetInt32PropertyWithToken(m_flagsColumnToken, m_flags);
	}
	return ret;
}

NS_IMETHODIMP nsDBFolderInfo::OrFlags(PRInt32 flags, PRInt32 *result)
{
	m_flags |= flags;
	*result = m_flags;
	return SetInt32PropertyWithToken(m_flagsColumnToken, m_flags);
}

NS_IMETHODIMP nsDBFolderInfo::AndFlags(PRInt32 flags, PRInt32 *result)
{
	m_flags &= flags;
	*result = m_flags;
	return SetInt32PropertyWithToken(m_flagsColumnToken, m_flags);
}

NS_IMETHODIMP	nsDBFolderInfo::GetImapUidValidity(PRInt32 *result) 
{
	*result = m_ImapUidValidity;
	return NS_OK;
}

NS_IMETHODIMP nsDBFolderInfo::SetImapUidValidity(PRInt32 uidValidity) 
{
	m_ImapUidValidity = uidValidity;
	return SetUint32PropertyWithToken(m_imapUidValidityColumnToken, m_ImapUidValidity);
}



NS_IMETHODIMP nsDBFolderInfo::GetLastMessageLoaded(nsMsgKey *lastLoaded) 
{
	*lastLoaded = m_lastMessageLoaded;
	return NS_OK;
}

NS_IMETHODIMP nsDBFolderInfo::SetLastMessageLoaded(nsMsgKey lastLoaded) 
{
	m_lastMessageLoaded = lastLoaded;
	return SetUint32PropertyWithToken(m_lastMessageLoadedColumnToken, m_lastMessageLoaded);
}

PRBool nsDBFolderInfo::TestFlag(PRInt32 flags)
{
	return (m_flags & flags) != 0;
}

PRInt16	nsDBFolderInfo::GetCSID() 
{
	return m_csid;
}

void nsDBFolderInfo::SetCSID(PRInt16 csid) 
{
	m_csid = csid;
}

PRInt16	nsDBFolderInfo::GetIMAPHierarchySeparator() 
{
	return m_IMAPHierarchySeparator;
}

void nsDBFolderInfo::SetIMAPHierarchySeparator(PRInt16 hierarchySeparator) 
{
	m_IMAPHierarchySeparator = hierarchySeparator; 
}

PRInt32	nsDBFolderInfo::GetImapTotalPendingMessages() 
{
	return m_totalPendingMessages;
}

void nsDBFolderInfo::ChangeImapTotalPendingMessages(PRInt32 delta)
{
	m_totalPendingMessages+=delta;
	SetInt32PropertyWithToken(m_totalPendingMessagesColumnToken, m_totalPendingMessages);
}

PRInt32	nsDBFolderInfo::GetImapUnreadPendingMessages() 
{
	return m_unreadPendingMessages;
}

void nsDBFolderInfo::ChangeImapUnreadPendingMessages(PRInt32 delta) 
{
	m_unreadPendingMessages+=delta;
	SetInt32PropertyWithToken(m_unreadPendingMessagesColumnToken, m_unreadPendingMessages);
}


void nsDBFolderInfo::SetKnownArtsSet(nsString &newsArtSet)
{
	SetProperty(kKnownArtsSetColumnName, newsArtSet);
}

void nsDBFolderInfo::GetKnownArtsSet(nsString &newsArtSet)
{
	GetProperty(kKnownArtsSetColumnName, newsArtSet);
}

	// get arbitrary property, aka row cell value.

NS_IMETHODIMP	nsDBFolderInfo::GetProperty(const char *propertyName, nsString &resultProperty)
{
	nsresult err = NS_OK;
	mdb_token	property_token;

	err = m_mdb->GetStore()->StringToToken(m_mdb->GetEnv(),  propertyName, &property_token);
	if (err == NS_OK)
		err = m_mdb->RowCellColumnTonsString(m_mdbRow, property_token, resultProperty);

	return err;
}

NS_IMETHODIMP	nsDBFolderInfo::SetUint32Property(const char *propertyName, PRUint32 propertyValue)
{
	struct mdbYarn yarn;
	char	int32StrBuf[20];
	yarn.mYarn_Buf = int32StrBuf;
	yarn.mYarn_Size = sizeof(int32StrBuf);
	yarn.mYarn_Fill = sizeof(int32StrBuf);

	mdb_token	property_token;

	nsresult err = m_mdb->GetStore()->StringToToken(m_mdb->GetEnv(),  propertyName, &property_token);
	if (err == NS_OK)
	{
		nsMsgDatabase::UInt32ToYarn(&yarn, propertyValue);
		err = m_mdbRow->AddColumn(m_mdb->GetEnv(), property_token, &yarn);
	}
	return err;
}

NS_IMETHODIMP	nsDBFolderInfo::SetProperty(const char *propertyName, nsString &propertyStr)
{
	nsresult err = NS_OK;
	mdb_token	property_token;

	err = m_mdb->GetStore()->StringToToken(m_mdb->GetEnv(),  propertyName, &property_token);
	if (err == NS_OK)
		return SetPropertyWithToken(property_token, propertyStr);

	return err;
}

nsresult nsDBFolderInfo::SetPropertyWithToken(mdb_token aProperty, nsString &propertyStr)
{
	struct mdbYarn yarn;

	yarn.mYarn_Grow = NULL;
	nsresult err = m_mdbRow->AddColumn(m_mdb->GetEnv(), aProperty, m_mdb->nsStringToYarn(&yarn, &propertyStr));
	delete[] yarn.mYarn_Buf;	// won't need this when we have nsCString
	return err;
}

nsresult	nsDBFolderInfo::SetUint32PropertyWithToken(mdb_token aProperty, PRUint32 propertyValue)
{
	struct mdbYarn yarn;
	char	int32StrBuf[20];
	yarn.mYarn_Buf = int32StrBuf;
	yarn.mYarn_Size = sizeof(int32StrBuf);
	yarn.mYarn_Fill = sizeof(int32StrBuf);

	nsMsgDatabase::UInt32ToYarn(&yarn, propertyValue);
	nsresult err = m_mdbRow->AddColumn(m_mdb->GetEnv(), aProperty, &yarn);
	return err;
}

nsresult	nsDBFolderInfo::SetInt32PropertyWithToken(mdb_token aProperty, PRInt32 propertyValue)
{
	nsString propertyStr;
	propertyStr.Append(propertyValue, 10);
	return SetPropertyWithToken(aProperty, propertyStr);
}

nsresult nsDBFolderInfo::GetPropertyWithToken(mdb_token aProperty, nsString &resultProperty)
{
	return m_mdb->RowCellColumnTonsString(m_mdbRow, aProperty, resultProperty);
}

nsresult nsDBFolderInfo::GetUint32PropertyWithToken(mdb_token aProperty, PRUint32 &propertyValue)
{
	return m_mdb->RowCellColumnToUInt32(m_mdbRow, aProperty, propertyValue);
}

nsresult nsDBFolderInfo::GetInt32PropertyWithToken(mdb_token aProperty, PRInt32 &propertyValue)
{
	return m_mdb->RowCellColumnToUInt32(m_mdbRow, aProperty, (PRUint32 &) propertyValue);
}

NS_IMETHODIMP nsDBFolderInfo::GetUint32Property(const char *propertyName, PRUint32 &propertyValue)
{
	nsresult err = NS_OK;
	mdb_token	property_token;

	err = m_mdb->GetStore()->StringToToken(m_mdb->GetEnv(),  propertyName, &property_token);
	if (err == NS_OK)
		return GetUint32PropertyWithToken(property_token, propertyValue);
	return err;
}

