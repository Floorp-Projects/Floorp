/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The program is provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 */

#include "MITREObject.h"
#include "String.h"
#include "baseutils.h"
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
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
 * <BR/>
 * <PRE>
 * Modifications:
 * 19990806: Larry Fitzpatrick
 *   - changed constant short result types to enum
 * </PRE>
*/

/**
 * Represents the result of an expression evaluation
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/
class ExprResult : public MITREObject {

public:

    //-- ResultTypes
    enum _ResultType {
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

    void setValue(MBool boolean);

    void setValue(const BooleanResult& boolResult);

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

    void setValue(double dbl);

    void setValue(const NumberResult& nbrResult);

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

    String& getValue();
    void setValue(const String& str);

    virtual short  getResultType();
    virtual void   stringValue(String& str);
    virtual MBool  booleanValue();
    virtual double numberValue();


private:
    String value;
};

#endif

