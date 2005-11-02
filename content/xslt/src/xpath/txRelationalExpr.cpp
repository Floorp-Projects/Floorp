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
 * $Id: txRelationalExpr.cpp,v 1.5 2005/11/02 07:33:48 peterv%netscape.com Exp $
 */

#include "Expr.h"
#include "XMLDOMUtils.h"

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
 *  Compares the two ExprResults based on XPath 1.0 Recommendation (section 3.4)
**/
MBool RelationalExpr::compareResults(ExprResult* left, ExprResult* right) {


    short ltype = left->getResultType();
    short rtype = right->getResultType();

    MBool result = MB_FALSE;

    //-- handle case for just Left NodeSet or Both NodeSets
    if (ltype == ExprResult::NODESET) {
        NodeSet* nodeSet = (NodeSet*)left;
        for ( int i = 0; i < nodeSet->size(); i++) {
                String str;
                Node* node = nodeSet->get(i);
                XMLDOMUtils::getNodeValue(node, &str);
                StringResult strResult(str);
                result = compareResults(&strResult, right);
                if ( result ) break;
        }
    }
    //-- handle case for Just Right NodeSet
    else if ( rtype == ExprResult::NODESET) {
        NodeSet* nodeSet = (NodeSet*)right;
        for ( int i = 0; i < nodeSet->size(); i++) {
                String str;
                Node* node = nodeSet->get(i);
                XMLDOMUtils::getNodeValue(node, &str);
                StringResult strResult(str);
                result = compareResults(left, &strResult);
                if ( result ) break;
        }
    }
    //-- neither NodeSet
    else {
        if ( op == NOT_EQUAL) {

            if ((ltype == ExprResult::BOOLEAN)
                    || (rtype == ExprResult::BOOLEAN)) {
                result = (left->booleanValue() != right->booleanValue());
            }
            else if ((ltype == ExprResult::NUMBER) ||
                            (rtype == ExprResult::NUMBER)) {
                result = (left->numberValue() != right->numberValue());
            }
            else {
                String lStr;
                left->stringValue(lStr);
                String rStr;
                right->stringValue(rStr);
                result = !lStr.isEqual(rStr);
            }
        }
        else if ( op == EQUAL) {

            if ((ltype == ExprResult::BOOLEAN)
                    || (rtype == ExprResult::BOOLEAN)) {
                result = (left->booleanValue() == right->booleanValue());
            }
            else if ((ltype == ExprResult::NUMBER) ||
                            (rtype == ExprResult::NUMBER)) {
                result = (left->numberValue() == right->numberValue());
            }
            else {
                String lStr;
                left->stringValue(lStr);
                String rStr;
                right->stringValue(rStr);
                result = lStr.isEqual(rStr);
            }

        }
        else {
            double leftDbl = left->numberValue();
            double rightDbl = right->numberValue();
            switch( op ) {
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
            }
        }
    }
    return result;
} //-- compareResult

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* RelationalExpr::evaluate(Node* context, ContextState* cs) {

    //-- get result of left expression
    ExprResult* lResult = 0;
    if ( leftExpr ) lResult = leftExpr->evaluate(context, cs);
    else return new BooleanResult();

    //-- get result of right expr
    ExprResult* rResult = 0;
    if ( rightExpr ) rResult = rightExpr->evaluate(context, cs);
    else {
        delete lResult;
        return new BooleanResult();
    }
    BooleanResult* boolResult = new BooleanResult(compareResults(lResult, rResult));
    delete lResult;
    delete rResult;
    return boolResult;
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

