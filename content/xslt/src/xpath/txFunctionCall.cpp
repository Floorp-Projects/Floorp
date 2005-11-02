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
 * $Id: txFunctionCall.cpp,v 1.2 2005/11/02 07:33:39 kvisco%ziplink.net Exp $
 */

#include "Expr.h"

/**
 * This class represents a FunctionCall as defined by the XSL Working Draft
 * @author <A HREF="mailto:kvisco@ziplink">Keith Visco</A>
 * @version $Revision: 1.2 $ $Date: 2005/11/02 07:33:39 $
**/

const String FunctionCall::INVALID_PARAM_COUNT =
        "invalid number of parameters for function: ";

//- Constructors -/

/**
 * Creates a new FunctionCall
**/
FunctionCall::FunctionCall() {
    this->name = "void";
} //-- FunctionCall

/**
 * Creates a new FunctionCall with the given function
 * Note: The object references in parameters will be deleted when this
 * FunctionCall gets destroyed.
**/
FunctionCall::FunctionCall(const String& name) {
    //-- copy name
    this->name = name;
} //-- FunctionCall

/**
 * Creates a new FunctionCall with the given function name and parameter list
 * Note: The object references in parameters will be deleted when this
 * FunctionCall gets destroyed.
**/
FunctionCall::FunctionCall(const String& name, List* parameters) {
    //-- copy name
    this->name = name;

    if (parameters) {
       ListIterator* pIter = parameters->iterator();
       while ( pIter->hasNext() ) {
           params.add(pIter->next());
       }
       delete pIter;
    }

} //-- FunctionCall


/**
 * Destructor
**/
FunctionCall::~FunctionCall() {

    ListIterator* iter = params.iterator();
    while ( iter->hasNext() ) {
        iter->next();
        Expr* expr = (Expr*) iter->remove();
        delete expr;
    }
    delete iter;
} //-- ~FunctionCall

  //------------------/
 //- Public Methods -/
//------------------/

/**
 * Adds the given parameter to this FunctionCall's parameter list
 * @param expr the Expr to add to this FunctionCall's parameter list
**/
void FunctionCall::addParam(Expr* expr) {
    if ( expr ) params.add(expr);
} //-- addParam

/**
 * Evaluates the given Expression and converts it's result to a String.
 * The value is appended to the given destination String
**/
void FunctionCall::evaluateToString
    (Expr* expr, Node* context, ContextState* cs, String& dest)
{
    if (!expr) return;
    ExprResult* exprResult = expr->evaluate(context, cs);
    exprResult->stringValue(dest);
    delete exprResult;
} //-- evaluateToString

/**
 * Evaluates the given Expression and converts it's result to a number.
**/
double FunctionCall::evaluateToNumber
    (Expr* expr, Node* context, ContextState* cs)
{
    double result = Double::NaN;
    if (!expr) return result;
    ExprResult* exprResult = expr->evaluate(context, cs);
    result =  exprResult->numberValue();
    delete exprResult;
    return result;
} //-- evaluateToNumber

/**
 * Returns the name of this FunctionCall
 * @return the name of this FunctionCall
**/
const String& FunctionCall::getName() {
    return (const String&) this->name;
} //-- getName

/**
 * Called to check number of parameters
**/
MBool FunctionCall::requireParams
    (int paramCountMin, int paramCountMax, ContextState* cs)
{

    int argc = params.getLength();
    if (( argc < paramCountMin) || (argc > paramCountMax)) {
        String err(INVALID_PARAM_COUNT);
        toString(err);
        cs->recieveError(err);
        return MB_FALSE;
    }
    return MB_TRUE;
} //-- requireParams

/**
 * Called to check number of parameters
**/
MBool FunctionCall::requireParams(int paramCountMin, ContextState* cs) {
    int argc = params.getLength();
    if (argc < paramCountMin) {
        String err(INVALID_PARAM_COUNT);
        toString(err);
        cs->recieveError(err);
        return MB_FALSE;
    }
    return MB_TRUE;
} //-- requireParams

/**
 * Sets the function name of this FunctionCall
 * @param name the name of this Function
**/
void FunctionCall::setName(const String& name) {
    this->name.clear();
    this->name.append(name);
} //-- setName

/**
 * Returns the String representation of this NodeExpr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this NodeExpr.
**/
void FunctionCall::toString(String& dest) {
    dest.append(this->name);
    dest.append('(');
    //-- add parameters
    ListIterator* iterator = params.iterator();
    int argc = 0;
    while ( iterator->hasNext() ) {
        if ( argc > 0 ) dest.append(',');
        Expr* expr = (Expr*)iterator->next();
        expr->toString(dest);
        ++argc;

    }
    delete iterator;
    dest.append(')');
} //-- toString

