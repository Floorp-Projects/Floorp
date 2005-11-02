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
 * $Id: txExprResult.h,v 1.10 2005/11/02 07:33:52 nisheeth%netscape.com Exp $
 */

#ifndef TRANSFRMX_EXPRRESULT_H
#define TRANSFRMX_EXPRRESULT_H

#include "TxObject.h"
#include "primitives.h"

/*
 * ExprResult
 * <BR />
 * Classes Represented:
 * BooleanResult, ExprResult, NumberResult, StringResult
 * <BR/>
 * Note: for NodeSet, see NodeSet.h <BR />
*/

class ExprResult : public TxObject {

public:

    //-- ResultTypes
    enum ResultType {
        NODESET  = 1,
        STRING,
        BOOLEAN,
        TREE_FRAGMENT,
        NUMBER
    };

    virtual ~ExprResult() {};

    /**
     * Returns the type of ExprResult represented
     * @return the type of ExprResult represented
    **/
    virtual short getResultType()      = 0;

    /**
     * Creates a String representation of this ExprResult
     * @param str the destination string to append the String representation to.
    **/
    virtual void stringValue(String& str) = 0;

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

};

class BooleanResult : public ExprResult {

public:

    BooleanResult();
    BooleanResult(MBool boolean);
    BooleanResult(const BooleanResult& boolResult);

    MBool getValue() const;

    virtual short  getResultType();
    virtual void   stringValue(String& str);
    virtual MBool  booleanValue();
    virtual double numberValue();

private:
    MBool value;
};

class NumberResult : public ExprResult {

public:

    NumberResult();
    NumberResult(double dbl);
    NumberResult(const NumberResult& nbrResult);

    double getValue() const;
    MBool isNaN() const;

    virtual short  getResultType();
    virtual void   stringValue(String& str);
    virtual MBool  booleanValue();
    virtual double numberValue();

private:
    double value;

};


class StringResult : public ExprResult {

public:

    StringResult();
    StringResult(String& str);
    StringResult(const String& str);
    StringResult(const StringResult& strResult);
    StringResult(const char* str);

    String& getValue();

    virtual short  getResultType();
    virtual void   stringValue(String& str);
    virtual MBool  booleanValue();
    virtual double numberValue();


private:
    String value;
};

#endif

