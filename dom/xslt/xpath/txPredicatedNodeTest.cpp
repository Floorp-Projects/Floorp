/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"
#include "txExprResult.h"
#include "txSingleNodeContext.h"

txPredicatedNodeTest::txPredicatedNodeTest(txNodeTest* aNodeTest,
                                           Expr* aPredicate)
    : mNodeTest(aNodeTest), mPredicate(aPredicate) {
  NS_ASSERTION(!mPredicate->isSensitiveTo(Expr::NODESET_CONTEXT),
               "predicate must not be context-nodeset-sensitive");
}

nsresult txPredicatedNodeTest::matches(const txXPathNode& aNode,
                                       txIMatchContext* aContext,
                                       bool& aMatched) {
  nsresult rv = mNodeTest->matches(aNode, aContext, aMatched);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aMatched) {
    return NS_OK;
  }

  txSingleNodeContext context(aNode, aContext);
  RefPtr<txAExprResult> res;
  rv = mPredicate->evaluate(&context, getter_AddRefs(res));
  NS_ENSURE_SUCCESS(rv, rv);

  aMatched = res->booleanValue();
  return NS_OK;
}

double txPredicatedNodeTest::getDefaultPriority() { return 0.5; }

bool txPredicatedNodeTest::isSensitiveTo(Expr::ContextSensitivity aContext) {
  return mNodeTest->isSensitiveTo(aContext) ||
         mPredicate->isSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
void txPredicatedNodeTest::toString(nsAString& aDest) {
  mNodeTest->toString(aDest);
  aDest.Append(char16_t('['));
  mPredicate->toString(aDest);
  aDest.Append(char16_t(']'));
}
#endif
