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
 * $Id: txExprResult.h,v 1.5 2005/11/02 07:33:47 Peter.VanderBeken%pandora.be Exp $
 */

#include "MITREObject.h"
#include "dom.h"
#include "primitives.h"

#ifndef MITREXSL_EXPRRESULT_H
#define MITREXSL_EXPRRESULT_H


/*
 * ExprResult
 * <BR />
 * Classes Represented:
 * BooleanResult, ExprResult, NumberResult, StringResult
 * <BR/>
 * Note: for NodeSet, see NodeSet.h <BR />
 * @author <A HREF="mailto:kvisco@ziplink.net">Keith Visco</A>
 * @version $Revision: 1.5 $ $Date: 2005/11/02 07:33:47 $
*/

class ExprResult : public MITREObject {

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
    virtual void stringValue(DOMString& str) = 0;

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

    void setValue(MBool boolean);

    void setValue(const BooleanResult& boolResult);

    virtual short  getResultType();
    virtual void   stringValue(DOMString& str);
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

    void setValue(double dbl);

    void setValue(const NumberResult& nbrResult);

    MBool isNaN() const;

    virtual short  getResultType();
    virtual void   stringValue(DOMString& str);
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

    String& getValue();
    void setValue(const String& str);

    virtual short  getResultType();
    virtual void   stringValue(DOMString& str);
    virtual MBool  booleanValue();
    virtual double numberValue();


private:
    String value;
};

#endif

