/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"
#include "txIXPathContext.h"

/*
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation.
 * @return the result of the evaluation.
 */
nsresult
UnaryExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nullptr;

    RefPtr<txAExprResult> exprRes;
    nsresult rv = expr->evaluate(aContext, getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    double value = exprRes->numberValue();
#ifdef HPUX
    /*
     * Negation of a zero doesn't produce a negative
     * zero on HPUX. Perform the operation by multiplying with
     * -1.
     */
    return aContext->recycler()->getNumberResult(-1 * value, aResult);
#else
    return aContext->recycler()->getNumberResult(-value, aResult);
#endif
}

TX_IMPL_EXPR_STUBS_1(UnaryExpr, NODESET_RESULT, expr)

bool
UnaryExpr::isSensitiveTo(ContextSensitivity aContext)
{
    return expr->isSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
void
UnaryExpr::toString(nsAString& str)
{
    if (!expr)
        return;
    str.Append(char16_t('-'));
    expr->toString(str);
}
#endif
