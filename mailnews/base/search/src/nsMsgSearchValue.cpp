/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
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

#include "MailNewsTypes.h"
#include "nsMsgSearchValue.h"

nsMsgSearchValueImpl::nsMsgSearchValueImpl(nsMsgSearchValue *aInitialValue)
{
    NS_INIT_ISUPPORTS();
    mValue = *aInitialValue;
    if (IS_STRING_ATTRIBUTE(aInitialValue->attribute))
        mValue.string = nsCRT::strdup(aInitialValue->string);
    else
        mValue.string = 0;
}

nsMsgSearchValueImpl::~nsMsgSearchValueImpl()
{
    if (IS_STRING_ATTRIBUTE(mValue.attribute))
        nsCRT::free(mValue.string);

}

NS_IMPL_ISUPPORTS1(nsMsgSearchValueImpl, nsIMsgSearchValue)

NS_IMETHODIMP
nsMsgSearchValueImpl::GetStr(PRUnichar** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_TRUE(IS_STRING_ATTRIBUTE(mValue.attribute), NS_ERROR_ILLEGAL_VALUE);
    *aResult = NS_ConvertUTF8toUCS2(mValue.string).ToNewUnicode();
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::SetStr(const PRUnichar* aValue)
{
    NS_ENSURE_TRUE(IS_STRING_ATTRIBUTE(mValue.attribute), NS_ERROR_ILLEGAL_VALUE);
    if (mValue.string)
        nsCRT::free(mValue.string);
    mValue.string = NS_ConvertUCS2toUTF8(aValue).ToNewCString();
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::GetPriority(nsMsgPriorityValue *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_TRUE(mValue.attribute == nsMsgSearchAttrib::Priority, NS_ERROR_ILLEGAL_VALUE);
    *aResult = mValue.u.priority;
    return NS_OK;
}
NS_IMETHODIMP
nsMsgSearchValueImpl::SetPriority(nsMsgPriorityValue aValue)
{
    NS_ENSURE_TRUE(mValue.attribute == nsMsgSearchAttrib::Priority, NS_ERROR_ILLEGAL_VALUE);
    mValue.u.priority = aValue;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::GetStatus(PRUint32 *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_TRUE(mValue.attribute == nsMsgSearchAttrib::MsgStatus, NS_ERROR_ILLEGAL_VALUE);
    *aResult = mValue.u.msgStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::SetStatus(PRUint32 aValue)
{
    NS_ENSURE_TRUE(mValue.attribute == nsMsgSearchAttrib::MsgStatus, NS_ERROR_ILLEGAL_VALUE);
    mValue.u.msgStatus = aValue;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::GetSize(PRUint32 *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_TRUE(mValue.attribute == nsMsgSearchAttrib::Size, NS_ERROR_ILLEGAL_VALUE);
    *aResult = mValue.u.size;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::SetSize(PRUint32 aValue)
{
    NS_ENSURE_TRUE(mValue.attribute == nsMsgSearchAttrib::Size, NS_ERROR_ILLEGAL_VALUE);
    mValue.u.size = aValue;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::GetMsgKey(nsMsgKey *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_TRUE(mValue.attribute == nsMsgSearchAttrib::MessageKey, NS_ERROR_ILLEGAL_VALUE);
    *aResult = mValue.u.key;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::SetMsgKey(nsMsgKey aValue)
{
    NS_ENSURE_TRUE(mValue.attribute == nsMsgSearchAttrib::MessageKey, NS_ERROR_ILLEGAL_VALUE);
    mValue.u.key = aValue;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::GetAge(PRUint32 *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_TRUE(mValue.attribute == nsMsgSearchAttrib::AgeInDays, NS_ERROR_ILLEGAL_VALUE);
    *aResult = mValue.u.age;
    return NS_OK;
}
NS_IMETHODIMP
nsMsgSearchValueImpl::SetAge(PRUint32 aValue)
{
    NS_ENSURE_TRUE(mValue.attribute == nsMsgSearchAttrib::AgeInDays, NS_ERROR_ILLEGAL_VALUE);
    mValue.u.age = aValue;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::GetFolder(nsIMsgFolder* *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_TRUE(mValue.attribute == nsMsgSearchAttrib::FolderInfo, NS_ERROR_ILLEGAL_VALUE);
    *aResult = mValue.u.folder;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}
NS_IMETHODIMP
nsMsgSearchValueImpl::SetFolder(nsIMsgFolder* aValue)
{
    NS_ENSURE_TRUE(mValue.attribute == nsMsgSearchAttrib::FolderInfo, NS_ERROR_ILLEGAL_VALUE);
    mValue.u.folder = aValue;
    
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::GetDate(PRTime *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_TRUE(mValue.attribute == nsMsgSearchAttrib::Date, NS_ERROR_ILLEGAL_VALUE);
    *aResult = mValue.u.date;
    return NS_OK;
}
NS_IMETHODIMP
nsMsgSearchValueImpl::SetDate(PRTime aValue)
{
    NS_ENSURE_TRUE(mValue.attribute == nsMsgSearchAttrib::Date, NS_ERROR_ILLEGAL_VALUE);
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

NS_IMETHODIMP
nsMsgSearchValueImpl::ToString(PRUnichar **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    nsAutoString resultStr;
    resultStr.AssignWithConversion("[nsIMsgSearchValue: ");
    if (IS_STRING_ATTRIBUTE(mValue.attribute)) {
        resultStr.Append(NS_ConvertUTF8toUCS2(mValue.string));
        return NS_OK;
    }

    
    switch (mValue.attribute) {

    case nsMsgSearchAttrib::Priority:
    case nsMsgSearchAttrib::Date:
    case nsMsgSearchAttrib::MsgStatus:
    case nsMsgSearchAttrib::MessageKey:
    case nsMsgSearchAttrib::Size:
    case nsMsgSearchAttrib::AgeInDays:
    case nsMsgSearchAttrib::FolderInfo:
        resultStr.AppendWithConversion("type=");
        resultStr.AppendInt(mValue.attribute);
        break;
    default:
        NS_ASSERTION(0, "Unknown search value type");
    }        

    resultStr.AppendWithConversion("]");

    *aResult = resultStr.ToNewUnicode();
    return NS_OK;
}
