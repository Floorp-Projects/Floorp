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

  //------------------/
 //- RelationalExpr -/
//------------------/

RelationalExpr::RelationalExpr() {
    this->op = EQUAL;
    this->leftExpr  = 0;
    this->rightExpr = 0;
} //-- RelationalExpr

RelationalExpr::RelationalExpr(Expr* leftExpr, Expr* rightExpr, short op) {
    this->op = op;
    this->leftExpr = leftExpr;
    this->rightExpr = rightExpr;
} //-- RelationalExpr

RelationalExpr::~RelationalExpr() {
    delete leftExpr;
    delete rightExpr;
} //-- ~RelationalExpr

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* RelationalExpr::evaluate(Node* context, ContextState* cs) {


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

    MBool result = MB_FALSE;

    switch ( op ) {
        case NOT_EQUAL:
            result = (MBool) (leftDbl != rightDbl);
            break;
        case LESS_THAN:
            result = (MBool) (leftDbl < rightDbl);
            break;
        case LESS_OR_EQUAL:
            result = (MBool) (leftDbl <= rightDbl);
            break;
        case GREATER_THAN :
            result = (MBool) (leftDbl > rightDbl);
            break;
        case GREATER_OR_EQUAL:
            result = (MBool) (leftDbl >= rightDbl);
            break;
        default:
            result = (MBool) (leftDbl == rightDbl);
            break;
    }
   return new BooleanResult(result);
} //-- evaluate

/**
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
**/
void RelationalExpr::toString(String& str) {

    if ( leftExpr ) leftExpr->toString(str);
    else str.append("null");

    switch ( op ) {
        case NOT_EQUAL:
            str.append("!=");
            break;
        case LESS_THAN:
            str.append("<");
            break;
        case LESS_OR_EQUAL:
            str.append("<=");
            break;
        case GREATER_THAN :
            str.append(">");
            break;
        case GREATER_OR_EQUAL:
            str.append(">=");
            break;
        default:
            str.append("=");
            break;
    }

    if ( rightExpr ) rightExpr->toString(str);
    else str.append("null");

} //-- toString

