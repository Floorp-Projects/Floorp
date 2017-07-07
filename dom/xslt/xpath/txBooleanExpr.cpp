/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * Represents a BooleanExpr, a binary expression that
 * performs a boolean operation between its lvalue and rvalue.
**/

#include "txExpr.h"
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
    *aResult = nullptr;

    bool lval;
    nsresult rv = leftExpr->evaluateToBool(aContext, lval);
    NS_ENSURE_SUCCESS(rv, rv);

    // check for early decision
    if (op == OR && lval) {
        aContext->recycler()->getBoolResult(true, aResult);

        return NS_OK;
    }
    if (op == AND && !lval) {
        aContext->recycler()->getBoolResult(false, aResult);

        return NS_OK;
    }

    bool rval;
    rv = rightExpr->evaluateToBool(aContext, rval);
    NS_ENSURE_SUCCESS(rv, rv);

    // just use rval, since we already checked lval
    aContext->recycler()->getBoolResult(rval, aResult);

    return NS_OK;
} //-- evaluate

TX_IMPL_EXPR_STUBS_2(BooleanExpr, BOOLEAN_RESULT, leftExpr, rightExpr)

bool
BooleanExpr::isSensitiveTo(ContextSensitivity aContext)
{
    return leftExpr->isSensitiveTo(aContext) ||
           rightExpr->isSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
void
BooleanExpr::toString(nsAString& str)
{
    if ( leftExpr ) leftExpr->toString(str);
    else str.AppendLiteral("null");

    switch ( op ) {
        case OR:
            str.AppendLiteral(" or ");
            break;
        default:
            str.AppendLiteral(" and ");
            break;
    }
    if ( rightExpr ) rightExpr->toString(str);
    else str.AppendLiteral("null");

}
#endif
