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
    nsresult rv = NS_OK;

    txListIterator iter(&predicates);
    while (iter.hasNext() && !nodes->isEmpty()) {
        Expr* expr = (Expr*)iter.next();
        txNodeSetContext predContext(nodes, aContext);
        /*
         * add nodes to newNodes that match the expression
         * or, if the result is a number, add the node with the right
         * position
         */
        PRInt32 index = 0;
        while (predContext.hasNext()) {
            predContext.next();
            nsRefPtr<txAExprResult> exprResult;
            rv = expr->evaluate(&predContext, getter_AddRefs(exprResult));
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

/*
 * returns true if this predicate list is empty
 */
MBool PredicateList::isEmpty()
{
    return (MBool)(predicates.getLength() == 0);
} // isEmpty

#ifdef TX_TO_STRING
void PredicateList::toString(nsAString& dest)
{
    txListIterator iter(&predicates);
    while (iter.hasNext()) {
        Expr* expr = (Expr*) iter.next();
        dest.Append(PRUnichar('['));
        expr->toString(dest);
        dest.Append(PRUnichar(']'));
    }
}
#endif
