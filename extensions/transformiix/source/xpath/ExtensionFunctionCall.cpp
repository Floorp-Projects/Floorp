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
 * The Original Code is XSL:P XSLT processor.
 * 
 * The Initial Developer of the Original Code is Keith Visco.
 * Portions created by Keith Visco are Copyright (C) 1999, 2000 Keith Visco.
 * All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *   -- original author.
 * 
 */

#include "FunctionLib.h"


const String ExtensionFunctionCall::UNDEFINED_FUNCTION = "Undefined Function: ";

/**
 * Creates an ExtensionFunctionCall with the given function name.
 * @param name the name of the extension function which to invoke
**/
ExtensionFunctionCall::ExtensionFunctionCall(const String& name) : FunctionCall(name)
{
   this->fname = name;
   this->fnCall = 0;

} //-- ExtensionFunctionCall

/**
 * Deletes an instance of ExtensionFunctioncall
**/
ExtensionFunctionCall::~ExtensionFunctionCall() {
    delete fnCall;
} //-- ~ExtensionFunctionCall

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* ExtensionFunctionCall::evaluate(Node* context, ContextState* cs) {

    //-- check for existing function call resolution

    if (!fnCall) {
        fnCall = cs->resolveFunctionCall(fname);

        if (!fnCall) {
            String err(UNDEFINED_FUNCTION);
            err.append(fname);
            return new StringResult(err);
        }

        //copy parameters
        ListIterator* iter = params.iterator();
        while (iter->hasNext()) {
           fnCall->addParam( new ExprWrapper( (Expr*) iter->next() ) );
        }
        delete iter;
    }

    //-- delegate
    return fnCall->evaluate(context, cs);

} //-- evaluate

//---------------------------------/
//- Implementation of ExprWrapper -/
//---------------------------------/

/**
 * Creates a new ExprWrapper for the given Expr
**/
ExprWrapper::ExprWrapper(Expr* expr) {
    this->expr = expr;
}

/**
 * Destructor for ExprWrapper
**/
ExprWrapper::~ExprWrapper() {
    //-- DO NOTHING!
}

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* ExprWrapper::evaluate(Node* context, ContextState* cs) {
   //-- just delegate
   if (!expr) return 0; // <-- hopefully this will never happen
   return expr->evaluate(context, cs);
} //-- evaluate

/**
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
**/
void ExprWrapper::toString(String& str) {
    //-- just delegate
    if (expr) expr->toString(str);
} //-- toString

