#include "XSLTFunctions.h"
#include "ProcessorState.h"
#include "Names.h"

/*
  Implementation of XSLT 1.0 extension function: current
*/

/**
 * Creates a new current function call
**/
CurrentFunctionCall::CurrentFunctionCall(ProcessorState* aPs) 
    : FunctionCall(CURRENT_FN), mPs(aPs)
{
}

/*
 * Evaluates this Expr
 *
 * @return NodeSet containing the context node used for the complete
 * Expr or Pattern.
 */
ExprResult* CurrentFunctionCall::evaluate(txIEvalContext* aContext)
{
    return new NodeSet(mPs->getEvalContext()->getContextNode());
}

