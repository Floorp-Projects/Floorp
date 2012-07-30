/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"
#include "txIXPathContext.h"
#include "txNodeSet.h"

  //-------------/
 //- UnionExpr -/
//-------------/

    //-----------------------------/
  //- Virtual methods from Expr -/
//-----------------------------/

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
nsresult
UnionExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nullptr;
    nsRefPtr<txNodeSet> nodes;
    nsresult rv = aContext->recycler()->getNodeSet(getter_AddRefs(nodes));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 i, len = mExpressions.Length();
    for (i = 0; i < len; ++i) {
        nsRefPtr<txAExprResult> exprResult;
        rv = mExpressions[i]->evaluate(aContext, getter_AddRefs(exprResult));
        NS_ENSURE_SUCCESS(rv, rv);

        if (exprResult->getResultType() != txAExprResult::NODESET) {
            //XXX ErrorReport: report nonnodeset error
            return NS_ERROR_XSLT_NODESET_EXPECTED;
        }

        nsRefPtr<txNodeSet> resultSet, ownedSet;
        resultSet = static_cast<txNodeSet*>
                               (static_cast<txAExprResult*>(exprResult));
        exprResult = nullptr;
        rv = aContext->recycler()->
            getNonSharedNodeSet(resultSet, getter_AddRefs(ownedSet));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = nodes->addAndTransfer(ownedSet);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    *aResult = nodes;
    NS_ADDREF(*aResult);

    return NS_OK;
} //-- evaluate

Expr::ExprType
UnionExpr::getType()
{
  return UNION_EXPR;
}

TX_IMPL_EXPR_STUBS_LIST(UnionExpr, NODESET_RESULT, mExpressions)

bool
UnionExpr::isSensitiveTo(ContextSensitivity aContext)
{
    PRUint32 i, len = mExpressions.Length();
    for (i = 0; i < len; ++i) {
        if (mExpressions[i]->isSensitiveTo(aContext)) {
            return true;
        }
    }

    return false;
}

#ifdef TX_TO_STRING
void
UnionExpr::toString(nsAString& dest)
{
    PRUint32 i;
    for (i = 0; i < mExpressions.Length(); ++i) {
        if (i > 0)
            dest.AppendLiteral(" | ");
        mExpressions[i]->toString(dest);
    }
}
#endif
