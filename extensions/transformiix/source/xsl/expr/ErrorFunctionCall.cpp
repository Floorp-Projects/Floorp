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
 *  Creates an Error FunctionCall with no error message
**/
ErrorFunctionCall::ErrorFunctionCall() : FunctionCall(ERROR_FN) {};

/**
 * Creates an Error FunctionCall with the given error message
**/
ErrorFunctionCall::ErrorFunctionCall(const String& errorMsg) : FunctionCall(ERROR_FN) {
    //-- copy errorMsg
    this->errorMessage = errorMsg;
} //-- ErrorFunctionCall

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* ErrorFunctionCall::evaluate(Node* context, ContextState* cs) {
    return new StringResult( errorMessage);
} //-- evaluate

void ErrorFunctionCall::setErrorMessage(String& errorMsg) {
    //-- copy errorMsg
    this->errorMessage = errorMsg;
} //-- setError
