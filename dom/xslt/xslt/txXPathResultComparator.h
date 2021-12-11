/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_XPATHRESULTCOMPARATOR_H
#define TRANSFRMX_XPATHRESULTCOMPARATOR_H

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/intl/Collator.h"
#include "mozilla/UniquePtr.h"
#include "txCore.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class Expr;
class txIEvalContext;

/*
 * Result comparators
 */
class txXPathResultComparator {
 public:
  virtual ~txXPathResultComparator() = default;

  /*
   * Compares two XPath results. Returns -1 if val1 < val2,
   * 1 if val1 > val2 and 0 if val1 == val2.
   */
  virtual int compareValues(txObject* val1, txObject* val2) = 0;

  /*
   * Create a sortable value.
   */
  virtual nsresult createSortableValue(Expr* aExpr, txIEvalContext* aContext,
                                       txObject*& aResult) = 0;
};

/*
 * Compare results as stings (data-type="text")
 */
class txResultStringComparator : public txXPathResultComparator {
 public:
  txResultStringComparator(bool aAscending, bool aUpperFirst,
                           const nsString& aLanguage);

  int compareValues(txObject* aVal1, txObject* aVal2) override;
  nsresult createSortableValue(Expr* aExpr, txIEvalContext* aContext,
                               txObject*& aResult) override;

 private:
  mozilla::UniquePtr<const mozilla::intl::Collator> mCollator;
  nsresult init(const nsString& aLanguage);
  int mSorting;

  class StringValue : public txObject {
   public:
    StringValue();
    ~StringValue();

    nsresult initCaseKey(const mozilla::intl::Collator& aCollator);

    nsTArray<uint8_t> mKey;
    // Either mCaseKeyString is non-null, or we have a usable key in mCaseKey
    // already.
    mozilla::UniquePtr<nsString> mCaseKeyString;
    nsTArray<uint8_t> mCaseKey;
  };
};

/*
 * Compare results as numbers (data-type="number")
 */
class txResultNumberComparator : public txXPathResultComparator {
 public:
  explicit txResultNumberComparator(bool aAscending);

  int compareValues(txObject* aVal1, txObject* aVal2) override;
  nsresult createSortableValue(Expr* aExpr, txIEvalContext* aContext,
                               txObject*& aResult) override;

 private:
  int mAscending;

  class NumberValue : public txObject {
   public:
    double mVal;
  };
};

#endif
