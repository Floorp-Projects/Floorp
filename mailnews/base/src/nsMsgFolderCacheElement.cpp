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

NS_IMETHODIMP 	 nsMsgFolderCacheElement::GetStringProperty(const char *propertyName, char **result)
{
	if (!propertyName || !result || !m_mdbRow)
		return NS_ERROR_NULL_POINTER;

	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolderCacheElement::GetInt32Property(const char *propertyName, PRInt32 *result)
{
	if (!propertyName || !result || !m_mdbRow)
		return NS_ERROR_NULL_POINTER;
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolderCacheElement::SetStringProperty(const char *propertyName, const char *propertyValue)
{
	if (!propertyName || !m_mdbRow)
		return NS_ERROR_NULL_POINTER;
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolderCacheElement::SetInt32Property(const char *propertyName, PRInt32 propertyValue)
{
	if (!propertyName || !m_mdbRow)
		return NS_ERROR_NULL_POINTER;
	return NS_ERROR_NOT_IMPLEMENTED;
}

