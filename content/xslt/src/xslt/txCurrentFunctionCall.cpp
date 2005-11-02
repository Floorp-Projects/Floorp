#include "XSLTFunctions.h"
#include "Names.h"

/*
  Implementation of XSLT 1.0 extension function: current
*/

/**
 * Creates a new current function call
**/
CurrentFunctionCall::CurrentFunctionCall(ProcessorState* ps) :
        FunctionCall(CURRENT_FN)
{
    this->processorState = ps;
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

    NodeSet* result = new NodeSet(1);
    result->add(processorState->getCurrentNode());
    return result;
} //-- evaluate

