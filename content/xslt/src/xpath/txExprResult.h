/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *   -- original author.
 * Larry Fitzpatrick, OpenText, lef@opentext.com
 *   -- changed constant short result types to enum
 *
 */

#ifndef TRANSFRMX_EXPRRESULT_H
#define TRANSFRMX_EXPRRESULT_H

#include "primitives.h"
#include "TxObject.h"
#include "nsString.h"
#include "txResultRecycler.h"
#include "nsAutoPtr.h"

/*
 * ExprResult
 *
 * Classes Represented:
 * BooleanResult, ExprResult, NumberResult, StringResult
 *
 * Note: for NodeSet, see NodeSet.h
*/

class txAExprResult : public TxObject
{
public:
    friend class txResultRecycler;
    enum ResultType {
        NODESET,
        BOOLEAN,
        NUMBER,
        STRING,
        RESULT_TREE_FRAGMENT
    };

    txAExprResult(txResultRecycler* aRecycler) : mRecycler(aRecycler) {}
    virtual ~txAExprResult() {};

    void AddRef()
    {
        ++mRefCnt;
    }
    void Release()
    {
        if (--mRefCnt == 0) {
            if (mRecycler) {
                mRecycler->recycle(this);
            }
            else {
                delete this;
            }
        }
    }

    /**
     * Returns the type of ExprResult represented
     * @return the type of ExprResult represented
    **/
    virtual short getResultType()      = 0;

    /**
     * Creates a String representation of this ExprResult
     * @param str the destination string to append the String representation to.
    **/
    virtual void stringValue(nsAString& str) = 0;

    /**
     * Returns a pointer to the stringvalue if possible. Otherwise null is
     * returned.
     */
    virtual nsAString* stringValuePointer() = 0;

    /**
     * Converts this ExprResult to a Boolean (MBool) value
     * @return the Boolean value
    **/
    virtual MBool booleanValue()          = 0;

    /**
     * Converts this ExprResult to a Number (double) value
     * @return the Number value
    **/
    virtual double numberValue()          = 0;

private:
    nsAutoRefCnt mRefCnt;
    nsRefPtr<txResultRecycler> mRecycler;
};

#define TX_DECL_EXPRRESULT                                        \
    virtual short getResultType();                                \
    virtual void stringValue(nsAString& str);                     \
    virtual nsAString* stringValuePointer();                      \
    virtual PRBool booleanValue();                                \
    virtual double numberValue();                                 \


class BooleanResult : public txAExprResult {

public:
    BooleanResult(); // Not to be implemented
    BooleanResult(MBool aValue);

    TX_DECL_EXPRRESULT

private:
    MBool value;
};

class NumberResult : public txAExprResult {

public:
    NumberResult(); // Not to be implemented
    NumberResult(double aValue, txResultRecycler* aRecycler);

    TX_DECL_EXPRRESULT

    double value;

};


class StringResult : public txAExprResult {
public:
    StringResult();
    StringResult(txResultRecycler* aRecycler);
    StringResult(const nsAString& aValue, txResultRecycler* aRecycler);

    TX_DECL_EXPRRESULT

    nsString mValue;
};

#endif

