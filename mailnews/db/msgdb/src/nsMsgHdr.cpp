/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h"
#include "nsMsgHdr.h"
#include "nsMsgDatabase.h"
#include "nsMsgUtils.h"
#include "nsIMsgHeaderParser.h"
#include "nsMsgMimeCID.h"
#include "nsIMimeConverter.h"
#include "nsXPIDLString.h"

NS_IMPL_ISUPPORTS(nsMsgHdr, NS_GET_IID(nsIMsgDBHdr))

static NS_DEFINE_CID(kCMimeConverterCID, NS_MIME_CONVERTER_CID);

#define FLAGS_INITED 0x1
#define CACHED_VALUES_INITED 0x2
#define REFERENCES_INITED 0x4
#define THREAD_PARENT_INITED 0x8

nsMsgHdr::nsMsgHdr(nsMsgDatabase *db, nsIMdbRow *dbRow)
{
    NS_INIT_REFCNT();
    MOZ_COUNT_CTOR(nsMsgHdr);
	m_mdb = db;
	Init();
	m_mdbRow = dbRow;
	if(m_mdb)
	{
		m_mdb->AddRef();
		mdbOid outOid;
		if (dbRow && dbRow->GetOid(m_mdb->GetEnv(), &outOid) == NS_OK)
		{
			m_messageKey = outOid.mOid_Id;
			m_mdb->AddHdrToUseCache((nsIMsgDBHdr *) this, m_messageKey);
		}
	}
}


void nsMsgHdr::Init()
{
	m_initedValues = 0;
	m_statusOffset = 0xffffffff;
	m_messageKey = nsMsgKey_None;
	m_messageSize = 0;
	m_date = LL_ZERO;
	m_csID = 0;
	m_flags = 0;
	m_mdbRow = NULL;
	m_numReferences = 0;
	m_threadId = nsMsgKey_None;
	m_threadParent = nsMsgKey_None;
}

nsresult nsMsgHdr::InitCachedValues()
{
	nsresult err = NS_OK;

	if (!m_mdb || !m_mdbRow)
	    return NS_ERROR_NULL_POINTER;

	if (!(m_initedValues & CACHED_VALUES_INITED))
	{
		PRUint32 uint32Value;
		mdbOid outOid;
		if (m_mdbRow->GetOid(m_mdb->GetEnv(), &outOid) == NS_OK)
			m_messageKey = outOid.mOid_Id;

		err = GetUInt32Column(m_mdb->m_messageSizeColumnToken, &m_messageSize);

	    err = GetUInt32Column(m_mdb->m_dateColumnToken, &uint32Value);
	    nsMsgDatabase::Seconds2PRTime(uint32Value, &m_date);

		err = GetUInt32Column(m_mdb->m_messageThreadIdColumnToken, &m_threadId);
		err = GetUInt32Column(m_mdb->m_numReferencesColumnToken, &uint32Value);
		if (NS_SUCCEEDED(err))
			m_numReferences = (PRUint16) uint32Value;

		if (NS_SUCCEEDED(err))
			m_initedValues |= CACHED_VALUES_INITED;
	}
	return err;
}

nsresult nsMsgHdr::InitFlags()
{

	nsresult err = NS_OK;

	if (!m_mdb)
	    return NS_ERROR_NULL_POINTER;

	if(!(m_initedValues & FLAGS_INITED))
	{
		err = GetUInt32Column(m_mdb->m_flagsColumnToken, &m_flags);
    m_flags &= ~MSG_FLAG_NEW; // don't get new flag from MDB

		if(NS_SUCCEEDED(err))
			m_initedValues |= FLAGS_INITED;
	}

	return err;

}

nsMsgHdr::~nsMsgHdr()
{
    MOZ_COUNT_DTOR(nsMsgHdr);
	if (m_mdbRow)
	{
		if (m_mdb)
		{	// presumably, acquiring a row increments strong ref count
			m_mdbRow->CutStrongRef(m_mdb->GetEnv());
			m_mdb->RemoveHdrFromUseCache((nsIMsgDBHdr *) this, m_messageKey);
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

	if (!(m_initedValues & CACHED_VALUES_INITED))
		InitCachedValues();

	if (result)
	{
		*result = m_threadId;
		return NS_OK;
	}
    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgHdr::SetThreadId(nsMsgKey inKey)
{
	m_threadId = inKey;
	SetUInt32Column(m_threadId, m_mdb->m_messageThreadIdColumnToken);
	return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::SetMessageKey(nsMsgKey value)
{
	m_messageKey = value;
	return NS_OK;
}

nsresult nsMsgHdr::GetRawFlags(PRUint32 *result)
{
	if (!(m_initedValues & FLAGS_INITED))
		InitFlags();
	*result = m_flags;
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::GetFlags(PRUint32 *result)
{
	if (!(m_initedValues & FLAGS_INITED))
		InitFlags();
	if (m_mdb)
		*result = m_mdb->GetStatusFlags(this, m_flags);
	else
		*result = m_flags;
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::SetFlags(PRUint32 flags)
{
	m_flags = flags;
  // don't write out MSG_FLAG_NEW to MDB.
	SetUInt32Column(m_flags & ~MSG_FLAG_NEW, m_mdb->m_flagsColumnToken);
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

NS_IMETHODIMP nsMsgHdr::MarkRead(PRBool bRead)
{
	nsresult rv = NS_OK;

	if(m_mdb)
	{
		nsMsgKey key;
		rv = GetMessageKey(&key);
		if(NS_SUCCEEDED(rv))
			rv = m_mdb->MarkRead(key, bRead, nsnull);
	}


	return rv;
}

NS_IMETHODIMP nsMsgHdr::MarkFlagged(PRBool bFlagged)
{
	nsresult rv = NS_OK;

	if(m_mdb)
	{
		nsMsgKey key;
		rv = GetMessageKey(&key);
		if(NS_SUCCEEDED(rv))
			rv = m_mdb->MarkMarked(key, bFlagged, nsnull);
	}


	return rv;
}

NS_IMETHODIMP nsMsgHdr::GetProperty(const char *propertyName, nsString &resultProperty)
{
	nsresult err = NS_OK;
	mdb_token	property_token;

	if (m_mdb->GetStore())
		err = m_mdb->GetStore()->StringToToken(m_mdb->GetEnv(),  propertyName, &property_token);
	else
		err = NS_ERROR_NULL_POINTER;
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
		delete[] (char *)yarn.mYarn_Buf;	// won't need this when we have nsCString
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
	if (!(m_initedValues & CACHED_VALUES_INITED))
		InitCachedValues();

    *result = m_numReferences;
	return NS_OK;
}

nsresult nsMsgHdr::ParseReferences(nsCString &references)
{
	const char *startNextRef = references.GetBuffer();
	nsCAutoString resultReference;

	while (startNextRef && *startNextRef)
	{
		startNextRef = GetNextReference(startNextRef, resultReference);
		m_references.AppendCString(resultReference);
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::GetStringReference(PRInt32 refNum, nsCString &resultReference)
{
	nsresult err = NS_OK;

	if(!(m_initedValues & REFERENCES_INITED))
	{
		nsCAutoString references;
		err = m_mdb->RowCellColumnTonsCString(GetMDBRow(), m_mdb->m_referencesColumnToken, references);
		
		if(NS_SUCCEEDED(err))
		{
			ParseReferences(references);
			m_initedValues |= REFERENCES_INITED;
		}
	}

	m_references.CStringAt(refNum, resultReference);
	return err;
}

NS_IMETHODIMP nsMsgHdr::GetDate(PRTime *result) 
{
	if (!(m_initedValues & CACHED_VALUES_INITED))
		InitCachedValues();

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
	nsCAutoString CStrReference(references);
	ParseReferences(CStrReference);

	m_numReferences = m_references.Count();
	SetUInt32Column(m_numReferences, m_mdb->m_numReferencesColumnToken);

	m_initedValues |= REFERENCES_INITED;

	return SetStringColumn(references, m_mdb->m_referencesColumnToken);
}

NS_IMETHODIMP nsMsgHdr::SetRecipients(const char *recipients)
{
	// need to put in rfc822 address parsing code here (or make caller do it...)
	return SetStringColumn(recipients, m_mdb->m_recipientsColumnToken);
}

NS_IMETHODIMP nsMsgHdr::SetRecipientsIsNewsgroup(PRBool rfc822)
{
    m_recipientsIsNewsgroup = rfc822; // ???
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::GetRecipientsIsNewsgroup(PRBool *rfc822)
{
    NS_ENSURE_ARG_POINTER(rfc822);
    (*rfc822) = m_recipientsIsNewsgroup;
    return NS_OK;
}

nsresult nsMsgHdr::BuildRecipientsFromArray(const char *names, const char *addresses, PRUint32 numAddresses, nsCAutoString& allRecipients)
{
	nsresult ret;
	const char *curName = names;
	const char *curAddress = addresses;
	nsIMsgHeaderParser *headerParser = m_mdb->GetHeaderParser();

	for (PRUint32 i = 0; i < numAddresses; i++, curName += strlen(curName) + 1, curAddress += strlen(curAddress) + 1)
	{
		if (i > 0)
			allRecipients += ", ";

		if (headerParser)
		{
		   char * fullAddress;
		   ret = headerParser->MakeFullAddress(nsnull, curName, curAddress, &fullAddress);
		   if (NS_SUCCEEDED(ret) && fullAddress)
		   {
		      allRecipients += fullAddress;
		      nsCRT::free(fullAddress);
		      continue;
		   }
		}

        // Just in case the parser failed...
		if (strlen(curName))
		{
			allRecipients += curName;
			allRecipients += ' ';
		}

		if (strlen(curAddress))
		{
			allRecipients += '<';
			allRecipients += curAddress;
			allRecipients += '>';
		}
	}
	
	return ret;
}

NS_IMETHODIMP nsMsgHdr::SetRecipientsArray(const char *names, const char *addresses, PRUint32 numAddresses)
{
	nsresult ret;
	nsCAutoString	allRecipients;
	
    ret = BuildRecipientsFromArray(names, addresses, numAddresses, allRecipients);
    if (NS_FAILED(ret))
        return ret;

	ret = SetRecipients(allRecipients);
    SetRecipientsIsNewsgroup(PR_TRUE);
	return ret;
}

NS_IMETHODIMP nsMsgHdr::SetCcList(const char *ccList)
{
	return SetStringColumn(ccList, m_mdb->m_ccListColumnToken);
}

// ###should make helper routine that takes column token!
NS_IMETHODIMP nsMsgHdr::SetCCListArray(const char *names, const char *addresses, PRUint32 numAddresses)
{
	nsresult ret;
	nsCAutoString	allRecipients;
	
    ret = BuildRecipientsFromArray(names, addresses, numAddresses, allRecipients);
    if (NS_FAILED(ret))
        return ret;

	ret = SetCcList(allRecipients);
	return ret;
}


NS_IMETHODIMP nsMsgHdr::SetMessageSize(PRUint32 messageSize)
{
	SetUInt32Column(messageSize, m_mdb->m_messageSizeColumnToken);
	m_messageSize = messageSize;
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::SetLineCount(PRUint32 lineCount)
{
	SetUInt32Column(lineCount, m_mdb->m_numLinesColumnToken);
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::SetStatusOffset(PRUint32 statusOffset)
{
	return SetUInt32Column(statusOffset, m_mdb->m_statusOffsetColumnToken);
}

NS_IMETHODIMP nsMsgHdr::SetDate(PRTime date)
{
	m_date = date;
	PRUint32 seconds;
	nsMsgDatabase::PRTime2Seconds(date, &seconds);
    return SetUInt32Column((PRUint32) seconds, m_mdb->m_dateColumnToken);
}

NS_IMETHODIMP nsMsgHdr::GetStatusOffset(PRUint32 *result)
{
	PRUint32 offset = 0;
	nsresult res = GetUInt32Column(m_mdb->m_statusOffsetColumnToken, &offset);

	*result = offset;
    return res;
}

NS_IMETHODIMP nsMsgHdr::SetPriority(nsMsgPriorityValue priority)
{
	SetUInt32Column((PRUint32) priority, m_mdb->m_priorityColumnToken);
	return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::GetPriority(nsMsgPriorityValue *result)
{
	if (!result)
	    return NS_ERROR_NULL_POINTER;

	PRUint32 priority = 0;
	nsresult rv = GetUInt32Column(m_mdb->m_priorityColumnToken, &priority);
    if (NS_FAILED(rv)) return rv;
    
	*result = (nsMsgPriorityValue) priority;
	return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::GetMessageOffset(PRUint32 *result)
{
    *result = m_messageKey;
    return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::GetMessageSize(PRUint32 *result)
{
	PRUint32 size;
	nsresult res = GetUInt32Column(m_mdb->m_messageSizeColumnToken, &size);

	*result = size;
	return res;
}

NS_IMETHODIMP nsMsgHdr::GetLineCount(PRUint32 *result)
{
        PRUint32 linecount;
        nsresult res = GetUInt32Column(m_mdb->m_numLinesColumnToken, &linecount);
        *result = linecount;
        return res;
}

NS_IMETHODIMP nsMsgHdr::SetPriorityString(const char *priority)
{
	nsMsgPriorityValue priorityVal = nsMsgPriority::normal;

	// NS_MsgGetPriorityFromString will return normal in case of error,
	// so we can ignore return value.
	nsresult res;
    res = NS_MsgGetPriorityFromString(priority, &priorityVal);
	return SetPriority(priorityVal);
}

NS_IMETHODIMP nsMsgHdr::GetAuthor(char* *resultAuthor)
{
	return m_mdb->RowCellColumnToCharPtr(GetMDBRow(), m_mdb->m_senderColumnToken, resultAuthor);
}

NS_IMETHODIMP nsMsgHdr::GetSubject(char* *resultSubject)
{
	return m_mdb->RowCellColumnToCharPtr(GetMDBRow(), m_mdb->m_subjectColumnToken, resultSubject);
}

NS_IMETHODIMP nsMsgHdr::GetRecipients(char* *resultRecipients)
{
	return m_mdb->RowCellColumnToCharPtr(GetMDBRow(), m_mdb->m_recipientsColumnToken, resultRecipients);
}

NS_IMETHODIMP nsMsgHdr::GetCcList(char * *resultCCList)
{
	return m_mdb->RowCellColumnToCharPtr(GetMDBRow(), m_mdb->m_ccListColumnToken, resultCCList);
}

NS_IMETHODIMP nsMsgHdr::GetMessageId(char * *resultMessageId)
{
	return m_mdb->RowCellColumnToCharPtr(GetMDBRow(), m_mdb->m_messageIdColumnToken, resultMessageId);
}

NS_IMETHODIMP nsMsgHdr::GetMime2DecodedAuthor(PRUnichar* *resultAuthor)
{
	return m_mdb->RowCellColumnToMime2DecodedString(GetMDBRow(), m_mdb->m_senderColumnToken, resultAuthor);
}

NS_IMETHODIMP nsMsgHdr::GetMime2DecodedSubject(PRUnichar* *resultSubject)
{
	return m_mdb->RowCellColumnToMime2DecodedString(GetMDBRow(), m_mdb->m_subjectColumnToken, resultSubject);
}

NS_IMETHODIMP nsMsgHdr::GetMime2DecodedRecipients(PRUnichar* *resultRecipients)
{
	return m_mdb->RowCellColumnToMime2DecodedString(GetMDBRow(), m_mdb->m_recipientsColumnToken, resultRecipients);
}


NS_IMETHODIMP nsMsgHdr::GetAuthorCollationKey(PRUnichar* *resultAuthor)
{
	nsCAutoString cSender;
	char *name = nsnull;

	nsresult ret = m_mdb->RowCellColumnTonsCString(GetMDBRow(), m_mdb->m_senderColumnToken, cSender);
	if (NS_SUCCEEDED(ret))
	{
		nsIMsgHeaderParser *headerParser = m_mdb->GetHeaderParser();
		if (headerParser)
		{
			// apply mime decode
			nsIMimeConverter *converter;
			ret = nsComponentManager::CreateInstance(kCMimeConverterCID, nsnull, 
												NS_GET_IID(nsIMimeConverter), (void **)&converter);

			if (NS_SUCCEEDED(ret) && nsnull != converter) 
			{
				char *resultStr = nsnull;
				char *charset = nsnull;
				m_mdb->m_dbFolderInfo->GetCharPtrCharacterSet(&charset);
				char charsetName[128];
				PL_strncpy(charsetName, charset, sizeof(charsetName));

				ret = converter->DecodeMimePartIIStr(cSender.GetBuffer(), charsetName, &resultStr);
				if (NS_SUCCEEDED(ret))
				{
					ret = headerParser->ExtractHeaderAddressName (charsetName, resultStr, &name);
				}
				NS_RELEASE(converter);
				PR_FREEIF(resultStr);
				PR_FREEIF(charset);
			}

		}
	}
	if (NS_SUCCEEDED(ret))
	{
		nsAutoString autoString; autoString.AssignWithConversion(name);
        PRUnichar *uniName = autoString.ToNewUnicode();
		ret = m_mdb->CreateCollationKey(uniName, resultAuthor);
        Recycle(uniName);
	}

	if(name)
		PL_strfree(name);

	return ret;
}

NS_IMETHODIMP nsMsgHdr::GetSubjectCollationKey(PRUnichar* *resultSubject)
{
	return m_mdb->RowCellColumnToCollationKey(GetMDBRow(), m_mdb->m_subjectColumnToken, resultSubject);
}

NS_IMETHODIMP nsMsgHdr::GetRecipientsCollationKey(PRUnichar* *resultRecipients)
{
	return m_mdb->RowCellColumnToCollationKey(GetMDBRow(), m_mdb->m_recipientsColumnToken, resultRecipients);
}

NS_IMETHODIMP nsMsgHdr::GetCharset(char **aCharset)
{
	return m_mdb->RowCellColumnToCharPtr(GetMDBRow(), m_mdb->m_messageCharSetColumnToken, aCharset);
}

NS_IMETHODIMP nsMsgHdr::SetCharset(const char *aCharset)
{
  return SetStringColumn(aCharset, m_mdb->m_messageCharSetColumnToken);
}

NS_IMETHODIMP nsMsgHdr::SetThreadParent(nsMsgKey inKey)
{
	m_threadParent = inKey;
	SetUInt32Column(m_threadParent, m_mdb->m_threadParentColumnToken);
	m_initedValues |= THREAD_PARENT_INITED;
	return NS_OK;
}

NS_IMETHODIMP nsMsgHdr::GetThreadParent(nsMsgKey *result)
{
	nsresult res;
	if(!(m_initedValues & THREAD_PARENT_INITED))
	{
		res = GetUInt32Column(m_mdb->m_threadParentColumnToken, &m_threadParent, nsMsgKey_None);
		if (NS_SUCCEEDED(res))
			m_initedValues |= THREAD_PARENT_INITED;
	}
    *result = m_threadParent;
    return NS_OK;
}

nsresult nsMsgHdr::SetStringColumn(const char *str, mdb_token token)
{
	struct mdbYarn yarn;
	yarn.mYarn_Buf = (void *) (str ? str : "");
	yarn.mYarn_Size = PL_strlen((const char *) yarn.mYarn_Buf) + 1;
	yarn.mYarn_Fill = yarn.mYarn_Size - 1;
	yarn.mYarn_Form = 0;
	yarn.mYarn_Grow = NULL;
	return m_mdbRow->AddColumn(m_mdb->GetEnv(), token, &yarn);
}

nsresult nsMsgHdr::SetUInt32Column(PRUint32 value, mdb_token token)
{
	char	yarnBuf[100];

	struct mdbYarn yarn;
	yarn.mYarn_Buf = (void *) yarnBuf;
	yarn.mYarn_Size = sizeof(yarnBuf);
	yarn.mYarn_Fill = yarn.mYarn_Size;
	yarn.mYarn_Form = 0;
	yarn.mYarn_Grow = NULL;
	return m_mdbRow->AddColumn(m_mdb->GetEnv(),  token, nsMsgDatabase::UInt32ToYarn(&yarn, value));
}

nsresult nsMsgHdr::GetUInt32Column(mdb_token token, PRUint32 *pvalue, PRUint32 defaultValue)
{
	return m_mdb->RowCellColumnToUInt32(GetMDBRow(), token, pvalue, defaultValue);
}

// get the next <> delimited reference from nextRef and copy it into reference,
const char *nsMsgHdr::GetNextReference(const char *startNextRef, nsCString &reference)
{
	const char *ptr = startNextRef;

	reference.Truncate(0);
	while ((*ptr == '<' || *ptr == ' ') && *ptr)
		ptr++;

	for (int i = 0; *ptr && *ptr != '>'; i++)
		reference += *ptr++;

	if (*ptr == '>')
		ptr++;
	return ptr;
}
// Get previous <> delimited reference - used to go backwards through the
// reference string. Caller will need to make sure that prevRef is not before
// the start of the reference string when we return.
const char *nsMsgHdr::GetPrevReference(const char *prevRef, nsCString &reference)
{
	const char *ptr = prevRef;

	while ((*ptr == '>' || *ptr == ' ') && *ptr)
		ptr--;

	// scan back to '<'
	for (int i = 0; *ptr && *ptr != '<' ; i++)
		ptr--;

	GetNextReference(ptr, reference);
	if (*ptr == '<')
		ptr--;
	return ptr;
}

PRBool nsMsgHdr::IsParentOf(nsIMsgDBHdr *possibleChild)
{
	PRUint16 numReferences = 0;
	possibleChild->GetNumReferences(&numReferences);
	nsCAutoString reference;
	nsXPIDLCString messageId;

	GetMessageId(getter_Copies(messageId));
	possibleChild->GetStringReference(numReferences - 1, reference);

	return (reference.Equals(messageId));
}

