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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Created by: Paul Sandoz   <paul.sandoz@sun.com> 
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

#include "nsAbBooleanExpression.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsAbBooleanConditionString, nsIAbBooleanConditionString)

nsAbBooleanConditionString::nsAbBooleanConditionString() :
    mCondition (nsIAbBooleanConditionTypes::Exists)
{
  NS_INIT_ISUPPORTS();
}

nsAbBooleanConditionString::~nsAbBooleanConditionString()
{
}

/* attribute nsAbBooleanConditionType condition; */
NS_IMETHODIMP nsAbBooleanConditionString::GetCondition(nsAbBooleanConditionType *aCondition)
{
    if (!aCondition)
        return NS_ERROR_NULL_POINTER;

    *aCondition = mCondition;

    return NS_OK;
}
NS_IMETHODIMP nsAbBooleanConditionString::SetCondition(nsAbBooleanConditionType aCondition)
{
    mCondition = aCondition;

    return NS_OK;
}

/* attribute string name; */
NS_IMETHODIMP nsAbBooleanConditionString::GetName(char** aName)
{
    if (!aName)
        return NS_ERROR_NULL_POINTER;

    const char* value = mName.get ();

    if (!value)
        *aName = 0;
    else
        *aName = mName.ToNewCString ();

    return NS_OK;

}
NS_IMETHODIMP nsAbBooleanConditionString::SetName(const char* aName)
{
    if (!aName)
        return NS_ERROR_NULL_POINTER;

    mName = aName;

    return NS_OK;
}

/* attribute wstring value; */
NS_IMETHODIMP nsAbBooleanConditionString::GetValue(PRUnichar** aValue)
{
    if (!aValue)
        return NS_ERROR_NULL_POINTER;

    *aValue = mValue.ToNewUnicode();

    return NS_OK;
}
NS_IMETHODIMP nsAbBooleanConditionString::SetValue(const PRUnichar * aValue)
{
    if (!aValue)
        return NS_ERROR_NULL_POINTER;

    mValue = aValue;

    return NS_OK;
}

NS_METHOD nsAbBooleanConditionString::Create(nsISupports* aOuter, REFNSIID aIID, void* *aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsAbBooleanConditionString* it = new nsAbBooleanConditionString ();
    if (it == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_IF_ADDREF(it);
    nsresult rv = it->QueryInterface(aIID, aResult);
    NS_RELEASE(it);
    return rv;
}

nsresult NS_NewIAbBooleanConditionString(nsIAbBooleanConditionString** aInstancePtrResult)
{
    nsresult rv;
    rv = nsAbBooleanConditionString::Create(NULL, NS_GET_IID(nsIAbBooleanConditionString), NS_REINTERPRET_CAST(void**,aInstancePtrResult));
    return rv;
}





NS_IMPL_THREADSAFE_ISUPPORTS1(nsAbBooleanExpression, nsIAbBooleanExpression)

nsAbBooleanExpression::nsAbBooleanExpression() :
    mOperation (nsIAbBooleanOperationTypes::AND)
{
  NS_INIT_ISUPPORTS();
}

nsAbBooleanExpression::~nsAbBooleanExpression()
{
}

/* attribute nsAbBooleanOperationType operation; */
NS_IMETHODIMP nsAbBooleanExpression::GetOperation(nsAbBooleanOperationType *aOperation)
{
    if (!aOperation)
        return NS_ERROR_NULL_POINTER;

    *aOperation = mOperation;

    return NS_OK;
}
NS_IMETHODIMP nsAbBooleanExpression::SetOperation(nsAbBooleanOperationType aOperation)
{
    mOperation = aOperation;

    return NS_OK;
}

/* attribute nsISupportsArray expressions; */
NS_IMETHODIMP nsAbBooleanExpression::GetExpressions(nsISupportsArray** aExpressions)
{
    if (!aExpressions)
        return NS_ERROR_NULL_POINTER;

    if (!mExpressions)
        NS_NewISupportsArray(getter_AddRefs(mExpressions));

    *aExpressions = mExpressions;
    NS_IF_ADDREF(*aExpressions);

    return NS_OK;

    
}
NS_IMETHODIMP nsAbBooleanExpression::SetExpressions(nsISupportsArray* aExpressions)
{
    if (!aExpressions)
        return NS_ERROR_NULL_POINTER;

    mExpressions = aExpressions;

    return NS_OK;
}

/* void asetExpressions (in unsigned long aExpressionsSize, [array, size_is (aExpressionsSize)] in nsISupports aExpressionsArray); */
NS_IMETHODIMP nsAbBooleanExpression::AsetExpressions(PRUint32 aExpressionsSize, nsISupports **aExpressionsArray)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void agetExpressions (out unsigned long aExpressionsSize, [array, size_is (aExpressionsSize), retval] out nsISupports aExpressionsArray); */
NS_IMETHODIMP nsAbBooleanExpression::AgetExpressions(PRUint32 *aExpressionsSize, nsISupports ***aExpressionsArray)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsAbBooleanExpression::Create(nsISupports* aOuter, REFNSIID aIID, void* *aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsAbBooleanExpression* it = new nsAbBooleanExpression();
    if (it == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_IF_ADDREF(it);
    nsresult rv = it->QueryInterface(aIID, aResult);
    NS_RELEASE(it);
    return rv;
}

nsresult NS_NewIAbBooleanExpression(nsIAbBooleanExpression** aInstancePtrResult)
{
    nsresult rv;
    rv = nsAbBooleanExpression::Create(NULL, NS_GET_IID(nsIAbBooleanExpression), NS_REINTERPRET_CAST(void**, aInstancePtrResult));
    return rv;
}

