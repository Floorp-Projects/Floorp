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
#include "nsMsgFolderCacheElement.h"

nsMsgFolderCacheElement::nsMsgFolderCacheElement()
{
	m_mdbRow = nsnull;
	m_owningCache = nsnull;
	m_folderURI = nsnull;
	NS_INIT_REFCNT();
}

nsMsgFolderCacheElement::~nsMsgFolderCacheElement()
{
	NS_IF_RELEASE(m_mdbRow);
	NS_IF_RELEASE(m_owningCache);
	PR_FREEIF(m_folderURI);
}


NS_IMPL_ISUPPORTS(nsMsgFolderCacheElement, GetIID());

NS_IMPL_GETTER_STR(nsMsgFolderCacheElement::GetURI, m_folderURI)
NS_IMPL_SETTER_STR(nsMsgFolderCacheElement::SetURI, m_folderURI)

void nsMsgFolderCacheElement::SetOwningCache(nsMsgFolderCache *owningCache)
{
	m_owningCache = owningCache;
	if (owningCache)
		NS_ADDREF(owningCache);
}

NS_IMETHODIMP 	 nsMsgFolderCacheElement::GetStringProperty(const char *propertyName, char **result)
{
	if (!propertyName || !result || !m_mdbRow || !m_owningCache)
		return NS_ERROR_NULL_POINTER;

	mdb_token	property_token;

	nsresult ret = m_owningCache->GetStore()->StringToToken(m_owningCache->GetEnv(),  propertyName, &property_token);
	if (ret == NS_OK)
	{
		ret = m_owningCache->RowCellColumnToCharPtr(m_mdbRow, property_token, result);
	}
	return ret;
}

NS_IMETHODIMP nsMsgFolderCacheElement::GetInt32Property(const char *propertyName, PRInt32 *aResult)
{
	if (!propertyName || !aResult || !m_mdbRow)
		return NS_ERROR_NULL_POINTER;

	char *resultStr;
	GetStringProperty(propertyName, &resultStr);
	if (!resultStr)
		return NS_ERROR_NULL_POINTER;

	PRInt32 result;
	char *p = resultStr;

	for (result = 0; *p; p++)
	{
		char C = *p;

		PRInt8 unhex = ((C >= '0' && C <= '9') ? C - '0' :
			((C >= 'A' && C <= 'F') ? C - 'A' + 10 :
			 ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : -1)));
		if (unhex < 0)
			break;
		result = (result << 4) | unhex;
	}

    PR_FREEIF(resultStr);
	*aResult = result;

	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolderCacheElement::SetStringProperty(const char *propertyName, const char *propertyValue)
{
	if (!propertyName || !m_mdbRow)
		return NS_ERROR_NULL_POINTER;
	nsresult err = NS_OK;
	mdb_token	property_token;

	if (m_owningCache)
	{
		err = m_owningCache->GetStore()->StringToToken(m_owningCache->GetEnv(),  propertyName, &property_token);
		if (err == NS_OK)
		{
			struct mdbYarn yarn;

			yarn.mYarn_Grow = NULL;
			if (m_mdbRow)
			{
				yarn.mYarn_Buf = (void *) propertyValue;
				yarn.mYarn_Size = PL_strlen((const char *) yarn.mYarn_Buf) + 1;
				yarn.mYarn_Fill = yarn.mYarn_Size - 1;
				yarn.mYarn_Form = 0;	// what to do with this? we're storing csid in the msg hdr...
				nsresult err = m_mdbRow->AddColumn(m_owningCache->GetEnv(), property_token, &yarn);
				return err;
			}
		}
	}
	return err;
}

NS_IMETHODIMP nsMsgFolderCacheElement::SetInt32Property(const char *propertyName, PRInt32 propertyValue)
{
	if (!propertyName || !m_mdbRow)
		return NS_ERROR_NULL_POINTER;
	nsCAutoString propertyStr;
	propertyStr.Append(propertyValue, 16);
	return SetStringProperty(propertyName, propertyStr);

}

void nsMsgFolderCacheElement::SetMDBRow(nsIMdbRow	*row) 
{
	if (m_mdbRow)
		NS_RELEASE(m_mdbRow);
	m_mdbRow = row;
	if (row)
		NS_ADDREF(row);
}
