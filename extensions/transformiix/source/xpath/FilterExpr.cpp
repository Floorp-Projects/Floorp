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
 *   Bob Miller <kbob@oblix.com>
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

#ifdef TX_TO_STRING
void
FilterExpr::toString(nsAString& str)
{
    if ( expr ) expr->toString(str);
    else str.AppendLiteral("null");
    PredicateList::toString(str);
}
#endif

