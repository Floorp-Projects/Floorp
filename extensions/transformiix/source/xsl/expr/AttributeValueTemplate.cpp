/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */

/**
 * AttributeValueTemplate
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
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



