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

#include "nsMsgHdr.h"
#include "nsMsgDatabase.h"

nsMsgHdr::nsMsgHdr()
{
	mRefCnt = 1;
	m_mdb = NULL;
	m_mdbRow = NULL;
	m_messageKey = MSG_MESSAGEKEYNONE;
}

nsMsgHdr::nsMsgHdr(nsMsgDatabase *db, mdbRow *dbRow)
{
	mRefCnt = 1;
	m_mdb = db;
	m_mdbRow = dbRow;
	m_messageKey = MSG_MESSAGEKEYNONE;
}

nsMsgHdr::~nsMsgHdr()
{
	if (m_mdbRow)
	{
		if (m_mdb)
		{	// presumably, acquiring a row increments strong ref count
			m_mdbRow->CutStrongRef(m_mdb->GetEnv());
			m_mdb->Release();
		}
	}
}

// ref counting methods - if we inherit from nsISupports, we won't need these,
// and we can take advantage of the nsISupports ref-counting tracing methods
nsrefcnt nsMsgHdr::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt nsMsgHdr::Release(void)
{
	NS_PRECONDITION(0 != mRefCnt, "dup release");
	if (--mRefCnt == 0) 
	{
		delete this;
		return 0;
	}
	return mRefCnt;
}

MessageKey  nsMsgHdr::GetMessageKey()
{
	if (m_messageKey == MSG_MESSAGEKEYNONE && m_mdbRow != NULL)
	{
		mdbOid outOid;
		if (m_mdbRow->GetOid(m_mdb->GetEnv(), &outOid) == NS_OK)
			m_messageKey = outOid.mOid_Id;

	}
	return m_messageKey;
}

nsresult	nsMsgHdr::GetProperty(const char *propertyName, nsString &resultProperty)
{
	nsresult err = NS_OK;
	return err;
}

uint16		nsMsgHdr::GetNumReferences()
{
	return 0;
}

nsresult	nsMsgHdr::GetStringReference(PRInt32 refNum, nsString &resultReference)
{
	nsresult err = NS_OK;
	return err;
}

time_t		nsMsgHdr::GetDate() 
{
	return m_date;
}
