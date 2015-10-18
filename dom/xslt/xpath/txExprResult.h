/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_EXPRRESULT_H
#define TRANSFRMX_EXPRRESULT_H

#include "nsString.h"
#include "nsAutoPtr.h"
#include "txCore.h"
#include "txResultRecycler.h"

/*
 * ExprResult
 *
 * Classes Represented:
 * BooleanResult, ExprResult, NumberResult, StringResult
 *
 * Note: for NodeSet, see NodeSet.h
*/

class txAExprResult
{
public:
    friend class txResultRecycler;

    // Update txLiteralExpr::getReturnType and sTypes in txEXSLTFunctions.cpp if
    // this enum is changed.
    enum ResultType {
        NODESET = 0,
        BOOLEAN,
        NUMBER,
        STRING,
        RESULT_TREE_FRAGMENT
    };

    explicit txAExprResult(txResultRecycler* aRecycler) : mRecycler(aRecycler)
    {
    }
    virtual ~txAExprResult()
    {
    }

    void AddRef()
    {
        ++mRefCnt;
        NS_LOG_ADDREF(this, mRefCnt, "txAExprResult", sizeof(*this));
    }
    
    void Release(); // Implemented in txResultRecycler.cpp

    /**
     * Returns the type of ExprResult represented
     * @return the type of ExprResult represented
    **/
    virtual short getResultType()      = 0;

    /**
     * Creates a String representation of this ExprResult
     * @param aResult the destination string to append the String
     *                representation to.
    **/
    virtual void stringValue(nsString& aResult) = 0;

    /**
     * Returns a pointer to the stringvalue if possible. Otherwise null is
     * returned.
     */
    virtual const nsString* stringValuePointer() = 0;

    /**
     * Converts this ExprResult to a Boolean (bool) value
     * @return the Boolean value
    **/
    virtual bool booleanValue()          = 0;

    /**
     * Converts this ExprResult to a Number (double) value
     * @return the Number value
    **/
    virtual double numberValue()          = 0;

private:
    nsAutoRefCnt mRefCnt;
    RefPtr<txResultRecycler> mRecycler;
};

#define TX_DECL_EXPRRESULT                                        \
    virtual short getResultType();                                \
    virtual void stringValue(nsString& aString);                  \
    virtual const nsString* stringValuePointer();                 \
    virtual bool booleanValue();                                \
    virtual double numberValue();                                 \


class BooleanResult : public txAExprResult {

public:
    explicit BooleanResult(bool aValue);

    TX_DECL_EXPRRESULT

private:
    bool value;
};

class NumberResult : public txAExprResult {

public:
    NumberResult(double aValue, txResultRecycler* aRecycler);

    TX_DECL_EXPRRESULT

    double value;

};


class StringResult : public txAExprResult {
public:
    explicit StringResult(txResultRecycler* aRecycler);
    StringResult(const nsAString& aValue, txResultRecycler* aRecycler);

    TX_DECL_EXPRRESULT

    nsString mValue;
};

#endif

