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

#include "nsDBFolderInfo.h"
#include "nsMsgDatabase.h"

const char *kDBFolderInfoScope = "ns:msg:db:row:scope:dbfolderinfo:all";
const char *kDBFolderInfoTableKind = "ns:msg:db:table:kind:dbfolderinfo";
struct mdbOid gDBFolderInfoOID;

nsDBFolderInfo::nsDBFolderInfo(nsMsgDatabase *mdb)
{
	m_mdbTable = NULL;
	m_mdbRow = NULL;

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
				gDBFolderInfoOID.mOid_Scope = 1;
			}
		}
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

// ref counting methods - if we inherit from nsISupports, we won't need these,
// and we can take advantage of the nsISupports ref-counting tracing methods
nsrefcnt nsDBFolderInfo::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt nsDBFolderInfo::Release(void)
{
	NS_PRECONDITION(0 != mRefCnt, "dup release");
	if (--mRefCnt == 0) 
	{
		delete this;
		return 0;
	}
	return mRefCnt;
}

// this routine sets up a new db to know about the dbFolderInfo stuff...
nsresult nsDBFolderInfo::AddToNewMDB()
{
	nsresult ret = NS_OK;
	if (m_mdb && m_mdb->GetStore())
	{
		mdbStore *store = m_mdb->GetStore();
		// create the unique table for the dbFolderInfo.
		mdb_err err = store->NewTable(m_mdb->GetEnv(), m_rowScopeToken, 
			m_tableKindToken, PR_TRUE, &m_mdbTable);

		// create the singleton row for the dbFolderInfo.
		err  = store->NewRowWithOid(m_mdb->GetEnv(), m_rowScopeToken,
			&gDBFolderInfoOID, &m_mdbRow);

		// add the row to the singleton table.
		if (NS_SUCCEEDED(err))
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
		mdbStore *store = m_mdb->GetStore();
		if (store)
		{
			mdb_count outTableCount; // current number of such tables
			mdb_bool mustBeUnique, // whether port can hold only one of these
			ret = store->GetTableKind(m_mdb->GetEnv(), m_rowScopeToken, m_tableKindToken, &outTableCount, 
				&mustBeUnique, &m_mdbTable);
			PR_ASSERT(mustBeUnique && outTableCount == 1);
		}
	}
	return ret;
}

void nsDBFolderInfo::SetHighWater(MessageKey highWater, PRBool force /* = FALSE */)
{

}

MessageKey	nsDBFolderInfo::GetHighWater() 
{
	return m_articleNumHighWater;
}

void nsDBFolderInfo::SetExpiredMark(MessageKey expiredKey)
{
}

int	nsDBFolderInfo::GetDiskVersion() 
{
	return m_version;
}

PRBool nsDBFolderInfo::AddLaterKey(MessageKey key, time_t until)
{
	return PR_FALSE;
}

PRInt32	nsDBFolderInfo::GetNumLatered()
{
	return 0;
}

MessageKey	nsDBFolderInfo::GetLateredAt(PRInt32 laterIndex, time_t *pUntil)
{
	return MSG_MESSAGEKEYNONE;
}

void nsDBFolderInfo::RemoveLateredAt(PRInt32 laterIndex)
{
}

void nsDBFolderInfo::SetMailboxName(const char *newBoxName)
{
}

void nsDBFolderInfo::GetMailboxName(nsString &boxName)
{
}

void nsDBFolderInfo::SetViewType(PRInt32 viewType)
{
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

PRInt32	nsDBFolderInfo::ChangeNumNewMessages(PRInt32 delta)
{
	return 0;
}

PRInt32	nsDBFolderInfo::ChangeNumMessages(PRInt32 delta)
{
	return 0;
}

PRInt32	nsDBFolderInfo::ChangeNumVisibleMessages(PRInt32 delta)
{
	return 0;
}

PRInt32	nsDBFolderInfo::GetNumNewMessages() 
{
	return m_numNewMessages;
}

PRInt32	nsDBFolderInfo::GetNumMessages() 
{
	return m_numMessages;
}

PRInt32	nsDBFolderInfo::GetNumVisibleMessages() 
{
	return m_numVisibleMessages;
}

PRInt32	nsDBFolderInfo::GetFlags()
{
	return 0;
}

void nsDBFolderInfo::SetFlags(PRInt32 flags)
{
}

void nsDBFolderInfo::OrFlags(PRInt32 flags)
{
}

void nsDBFolderInfo::AndFlags(PRInt32 flags)
{
}

PRBool nsDBFolderInfo::TestFlag(PRInt32 flags)
{
	return PR_FALSE;
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
	return m_TotalPendingMessages;
}

void nsDBFolderInfo::ChangeImapTotalPendingMessages(PRInt32 delta)
{
	m_TotalPendingMessages+=delta;
}

PRInt32	nsDBFolderInfo::GetImapUnreadPendingMessages() 
{
	return m_UnreadPendingMessages;
}

void nsDBFolderInfo::ChangeImapUnreadPendingMessages(PRInt32 delta) 
{
	m_UnreadPendingMessages+=delta;
}


PRInt32	nsDBFolderInfo::GetImapUidValidity() 
{
	return m_ImapUidValidity;
}

void nsDBFolderInfo::SetImapUidValidity(PRInt32 uidValidity) 
{
	m_ImapUidValidity=uidValidity;
}

MessageKey nsDBFolderInfo::GetLastMessageLoaded() 
{
	return m_lastMessageLoaded;
}

void nsDBFolderInfo::SetLastMessageLoaded(MessageKey lastLoaded) 
{
	m_lastMessageLoaded = lastLoaded;
}

	// get arbitrary property, aka row cell value.

nsresult nsDBFolderInfo::GetProperty(const char *propertyName, nsString &resultProperty)
{
	return NS_OK;
}

