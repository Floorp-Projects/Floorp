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
 * Bob Miller, kbob@oblix.com
 *    -- plugged core leak.
 * 
 */

#include "Expr.h"
#include "txNodeSet.h"
#include "txIXPathContext.h"

//-- Implementation of FilterExpr --/

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
FilterExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nsnull;

    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = expr->evaluate(aContext, getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(exprRes->getResultType() == txAExprResult::NODESET,
                   NS_ERROR_XSLT_NODESET_EXPECTED);

    nsRefPtr<txNodeSet> nodes =
        NS_STATIC_CAST(txNodeSet*, NS_STATIC_CAST(txAExprResult*, exprRes));
    // null out exprRes so that we can test for shared-ness
    exprRes = nsnull;

    nsRefPtr<txNodeSet> nonShared;
    rv = aContext->recycler()->getNonSharedNodeSet(nodes,
                                                   getter_AddRefs(nonShared));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = evaluatePredicates(nonShared, aContext);
    NS_ENSURE_SUCCESS(rv, rv);

    *aResult = nonShared;
    NS_ADDREF(*aResult);

    return NS_OK;
} //-- evaluate

/**
 * Creates a String representation of this Expr
 * @param str the destination String to append to
 * @see Expr
**/
void FilterExpr::toString(nsAString& str) {
    if ( expr ) expr->toString(str);
    else str.Append(NS_LITERAL_STRING("null"));
    PredicateList::toString(str);
} //-- toString

