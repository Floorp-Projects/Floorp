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
 * Marc Schefer, schefer@xo3.com
 *   -- fixed a bug regarding Or expressions
 *      - If the left hand was false, the right hand expression
 *        was not getting checked.
 *
 */


/**
 * Represents a BooleanExpr, a binary expression that
 * performs a boolean operation between it's lvalue and rvalue.
**/

#include "Expr.h"
#include "ExprResult.h"
#include "txIXPathContext.h"

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
nsresult
BooleanExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nsnull;

    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = leftExpr->evaluate(aContext, getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);
    
    PRBool lval = exprRes->booleanValue();

    // check for early decision
    if (op == OR && lval) {
        aContext->recycler()->getBoolResult(PR_TRUE, aResult);
        
        return NS_OK;
    }
    if (op == AND && !lval) {
        aContext->recycler()->getBoolResult(PR_FALSE, aResult);

        return NS_OK;
    }

    rv = rightExpr->evaluate(aContext, getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    // just use rval, since we already checked lval
    aContext->recycler()->getBoolResult(exprRes->booleanValue(), aResult);

    return NS_OK;
} //-- evaluate

/**
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
**/
void BooleanExpr::toString(nsAString& str) {

    if ( leftExpr ) leftExpr->toString(str);
    else str.Append(NS_LITERAL_STRING("null"));

    switch ( op ) {
        case OR:
            str.Append(NS_LITERAL_STRING(" or "));
            break;
        default:
            str.Append(NS_LITERAL_STRING(" and "));
            break;
    }
    if ( rightExpr ) rightExpr->toString(str);
    else str.Append(NS_LITERAL_STRING("null"));

} //-- toString

