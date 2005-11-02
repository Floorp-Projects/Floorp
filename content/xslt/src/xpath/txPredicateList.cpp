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
        iter.next();
        Expr* expr = (Expr*)iter.remove();
        delete expr;
    }
} // ~PredicateList

/*
 * Adds the given Expr to the list
 * @param expr the Expr to add to the list
 */
void PredicateList::add(Expr* expr)
{
    predicates.add(expr);
} // add

void PredicateList::evaluatePredicates(NodeSet* nodes, ContextState* cs)
{
    NS_ASSERTION(nodes, "called evaluatePredicates with NULL NodeSet");
    if (!nodes)
        return;

    cs->getNodeSetStack()->push(nodes);
    NodeSet newNodes;
    // optimize; set DuplicateChecking to MB_FALSE,
    // restore original state later
    // we only work with |Node|s already in the NodeSet, so they
    // have been checked already, no need to check again in here.
    MBool ndsCheckDupl = nodes->getDuplicateChecking();
    nodes->setDuplicateChecking(MB_FALSE);
    newNodes.setDuplicateChecking(MB_FALSE);
    txListIterator iter(&predicates);
    while (iter.hasNext()) {
        Expr* expr = (Expr*)iter.next();
        /*
         * add nodes to newNodes that match the expression
         * or, if the result is a number, add the node with the right
         * position
         */
        newNodes.clear();
        int nIdx;
        for (nIdx = 0; nIdx < nodes->size(); nIdx++) {
            Node* node = nodes->get(nIdx);
            ExprResult* exprResult = expr->evaluate(node, cs);
            if (!exprResult)
                break;
            switch(exprResult->getResultType()) {
                case ExprResult::NUMBER :
                    // handle default, [position() == numberValue()]
                    if ((double)(nIdx+1) == exprResult->numberValue())
                        newNodes.add(node);
                    break;
                default:
                    if (exprResult->booleanValue())
                        newNodes.add(node);
                    break;
            }
            delete exprResult;
        }
        // Move new NodeSet to the current one
        nodes->clear();
        newNodes.copyInto(*nodes);
    }
    cs->getNodeSetStack()->pop();
    // restore DuplicateChecking of NodeSet
    nodes->setDuplicateChecking(ndsCheckDupl);
} // evaluatePredicates

/*
 * returns true if this predicate list is empty
 */
MBool PredicateList::isEmpty()
{
    return (MBool)(predicates.getLength() == 0);
} // isEmpty

void PredicateList::toString(String& dest)
{
    txListIterator iter(&predicates);
    while (iter.hasNext()) {
        Expr* expr = (Expr*) iter.next();
        dest.append("[");
        expr->toString(dest);
        dest.append("]");
    }
} // toString

