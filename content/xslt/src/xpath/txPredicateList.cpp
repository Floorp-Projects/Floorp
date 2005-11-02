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
 */

#include "Expr.h"
#include "txNodeSet.h"
#include "txNodeSetContext.h"

/*
 * Represents an ordered list of Predicates,
 * for use with Step and Filter Expressions
 */

PredicateList::PredicateList()
{
} // PredicateList

/*
 * Destructor, will delete all Expressions in the list
 */
PredicateList::~PredicateList()
{
    txListIterator iter(&predicates);
    while (iter.hasNext()) {
        delete (Expr*)iter.next();
    }
} // ~PredicateList

/*
 * Adds the given Expr to the list
 * @param expr the Expr to add to the list
 */
nsresult
PredicateList::add(Expr* aExpr)
{
    NS_ASSERTION(aExpr, "missing expression");
    nsresult rv = predicates.add(aExpr);
    if (NS_FAILED(rv)) {
        delete aExpr;
    }
    return rv;
} // add

nsresult
PredicateList::evaluatePredicates(txNodeSet* nodes,
                                  txIMatchContext* aContext)
{
    NS_ASSERTION(nodes, "called evaluatePredicates with NULL NodeSet");
    nsRefPtr<txNodeSet> newNodes;
    nsresult rv = aContext->recycler()->getNodeSet(getter_AddRefs(newNodes));
    NS_ENSURE_SUCCESS(rv, rv);
    
    txListIterator iter(&predicates);
    while (iter.hasNext() && !nodes->isEmpty()) {
        Expr* expr = (Expr*)iter.next();
        txNodeSetContext predContext(nodes, aContext);
        /*
         * add nodes to newNodes that match the expression
         * or, if the result is a number, add the node with the right
         * position
         */
        newNodes->clear();
        while (predContext.hasNext()) {
            predContext.next();
            nsRefPtr<txAExprResult> exprResult;
            rv = expr->evaluate(&predContext, getter_AddRefs(exprResult));
            NS_ENSURE_SUCCESS(rv, rv);

            switch(exprResult->getResultType()) {
                case txAExprResult::NUMBER:
                    // handle default, [position() == numberValue()]
                    if ((double)predContext.position() ==
                        exprResult->numberValue())
                        newNodes->append(predContext.getContextNode());
                    break;
                default:
                    if (exprResult->booleanValue())
                        newNodes->append(predContext.getContextNode());
                    break;
            }
        }
        // Move new NodeSet to the current one
        nodes->clear();
        nodes->append(newNodes);
    }
    
    return NS_OK;
}

/*
 * returns true if this predicate list is empty
 */
MBool PredicateList::isEmpty()
{
    return (MBool)(predicates.getLength() == 0);
} // isEmpty

void PredicateList::toString(nsAString& dest)
{
    txListIterator iter(&predicates);
    while (iter.hasNext()) {
        Expr* expr = (Expr*) iter.next();
        dest.Append(PRUnichar('['));
        expr->toString(dest);
        dest.Append(PRUnichar(']'));
    }
} // toString

