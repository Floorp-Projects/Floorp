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
 * Represents a AdditiveExpr, an binary expression that
 * performs an additive operation between it's lvalue and rvalue:<BR/>
 *  +   : addition
 *  -   : substraction
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/

#include "Expr.h"

/**
 * Creates a new AdditiveExpr using the default operator (ADDITION)
**/
AdditiveExpr::AdditiveExpr() {
    this->op = ADDITION;
    this->leftExpr  = 0;
    this->rightExpr = 0;
} //-- AdditiveExpr

/**
 * Creates a new AdditiveExpr using the given operator
**/
AdditiveExpr::AdditiveExpr(Expr* leftExpr, Expr* rightExpr, short op) {
    this->op = op;
    this->leftExpr = leftExpr;
    this->rightExpr = rightExpr;
} //-- AdditiveExpr

AdditiveExpr::~AdditiveExpr() {
    delete leftExpr;
    delete rightExpr;
} //-- ~AdditiveExpr

/**
 * Sets the left side of this AdditiveExpr
**/
void AdditiveExpr::setLeftExpr(Expr* leftExpr) {
    this->leftExpr = leftExpr;
} //-- setLeftExpr

/**
 * Sets the right side of this AdditiveExpr
**/
void AdditiveExpr::setRightExpr(Expr* rightExpr) {
    this->rightExpr = rightExpr;
} //-- setRightExpr

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* AdditiveExpr::evaluate(Node* context, ContextState* cs) {


    double rightDbl = Double::NaN;
    ExprResult* exprRes = 0;

    if ( rightExpr ) {
        exprRes = rightExpr->evaluate(context, cs);
        if ( exprRes ) rightDbl = exprRes->numberValue();
        delete exprRes;
    }

    double leftDbl = Double::NaN;
    if ( leftExpr ) {
        exprRes = leftExpr->evaluate(context, cs);
        if ( exprRes ) leftDbl = exprRes->numberValue();
        delete exprRes;
    }

    double result = 0;

    switch ( op ) {
        case SUBTRACTION:
            result = leftDbl - rightDbl;
            break;
        default:
            result = leftDbl + rightDbl;
            break;
    }
   return new NumberResult(result);
} //-- evaluate

/**
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
**/
void AdditiveExpr::toString(String& str) {

    if ( leftExpr ) leftExpr->toString(str);
    else str.append("null");

    switch ( op ) {
        case SUBTRACTION:
            str.append(" - ");
            break;
        default:
            str.append(" + ");
            break;
    }
    if ( rightExpr ) rightExpr->toString(str);
    else str.append("null");

} //-- toString

