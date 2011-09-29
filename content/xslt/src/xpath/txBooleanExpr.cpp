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


/**
 * Represents a BooleanExpr, a binary expression that
 * performs a boolean operation between its lvalue and rvalue.
**/

#include "txExpr.h"
#include "txIXPathContext.h"

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
nsresult
BooleanExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nsnull;

    bool lval;
    nsresult rv = leftExpr->evaluateToBool(aContext, lval);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // check for early decision
    if (op == OR && lval) {
        aContext->recycler()->getBoolResult(PR_TRUE, aResult);
        
        return NS_OK;
    }
    if (op == AND && !lval) {
        aContext->recycler()->getBoolResult(PR_FALSE, aResult);

        return NS_OK;
    }

    bool rval;
    rv = rightExpr->evaluateToBool(aContext, rval);
    NS_ENSURE_SUCCESS(rv, rv);

    // just use rval, since we already checked lval
    aContext->recycler()->getBoolResult(rval, aResult);

    return NS_OK;
} //-- evaluate

TX_IMPL_EXPR_STUBS_2(BooleanExpr, BOOLEAN_RESULT, leftExpr, rightExpr)

bool
BooleanExpr::isSensitiveTo(ContextSensitivity aContext)
{
    return leftExpr->isSensitiveTo(aContext) ||
           rightExpr->isSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
void
BooleanExpr::toString(nsAString& str)
{
    if ( leftExpr ) leftExpr->toString(str);
    else str.AppendLiteral("null");

    switch ( op ) {
        case OR:
            str.AppendLiteral(" or ");
            break;
        default:
            str.AppendLiteral(" and ");
            break;
    }
    if ( rightExpr ) rightExpr->toString(str);
    else str.AppendLiteral("null");

}
#endif
