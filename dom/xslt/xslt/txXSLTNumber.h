/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TXXSLTNUMBER_H
#define TRANSFRMX_TXXSLTNUMBER_H

#include "nsError.h"
#include "txList.h"
#include "nsString.h"

class Expr;
class txPattern;
class txIEvalContext;
class txIMatchContext;
class txXPathTreeWalker;

class txXSLTNumber {
public:
    enum LevelType {
        eLevelSingle,
        eLevelMultiple,
        eLevelAny
    };

    static nsresult createNumber(Expr* aValueExpr, txPattern* aCountPattern,
                                 txPattern* aFromPattern, LevelType aLevel,
                                 Expr* aGroupSize, Expr* aGroupSeparator,
                                 Expr* aFormat, txIEvalContext* aContext,
                                 nsAString& aResult);

private:
    static nsresult getValueList(Expr* aValueExpr, txPattern* aCountPattern,
                                 txPattern* aFromPattern, LevelType aLevel,
                                 txIEvalContext* aContext, txList& aValues,
                                 nsAString& aValueString);

    static nsresult getCounters(Expr* aGroupSize, Expr* aGroupSeparator,
                                Expr* aFormat, txIEvalContext* aContext,
                                txList& aCounters, nsAString& aHead,
                                nsAString& aTail);

    /**
     * getSiblingCount uses aWalker to walk the siblings of aWalker's current
     * position.
     *
     */
    static nsresult getSiblingCount(txXPathTreeWalker& aWalker,
                                    txPattern* aCountPattern,
                                    txIMatchContext* aContext,
                                    int32_t* aCount);

    static bool getPrevInDocumentOrder(txXPathTreeWalker& aWalker);

    static bool isAlphaNumeric(char16_t ch);
};

class txFormattedCounter {
public:
    virtual ~txFormattedCounter()
    {
    }

    virtual void appendNumber(int32_t aNumber, nsAString& aDest) = 0;

    static nsresult getCounterFor(const nsString& aToken, int aGroupSize,
                                  const nsAString& aGroupSeparator,
                                  txFormattedCounter*& aCounter);

    nsString mSeparator;
};

#endif //TRANSFRMX_TXXSLTNUMBER_H
