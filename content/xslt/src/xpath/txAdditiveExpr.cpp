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
 */

/**
 * Represents a AdditiveExpr, an binary expression that
 * performs an additive operation between it's lvalue and rvalue:
 *  +   : addition
 *  -   : subtraction
**/

#include "Expr.h"
#include "ExprResult.h"
#include "primitives.h"
#include "txIXPathContext.h"

/**
 * Creates a new AdditiveExpr using the given operator
**/
AdditiveExpr::AdditiveExpr(Expr* leftExpr, Expr* rightExpr, short op) {
    this->op = op;
    this->leftExpr = leftExpr;
    this->rightExpr = rightExpr;
} //-- AdditiveExpr

AdditiveExpr::~AdditiveExpr() {
    delete leftExpr;
    delete rightExpr;
} //-- ~AdditiveExpr

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
nsresult
AdditiveExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nsnull;

    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = rightExpr->evaluate(aContext, getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    double rightDbl = exprRes->numberValue();

    rv = leftExpr->evaluate(aContext, getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    double leftDbl = exprRes->numberValue();

    double result = 0;
    switch ( op ) {
        case SUBTRACTION:
            result = leftDbl - rightDbl;
            break;
        default:
            result = leftDbl + rightDbl;
            break;
    }

    return aContext->recycler()->getNumberResult(result, aResult);
} //-- evaluate

/**
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
**/
void AdditiveExpr::toString(nsAString& str) {

    if ( leftExpr ) leftExpr->toString(str);
    else str.Append(NS_LITERAL_STRING("null"));

    switch ( op ) {
        case SUBTRACTION:
            str.Append(NS_LITERAL_STRING(" - "));
            break;
        default:
            str.Append(NS_LITERAL_STRING(" + "));
            break;
    }
    if ( rightExpr ) rightExpr->toString(str);
    else str.Append(NS_LITERAL_STRING("null"));

} //-- toString

