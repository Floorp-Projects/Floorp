/*
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
#include "NodeSet.h"
#include "txIXPathContext.h"

  //-------------/
 //- UnionExpr -/
//-------------/


/**
 * Destructor, will delete all Path Expressions
**/
UnionExpr::~UnionExpr() {
    txListIterator iter(&expressions);
    while (iter.hasNext()) {
         delete (Expr*)iter.next();
    }
} //-- ~UnionExpr

/**
 * Adds the Expr to this UnionExpr
 * @param expr the Expr to add to this UnionExpr
**/
nsresult
UnionExpr::addExpr(nsAutoPtr<Expr> aExpr)
{
    nsresult rv = expressions.add(aExpr);
    NS_ENSURE_SUCCESS(rv, rv);
    
    aExpr.forget();
    
    return NS_OK;
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
UnionExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nsnull;
    nsRefPtr<NodeSet> nodes;
    nsresult rv = aContext->recycler()->getNodeSet(getter_AddRefs(nodes));
    NS_ENSURE_SUCCESS(rv, rv);

    txListIterator iter(&expressions);
    while (iter.hasNext()) {
        Expr* expr = (Expr*)iter.next();
        nsRefPtr<txAExprResult> exprResult;
        rv = expr->evaluate(aContext, getter_AddRefs(exprResult));
        NS_ENSURE_SUCCESS(rv, rv);

        if (exprResult->getResultType() != txAExprResult::NODESET) {
            //XXX ErrorReport: report nonnodeset error
            return NS_ERROR_XSLT_NODESET_EXPECTED;
        }
        rv = nodes->add(NS_STATIC_CAST(NodeSet*, NS_STATIC_CAST(txAExprResult*,
                                                                exprResult)));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    *aResult = nodes;
    NS_ADDREF(*aResult);

    return NS_OK;
} //-- evaluate

/**
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 *  any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
**/
void UnionExpr::toString(nsAString& dest) {
    txListIterator iter(&expressions);

    short count = 0;
    while (iter.hasNext()) {
        //-- set operator
        if (count > 0)
            dest.Append(NS_LITERAL_STRING(" | "));
        ((Expr*)iter.next())->toString(dest);
        ++count;
    }
} //-- toString

