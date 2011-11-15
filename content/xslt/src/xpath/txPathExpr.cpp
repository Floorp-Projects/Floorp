/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Keith Visco <kvisco@ziplink.net> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "txExpr.h"
#include "txNodeSet.h"
#include "txNodeSetContext.h"
#include "txSingleNodeContext.h"
#include "txXMLUtils.h"
#include "txXPathTreeWalker.h"

  //------------/
 //- PathExpr -/
//------------/

/**
 * Adds the Expr to this PathExpr
 * @param expr the Expr to add to this PathExpr
**/
nsresult
PathExpr::addExpr(Expr* aExpr, PathOperator aPathOp)
{
    NS_ASSERTION(!mItems.IsEmpty() || aPathOp == RELATIVE_OP,
                 "First step has to be relative in PathExpr");
    PathExprItem* pxi = mItems.AppendElement();
    if (!pxi) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    pxi->expr = aExpr;
    pxi->pathOp = aPathOp;

    return NS_OK;
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
nsresult
PathExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nsnull;

    // We need to evaluate the first step with the current context since it
    // can depend on the context size and position. For example:
    // key('books', concat('book', position()))
    nsRefPtr<txAExprResult> res;
    nsresult rv = mItems[0].expr->evaluate(aContext, getter_AddRefs(res));
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(res->getResultType() == txAExprResult::NODESET,
                   NS_ERROR_XSLT_NODESET_EXPECTED);

    nsRefPtr<txNodeSet> nodes = static_cast<txNodeSet*>
                                           (static_cast<txAExprResult*>
                                                       (res));
    if (nodes->isEmpty()) {
        res.swap(*aResult);

        return NS_OK;
    }
    res = nsnull; // To allow recycling

    // Evaluate remaining steps
    PRUint32 i, len = mItems.Length();
    for (i = 1; i < len; ++i) {
        PathExprItem& pxi = mItems[i];
        nsRefPtr<txNodeSet> tmpNodes;
        txNodeSetContext eContext(nodes, aContext);
        while (eContext.hasNext()) {
            eContext.next();

            nsRefPtr<txNodeSet> resNodes;
            if (pxi.pathOp == DESCENDANT_OP) {
                rv = aContext->recycler()->getNodeSet(getter_AddRefs(resNodes));
                NS_ENSURE_SUCCESS(rv, rv);

                rv = evalDescendants(pxi.expr, eContext.getContextNode(),
                                     &eContext, resNodes);
                NS_ENSURE_SUCCESS(rv, rv);
            }
            else {
                nsRefPtr<txAExprResult> res;
                rv = pxi.expr->evaluate(&eContext, getter_AddRefs(res));
                NS_ENSURE_SUCCESS(rv, rv);

                if (res->getResultType() != txAExprResult::NODESET) {
                    //XXX ErrorReport: report nonnodeset error
                    return NS_ERROR_XSLT_NODESET_EXPECTED;
                }
                resNodes = static_cast<txNodeSet*>
                                      (static_cast<txAExprResult*>
                                                  (res));
            }

            if (tmpNodes) {
                if (!resNodes->isEmpty()) {
                    nsRefPtr<txNodeSet> oldSet;
                    oldSet.swap(tmpNodes);
                    rv = aContext->recycler()->
                        getNonSharedNodeSet(oldSet, getter_AddRefs(tmpNodes));
                    NS_ENSURE_SUCCESS(rv, rv);

                    oldSet.swap(resNodes);
                    rv = aContext->recycler()->
                        getNonSharedNodeSet(oldSet, getter_AddRefs(resNodes));
                    NS_ENSURE_SUCCESS(rv, rv);

                    tmpNodes->addAndTransfer(resNodes);
                }
            }
            else {
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
} //-- evaluate

/**
 * Selects from the descendants of the context node
 * all nodes that match the Expr
**/
nsresult
PathExpr::evalDescendants(Expr* aStep, const txXPathNode& aNode,
                          txIMatchContext* aContext, txNodeSet* resNodes)
{
    txSingleNodeContext eContext(aNode, aContext);
    nsRefPtr<txAExprResult> res;
    nsresult rv = aStep->evaluate(&eContext, getter_AddRefs(res));
    NS_ENSURE_SUCCESS(rv, rv);

    if (res->getResultType() != txAExprResult::NODESET) {
        //XXX ErrorReport: report nonnodeset error
        return NS_ERROR_XSLT_NODESET_EXPECTED;
    }

    txNodeSet* oldSet = static_cast<txNodeSet*>
                                   (static_cast<txAExprResult*>(res));
    nsRefPtr<txNodeSet> newSet;
    rv = aContext->recycler()->getNonSharedNodeSet(oldSet,
                                                   getter_AddRefs(newSet));
    NS_ENSURE_SUCCESS(rv, rv);

    resNodes->addAndTransfer(newSet);

    bool filterWS = aContext->isStripSpaceAllowed(aNode);

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
} //-- evalDescendants

Expr::ExprType
PathExpr::getType()
{
  return PATH_EXPR;
}

TX_IMPL_EXPR_STUBS_BASE(PathExpr, NODESET_RESULT)

Expr*
PathExpr::getSubExprAt(PRUint32 aPos)
{
    return aPos < mItems.Length() ? mItems[aPos].expr.get() : nsnull;
}
void
PathExpr::setSubExprAt(PRUint32 aPos, Expr* aExpr)
{
    NS_ASSERTION(aPos < mItems.Length(), "setting bad subexpression index");
    mItems[aPos].expr.forget();
    mItems[aPos].expr = aExpr;
}


bool
PathExpr::isSensitiveTo(ContextSensitivity aContext)
{
    if (mItems[0].expr->isSensitiveTo(aContext)) {
        return true;
    }

    // We're creating a new node/nodeset so we can ignore those bits.
    Expr::ContextSensitivity context =
        aContext & ~(Expr::NODE_CONTEXT | Expr::NODESET_CONTEXT);
    if (context == NO_CONTEXT) {
        return false;
    }

    PRUint32 i, len = mItems.Length();
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
void
PathExpr::toString(nsAString& dest)
{
    if (!mItems.IsEmpty()) {
        NS_ASSERTION(mItems[0].pathOp == RELATIVE_OP,
                     "First step should be relative");
        mItems[0].expr->toString(dest);
    }
    
    PRUint32 i, len = mItems.Length();
    for (i = 1; i < len; ++i) {
        switch (mItems[i].pathOp) {
            case DESCENDANT_OP:
                dest.AppendLiteral("//");
                break;
            case RELATIVE_OP:
                dest.Append(PRUnichar('/'));
                break;
        }
        mItems[i].expr->toString(dest);
    }
}
#endif
