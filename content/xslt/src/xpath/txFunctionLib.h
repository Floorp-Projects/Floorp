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
 *
 * Olivier Gerardin, ogerardin@vo.lu
 *   -- added number functions
 *    
 * $Id: txFunctionLib.h,v 1.1 2005/11/02 07:33:43 kvisco%ziplink.net Exp $
 */



#ifndef MITREXSL_FUNCTIONLIB_H
#define MITREXSL_FUNCTIONLIB_H

#include "String.h"
#include "primitives.h"
#include "NodeSet.h"
#include "List.h"
#include "dom.h"
#include "ExprResult.h"
#include "baseutils.h"
#include "Expr.h"
#include "Names.h"
#include "XMLUtils.h"
#include <math.h>

class XPathNames {

public:
//-- Function Names
static const String BOOLEAN_FN;
static const String CONCAT_FN;
static const String CONTAINS_FN;
static const String COUNT_FN ;
static const String FALSE_FN;
static const String LAST_FN;
static const String LOCAL_NAME_FN;
static const String NAME_FN;
static const String NAMESPACE_URI_FN;
static const String NOT_FN;
static const String POSITION_FN;
static const String STARTS_WITH_FN;
static const String STRING_FN;
static const String STRING_LENGTH_FN;
static const String SUBSTRING_FN;
static const String SUBSTRING_AFTER_FN;
static const String SUBSTRING_BEFORE_FN;
static const String TRANSLATE_FN;
static const String TRUE_FN;
// OG+
static const String NUMBER_FN;
static const String ROUND_FN;
static const String CEILING_FN;
static const String FLOOR_FN;
// OG-

//-- internal XSL processor functions
static const String ERROR_FN;


}; //-- XPathNames

/**
 * This class represents a FunctionCall as defined by the XSL
 * Working Draft.
 * This file was ported from XSL:P <BR />
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
 * <BR/>
 * <PRE>
 * Modifications:
 * 19990805: Keith Visco
 *   - added NodeSetFunctionCall
 *   - moved position() function into NodeSetFunctionCall
 *   - removed PositionFunctionCall
 * 19990806: Larry Fitzpatrick
 *   - changed constant short declarations for BooleanFunctionCall
 *     with enumerations
 * 19990806: Keith Visco
 *   - added StringFunctionCall
 *   - stated using Larry's enum suggestion instead of using static const shorts,
 *     as you can see, I am a Java developer! ;-)
 * </PRE>
**/
class FunctionCall : public Expr {

public:

    static const String INVALID_PARAM_COUNT;


    virtual ~FunctionCall();

    /**
     * Adds the given parameter to this FunctionCall's parameter list
     * @param expr the Expr to add to this FunctionCall's parameter list
    **/
    void addParam(Expr* expr);

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param ps the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs) = 0;

    /**
     * Returns the name of this FunctionCall
     * @return the name of this FunctionCall
    **/
    const String& getName();

    virtual MBool requireParams(int paramCountMin, ContextState* cs);

    virtual MBool requireParams(int paramCountMin,
                                int paramCountMax,
                                ContextState* cs);

    /**
     * Sets the function name of this FunctionCall
     * @param name the name of this Function
    **/
    void setName(const String& name);
    /**
     * Returns the String representation of this Pattern.
     * @param dest the String to use when creating the String
     * representation. The String representation will be appended to
     * any data in the destination String, to allow cascading calls to
     * other #toString() methods for Expressions.
     * @return the String representation of this Pattern.
    **/
    virtual void toString(String& dest);


protected:

    List params;

    FunctionCall();
    FunctionCall(const String& name);
    FunctionCall(const String& name, List* parameters);


    /**
     * Evaluates the given Expression and converts it's result to a String.
     * The value is appended to the given destination String
    **/
    void FunctionCall::evaluateToString
        (Expr* expr, Node* context, ContextState* cs, String& dest);

    /**
     * Evaluates the given Expression and converts it's result to a number.
    **/
    double evaluateToNumber(Expr* expr, Node* context, ContextState* cs);

private:

    String name;
}; //-- FunctionCall

/**
 * Represents the Set of boolean functions
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/
class BooleanFunctionCall : public FunctionCall {

public:

    enum _BooleanFunctions { BOOLEAN = 1, FALSE, NOT, TRUE };

    /**
     * Creates a default BooleanFunctionCall, which always evaluates to False
    **/
    BooleanFunctionCall();

    /**
     * Creates a BooleanFunctionCall of the given type
    **/
    BooleanFunctionCall(short type);

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param ps the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);

private:
    short type;
}; //-- BooleanFunctionCall

/**
 * Internal Function created when there is an Error during parsing
 * an Expression
**/
class ErrorFunctionCall : public FunctionCall {
public:

    ErrorFunctionCall();
    ErrorFunctionCall(const String& errorMsg);

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param ps the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);

    void setErrorMessage(String& errorMsg);

private:

    String errorMessage;

}; //-- ErrorFunctionCall

/**
 *  Represents the XPath NodeSet function calls
**/
class NodeSetFunctionCall : public FunctionCall {

public:

    enum _NodeSetFunctions {
        COUNT = 1,      //-- count()
        LAST,           //-- last()
        LOCAL_NAME,     //-- local-name()
        NAMESPACE_URI,  //-- namespace-uri()
        NAME,           //-- name()
        POSITION        //-- position()
    };

    /**
     * Creates a default NodeSetFunction call. Position function is the default.
    **/
    NodeSetFunctionCall();

    /**
     * Creates a NodeSetFunctionCall of the given type
    **/
    NodeSetFunctionCall(short type);

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param ps the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);

private:
    short type;
}; //-- NodeSetFunctionCall


/**
 * Represents the XPath String Function Calls
**/
class StringFunctionCall : public FunctionCall {

public:

    enum _StringFunctions {
        CONCAT = 1,            //-- concat()
        CONTAINS,              //-- contains()
        NORMALIZE,             //-- normalize()
        STARTS_WITH,           //-- starts-with()
        STRING,                //-- string()
        STRING_LENGTH,         //-- string-length()
        SUBSTRING,             //-- substring()
        SUBSTRING_AFTER,       //-- substring-after()
        SUBSTRING_BEFORE,      //-- substring-before()
        TRANSLATE              //-- translate()
    };

    /**
     * Creates a default String function. String() function is the default.
    **/
    StringFunctionCall();

    /**
     * Creates a String function of the given type
    **/
    StringFunctionCall(short type);

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param ps the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);

private:
    short type;
}; //-- StringFunctionCall


// OG+
/**
 * Represents the XPath Number Function Calls
**/
class NumberFunctionCall : public FunctionCall {

public:

    enum _NumberFunctions {
        NUMBER = 1,            //-- number()
	ROUND,                 //-- round()
	FLOOR,                 //-- floor()
	CEILING                //-- ceiling()
    };

    /**
     * Creates a default Number function. number() function is the default.
    **/
    NumberFunctionCall();

    /**
     * Creates a Number function of the given type
    **/
    NumberFunctionCall(short type);

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param ps the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);

private:
    short type;
}; //-- NumberFunctionCall
// OG-

#endif
