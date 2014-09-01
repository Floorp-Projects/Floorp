/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_XPATHRESULTCOMPARATOR_H
#define TRANSFRMX_XPATHRESULTCOMPARATOR_H

#include "mozilla/Attributes.h"
#include "txCore.h"
#include "nsCOMPtr.h"
#include "nsICollation.h"
#include "nsString.h"

class Expr;
class txIEvalContext;

/*
 * Result comparators
 */
class txXPathResultComparator
{
public:
    virtual ~txXPathResultComparator()
    {
    }

    /*
     * Compares two XPath results. Returns -1 if val1 < val2,
     * 1 if val1 > val2 and 0 if val1 == val2.
     */
    virtual int compareValues(txObject* val1, txObject* val2) = 0;
    
    /*
     * Create a sortable value.
     */
    virtual nsresult createSortableValue(Expr *aExpr, txIEvalContext *aContext,
                                         txObject *&aResult) = 0;
};

/*
 * Compare results as stings (data-type="text")
 */
class txResultStringComparator : public txXPathResultComparator
{
public:
    txResultStringComparator(bool aAscending, bool aUpperFirst,
                             const nsAFlatString& aLanguage);

    int compareValues(txObject* aVal1, txObject* aVal2) MOZ_OVERRIDE;
    nsresult createSortableValue(Expr *aExpr, txIEvalContext *aContext,
                                 txObject *&aResult) MOZ_OVERRIDE;
private:
    nsCOMPtr<nsICollation> mCollation;
    nsresult init(const nsAFlatString& aLanguage);
    nsresult createRawSortKey(const int32_t aStrength,
                              const nsString& aString,
                              uint8_t** aKey,
                              uint32_t* aLength);
    int mSorting;

    class StringValue : public txObject
    {
    public:
        StringValue();
        ~StringValue();

        uint8_t* mKey;
        void* mCaseKey;
        uint32_t mLength, mCaseLength;
    };
};

/*
 * Compare results as numbers (data-type="number")
 */
class txResultNumberComparator : public txXPathResultComparator
{
public:
    explicit txResultNumberComparator(bool aAscending);

    int compareValues(txObject* aVal1, txObject* aVal2) MOZ_OVERRIDE;
    nsresult createSortableValue(Expr *aExpr, txIEvalContext *aContext,
                                 txObject *&aResult) MOZ_OVERRIDE;

private:
    int mAscending;

    class NumberValue : public txObject
    {
    public:
        double mVal;
    };
};

#endif
