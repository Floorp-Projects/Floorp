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
 * The Initial Developer of the Original Code is Jonas Sicking.
 * Portions created by Jonas Sicking are Copyright (C) 2001, Jonas Sicking.
 * All rights reserved.
 *
 * Contributor(s):
 * Jonas Sicking, sicking@bigfoot.com
 *   -- original author.
 *
 * NaN/Infinity code copied from the JS-library with permission from
 * Netscape Communications Corporation: http://www.mozilla.org/js
 * http://lxr.mozilla.org/seamonkey/source/js/src/jsinterp.c
 *
 */

#include "Expr.h"
#include "ExprResult.h"
#include "txIXPathContext.h"

UnaryExpr::UnaryExpr(nsAutoPtr<Expr> aExpr)
    : expr(aExpr)
{
}

/*
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation.
 * @return the result of the evaluation.
 */
nsresult
UnaryExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nsnull;

    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = expr->evaluate(aContext, getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    double value = exprRes->numberValue();
#ifdef HPUX
    /*
     * Negation of a zero doesn't produce a negative
     * zero on HPUX. Perform the operation by multiplying with
     * -1.
     */
    return aContext->recycler()->getNumberResult(-1 * value, aResult);
#else
    return aContext->recycler()->getNumberResult(-value, aResult);
#endif
}

/*
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
 */
void UnaryExpr::toString(nsAString& str)
{
    if (!expr)
        return;
    str.Append(PRUnichar('-'));
    expr->toString(str);
}
