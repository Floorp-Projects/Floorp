/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
