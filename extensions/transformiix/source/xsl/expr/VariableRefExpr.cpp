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

  //-------------------/
 //- VariableRefExpr -/
//-------------------/

/**
 * Default constructor
**/
VariableRefExpr::VariableRefExpr() {
} //-- VariableRefExpr

/**
 * Creates a VariableRefExpr with the given variable name
**/
VariableRefExpr::VariableRefExpr(const String& name) {
    this->name = name;
} //-- VariableRefExpr

/**
 * Creates a VariableRefExpr with the given variable name
**/
VariableRefExpr::VariableRefExpr(String& name) {
    this->name = name;
} //-- VariableRefExpr

/**
 * Default destructor
**/
VariableRefExpr::~VariableRefExpr() {
} //-- ~VariableRefExpr

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* VariableRefExpr::evaluate(Node* context, ContextState* cs) {

    ExprResult* exprResult = cs->getVariable(name);
    //-- make copy to prevent deletetion
    //-- I know, I should add a #copy method to ExprResult, I will
    ExprResult* copyOfResult = 0;

    if ( exprResult ) {
        switch ( exprResult->getResultType() ) {
            //-- BooleanResult
            case ExprResult::BOOLEAN :
                copyOfResult = new BooleanResult(exprResult->booleanValue());
                break;
            //-- NodeSet
            case ExprResult::NODESET :
            {
                NodeSet* src = (NodeSet*)exprResult;
                NodeSet* dest = new NodeSet(src->size());
                for ( int i = 0; i < src->size(); i++)
                    dest->add(src->get(i));
                copyOfResult = dest;
                break;
            }
            //-- NumberResult
            case ExprResult::NUMBER :
                copyOfResult = new NumberResult(exprResult->numberValue());
                break;
            //-- StringResult
            default:
                StringResult* strResult = new StringResult();
                exprResult->stringValue(strResult->getValue());
                copyOfResult = strResult;
                break;
        }
    }
    else copyOfResult = new StringResult();

    return copyOfResult;
} //-- evaluate

/**
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
**/
void VariableRefExpr::toString(String& str) {
    str.append('$');
    str.append(name);
} //-- toString

