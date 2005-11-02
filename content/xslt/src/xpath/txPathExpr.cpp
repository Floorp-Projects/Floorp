/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *   -- original author.
 *
 * Bob Miller, kbob@oblix.com
 *    -- plugged core leak.
 *
 * Marina Mechtcheriakova, mmarina@mindspring.com
 *    -- fixed bug in PathExpr::matches
 *       - foo//bar would not match properly if there was more than
 *         one node in the NodeSet (nodes) on the final iteration
 *
 */

#include "Expr.h"
#include "txNodeSet.h"
#include "txNodeSetContext.h"
#include "txSingleNodeContext.h"
#include "XMLUtils.h"

  //------------/
 //- PathExpr -/
//------------/

/**
 * Creates a new PathExpr
**/
PathExpr::PathExpr()
{
    //-- do nothing
}

/**
 * Destructor, will delete all Expressions
**/
PathExpr::~PathExpr()
{
    txListIterator iter(&expressions);
    while (iter.hasNext()) {
         delete NS_STATIC_CAST(PathExprItem*, iter.next());
    }
} //-- ~PathExpr

/**
 * Adds the Expr to this PathExpr
 * @param expr the Expr to add to this PathExpr
**/
nsresult
PathExpr::addExpr(Expr* aExpr, PathOperator pathOp)
{
    NS_ASSERTION(expressions.getLength() > 0 || pathOp == RELATIVE_OP,
                 "First step has to be relative in PathExpr");
    PathExprItem* pxi = new PathExprItem(aExpr, pathOp);
    if (!pxi) {
        delete aExpr;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult rv = expressions.add(pxi);
    if (NS_FAILED(rv)) {
        delete pxi;
    }
    return rv;
} //-- addExpr

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

    nsRefPtr<txNodeSet> nodes;
    nsresult rv = aContext->recycler()->getNodeSet(aContext->getContextNode(),
                                                   getter_AddRefs(nodes));
    NS_ENSURE_SUCCESS(rv, rv);

    txListIterator iter(&expressions);
    PathExprItem* pxi;
    while ((pxi = (PathExprItem*)iter.next())) {
        nsRefPtr<txNodeSet> tmpNodes;
        txNodeSetContext eContext(nodes, aContext);
        while (eContext.hasNext()) {
            eContext.next();

            nsRefPtr<txNodeSet> resNodes;
            if (pxi->pathOp == DESCENDANT_OP) {
                rv = aContext->recycler()->getNodeSet(getter_AddRefs(resNodes));
                NS_ENSURE_SUCCESS(rv, rv);

                rv = evalDescendants(pxi->expr, eContext.getContextNode(),
                                     &eContext, resNodes);
                NS_ENSURE_SUCCESS(rv, rv);
            }
            else {
                nsRefPtr<txAExprResult> res;
                rv = pxi->expr->evaluate(&eContext, getter_AddRefs(res));
                NS_ENSURE_SUCCESS(rv, rv);

                if (res->getResultType() != txAExprResult::NODESET) {
                    //XXX ErrorReport: report nonnodeset error
                    return NS_ERROR_XSLT_NODESET_EXPECTED;
                }
                resNodes = NS_STATIC_CAST(txNodeSet*,
                                          NS_STATIC_CAST(txAExprResult*,
                                                         res));
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

    txNodeSet* oldSet = NS_STATIC_CAST(txNodeSet*,
                                       NS_STATIC_CAST(txAExprResult*, res));
    nsRefPtr<txNodeSet> newSet;
    rv = aContext->recycler()->getNonSharedNodeSet(oldSet,
                                                   getter_AddRefs(newSet));
    NS_ENSURE_SUCCESS(rv, rv);

    resNodes->addAndTransfer(newSet);

    MBool filterWS = aContext->isStripSpaceAllowed(aNode);

    txXPathTreeWalker walker(aNode);
    if (!walker.moveToFirstChild()) {
        return NS_OK;
    }

    do {
        if (!(filterWS &&
              (walker.getNodeType() == txXPathNodeType::TEXT_NODE ||
               walker.getNodeType() == txXPathNodeType::CDATA_SECTION_NODE) &&
              txXPathNodeUtils::isWhitespace(walker.getCurrentPosition()))) {
            rv = evalDescendants(aStep, walker.getCurrentPosition(), aContext,
                                 resNodes);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    } while (walker.moveToNextSibling());

    return NS_OK;
} //-- evalDescendants

/**
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
**/
void PathExpr::toString(nsAString& dest)
{
    txListIterator iter(&expressions);
    
    PathExprItem* pxi = (PathExprItem*)iter.next();
    if (pxi) {
        NS_ASSERTION(pxi->pathOp == RELATIVE_OP,
                     "First step should be relative");
        pxi->expr->toString(dest);
    }
    
    while ((pxi = (PathExprItem*)iter.next())) {
        switch (pxi->pathOp) {
            case DESCENDANT_OP:
                dest.Append(NS_LITERAL_STRING("//"));
                break;
            case RELATIVE_OP:
                dest.Append(PRUnichar('/'));
                break;
        }
        pxi->expr->toString(dest);
    }
} //-- toString
