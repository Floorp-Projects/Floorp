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
#include "nsMsgHdr.h"
#include "nsMsgDatabase.h"

NS_IMPL_ADDREF(nsMsgHdr)
NS_IMPL_RELEASE(nsMsgHdr)

NS_IMETHODIMP
nsMsgHdr::QueryInterface(REFNSIID iid, void** result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
    if(iid.Equals(nsIMessage::IID()) ||
       iid.Equals(::nsISupports::IID())) {
		*result = NS_STATIC_CAST(nsIMessage*, this);
		AddRef();
		return NS_OK;
	}
	return nsRDFResource::QueryInterface(iid, result);
}

nsMsgHdr::nsMsgHdr(nsMsgDatabase *db, mdbRow *dbRow)
    : nsRDFResource(nsnull)
{
    NS_INIT_REFCNT();
	m_mdb = db;
	m_mdbRow = dbRow;
	Init();
}

void nsMsgHdr::Init()
{
	m_statusOffset = -1;
	m_messageKey = nsMsgKey_None;
	m_csID = 0;
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

NS_IMETHODIMP nsMsgHdr::GetMessageKey(nsMsgKey *result)
{
	if (m_messageKey == nsMsgKey_None && m_mdbRow != NULL)
	{
		mdbOid outOid;
		if (m_mdbRow->GetOid(m_mdb->GetEnv(), &outOid) == NS_OK)
			m_messageKey = outOid.mOid_Id;

	}
	*result = m_messageKey;
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::GetThreadId(nsMsgKey *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgHdr::SetMessageKey(nsMsgKey value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgHdr::GetFlags(PRUint32 *result)
{
    *result = m_flags;
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::SetFlags(PRUint32 flags)
{
	m_flags = flags;
	SetUInt32Column(m_mdb->m_flagsColumnToken, m_flags);
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::OrFlags(PRUint32 flags, PRUint32 *result)
{
	if ((m_flags & flags) != flags)
		SetFlags (m_flags | flags);
	*result = m_flags;
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::AndFlags(PRUint32 flags, PRUint32 *result)
{
	if ((m_flags & flags) != m_flags)
		SetFlags (m_flags & flags);
	*result = m_flags;
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::GetProperty(const char *propertyName, nsString &resultProperty)
{
	nsresult err = NS_OK;
	mdb_token	property_token;

	err = m_mdb->GetStore()->StringToToken(m_mdb->GetEnv(),  propertyName, &property_token);
	if (err == NS_OK)
		err = m_mdb->RowCellColumnTonsString(GetMDBRow(), property_token, resultProperty);

	return err;
}

NS_IMETHODIMP nsMsgHdr::SetProperty(const char *propertyName, nsString &propertyStr)
{
	nsresult err = NS_OK;
	mdb_token	property_token;

	err = m_mdb->GetStore()->StringToToken(m_mdb->GetEnv(),  propertyName, &property_token);
	if (err == NS_OK)
	{
		struct mdbYarn yarn;

		yarn.mYarn_Grow = NULL;
		err = m_mdbRow->AddColumn(m_mdb->GetEnv(), property_token, m_mdb->nsStringToYarn(&yarn, &propertyStr));
		delete[] yarn.mYarn_Buf;	// won't need this when we have nsCString
	}
	return err;
}

NS_IMETHODIMP nsMsgHdr::GetUint32Property(const char *propertyName, PRUint32 *pResult)
{
	nsresult err = NS_OK;
	mdb_token	property_token;

	err = m_mdb->GetStore()->StringToToken(m_mdb->GetEnv(),  propertyName, &property_token);
	if (err == NS_OK)
		err = m_mdb->RowCellColumnToUInt32(GetMDBRow(), property_token, pResult);

	return err;
}

NS_IMETHODIMP nsMsgHdr::SetUint32Property(const char *propertyName, PRUint32 value)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMsgHdr::GetNumReferences(PRUint16 *result)
{
    *result = 0;
	return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::GetStringReference(PRInt32 refNum, nsString &resultReference)
{
	nsresult err = NS_OK;
	return err;
}

NS_IMETHODIMP nsMsgHdr::GetDate(time_t *result) 
{
	*result = m_date;
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::SetMessageId(const char *messageId)
{
	return SetStringColumn(messageId, m_mdb->m_messageIdColumnToken);
}

NS_IMETHODIMP nsMsgHdr::SetSubject(const char *subject)
{
	return SetStringColumn(subject, m_mdb->m_subjectColumnToken);
}

NS_IMETHODIMP nsMsgHdr::SetAuthor(const char *author)
{
	return SetStringColumn(author, m_mdb->m_senderColumnToken);
}

NS_IMETHODIMP nsMsgHdr::SetReferences(const char *references)
{
	return SetStringColumn(references, m_mdb->m_referencesColumnToken);
}

NS_IMETHODIMP nsMsgHdr::SetRecipients(const char *recipients, PRBool rfc822 /* = PR_TRUE */)
{
	// need to put in rfc822 address parsing code here (or make caller do it...)
	return SetStringColumn(recipients, m_mdb->m_recipientsColumnToken);
}

NS_IMETHODIMP nsMsgHdr::SetCCList(const char *ccList)
{
	return SetStringColumn(ccList, m_mdb->m_ccListColumnToken);
}


NS_IMETHODIMP nsMsgHdr::SetMessageSize(PRUint32 messageSize)
{
	SetUInt32Column(m_mdb->m_messageSizeColumnToken, messageSize);
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::SetLineCount(PRUint32 lineCount)
{
	SetUInt32Column(m_mdb->m_numLinesColumnToken, lineCount);
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::SetStatusOffset(PRUint32 statusOffset)
{
	return SetUInt32Column(m_mdb->m_statusOffsetColumnToken, statusOffset);
}

NS_IMETHODIMP nsMsgHdr::SetDate(time_t date)
{
    return SetUInt32Column(m_mdb->m_dateColumnToken, (PRUint32) date);
}

NS_IMETHODIMP nsMsgHdr::GetStatusOffset(PRUint32 *result)
{
	PRUint32 offset;
	nsresult res = GetUInt32Column(m_mdb->m_statusOffsetColumnToken, &offset);

	*result = offset;
    return res;
}

NS_IMETHODIMP nsMsgHdr::SetPriority(nsMsgPriority priority)
{
	m_priority = priority; 
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::GetMessageOffset(PRUint32 *result)
{
    *result = m_messageKey;
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::SetPriority(const char *priority)
{
// ### TODO
//	m_priority = MSG_GetPriorityFromString(priority);
	return SetPriority(nsMsgPriority_Normal);
}

nsresult nsMsgHdr::SetStringColumn(const char *str, mdb_token token)
{
	struct mdbYarn yarn;
	yarn.mYarn_Buf = (void *) (str ? str : "");
	yarn.mYarn_Size = PL_strlen((const char *) yarn.mYarn_Buf) + 1;
	yarn.mYarn_Fill = yarn.mYarn_Size;
	yarn.mYarn_Form = 0;
	return m_mdbRow->AddColumn(m_mdb->GetEnv(), token, &yarn);
}

nsresult nsMsgHdr::SetUInt32Column(PRUint32 value, mdb_token token)
{
	struct mdbYarn yarn;
	return m_mdbRow->AddColumn(m_mdb->GetEnv(),  token, nsMsgDatabase::UInt32ToYarn(&yarn, value));
}

nsresult nsMsgHdr::GetUInt32Column(mdb_token token, PRUint32 *pvalue)
{
	return m_mdb->RowCellColumnToUInt32(GetMDBRow(), token, pvalue);
}
