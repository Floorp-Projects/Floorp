/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"
#include "txNodeSet.h"
#include "txIXPathContext.h"

using mozilla::Unused;
using mozilla::WrapUnique;

//-- Implementation of FilterExpr --/

//-----------------------------/
//- Virtual methods from Expr -/
//-----------------------------/

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ProcessorState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
 * @see Expr
 **/
nsresult FilterExpr::evaluate(txIEvalContext* aContext,
                              txAExprResult** aResult) {
  *aResult = nullptr;

  RefPtr<txAExprResult> exprRes;
  nsresult rv = expr->evaluate(aContext, getter_AddRefs(exprRes));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(exprRes->getResultType() == txAExprResult::NODESET,
                 NS_ERROR_XSLT_NODESET_EXPECTED);

  RefPtr<txNodeSet> nodes =
      static_cast<txNodeSet*>(static_cast<txAExprResult*>(exprRes));
  // null out exprRes so that we can test for shared-ness
  exprRes = nullptr;

  RefPtr<txNodeSet> nonShared;
  rv = aContext->recycler()->getNonSharedNodeSet(nodes,
                                                 getter_AddRefs(nonShared));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = evaluatePredicates(nonShared, aContext);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = nonShared;
  NS_ADDREF(*aResult);

  return NS_OK;
}  //-- evaluate

TX_IMPL_EXPR_STUBS_BASE(FilterExpr, NODESET_RESULT)

Expr* FilterExpr::getSubExprAt(uint32_t aPos) {
  if (aPos == 0) {
    return expr.get();
  }
  return PredicateList::getSubExprAt(aPos - 1);
}

void FilterExpr::setSubExprAt(uint32_t aPos, Expr* aExpr) {
  if (aPos == 0) {
    Unused << expr.release();
    expr = WrapUnique(aExpr);
  } else {
    PredicateList::setSubExprAt(aPos - 1, aExpr);
  }
}

bool FilterExpr::isSensitiveTo(ContextSensitivity aContext) {
  return expr->isSensitiveTo(aContext) ||
         PredicateList::isSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
void FilterExpr::toString(nsAString& str) {
  if (expr)
    expr->toString(str);
  else
    str.AppendLiteral("null");
  PredicateList::toString(str);
}
#endif
