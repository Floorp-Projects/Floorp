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
 * $Id: RelationalExpr.cpp,v 1.2 1999/11/15 07:13:13 nisheeth%netscape.com Exp $
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

