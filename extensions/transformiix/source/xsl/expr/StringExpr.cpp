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

#include "Expr.h"

/**
 * StringExpr
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/

/**
 * Creates a new StringExpr
**/
StringExpr::StringExpr() {};

StringExpr::StringExpr(String& value) {
    //-- copy value
    this->value = value;
} //-- StringExpr

StringExpr::StringExpr(const String& value) {
    //-- copy value
    this->value.append(value);
} //-- StringExpr

/**
 * Default Destructor
**/
StringExpr::~StringExpr() {};

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* StringExpr::evaluate(Node* context, ContextState* cs) {
   return new StringResult(value);
} //-- evaluate

/**
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
**/
void StringExpr::toString(String& str) {
    str.append('\'');
    str.append(value);
    str.append('\'');
} //-- toString

