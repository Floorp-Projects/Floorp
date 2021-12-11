/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  Implementation of an XPath LocationStep
*/

#include "txExpr.h"
#include "txIXPathContext.h"
#include "txNodeSet.h"
#include "txXPathTreeWalker.h"

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
nsresult LocationStep::evaluate(txIEvalContext* aContext,
                                txAExprResult** aResult) {
  NS_ASSERTION(aContext, "internal error");
  *aResult = nullptr;

  RefPtr<txNodeSet> nodes;
  nsresult rv = aContext->recycler()->getNodeSet(getter_AddRefs(nodes));
  NS_ENSURE_SUCCESS(rv, rv);

  txXPathTreeWalker walker(aContext->getContextNode());

  switch (mAxisIdentifier) {
    case ANCESTOR_AXIS: {
      if (!walker.moveToParent()) {
        break;
      }
      [[fallthrough]];
    }
    case ANCESTOR_OR_SELF_AXIS: {
      nodes->setReverse();

      do {
        rv = appendIfMatching(walker, aContext, nodes);
        NS_ENSURE_SUCCESS(rv, rv);
      } while (walker.moveToParent());

      break;
    }
    case ATTRIBUTE_AXIS: {
      if (!walker.moveToFirstAttribute()) {
        break;
      }

      do {
        rv = appendIfMatching(walker, aContext, nodes);
        NS_ENSURE_SUCCESS(rv, rv);
      } while (walker.moveToNextAttribute());
      break;
    }
    case DESCENDANT_OR_SELF_AXIS: {
      rv = appendIfMatching(walker, aContext, nodes);
      NS_ENSURE_SUCCESS(rv, rv);
      [[fallthrough]];
    }
    case DESCENDANT_AXIS: {
      rv = appendMatchingDescendants(walker, aContext, nodes);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
    case FOLLOWING_AXIS: {
      if (txXPathNodeUtils::isAttribute(walker.getCurrentPosition())) {
        walker.moveToParent();
        rv = appendMatchingDescendants(walker, aContext, nodes);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      bool cont = true;
      while (!walker.moveToNextSibling()) {
        if (!walker.moveToParent()) {
          cont = false;
          break;
        }
      }
      while (cont) {
        rv = appendIfMatching(walker, aContext, nodes);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = appendMatchingDescendants(walker, aContext, nodes);
        NS_ENSURE_SUCCESS(rv, rv);

        while (!walker.moveToNextSibling()) {
          if (!walker.moveToParent()) {
            cont = false;
            break;
          }
        }
      }
      break;
    }
    case FOLLOWING_SIBLING_AXIS: {
      while (walker.moveToNextSibling()) {
        rv = appendIfMatching(walker, aContext, nodes);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      break;
    }
    case NAMESPACE_AXIS:  //-- not yet implemented
#if 0
            // XXX DEBUG OUTPUT
            cout << "namespace axis not yet implemented"<<endl;
#endif
      break;
    case PARENT_AXIS: {
      if (walker.moveToParent()) {
        rv = appendIfMatching(walker, aContext, nodes);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      break;
    }
    case PRECEDING_AXIS: {
      nodes->setReverse();

      bool cont = true;
      while (!walker.moveToPreviousSibling()) {
        if (!walker.moveToParent()) {
          cont = false;
          break;
        }
      }
      while (cont) {
        rv = appendMatchingDescendantsRev(walker, aContext, nodes);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = appendIfMatching(walker, aContext, nodes);
        NS_ENSURE_SUCCESS(rv, rv);

        while (!walker.moveToPreviousSibling()) {
          if (!walker.moveToParent()) {
            cont = false;
            break;
          }
        }
      }
      break;
    }
    case PRECEDING_SIBLING_AXIS: {
      nodes->setReverse();

      while (walker.moveToPreviousSibling()) {
        rv = appendIfMatching(walker, aContext, nodes);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      break;
    }
    case SELF_AXIS: {
      rv = appendIfMatching(walker, aContext, nodes);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
    default:  // Children Axis
    {
      if (!walker.moveToFirstChild()) {
        break;
      }

      do {
        rv = appendIfMatching(walker, aContext, nodes);
        NS_ENSURE_SUCCESS(rv, rv);
      } while (walker.moveToNextSibling());
      break;
    }
  }

  // Apply predicates
  if (!isEmpty()) {
    rv = evaluatePredicates(nodes, aContext);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nodes->unsetReverse();

  NS_ADDREF(*aResult = nodes);

  return NS_OK;
}

nsresult LocationStep::appendIfMatching(const txXPathTreeWalker& aWalker,
                                        txIMatchContext* aContext,
                                        txNodeSet* aNodes) {
  bool matched;
  const txXPathNode& child = aWalker.getCurrentPosition();
  nsresult rv = mNodeTest->matches(child, aContext, matched);
  NS_ENSURE_SUCCESS(rv, rv);

  if (matched) {
    aNodes->append(child);
  }
  return NS_OK;
}

nsresult LocationStep::appendMatchingDescendants(
    const txXPathTreeWalker& aWalker, txIMatchContext* aContext,
    txNodeSet* aNodes) {
  txXPathTreeWalker walker(aWalker);
  if (!walker.moveToFirstChild()) {
    return NS_OK;
  }

  do {
    nsresult rv = appendIfMatching(walker, aContext, aNodes);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = appendMatchingDescendants(walker, aContext, aNodes);
    NS_ENSURE_SUCCESS(rv, rv);
  } while (walker.moveToNextSibling());

  return NS_OK;
}

nsresult LocationStep::appendMatchingDescendantsRev(
    const txXPathTreeWalker& aWalker, txIMatchContext* aContext,
    txNodeSet* aNodes) {
  txXPathTreeWalker walker(aWalker);
  if (!walker.moveToLastChild()) {
    return NS_OK;
  }

  do {
    nsresult rv = appendMatchingDescendantsRev(walker, aContext, aNodes);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = appendIfMatching(walker, aContext, aNodes);
    NS_ENSURE_SUCCESS(rv, rv);
  } while (walker.moveToPreviousSibling());

  return NS_OK;
}

Expr::ExprType LocationStep::getType() { return LOCATIONSTEP_EXPR; }

TX_IMPL_EXPR_STUBS_BASE(LocationStep, NODESET_RESULT)

Expr* LocationStep::getSubExprAt(uint32_t aPos) {
  return PredicateList::getSubExprAt(aPos);
}

void LocationStep::setSubExprAt(uint32_t aPos, Expr* aExpr) {
  PredicateList::setSubExprAt(aPos, aExpr);
}

bool LocationStep::isSensitiveTo(ContextSensitivity aContext) {
  return (aContext & NODE_CONTEXT) || mNodeTest->isSensitiveTo(aContext) ||
         PredicateList::isSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
void LocationStep::toString(nsAString& str) {
  switch (mAxisIdentifier) {
    case ANCESTOR_AXIS:
      str.AppendLiteral("ancestor::");
      break;
    case ANCESTOR_OR_SELF_AXIS:
      str.AppendLiteral("ancestor-or-self::");
      break;
    case ATTRIBUTE_AXIS:
      str.Append(char16_t('@'));
      break;
    case DESCENDANT_AXIS:
      str.AppendLiteral("descendant::");
      break;
    case DESCENDANT_OR_SELF_AXIS:
      str.AppendLiteral("descendant-or-self::");
      break;
    case FOLLOWING_AXIS:
      str.AppendLiteral("following::");
      break;
    case FOLLOWING_SIBLING_AXIS:
      str.AppendLiteral("following-sibling::");
      break;
    case NAMESPACE_AXIS:
      str.AppendLiteral("namespace::");
      break;
    case PARENT_AXIS:
      str.AppendLiteral("parent::");
      break;
    case PRECEDING_AXIS:
      str.AppendLiteral("preceding::");
      break;
    case PRECEDING_SIBLING_AXIS:
      str.AppendLiteral("preceding-sibling::");
      break;
    case SELF_AXIS:
      str.AppendLiteral("self::");
      break;
    default:
      break;
  }
  NS_ASSERTION(mNodeTest, "mNodeTest is null, that's verboten");
  mNodeTest->toString(str);

  PredicateList::toString(str);
}
#endif
