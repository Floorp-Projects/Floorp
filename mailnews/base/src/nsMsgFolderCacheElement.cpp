/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"
#include "nsMsgFolderCacheElement.h"
#include "prmem.h"

nsMsgFolderCacheElement::nsMsgFolderCacheElement()
{
	m_mdbRow = nsnull;
	m_owningCache = nsnull;
	m_folderKey = nsnull;
	NS_INIT_REFCNT();
}

nsMsgFolderCacheElement::~nsMsgFolderCacheElement()
{
	NS_IF_RELEASE(m_mdbRow);
	// circular reference, don't do it.
//	NS_IF_RELEASE(m_owningCache);
	PR_FREEIF(m_folderKey);
}


NS_IMPL_ISUPPORTS1(nsMsgFolderCacheElement, nsIMsgFolderCacheElement)

NS_IMPL_GETTER_STR(nsMsgFolderCacheElement::GetKey, m_folderKey)
NS_IMPL_SETTER_STR(nsMsgFolderCacheElement::SetKey, m_folderKey)

void nsMsgFolderCacheElement::SetOwningCache(nsMsgFolderCache *owningCache)
{
	m_owningCache = owningCache;
	// circular reference, don't do it.
//	if (owningCache)
//		NS_ADDREF(owningCache);
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

	char *resultStr = nsnull;
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

	return NS_OK;
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
				err = m_mdbRow->AddColumn(m_owningCache->GetEnv(), property_token, &yarn);
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
	propertyStr.AppendInt(propertyValue, 16);
	return SetStringProperty(propertyName, propertyStr.get());

}

void nsMsgFolderCacheElement::SetMDBRow(nsIMdbRow	*row) 
{
	if (m_mdbRow)
		NS_RELEASE(m_mdbRow);
	m_mdbRow = row;
	if (row)
		NS_ADDREF(row);
}
