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

#include "msgCore.h"
#include "nsMsgThread.h"
#include "nsMsgDatabase.h"


NS_IMPL_ISUPPORTS(nsMsgThread, nsMsgThread::GetIID())

nsMsgThread::nsMsgThread()
{
	Init();
}
nsMsgThread::nsMsgThread(nsMsgDatabase *db, nsIMdbTable *table)
{
	Init();
	m_mdbTable = table;
	m_mdbDB = db;
	if (db)
		db->AddRef();

	if (table && db)
		table->GetMetaRow(db->GetEnv(), nil, nil, &m_metaRow);
}

void nsMsgThread::Init()
{
	m_threadKey = nsMsgKey_None; 
	m_numChildren = 0;		
	m_numUnreadChildren = 0;	
	m_flags = 0;
	m_mdbTable = nsnull;
	m_mdbDB = nsnull;
	m_metaRow = nsnull;
	NS_INIT_REFCNT();
}


nsMsgThread::~nsMsgThread()
{
	if (m_mdbTable)
		m_mdbTable->Release();
	if (m_mdbDB)
		m_mdbDB->Release();
	if (m_metaRow)
		m_metaRow->Release();
}

NS_IMETHODIMP		nsMsgThread::SetThreadKey(nsMsgKey threadKey)
{
	nsresult ret = NS_OK;

	m_threadKey = threadKey;
	return ret;
}

NS_IMETHODIMP	nsMsgThread::GetThreadKey(nsMsgKey *result)
{
	if (result)
	{
		*result = m_threadKey;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgThread::GetFlags(PRUint32 *result)
{
	nsresult ret;

	if (result)
	{
		*result = m_flags;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgThread::SetFlags(PRUint32 flags)
{
	nsresult ret = NS_OK;

	m_flags = flags;
	return ret;
}


NS_IMETHODIMP nsMsgThread::GetNumChildren(PRUint32 *result)
{
	if (result)
	{
		*result = m_numChildren;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP nsMsgThread::GetNumUnreadChildren (PRUint32 *result)
{
	if (result)
	{
		*result = m_numUnreadChildren;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP nsMsgThread::AddChild(nsIMessage *child, PRBool threadInThread)
{
	nsresult ret = NS_OK;
    nsMsgHdr* hdr = NS_STATIC_CAST(nsMsgHdr*, child);          // closed system, cast ok

	nsIMdbRow *hdrRow = hdr->GetMDBRow();
	if (m_mdbTable)
	{
		m_mdbTable->AddRow(m_mdbDB->GetEnv(), hdrRow);
		ChangeChildCount(1);
	}

	return ret;
}


NS_IMETHODIMP nsMsgThread::GetChildAt(PRUint32 index, nsIMessage **result)
{
	nsresult ret = NS_OK;

	return ret;
}


NS_IMETHODIMP nsMsgThread::GetChild(nsMsgKey msgKey, nsIMessage **result)
{
	nsresult ret = NS_OK;

	return ret;
}


NS_IMETHODIMP nsMsgThread::GetChildHdrAt(PRUint32 index, nsIMessage **result)
{
	nsresult ret = NS_OK;

	return ret;
}


NS_IMETHODIMP nsMsgThread::RemoveChildAt(PRUint32 index)
{
	nsresult ret = NS_OK;

	return ret;
}


NS_IMETHODIMP nsMsgThread::RemoveChild(nsMsgKey msgKey)
{
	nsresult ret = NS_OK;

	return ret;
}


NS_IMETHODIMP nsMsgThread::MarkChildRead(PRBool bRead)
{
	nsresult ret = NS_OK;

	return ret;
}

nsresult nsMsgThread::ChangeChildCount(PRInt32 delta)
{
	nsresult ret = NS_OK;
	PRUint32 childCount = 0;
	m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadChildrenColumnToken, childCount);
	childCount += delta;

	ret = m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadChildrenColumnToken, childCount);
	return ret;
}

nsresult nsMsgThread::ChangeUnreadChildCount(PRInt32 delta)
{
	nsresult ret = NS_OK;
	PRUint32 childCount = 0;
	m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadUnreadChildrenColumnToken, childCount);
	childCount += delta;

	ret = m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadUnreadChildrenColumnToken, childCount);
	return ret;
}
