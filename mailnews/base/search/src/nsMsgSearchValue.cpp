/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Alec Flett <alecf@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "MailNewsTypes.h"
#include "nsMsgSearchValue.h"
#include "nsReadableUtils.h"
#include "nsISupportsObsolete.h"

nsMsgSearchValueImpl::nsMsgSearchValueImpl(nsMsgSearchValue *aInitialValue)
{
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

NS_IMPL_GETSET(nsMsgSearchValueImpl, Priority, nsMsgPriorityValue, mValue.u.priority)
NS_IMPL_GETSET(nsMsgSearchValueImpl, Status, PRUint32, mValue.u.msgStatus)
NS_IMPL_GETSET(nsMsgSearchValueImpl, Size, PRUint32, mValue.u.size)
NS_IMPL_GETSET(nsMsgSearchValueImpl, MsgKey, nsMsgKey, mValue.u.key)
NS_IMPL_GETSET(nsMsgSearchValueImpl, Age, PRUint32, mValue.u.age)
NS_IMPL_GETSET(nsMsgSearchValueImpl, Date, PRTime, mValue.u.date)
NS_IMPL_GETSET(nsMsgSearchValueImpl, Attrib, nsMsgSearchAttribValue, mValue.attribute)
NS_IMPL_GETSET(nsMsgSearchValueImpl, Label, nsMsgLabelValue, mValue.u.label)
NS_IMPL_GETSET(nsMsgSearchValueImpl, JunkStatus, PRUint32, mValue.u.junkStatus)

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
nsMsgSearchValueImpl::GetStr(PRUnichar** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_TRUE(IS_STRING_ATTRIBUTE(mValue.attribute), NS_ERROR_ILLEGAL_VALUE);
    *aResult = ToNewUnicode(NS_ConvertUTF8toUCS2(mValue.string));
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::SetStr(const PRUnichar* aValue)
{
    NS_ENSURE_TRUE(IS_STRING_ATTRIBUTE(mValue.attribute), NS_ERROR_ILLEGAL_VALUE);
    if (mValue.string)
        nsCRT::free(mValue.string);
    mValue.string = ToNewUTF8String(nsDependentString(aValue));
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchValueImpl::ToString(PRUnichar **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    nsAutoString resultStr;
    resultStr.AssignLiteral("[nsIMsgSearchValue: ");
    if (IS_STRING_ATTRIBUTE(mValue.attribute)) {
        AppendUTF8toUTF16(mValue.string, resultStr);
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
    case nsMsgSearchAttrib::Label:
    case nsMsgSearchAttrib::JunkStatus:
        resultStr.AppendLiteral("type=");
        resultStr.AppendInt(mValue.attribute);
        break;
    default:
        NS_ASSERTION(0, "Unknown search value type");
    }        

    resultStr.AppendLiteral("]");

    *aResult = ToNewUnicode(resultStr);
    return NS_OK;
}
