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

/**
 * AttributeValueTemplate
**/

#include "Expr.h"

/**
 * Create a new AttributeValueTemplate
**/
AttributeValueTemplate::AttributeValueTemplate() {};

/**
 * Default destructor
**/
AttributeValueTemplate::~AttributeValueTemplate() {
    ListIterator* iter = expressions.iterator();
    while ( iter->hasNext() ) {
        iter->next(); //advance iterator to allow remove
        Expr* expr = (Expr*)iter->remove();
        delete expr;
    }
    delete iter;

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
ExprResult* AttributeValueTemplate::evaluate(Node* context, ContextState* cs) {
    ListIterator* iter = expressions.iterator();
    String result;
    while ( iter->hasNext() ) {
        Expr* expr = (Expr*)iter->next();
        ExprResult* exprResult = expr->evaluate(context, cs);
        exprResult->stringValue(result);
        delete exprResult;
    }
    delete iter;
    return new StringResult(result);
} //-- evaluate

/**
* Returns the String representation of this Expr.
* @param dest the String to use when creating the String
* representation. The String representation will be appended to
* any data in the destination String, to allow cascading calls to
* other #toString() methods for Expressions.
* @return the String representation of this Expr.
**/
void AttributeValueTemplate::toString(String& str) {
    ListIterator* iter = expressions.iterator();
    while ( iter->hasNext() ) {
        str.append('{');
        Expr* expr = (Expr*)iter->next();
        expr->toString(str);
        str.append('}');
    }
    delete iter;
} //-- toString

