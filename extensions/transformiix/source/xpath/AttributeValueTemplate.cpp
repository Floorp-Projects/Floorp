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
 * AttributeValueTemplate
**/

#include "Expr.h"
#include "ExprResult.h"
#include "txIXPathContext.h"

/**
 * Create a new AttributeValueTemplate
**/
AttributeValueTemplate::AttributeValueTemplate() {}

/**
 * Default destructor
**/
AttributeValueTemplate::~AttributeValueTemplate() {
    txListIterator iter(&expressions);
    while (iter.hasNext()) {
        delete (Expr*)iter.next();
    }
} //-- ~AttributeValueTemplate

/**
 * Adds the given Expr to this AttributeValueTemplate
**/
void AttributeValueTemplate::addExpr(Expr* expr) {
    if (expr) expressions.add(expr);
} //-- addExpr

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
nsresult
AttributeValueTemplate::evaluate(txIEvalContext* aContext,
                                 txAExprResult** aResult)
{
    *aResult = nsnull;

    txListIterator iter(&expressions);
    nsRefPtr<StringResult> strRes;
    nsresult rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
    NS_ENSURE_SUCCESS(rv, rv);

    while (iter.hasNext()) {
        Expr* expr = (Expr*)iter.next();
        nsRefPtr<txAExprResult> exprResult;
        nsresult rv = expr->evaluate(aContext, getter_AddRefs(exprResult));
        NS_ENSURE_SUCCESS(rv, rv);

        exprResult->stringValue(strRes->mValue);
    }

    *aResult = strRes;

    NS_ADDREF(*aResult);

    return NS_OK;
} //-- evaluate

#ifdef TX_TO_STRING
void
AttributeValueTemplate::toString(nsAString& str)
{
    txListIterator iter(&expressions);
    while (iter.hasNext()) {
        str.Append(PRUnichar('{'));
        Expr* expr = (Expr*)iter.next();
        expr->toString(str);
        str.Append(PRUnichar('}'));
    }
}
#endif
