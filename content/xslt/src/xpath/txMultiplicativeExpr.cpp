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
 * NaN/Infinity code copied from the JS-library with permission from
 * Netscape Communications Corporation: http://www.mozilla.org/js
 * http://lxr.mozilla.org/seamonkey/source/js/src/jsinterp.c
 *
 */

/**
 * Represents a MultiplicativeExpr, an binary expression that
 * performs a multiplicative operation between it's lvalue and rvalue:
 *  *   : multiply
 * mod  : modulus
 * div  : divide
**/

#include "Expr.h"
#include "ExprResult.h"
#include <math.h>
#include "primitives.h"

/**
 * Creates a new MultiplicativeExpr using the given operator
**/
MultiplicativeExpr::MultiplicativeExpr(Expr* leftExpr, Expr* rightExpr, short op) {
    this->op = op;
    this->leftExpr = leftExpr;
    this->rightExpr = rightExpr;
} //-- MultiplicativeExpr

MultiplicativeExpr::~MultiplicativeExpr() {
    delete leftExpr;
    delete rightExpr;
} //-- ~MultiplicativeExpr

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* MultiplicativeExpr::evaluate(txIEvalContext* aContext)
{
    double rightDbl = Double::NaN;
    ExprResult* exprRes = 0;

    if ( rightExpr ) {
        exprRes = rightExpr->evaluate(aContext);
        if ( exprRes ) rightDbl = exprRes->numberValue();
        delete exprRes;
    }

    double leftDbl = Double::NaN;
    if ( leftExpr ) {
        exprRes = leftExpr->evaluate(aContext);
        if ( exprRes ) leftDbl = exprRes->numberValue();
        delete exprRes;
    }

    double result = 0;

    switch ( op ) {
        case DIVIDE:
            if (rightDbl == 0) {
#ifdef XP_PC
                /* XXX MSVC miscompiles such that (NaN == 0) */
                if (Double::isNaN(rightDbl))
                    result = Double::NaN;
                else
#endif
                if (leftDbl == 0 || Double::isNaN(leftDbl))
                    result = Double::NaN;
                else if (Double::isNeg(leftDbl) ^ Double::isNeg(rightDbl))
                    result = Double::NEGATIVE_INFINITY;
                else
                    result = Double::POSITIVE_INFINITY;
            }
            else
                result = leftDbl / rightDbl;
            break;
        case MODULUS:
            if (rightDbl == 0) {
                result = Double::NaN;
            }
            else {
#ifdef XP_PC
                /* Workaround MS fmod bug where 42 % (1/0) => NaN, not 42. */
                if (!Double::isInfinite(leftDbl) && Double::isInfinite(rightDbl))
                    result = leftDbl;
                else
#endif
                result = fmod(leftDbl, rightDbl);
            }
            break;
        default:
            result = leftDbl * rightDbl;
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
void MultiplicativeExpr::toString(String& str) {

    if ( leftExpr ) leftExpr->toString(str);
    else str.append("null");

    switch ( op ) {
        case DIVIDE:
            str.append(" div ");
            break;
        case MODULUS:
            str.append(" mod ");
            break;
        default:
            str.append(" * ");
            break;
    }
    if ( rightExpr ) rightExpr->toString(str);
    else str.append("null");

} //-- toString

