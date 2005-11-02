/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 */

#include "Expr.h"
#include "ExprResult.h"
#include "nsIAtom.h"
#include "txIXPathContext.h"

/**
 * This class represents a FunctionCall as defined by the XSL Working Draft
**/

const nsString FunctionCall::INVALID_PARAM_COUNT(
        NS_LITERAL_STRING("invalid number of parameters for function: "));

const nsString FunctionCall::INVALID_PARAM_VALUE(
        NS_LITERAL_STRING("invalid parameter value for function: "));

FunctionCall::FunctionCall()
{
}

/**
 * Destructor
**/
FunctionCall::~FunctionCall()
{
    txListIterator iter(&params);
    while (iter.hasNext()) {
        delete (Expr*)iter.next();
    }
} //-- ~FunctionCall

  //------------------/
 //- Public Methods -/
//------------------/

/**
 * Adds the given parameter to this FunctionCall's parameter list
 * @param expr the Expr to add to this FunctionCall's parameter list
**/
nsresult FunctionCall::addParam(Expr* aExpr)
{
    if (aExpr)
        params.add(aExpr);
    return NS_OK;
} //-- addParam

/*
 * Evaluates the given Expression and converts its result to a String.
 * The value is appended to the given destination String
 */
void FunctionCall::evaluateToString(Expr* aExpr, txIEvalContext* aContext,
                                    nsAString& aDest)
{
    NS_ASSERTION(aExpr, "missing expression");
    ExprResult* exprResult = aExpr->evaluate(aContext);
    if (!exprResult)
        return;

    exprResult->stringValue(aDest);
    delete exprResult;
}

/*
 * Evaluates the given Expression and converts its result to a number.
 */
double FunctionCall::evaluateToNumber(Expr* aExpr, txIEvalContext* aContext)
{
    NS_ASSERTION(aExpr, "missing expression");
    ExprResult* exprResult = aExpr->evaluate(aContext);
    if (!exprResult)
        return Double::NaN;

    double result = exprResult->numberValue();
    delete exprResult;
    return result;
}

/*
 * Evaluates the given Expression and converts its result to a boolean.
 */
MBool FunctionCall::evaluateToBoolean(Expr* aExpr, txIEvalContext* aContext)
{
    NS_ASSERTION(aExpr, "missing expression");
    ExprResult* exprResult = aExpr->evaluate(aContext);
    if (!exprResult)
        return MB_FALSE;

    MBool result = exprResult->booleanValue();
    delete exprResult;
    return result;
}

/*
 * Evaluates the given Expression and converts its result to a NodeSet.
 * If the result is not a NodeSet NULL is returned.
 */
NodeSet* FunctionCall::evaluateToNodeSet(Expr* aExpr, txIEvalContext* aContext)
{
    NS_ASSERTION(aExpr, "Missing expression to evaluate");
    ExprResult* exprResult = aExpr->evaluate(aContext);
    if (!exprResult)
        return 0;

    if (exprResult->getResultType() != ExprResult::NODESET) {
        aContext->receiveError(NS_LITERAL_STRING("NodeSet expected as argument"), NS_ERROR_XPATH_INVALID_ARG);
        delete exprResult;
        return 0;
    }

    return (NodeSet*)exprResult;
}

/**
 * Called to check number of parameters
**/
MBool FunctionCall::requireParams (int paramCountMin,
                                   int paramCountMax,
                                   txIEvalContext* aContext)
{
    int argc = params.getLength();
    if ((argc < paramCountMin) || (argc > paramCountMax)) {
        nsAutoString err(INVALID_PARAM_COUNT);
        toString(err);
        aContext->receiveError(err, NS_ERROR_XPATH_INVALID_ARG);
        return MB_FALSE;
    }
    return MB_TRUE;
} //-- requireParams

/**
 * Called to check number of parameters
**/
MBool FunctionCall::requireParams(int paramCountMin, txIEvalContext* aContext)
{
    int argc = params.getLength();
    if (argc < paramCountMin) {
        nsAutoString err(INVALID_PARAM_COUNT);
        toString(err);
        aContext->receiveError(err, NS_ERROR_XPATH_INVALID_ARG);
        return MB_FALSE;
    }
    return MB_TRUE;
} //-- requireParams

/**
 * Returns the String representation of this NodeExpr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this NodeExpr.
**/
void FunctionCall::toString(nsAString& aDest)
{
    nsCOMPtr<nsIAtom> functionNameAtom;
    nsAutoString functionName;
    if (NS_FAILED(getNameAtom(getter_AddRefs(functionNameAtom))) ||
        NS_FAILED(functionNameAtom->ToString(functionName))) {
        NS_ASSERTION(0, "Can't get function name.");
        return;
    }

    aDest.Append(functionName);
    aDest.Append(PRUnichar('('));
    txListIterator iter(&params);
    MBool addComma = MB_FALSE;
    while (iter.hasNext()) {
        if (addComma) {
            aDest.Append(PRUnichar(','));
        }
        addComma = MB_TRUE;
        Expr* expr = (Expr*)iter.next();
        expr->toString(aDest);
    }
    aDest.Append(PRUnichar(')'));
}
