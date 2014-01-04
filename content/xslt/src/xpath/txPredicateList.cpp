/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"
#include "txNodeSet.h"
#include "txNodeSetContext.h"

/*
 * Represents an ordered list of Predicates,
 * for use with Step and Filter Expressions
 */

nsresult
PredicateList::evaluatePredicates(txNodeSet* nodes,
                                  txIMatchContext* aContext)
{
    NS_ASSERTION(nodes, "called evaluatePredicates with nullptr NodeSet");
    nsresult rv = NS_OK;

    uint32_t i, len = mPredicates.Length();
    for (i = 0; i < len && !nodes->isEmpty(); ++i) {
        txNodeSetContext predContext(nodes, aContext);
        /*
         * add nodes to newNodes that match the expression
         * or, if the result is a number, add the node with the right
         * position
         */
        int32_t index = 0;
        while (predContext.hasNext()) {
            predContext.next();
            nsRefPtr<txAExprResult> exprResult;
            rv = mPredicates[i]->evaluate(&predContext,
                                          getter_AddRefs(exprResult));
            NS_ENSURE_SUCCESS(rv, rv);

            // handle default, [position() == numberValue()]
            if (exprResult->getResultType() == txAExprResult::NUMBER) {
                if ((double)predContext.position() == exprResult->numberValue()) {
                    nodes->mark(index);
                }
            }
            else if (exprResult->booleanValue()) {
                nodes->mark(index);
            }
            ++index;
        }
        // sweep the non-marked nodes
        nodes->sweep();
    }

    return NS_OK;
}

bool
PredicateList::isSensitiveTo(Expr::ContextSensitivity aContext)
{
    // We're creating a new node/nodeset so we can ignore those bits.
    Expr::ContextSensitivity context =
        aContext & ~(Expr::NODE_CONTEXT | Expr::NODESET_CONTEXT);
    if (context == Expr::NO_CONTEXT) {
        return false;
    }

    uint32_t i, len = mPredicates.Length();
    for (i = 0; i < len; ++i) {
        if (mPredicates[i]->isSensitiveTo(context)) {
            return true;
        }
    }

    return false;
}

#ifdef TX_TO_STRING
void PredicateList::toString(nsAString& dest)
{
    for (uint32_t i = 0; i < mPredicates.Length(); ++i) {
        dest.Append(char16_t('['));
        mPredicates[i]->toString(dest);
        dest.Append(char16_t(']'));
    }
}
#endif
