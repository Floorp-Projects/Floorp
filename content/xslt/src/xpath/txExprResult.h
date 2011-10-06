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
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Keith Visco <kvisco@ziplink.net> (Original Author)
 *   Larry Fitzpatrick, OpenText <lef@opentext.com>
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

    txAExprResult(txResultRecycler* aRecycler) : mRecycler(aRecycler)
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
    virtual void stringValue(nsString& aString);                  \
    virtual const nsString* stringValuePointer();                 \
    virtual bool booleanValue();                                \
    virtual double numberValue();                                 \


class BooleanResult : public txAExprResult {

public:
    BooleanResult(MBool aValue);

    TX_DECL_EXPRRESULT

private:
    MBool value;
};

class NumberResult : public txAExprResult {

public:
    NumberResult(double aValue, txResultRecycler* aRecycler);

    TX_DECL_EXPRRESULT

    double value;

};


class StringResult : public txAExprResult {
public:
    StringResult(txResultRecycler* aRecycler);
    StringResult(const nsAString& aValue, txResultRecycler* aRecycler);

    TX_DECL_EXPRRESULT

    nsString mValue;
};

#endif

