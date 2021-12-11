/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"
#include "txIXPathContext.h"
#include "txNodeSet.h"

#ifdef TX_TO_STRING
#  include "nsReadableUtils.h"
#endif

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
nsresult UnionExpr::evaluate(txIEvalContext* aContext,
                             txAExprResult** aResult) {
  *aResult = nullptr;
  RefPtr<txNodeSet> nodes;
  nsresult rv = aContext->recycler()->getNodeSet(getter_AddRefs(nodes));
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t i, len = mExpressions.Length();
  for (i = 0; i < len; ++i) {
    RefPtr<txAExprResult> exprResult;
    rv = mExpressions[i]->evaluate(aContext, getter_AddRefs(exprResult));
    NS_ENSURE_SUCCESS(rv, rv);

    if (exprResult->getResultType() != txAExprResult::NODESET) {
      // XXX ErrorReport: report nonnodeset error
      return NS_ERROR_XSLT_NODESET_EXPECTED;
    }

    RefPtr<txNodeSet> resultSet, ownedSet;
    resultSet =
        static_cast<txNodeSet*>(static_cast<txAExprResult*>(exprResult));
    exprResult = nullptr;
    rv = aContext->recycler()->getNonSharedNodeSet(resultSet,
                                                   getter_AddRefs(ownedSet));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = nodes->addAndTransfer(ownedSet);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aResult = nodes;
  NS_ADDREF(*aResult);

  return NS_OK;
}  //-- evaluate

Expr::ExprType UnionExpr::getType() { return UNION_EXPR; }

TX_IMPL_EXPR_STUBS_LIST(UnionExpr, NODESET_RESULT, mExpressions)

bool UnionExpr::isSensitiveTo(ContextSensitivity aContext) {
  uint32_t i, len = mExpressions.Length();
  for (i = 0; i < len; ++i) {
    if (mExpressions[i]->isSensitiveTo(aContext)) {
      return true;
    }
  }

  return false;
}

#ifdef TX_TO_STRING
void UnionExpr::toString(nsAString& aDest) {
  StringJoinAppend(aDest, u" | "_ns, mExpressions,
                   [](nsAString& dest, Expr* expr) { expr->toString(dest); });
}
#endif
