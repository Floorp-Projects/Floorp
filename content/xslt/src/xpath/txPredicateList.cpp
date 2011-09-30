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

nsresult
PredicateList::evaluatePredicates(txNodeSet* nodes,
                                  txIMatchContext* aContext)
{
    NS_ASSERTION(nodes, "called evaluatePredicates with NULL NodeSet");
    nsresult rv = NS_OK;

    PRUint32 i, len = mPredicates.Length();
    for (i = 0; i < len && !nodes->isEmpty(); ++i) {
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
        return PR_FALSE;
    }

    PRUint32 i, len = mPredicates.Length();
    for (i = 0; i < len; ++i) {
        if (mPredicates[i]->isSensitiveTo(context)) {
            return PR_TRUE;
        }
    }

    return PR_FALSE;
}

#ifdef TX_TO_STRING
void PredicateList::toString(nsAString& dest)
{
    for (PRUint32 i = 0; i < mPredicates.Length(); ++i) {
        dest.Append(PRUnichar('['));
        mPredicates[i]->toString(dest);
        dest.Append(PRUnichar(']'));
    }
}
#endif
