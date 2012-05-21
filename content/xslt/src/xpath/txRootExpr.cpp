/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"
#include "txNodeSet.h"
#include "txIXPathContext.h"
#include "txXPathTreeWalker.h"

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
nsresult
RootExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    txXPathTreeWalker walker(aContext->getContextNode());
    walker.moveToRoot();
    
    return aContext->recycler()->getNodeSet(walker.getCurrentPosition(),
                                            aResult);
}

TX_IMPL_EXPR_STUBS_0(RootExpr, NODESET_RESULT)

bool
RootExpr::isSensitiveTo(ContextSensitivity aContext)
{
    return !!(aContext & NODE_CONTEXT);
}

#ifdef TX_TO_STRING
void
RootExpr::toString(nsAString& dest)
{
    if (mSerialize)
        dest.Append(PRUnichar('/'));
}
#endif
