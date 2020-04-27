/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FloatingPoint.h"

#include "txXPathResultComparator.h"
#include "txExpr.h"
#include "nsComponentManagerUtils.h"
#include "txCore.h"
#include "nsCollationCID.h"

using namespace mozilla;

#define kAscending (1 << 0)
#define kUpperFirst (1 << 1)

txResultStringComparator::txResultStringComparator(bool aAscending,
                                                   bool aUpperFirst,
                                                   const nsString& aLanguage) {
  mSorting = 0;
  if (aAscending) mSorting |= kAscending;
  if (aUpperFirst) mSorting |= kUpperFirst;
  nsresult rv = init(aLanguage);
  if (NS_FAILED(rv)) NS_ERROR("Failed to initialize txResultStringComparator");
}

nsresult txResultStringComparator::init(const nsString& aLanguage) {
  nsresult rv;

  nsCOMPtr<nsICollationFactory> colFactory =
      do_CreateInstance(NS_COLLATIONFACTORY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aLanguage.IsEmpty()) {
    rv = colFactory->CreateCollation(getter_AddRefs(mCollation));
  } else {
    rv = colFactory->CreateCollationForLocale(NS_ConvertUTF16toUTF8(aLanguage),
                                              getter_AddRefs(mCollation));
  }

  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult txResultStringComparator::createSortableValue(Expr* aExpr,
                                                       txIEvalContext* aContext,
                                                       txObject*& aResult) {
  UniquePtr<StringValue> val(new StringValue);
  if (!val) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!mCollation) return NS_ERROR_FAILURE;

  val->mCaseKeyString = MakeUnique<nsString>();
  nsString& nsCaseKey = *val->mCaseKeyString;
  nsresult rv = aExpr->evaluateToString(aContext, nsCaseKey);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nsCaseKey.IsEmpty()) {
    aResult = val.release();

    return NS_OK;
  }

  rv = mCollation->AllocateRawSortKey(nsICollation::kCollationCaseInSensitive,
                                      nsCaseKey, val->mKey);
  NS_ENSURE_SUCCESS(rv, rv);

  aResult = val.release();

  return NS_OK;
}

int txResultStringComparator::compareValues(txObject* aVal1, txObject* aVal2) {
  StringValue* strval1 = (StringValue*)aVal1;
  StringValue* strval2 = (StringValue*)aVal2;

  if (!mCollation) return -1;

  if (strval1->mKey.Length() == 0) {
    if (strval2->mKey.Length() == 0) return 0;
    return ((mSorting & kAscending) ? -1 : 1);
  }

  if (strval2->mKey.Length() == 0) return ((mSorting & kAscending) ? 1 : -1);

  nsresult rv;
  int32_t result = -1;
  rv = mCollation->CompareRawSortKey(strval1->mKey, strval2->mKey, &result);
  if (NS_FAILED(rv)) {
    // XXX ErrorReport
    return -1;
  }

  if (result != 0) return ((mSorting & kAscending) ? 1 : -1) * result;

  if (strval1->mCaseKeyString && strval1->mKey.Length() != 0) {
    rv = strval1->initCaseKey(mCollation);
    if (NS_FAILED(rv)) {
      // XXX ErrorReport
      return -1;
    }
  }
  if (strval2->mCaseKeyString && strval2->mKey.Length() != 0) {
    rv = strval2->initCaseKey(mCollation);
    if (NS_FAILED(rv)) {
      // XXX ErrorReport
      return -1;
    }
  }
  rv = mCollation->CompareRawSortKey(strval1->mCaseKey, strval2->mCaseKey,
                                     &result);
  if (NS_FAILED(rv)) {
    // XXX ErrorReport
    return -1;
  }

  return ((mSorting & kAscending) ? 1 : -1) *
         ((mSorting & kUpperFirst) ? -1 : 1) * result;
}

txResultStringComparator::StringValue::StringValue() = default;

txResultStringComparator::StringValue::~StringValue() = default;

nsresult txResultStringComparator::StringValue::initCaseKey(
    nsICollation* aCollation) {
  nsresult rv = aCollation->AllocateRawSortKey(
      nsICollation::kCollationCaseSensitive, *mCaseKeyString, mCaseKey);
  if (NS_FAILED(rv)) {
    mCaseKey.SetLength(0);
    return rv;
  }

  mCaseKeyString = nullptr;
  return NS_OK;
}

txResultNumberComparator::txResultNumberComparator(bool aAscending) {
  mAscending = aAscending ? 1 : -1;
}

nsresult txResultNumberComparator::createSortableValue(Expr* aExpr,
                                                       txIEvalContext* aContext,
                                                       txObject*& aResult) {
  UniquePtr<NumberValue> numval(new NumberValue);
  if (!numval) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  RefPtr<txAExprResult> exprRes;
  nsresult rv = aExpr->evaluate(aContext, getter_AddRefs(exprRes));
  NS_ENSURE_SUCCESS(rv, rv);

  numval->mVal = exprRes->numberValue();

  aResult = numval.release();

  return NS_OK;
}

int txResultNumberComparator::compareValues(txObject* aVal1, txObject* aVal2) {
  double dval1 = ((NumberValue*)aVal1)->mVal;
  double dval2 = ((NumberValue*)aVal2)->mVal;

  if (mozilla::IsNaN(dval1)) return mozilla::IsNaN(dval2) ? 0 : -mAscending;

  if (mozilla::IsNaN(dval2)) return mAscending;

  if (dval1 == dval2) return 0;

  return (dval1 < dval2) ? -mAscending : mAscending;
}
