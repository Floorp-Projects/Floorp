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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

#include "MailNewsTypes.h"
#include "nsMsgSearchValue.h"

nsMsgSearchValueImpl::nsMsgSearchValueImpl(nsMsgSearchValue *aInitialValue)
{
    NS_INIT_ISUPPORTS();
    mValue = *aInitialValue;
    // XXX TODO - make a copy of the string if it's a string attribute
    // (right now we dont' know when it's a string!)

}

nsMsgSearchValueImpl::~nsMsgSearchValueImpl()
{
    // XXX TODO - free the string if it's a string attribute

}

NS_IMPL_ISUPPORTS1(nsMsgSearchValueImpl, nsIMsgSearchValue)

NS_IMETHODIMP
nsMsgSearchValueImpl::GetStr(char** aResult)
{
    *aResult = nsCRT::strdup(mValue.u.string);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::SetStr(const char* aValue)
{
    if (mValue.u.string)
        nsCRT::free(mValue.u.string);
    mValue.u.string = nsCRT::strdup(aValue);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::GetPriority(nsMsgPriority *aResult)
{
    *aResult = mValue.u.priority;
    return NS_OK;
}
NS_IMETHODIMP
nsMsgSearchValueImpl::SetPriority(nsMsgPriority aValue)
{
    aValue = mValue.u.priority;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::GetStatus(PRUint32 *aResult)
{
    *aResult = mValue.u.msgStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::SetStatus(PRUint32 aValue)
{
    mValue.u.msgStatus = aValue;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::GetSize(PRUint32 *aResult)
{
    *aResult = mValue.u.size;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::SetSize(PRUint32 aValue)
{
    mValue.u.size = aValue;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::GetMsgKey(nsMsgKey *aResult)
{
    *aResult = mValue.u.key;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::SetMsgKey(nsMsgKey aValue)
{
    mValue.u.key = aValue;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::GetAge(PRUint32 *aResult)
{
    *aResult = mValue.u.age;
    return NS_OK;
}
NS_IMETHODIMP
nsMsgSearchValueImpl::SetAge(PRUint32 aValue)
{
    mValue.u.age = aValue;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::GetFolder(nsIMsgFolder* *aResult)
{
    *aResult = mValue.u.folder;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}
NS_IMETHODIMP
nsMsgSearchValueImpl::SetFolder(nsIMsgFolder* aValue)
{
    mValue.u.folder = aValue;
    
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::GetDate(PRTime *aResult)
{
    *aResult = mValue.u.date;
    return NS_OK;
}
NS_IMETHODIMP
nsMsgSearchValueImpl::SetDate(PRTime aValue)
{
    mValue.u.date = aValue;
    return NS_OK;
}


NS_IMETHODIMP
nsMsgSearchValueImpl::GetAttrib(nsMsgSearchAttribValue *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    
    *aResult = mValue.attribute;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::SetAttrib(nsMsgSearchAttribValue aValue)
{
    mValue.attribute = aValue;
    return NS_OK;
}

