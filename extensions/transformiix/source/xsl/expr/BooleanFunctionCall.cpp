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

#include "FunctionLib.h"

/**
 * Creates a default BooleanFunctionCall, which always evaluates to False
**/
BooleanFunctionCall::BooleanFunctionCall() : FunctionCall(FALSE_FN) {
    this->type = FALSE;
} //-- BooleanFunctionCall

/**
 * Creates a BooleanFunctionCall of the given type
**/
BooleanFunctionCall::BooleanFunctionCall(short type) : FunctionCall()
{
    switch ( type ) {
        case BOOLEAN :
            FunctionCall::setName(BOOLEAN_FN);
            break;
        case NOT :
            FunctionCall::setName(NOT_FN);
            break;
        case TRUE :
            FunctionCall::setName(TRUE_FN);
            break;
        default:
            FunctionCall::setName(FALSE_FN);
            break;
    }
    this->type = type;
} //-- BooleanFunctionCall

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* BooleanFunctionCall::evaluate(Node* context, ContextState* cs) {

    BooleanResult* result = new BooleanResult();
    ListIterator* iter = params.iterator();
    int argc = params.getLength();
    Expr* param = 0;
    String err;


    switch ( type ) {
        case BOOLEAN :
            if ( requireParams(1,1,cs) ) {
                param = (Expr*)iter->next();
                ExprResult* exprResult = param->evaluate(context, cs);
                result->setValue(exprResult->booleanValue());
                delete exprResult;
            }
            break;
        case NOT :
            if ( requireParams(1,1,cs) ) {
                param = (Expr*)iter->next();
                ExprResult* exprResult = param->evaluate(context, cs);
                result->setValue(!exprResult->booleanValue());
                delete exprResult;
            }
            break;
        case TRUE :
            result->setValue(MB_TRUE);
            break;
        default:
            result->setValue(MB_FALSE);
            break;
    }
    delete iter;
    return result;
} //-- evaluate

