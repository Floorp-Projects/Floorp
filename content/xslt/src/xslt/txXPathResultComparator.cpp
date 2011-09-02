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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com>
 *   Peter Van der Beken <peterv@propagandism.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "txXPathResultComparator.h"
#include "txExpr.h"
#include "txCore.h"
#include "nsCollationCID.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsIServiceManager.h"
#include "nsLocaleCID.h"
#include "prmem.h"

#define kAscending (1<<0)
#define kUpperFirst (1<<1)

txResultStringComparator::txResultStringComparator(MBool aAscending,
                                                   MBool aUpperFirst,
                                                   const nsAFlatString& aLanguage)
{
    mSorting = 0;
    if (aAscending)
        mSorting |= kAscending;
    if (aUpperFirst)
        mSorting |= kUpperFirst;
    nsresult rv = init(aLanguage);
    if (NS_FAILED(rv))
        NS_ERROR("Failed to initialize txResultStringComparator");
}

nsresult txResultStringComparator::init(const nsAFlatString& aLanguage)
{
    nsresult rv;

    nsCOMPtr<nsILocaleService> localeService =
                    do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILocale> locale;
    if (!aLanguage.IsEmpty()) {
        rv = localeService->NewLocale(aLanguage,
                                      getter_AddRefs(locale));
    }
    else {
        rv = localeService->GetApplicationLocale(getter_AddRefs(locale));
    }
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsICollationFactory> colFactory =
                    do_CreateInstance(NS_COLLATIONFACTORY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = colFactory->CreateCollation(locale, getter_AddRefs(mCollation));
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

nsresult
txResultStringComparator::createSortableValue(Expr *aExpr,
                                              txIEvalContext *aContext,
                                              txObject *&aResult)
{
    nsAutoPtr<StringValue> val(new StringValue);
    if (!val) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!mCollation)
        return NS_ERROR_FAILURE;

    val->mCaseKey = new nsString;
    if (!val->mCaseKey) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsString& nsCaseKey = *(nsString *)val->mCaseKey;
    nsresult rv = aExpr->evaluateToString(aContext, nsCaseKey);
    NS_ENSURE_SUCCESS(rv, rv);

    if (nsCaseKey.IsEmpty()) {
        aResult = val.forget();

        return NS_OK;
    }

    rv = mCollation->AllocateRawSortKey(nsICollation::kCollationCaseInSensitive,
                                        nsCaseKey, &val->mKey, &val->mLength);
    NS_ENSURE_SUCCESS(rv, rv);

    aResult = val.forget();

    return NS_OK;
}

int txResultStringComparator::compareValues(TxObject* aVal1, TxObject* aVal2)
{
    StringValue* strval1 = (StringValue*)aVal1;
    StringValue* strval2 = (StringValue*)aVal2;

    if (!mCollation)
        return -1;

    if (strval1->mLength == 0) {
        if (strval2->mLength == 0)
            return 0;
        return ((mSorting & kAscending) ? -1 : 1);
    }

    if (strval2->mLength == 0)
        return ((mSorting & kAscending) ? 1 : -1);

    nsresult rv;
    PRInt32 result = -1;
    rv = mCollation->CompareRawSortKey(strval1->mKey, strval1->mLength,
                                       strval2->mKey, strval2->mLength,
                                       &result);
    if (NS_FAILED(rv)) {
        // XXX ErrorReport
        return -1;
    }

    if (result != 0)
        return ((mSorting & kAscending) ? 1 : -1) * result;

    if ((strval1->mCaseLength == 0) && (strval1->mLength != 0)) {
        nsString* caseString = (nsString *)strval1->mCaseKey;
        rv = mCollation->AllocateRawSortKey(nsICollation::kCollationCaseSensitive,
                                            *caseString,
                                            (PRUint8**)&strval1->mCaseKey, 
                                            &strval1->mCaseLength);
        if (NS_FAILED(rv)) {
            // XXX ErrorReport
            strval1->mCaseKey = caseString;
            strval1->mCaseLength = 0;
            return -1;
        }
        delete caseString;
    }
    if ((strval2->mCaseLength == 0) && (strval2->mLength != 0)) {
        nsString* caseString = (nsString *)strval2->mCaseKey;
        rv = mCollation->AllocateRawSortKey(nsICollation::kCollationCaseSensitive,
                                            *caseString,
                                            (PRUint8**)&strval2->mCaseKey, 
                                            &strval2->mCaseLength);
        if (NS_FAILED(rv)) {
            // XXX ErrorReport
            strval2->mCaseKey = caseString;
            strval2->mCaseLength = 0;
            return -1;
        }
        delete caseString;
    }
    rv = mCollation->CompareRawSortKey((PRUint8*)strval1->mCaseKey, strval1->mCaseLength,
                                       (PRUint8*)strval2->mCaseKey, strval2->mCaseLength,
                                       &result);
    if (NS_FAILED(rv)) {
        // XXX ErrorReport
        return -1;
    }

    return ((mSorting & kAscending) ? 1 : -1) *
           ((mSorting & kUpperFirst) ? -1 : 1) * result;
}

txResultStringComparator::StringValue::StringValue() : mKey(0),
                                                       mCaseKey(0),
                                                       mLength(0),
                                                       mCaseLength(0)
{
}

txResultStringComparator::StringValue::~StringValue()
{
    PR_Free(mKey);
    if (mCaseLength > 0)
        PR_Free((PRUint8*)mCaseKey);
    else
        delete (nsString*)mCaseKey;
}

txResultNumberComparator::txResultNumberComparator(MBool aAscending)
{
    mAscending = aAscending ? 1 : -1;
}

nsresult
txResultNumberComparator::createSortableValue(Expr *aExpr,
                                              txIEvalContext *aContext,
                                              TxObject *&aResult)
{
    nsAutoPtr<NumberValue> numval(new NumberValue);
    if (!numval) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = aExpr->evaluate(aContext, getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    numval->mVal = exprRes->numberValue();

    aResult = numval.forget();

    return NS_OK;
}

int txResultNumberComparator::compareValues(TxObject* aVal1, TxObject* aVal2)
{
    double dval1 = ((NumberValue*)aVal1)->mVal;
    double dval2 = ((NumberValue*)aVal2)->mVal;

    if (Double::isNaN(dval1))
        return Double::isNaN(dval2) ? 0 : -mAscending;

    if (Double::isNaN(dval2))
        return mAscending;

    if (dval1 == dval2)
        return 0;
    
    return (dval1 < dval2) ? -mAscending : mAscending;
}
