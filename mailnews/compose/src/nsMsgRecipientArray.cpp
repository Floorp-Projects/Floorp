/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsMsgRecipientArray.h"
#include "nsString.h"

nsMsgRecipientArray::nsMsgRecipientArray()
{
	NS_INIT_REFCNT();
	m_array = new nsStringArray;
}


nsMsgRecipientArray::~nsMsgRecipientArray()
{
	if (m_array)
		delete m_array;
}

/* the following macro actually implement addref, release and query interface for our class. */
NS_IMPL_ISUPPORTS1(nsMsgRecipientArray, nsIMsgRecipientArray)

nsresult nsMsgRecipientArray::StringAt(PRInt32 idx, PRUnichar **_retval)
{
	if (!_retval || !m_array)
		return NS_ERROR_NULL_POINTER;
	
	nsString aStr;
	m_array->StringAt(idx, aStr);
	*_retval = aStr.ToNewUnicode();
	return NS_OK;
}

nsresult nsMsgRecipientArray::AppendString(const PRUnichar *aString, PRBool *_retval)
{
	if (!_retval || !m_array)
		return NS_ERROR_NULL_POINTER;
		
	*_retval = m_array->AppendString(nsString(aString));

	return NS_OK;
}

nsresult nsMsgRecipientArray::RemoveStringAt(PRInt32 idx, PRBool *_retval)
{
	if (!_retval || !m_array)
		return NS_ERROR_NULL_POINTER;
		
	*_retval = m_array->RemoveStringAt(idx);

	return NS_OK;
}

nsresult nsMsgRecipientArray::Clear()
{
	if (!m_array)
		return NS_ERROR_NULL_POINTER;
	
	m_array->Clear();

	return NS_OK;
}

nsresult nsMsgRecipientArray::GetCount(PRInt32 *aCount)
{
	if (!aCount || !m_array)
		return NS_ERROR_NULL_POINTER;

	*aCount = m_array->Count();
	return NS_OK;
}
