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
 * The Original Code is the TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com>
 *   Peter Van der Beken <peterv@netscape.com>
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
#include "Expr.h"
#include "txNodeSorter.h"
#ifndef TX_EXE
#include "nsCollationCID.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsIServiceManager.h"
#include "nsLocaleCID.h"
#include "prmem.h"

static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
#endif

#define kAscending (1<<0)
#define kUpperFirst (1<<1)

txResultStringComparator::txResultStringComparator(MBool aAscending,
                                                   MBool aUpperFirst,
                                                   const String& aLanguage)
{
    mSorting = 0;
    if (aAscending)
        mSorting |= kAscending;
    if (aUpperFirst)
        mSorting |= kUpperFirst;
#ifndef TX_EXE
    nsresult rv = init(aLanguage);
    if (NS_FAILED(rv))
        NS_ERROR("Failed to initialize txResultStringComparator");
#endif
}

txResultStringComparator::~txResultStringComparator()
{
}

#ifndef TX_EXE
nsresult txResultStringComparator::init(const String& aLanguage)
{
    nsresult rv;

    nsCOMPtr<nsILocaleService> localeService =
                    do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILocale> locale;
    rv = localeService->NewLocale(aLanguage.getConstNSString().get(),
                                  getter_AddRefs(locale));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsICollationFactory> colFactory =
                    do_CreateInstance(kCollationFactoryCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = colFactory->CreateCollation(locale, getter_AddRefs(mCollation));
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}
#endif

TxObject* txResultStringComparator::createSortableValue(ExprResult* aExprRes)
{
    StringValue* val = new StringValue;

    if (!val)
        return 0;

#ifdef TX_EXE
    aExprRes->stringValue(val->mStr);
    // We don't support case-order on standalone
    val->mStr.toLowerCase();
#else
    if (!mCollation)
        return 0;

    val->mCaseKey = new String;
    if (!val->mCaseKey) {
        delete val;
        return 0;
    }

    aExprRes->stringValue(*(String *)val->mCaseKey);
    const nsString& nsCaseKey = ((String *)val->mCaseKey)->getConstNSString();
    if (nsCaseKey.IsEmpty()) {
        return val;        
    }
    nsresult rv = createRawSortKey(kCollationCaseInSensitive,
                                   nsCaseKey,
                                   &val->mKey, 
                                   &val->mLength);
    if (NS_FAILED(rv)) {
        NS_ERROR("Failed to create raw sort key");
        delete val;
        return 0;
    }
#endif
    return val;
}

int txResultStringComparator::compareValues(TxObject* aVal1, TxObject* aVal2)
{
    StringValue* strval1 = (StringValue*)aVal1;
    StringValue* strval2 = (StringValue*)aVal2;
#ifdef TX_EXE
    PRInt32 len1 = strval1->mStr.length();
    PRInt32 len2 = strval2->mStr.length();
    PRInt32 minLength = (len1 < len2) ? len1 : len2;

    PRInt32 c = 0;
    while (c < minLength) {
        UNICODE_CHAR ch1 = strval1->mStr.charAt(c);
        UNICODE_CHAR ch2 = strval2->mStr.charAt(c);
        if (ch1 < ch2)
            return ((mSorting & kAscending) ? 1 : -1) * -1;
        if (ch2 < ch1)
            return ((mSorting & kAscending) ? 1 : -1) * 1;
        c++;
    }

    if (len1 == len2)
        return 0;

    return ((mSorting & kAscending) ? 1 : -1) * ((len1 < len2) ? -1 : 1);
#else
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

    if (strval1->mCaseLength < 0) {
        String* caseString = (String *)strval1->mCaseKey;
        rv = createRawSortKey(kCollationCaseSensitive,
                              caseString->getConstNSString(),
                              (PRUint8**)&strval1->mCaseKey, 
                              &strval1->mCaseLength);
        if (NS_FAILED(rv)) {
            // XXX ErrorReport
            return -1;
        }
        delete caseString;
    }
    if (strval2->mCaseLength < 0) {
        String* caseString = (String *)strval2->mCaseKey;
        rv = createRawSortKey(kCollationCaseSensitive,
                              caseString->getConstNSString(),
                              (PRUint8**)&strval2->mCaseKey, 
                              &strval2->mCaseLength);
        if (NS_FAILED(rv)) {
            // XXX ErrorReport
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

    return ((mSorting & kAscending) ? 1 : -1) * ((mSorting & kUpperFirst) ? 1 : -1) * result;
#endif
}

#ifndef TX_EXE
nsresult txResultStringComparator::createRawSortKey(const nsCollationStrength aStrength,
                                                    const nsString& aString,
                                                    PRUint8** aKey,
                                                    PRUint32* aLength)
{
    nsresult rv = mCollation->GetSortKeyLen(aStrength, aString, aLength);
    *aKey = (PRUint8*)PR_MALLOC(*aLength);

    if (!*aKey)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = mCollation->CreateRawSortKey(aStrength, aString, *aKey, aLength);
    return rv;
}

txResultStringComparator::StringValue::StringValue() : mKey(0),
                                                       mLength(0),
                                                       mCaseKey(0),
                                                       mCaseLength(-1)
{
}

txResultStringComparator::StringValue::~StringValue()
{
    PR_Free(mKey);
    if (mCaseLength >= 0)
        PR_Free((PRUint8*)mCaseKey);
    else
        delete (String*)mCaseKey;
}
#endif

txResultNumberComparator::txResultNumberComparator(MBool aAscending)
{
    mAscending = aAscending ? 1 : -1;
}

TxObject* txResultNumberComparator::createSortableValue(ExprResult* aExprRes)
{
    NumberValue* numval = new NumberValue;
    if (numval)
        numval->mVal = aExprRes->numberValue();
    return numval;
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
