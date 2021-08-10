/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FloatingPoint.h"
#include "mozilla/intl/Collator.h"
#include "mozilla/intl/LocaleService.h"

#include "txXPathResultComparator.h"
#include "txExpr.h"
#include "nsComponentManagerUtils.h"
#include "txCore.h"

using namespace mozilla;
using Collator = mozilla::intl::Collator;

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
  auto result =
      aLanguage.IsEmpty()
          ? mozilla::intl::LocaleService::TryCreateComponent<Collator>()
          : Collator::TryCreate(NS_ConvertUTF16toUTF8(aLanguage).get());

  NS_ENSURE_TRUE(result.isOk(), NS_ERROR_FAILURE);
  auto collator = result.unwrap();

  // Sort in a case-insensitive way, where "base" letters are considered
  // equal, e.g: a = á, a = A, a ≠ b.
  auto optResult = collator->SetOptions(
      Collator::Options{.sensitivity = Collator::Sensitivity::Base});
  NS_ENSURE_TRUE(optResult.isOk(), NS_ERROR_FAILURE);

  mCollator = UniquePtr<const Collator>(collator.release());
  return NS_OK;
}

namespace mozilla::intl {

/**
 * mozilla::intl APIs require sizeable buffers. This class abstracts over
 * the nsTArray.
 */
class nsTArrayU8Buffer {
 public:
  using CharType = uint8_t;

  // Do not allow copy or move. Move could be added in the future if needed.
  nsTArrayU8Buffer(const nsTArrayU8Buffer&) = delete;
  nsTArrayU8Buffer& operator=(const nsTArrayU8Buffer&) = delete;

  explicit nsTArrayU8Buffer(nsTArray<CharType>& aArray) : mArray(aArray) {}

  /**
   * Ensures the buffer has enough space to accommodate |size| elements.
   */
  [[nodiscard]] bool reserve(size_t size) {
    mArray.SetCapacity(size);
    // nsTArray::SetCapacity returns void, return true to keep the API the same
    // as the other Buffer implementations.
    return true;
  }

  /**
   * Returns the raw data inside the buffer.
   */
  CharType* data() { return mArray.Elements(); }

  /**
   * Returns the count of elements written into the buffer.
   */
  size_t length() const { return mArray.Length(); }

  /**
   * Returns the buffer's overall capacity.
   */
  size_t capacity() const { return mArray.Capacity(); }

  /**
   * Resizes the buffer to the given amount of written elements.
   */
  void written(size_t amount) {
    MOZ_ASSERT(amount <= mArray.Capacity());
    // This sets |mArray|'s internal size so that it matches how much was
    // written. This is necessary because the write happens across FFI
    // boundaries.
    mArray.SetLengthAndRetainStorage(amount);
  }

 private:
  nsTArray<CharType>& mArray;
};
}  // namespace mozilla::intl

nsresult txResultStringComparator::createSortableValue(Expr* aExpr,
                                                       txIEvalContext* aContext,
                                                       txObject*& aResult) {
  UniquePtr<StringValue> val(new StringValue);

  if (!mCollator) {
    return NS_ERROR_FAILURE;
  }

  val->mCaseKeyString = MakeUnique<nsString>();
  nsString& nsCaseKey = *val->mCaseKeyString;
  nsresult rv = aExpr->evaluateToString(aContext, nsCaseKey);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nsCaseKey.IsEmpty()) {
    aResult = val.release();

    return NS_OK;
  }

  mozilla::intl::nsTArrayU8Buffer buffer(val->mKey);
  auto result = mCollator->GetSortKey(nsCaseKey, buffer);
  NS_ENSURE_TRUE(result.isOk(), NS_ERROR_FAILURE);

  aResult = val.release();

  return NS_OK;
}

int txResultStringComparator::compareValues(txObject* aVal1, txObject* aVal2) {
  StringValue* strval1 = (StringValue*)aVal1;
  StringValue* strval2 = (StringValue*)aVal2;

  if (!mCollator) {
    return -1;
  }

  if (strval1->mKey.Length() == 0) {
    if (strval2->mKey.Length() == 0) {
      return 0;
    }
    return ((mSorting & kAscending) ? -1 : 1);
  }

  if (strval2->mKey.Length() == 0) {
    return ((mSorting & kAscending) ? 1 : -1);
  }

  nsresult rv;
  int32_t result = mCollator->CompareSortKeys(strval1->mKey, strval2->mKey);

  if (result != 0) {
    return ((mSorting & kAscending) ? 1 : -1) * result;
  }

  if (strval1->mCaseKeyString && strval1->mKey.Length() != 0) {
    rv = strval1->initCaseKey(*mCollator);
    if (NS_FAILED(rv)) {
      // XXX ErrorReport
      return -1;
    }
  }
  if (strval2->mCaseKeyString && strval2->mKey.Length() != 0) {
    rv = strval2->initCaseKey(*mCollator);
    if (NS_FAILED(rv)) {
      // XXX ErrorReport
      return -1;
    }
  }
  result = mCollator->CompareSortKeys(strval1->mCaseKey, strval2->mCaseKey);

  return ((mSorting & kAscending) ? 1 : -1) *
         ((mSorting & kUpperFirst) ? -1 : 1) * result;
}

txResultStringComparator::StringValue::StringValue() = default;

txResultStringComparator::StringValue::~StringValue() = default;

nsresult txResultStringComparator::StringValue::initCaseKey(
    const mozilla::intl::Collator& aCollator) {
  mozilla::intl::nsTArrayU8Buffer buffer(mCaseKey);

  auto result = aCollator.GetSortKey(*mCaseKeyString, buffer);
  if (result.isErr()) {
    mCaseKey.SetLength(0);
    return NS_ERROR_FAILURE;
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
