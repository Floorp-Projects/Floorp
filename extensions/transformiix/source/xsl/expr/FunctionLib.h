/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
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
        LOCAL_PART,     //-- local-part()
        NAMESPACE,      //-- namespace()
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

#endif
