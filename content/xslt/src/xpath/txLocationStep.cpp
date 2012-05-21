/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
nsresult
LocationStep::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    NS_ASSERTION(aContext, "internal error");
    *aResult = nsnull;

    nsRefPtr<txNodeSet> nodes;
    nsresult rv = aContext->recycler()->getNodeSet(getter_AddRefs(nodes));
    NS_ENSURE_SUCCESS(rv, rv);

    txXPathTreeWalker walker(aContext->getContextNode());

    switch (mAxisIdentifier) {
        case ANCESTOR_AXIS:
        {
            if (!walker.moveToParent()) {
                break;
            }
            // do not break here
        }
        case ANCESTOR_OR_SELF_AXIS:
        {
            nodes->setReverse();

            do {
                if (mNodeTest->matches(walker.getCurrentPosition(), aContext)) {
                    nodes->append(walker.getCurrentPosition());
                }
            } while (walker.moveToParent());

            break;
        }
        case ATTRIBUTE_AXIS:
        {
            if (!walker.moveToFirstAttribute()) {
                break;
            }

            do {
                if (mNodeTest->matches(walker.getCurrentPosition(), aContext)) {
                    nodes->append(walker.getCurrentPosition());
                }
            } while (walker.moveToNextAttribute());
            break;
        }
        case DESCENDANT_OR_SELF_AXIS:
        {
            if (mNodeTest->matches(walker.getCurrentPosition(), aContext)) {
                nodes->append(walker.getCurrentPosition());
            }
            // do not break here
        }
        case DESCENDANT_AXIS:
        {
            fromDescendants(walker.getCurrentPosition(), aContext, nodes);
            break;
        }
        case FOLLOWING_AXIS:
        {
            if (txXPathNodeUtils::isAttribute(walker.getCurrentPosition())) {
                walker.moveToParent();
                fromDescendants(walker.getCurrentPosition(), aContext, nodes);
            }
            bool cont = true;
            while (!walker.moveToNextSibling()) {
                if (!walker.moveToParent()) {
                    cont = false;
                    break;
                }
            }
            while (cont) {
                if (mNodeTest->matches(walker.getCurrentPosition(), aContext)) {
                    nodes->append(walker.getCurrentPosition());
                }

                fromDescendants(walker.getCurrentPosition(), aContext, nodes);

                while (!walker.moveToNextSibling()) {
                    if (!walker.moveToParent()) {
                        cont = false;
                        break;
                    }
                }
            }
            break;
        }
        case FOLLOWING_SIBLING_AXIS:
        {
            while (walker.moveToNextSibling()) {
                if (mNodeTest->matches(walker.getCurrentPosition(), aContext)) {
                    nodes->append(walker.getCurrentPosition());
                }
            }
            break;
        }
        case NAMESPACE_AXIS: //-- not yet implemented
#if 0
            // XXX DEBUG OUTPUT
            cout << "namespace axis not yet implemented"<<endl;
#endif
            break;
        case PARENT_AXIS :
        {
            if (walker.moveToParent() &&
                mNodeTest->matches(walker.getCurrentPosition(), aContext)) {
                nodes->append(walker.getCurrentPosition());
            }
            break;
        }
        case PRECEDING_AXIS:
        {
            nodes->setReverse();

            bool cont = true;
            while (!walker.moveToPreviousSibling()) {
                if (!walker.moveToParent()) {
                    cont = false;
                    break;
                }
            }
            while (cont) {
                fromDescendantsRev(walker.getCurrentPosition(), aContext, nodes);

                if (mNodeTest->matches(walker.getCurrentPosition(), aContext)) {
                    nodes->append(walker.getCurrentPosition());
                }

                while (!walker.moveToPreviousSibling()) {
                    if (!walker.moveToParent()) {
                        cont = false;
                        break;
                    }
                }
            }
            break;
        }
        case PRECEDING_SIBLING_AXIS:
        {
            nodes->setReverse();

            while (walker.moveToPreviousSibling()) {
                if (mNodeTest->matches(walker.getCurrentPosition(), aContext)) {
                    nodes->append(walker.getCurrentPosition());
                }
            }
            break;
        }
        case SELF_AXIS:
        {
            if (mNodeTest->matches(walker.getCurrentPosition(), aContext)) {
                nodes->append(walker.getCurrentPosition());
            }
            break;
        }
        default: // Children Axis
        {
            if (!walker.moveToFirstChild()) {
                break;
            }

            do {
                if (mNodeTest->matches(walker.getCurrentPosition(), aContext)) {
                    nodes->append(walker.getCurrentPosition());
                }
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

void LocationStep::fromDescendants(const txXPathNode& aNode,
                                   txIMatchContext* aCs,
                                   txNodeSet* aNodes)
{
    txXPathTreeWalker walker(aNode);
    if (!walker.moveToFirstChild()) {
        return;
    }

    do {
        const txXPathNode& child = walker.getCurrentPosition();
        if (mNodeTest->matches(child, aCs)) {
            aNodes->append(child);
        }
        fromDescendants(child, aCs, aNodes);
    } while (walker.moveToNextSibling());
}

void LocationStep::fromDescendantsRev(const txXPathNode& aNode,
                                      txIMatchContext* aCs,
                                      txNodeSet* aNodes)
{
    txXPathTreeWalker walker(aNode);
    if (!walker.moveToLastChild()) {
        return;
    }

    do {
        const txXPathNode& child = walker.getCurrentPosition();
        fromDescendantsRev(child, aCs, aNodes);

        if (mNodeTest->matches(child, aCs)) {
            aNodes->append(child);
        }

    } while (walker.moveToPreviousSibling());
}

Expr::ExprType
LocationStep::getType()
{
  return LOCATIONSTEP_EXPR;
}


TX_IMPL_EXPR_STUBS_BASE(LocationStep, NODESET_RESULT)

Expr*
LocationStep::getSubExprAt(PRUint32 aPos)
{
    return PredicateList::getSubExprAt(aPos);
}

void
LocationStep::setSubExprAt(PRUint32 aPos, Expr* aExpr)
{
    PredicateList::setSubExprAt(aPos, aExpr);
}

bool
LocationStep::isSensitiveTo(ContextSensitivity aContext)
{
    return (aContext & NODE_CONTEXT) ||
           mNodeTest->isSensitiveTo(aContext) ||
           PredicateList::isSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
void
LocationStep::toString(nsAString& str)
{
    switch (mAxisIdentifier) {
        case ANCESTOR_AXIS :
            str.AppendLiteral("ancestor::");
            break;
        case ANCESTOR_OR_SELF_AXIS :
            str.AppendLiteral("ancestor-or-self::");
            break;
        case ATTRIBUTE_AXIS:
            str.Append(PRUnichar('@'));
            break;
        case DESCENDANT_AXIS:
            str.AppendLiteral("descendant::");
            break;
        case DESCENDANT_OR_SELF_AXIS:
            str.AppendLiteral("descendant-or-self::");
            break;
        case FOLLOWING_AXIS :
            str.AppendLiteral("following::");
            break;
        case FOLLOWING_SIBLING_AXIS:
            str.AppendLiteral("following-sibling::");
            break;
        case NAMESPACE_AXIS:
            str.AppendLiteral("namespace::");
            break;
        case PARENT_AXIS :
            str.AppendLiteral("parent::");
            break;
        case PRECEDING_AXIS :
            str.AppendLiteral("preceding::");
            break;
        case PRECEDING_SIBLING_AXIS :
            str.AppendLiteral("preceding-sibling::");
            break;
        case SELF_AXIS :
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
