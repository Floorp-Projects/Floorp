#include "XSLTFunctions.h"

/*
  Implementation of XSLT 1.0 extension function: current
*/

/**
 * Creates a new current function call
**/
CurrentFunctionCall::CurrentFunctionCall() :
        FunctionCall(CURRENT_FN)
{
}

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param cs the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
 * @see FunctionCall.h
**/
ExprResult* CurrentFunctionCall::evaluate(Node* context, ContextState* cs) {
//    NodeSet* result = new NodeSet(0);
//    result->add(cs->getCurrentNode);
//    return result;
    return new StringResult("function not yet implemented: current");
}

