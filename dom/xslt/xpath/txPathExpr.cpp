/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"
#include "txNodeSet.h"
#include "txNodeSetContext.h"
#include "txSingleNodeContext.h"
#include "txXMLUtils.h"
#include "txXPathTreeWalker.h"

using mozilla::Unused;
using mozilla::WrapUnique;

//------------/
//- PathExpr -/
//------------/

/**
 * Adds the Expr to this PathExpr
 * @param expr the Expr to add to this PathExpr
 **/
void PathExpr::addExpr(Expr* aExpr, PathOperator aPathOp) {
  NS_ASSERTION(!mItems.IsEmpty() || aPathOp == RELATIVE_OP,
               "First step has to be relative in PathExpr");
  PathExprItem* pxi = mItems.AppendElement();
  pxi->expr = WrapUnique(aExpr);
  pxi->pathOp = aPathOp;
}

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
nsresult PathExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult) {
  *aResult = nullptr;

  // We need to evaluate the first step with the current context since it
  // can depend on the context size and position. For example:
  // key('books', concat('book', position()))
  RefPtr<txAExprResult> res;
  nsresult rv = mItems[0].expr->evaluate(aContext, getter_AddRefs(res));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(res->getResultType() == txAExprResult::NODESET,
                 NS_ERROR_XSLT_NODESET_EXPECTED);

  RefPtr<txNodeSet> nodes =
      static_cast<txNodeSet*>(static_cast<txAExprResult*>(res));
  if (nodes->isEmpty()) {
    res.forget(aResult);

    return NS_OK;
  }
  res = nullptr;  // To allow recycling

  // Evaluate remaining steps
  uint32_t i, len = mItems.Length();
  for (i = 1; i < len; ++i) {
    PathExprItem& pxi = mItems[i];
    RefPtr<txNodeSet> tmpNodes;
    txNodeSetContext eContext(nodes, aContext);
    while (eContext.hasNext()) {
      eContext.next();

      RefPtr<txNodeSet> resNodes;
      if (pxi.pathOp == DESCENDANT_OP) {
        rv = aContext->recycler()->getNodeSet(getter_AddRefs(resNodes));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = evalDescendants(pxi.expr.get(), eContext.getContextNode(),
                             &eContext, resNodes);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        RefPtr<txAExprResult> res;
        rv = pxi.expr->evaluate(&eContext, getter_AddRefs(res));
        NS_ENSURE_SUCCESS(rv, rv);

        if (res->getResultType() != txAExprResult::NODESET) {
          // XXX ErrorReport: report nonnodeset error
          return NS_ERROR_XSLT_NODESET_EXPECTED;
        }
        resNodes = static_cast<txNodeSet*>(static_cast<txAExprResult*>(res));
      }

      if (tmpNodes) {
        if (!resNodes->isEmpty()) {
          RefPtr<txNodeSet> oldSet;
          oldSet.swap(tmpNodes);
          rv = aContext->recycler()->getNonSharedNodeSet(
              oldSet, getter_AddRefs(tmpNodes));
          NS_ENSURE_SUCCESS(rv, rv);

          oldSet.swap(resNodes);
          rv = aContext->recycler()->getNonSharedNodeSet(
              oldSet, getter_AddRefs(resNodes));
          NS_ENSURE_SUCCESS(rv, rv);

          tmpNodes->addAndTransfer(resNodes);
        }
      } else {
        tmpNodes = resNodes;
      }
    }
    nodes = tmpNodes;
    if (nodes->isEmpty()) {
      break;
    }
  }

  *aResult = nodes;
  NS_ADDREF(*aResult);

  return NS_OK;
}  //-- evaluate

/**
 * Selects from the descendants of the context node
 * all nodes that match the Expr
 **/
nsresult PathExpr::evalDescendants(Expr* aStep, const txXPathNode& aNode,
                                   txIMatchContext* aContext,
                                   txNodeSet* resNodes) {
  txSingleNodeContext eContext(aNode, aContext);
  RefPtr<txAExprResult> res;
  nsresult rv = aStep->evaluate(&eContext, getter_AddRefs(res));
  NS_ENSURE_SUCCESS(rv, rv);

  if (res->getResultType() != txAExprResult::NODESET) {
    // XXX ErrorReport: report nonnodeset error
    return NS_ERROR_XSLT_NODESET_EXPECTED;
  }

  txNodeSet* oldSet = static_cast<txNodeSet*>(static_cast<txAExprResult*>(res));
  RefPtr<txNodeSet> newSet;
  rv =
      aContext->recycler()->getNonSharedNodeSet(oldSet, getter_AddRefs(newSet));
  NS_ENSURE_SUCCESS(rv, rv);

  resNodes->addAndTransfer(newSet);

  bool filterWS;
  rv = aContext->isStripSpaceAllowed(aNode, filterWS);
  NS_ENSURE_SUCCESS(rv, rv);

  txXPathTreeWalker walker(aNode);
  if (!walker.moveToFirstChild()) {
    return NS_OK;
  }

  do {
    const txXPathNode& node = walker.getCurrentPosition();
    if (!(filterWS && txXPathNodeUtils::isText(node) &&
          txXPathNodeUtils::isWhitespace(node))) {
      rv = evalDescendants(aStep, node, aContext, resNodes);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } while (walker.moveToNextSibling());

  return NS_OK;
}  //-- evalDescendants

Expr::ExprType PathExpr::getType() { return PATH_EXPR; }

TX_IMPL_EXPR_STUBS_BASE(PathExpr, NODESET_RESULT)

Expr* PathExpr::getSubExprAt(uint32_t aPos) {
  return aPos < mItems.Length() ? mItems[aPos].expr.get() : nullptr;
}
void PathExpr::setSubExprAt(uint32_t aPos, Expr* aExpr) {
  NS_ASSERTION(aPos < mItems.Length(), "setting bad subexpression index");
  Unused << mItems[aPos].expr.release();
  mItems[aPos].expr = WrapUnique(aExpr);
}

bool PathExpr::isSensitiveTo(ContextSensitivity aContext) {
  if (mItems[0].expr->isSensitiveTo(aContext)) {
    return true;
  }

  // We're creating a new node/nodeset so we can ignore those bits.
  Expr::ContextSensitivity context =
      aContext & ~(Expr::NODE_CONTEXT | Expr::NODESET_CONTEXT);
  if (context == NO_CONTEXT) {
    return false;
  }

  uint32_t i, len = mItems.Length();
  for (i = 0; i < len; ++i) {
    NS_ASSERTION(!mItems[i].expr->isSensitiveTo(Expr::NODESET_CONTEXT),
                 "Step cannot depend on nodeset-context");
    if (mItems[i].expr->isSensitiveTo(context)) {
      return true;
    }
  }

  return false;
}

#ifdef TX_TO_STRING
void PathExpr::toString(nsAString& dest) {
  if (!mItems.IsEmpty()) {
    NS_ASSERTION(mItems[0].pathOp == RELATIVE_OP,
                 "First step should be relative");
    mItems[0].expr->toString(dest);
  }

  uint32_t i, len = mItems.Length();
  for (i = 1; i < len; ++i) {
    switch (mItems[i].pathOp) {
      case DESCENDANT_OP:
        dest.AppendLiteral("//");
        break;
      case RELATIVE_OP:
        dest.Append(char16_t('/'));
        break;
    }
    mItems[i].expr->toString(dest);
  }
}
#endif
