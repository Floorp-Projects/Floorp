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
 */

#include "Expr.h"

UnaryExpr::UnaryExpr(Expr* expr)
{
    this->expr = expr;
}

UnaryExpr::~UnaryExpr()
{
    delete expr;
}

/*
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation.
 * @return the result of the evaluation.
 */
ExprResult* UnaryExpr::evaluate(Node* context, ContextState* cs)
{
    ExprResult* exprRes = expr->evaluate(context, cs);
    double value = exprRes->numberValue();
    delete exprRes;
    return new NumberResult(-value);
}

/*
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
 */
void UnaryExpr::toString(String& str)
{
    if (!expr)
        return;
    str.append('-');
    expr->toString(str);
}
