#include "txAtoms.h"
#include "XSLTFunctions.h"
#include "txExecutionState.h"

/*
  Implementation of XSLT 1.0 extension function: current
*/

/**
 * Creates a new current function call
**/
CurrentFunctionCall::CurrentFunctionCall() 
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
    txExecutionState* es = 
        NS_STATIC_CAST(txExecutionState*, aContext->getPrivateContext());
    if (!es) {
        NS_ASSERTION(0,
            "called xslt extension function \"current\" with wrong context");
        // Just return an empty nodeset, this at least has the right
        // result type.
        return new NodeSet();
    }
    return new NodeSet(es->getEvalContext()->getContextNode());
}

nsresult CurrentFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    *aAtom = txXSLTAtoms::current;
    NS_ADDREF(*aAtom);
    return NS_OK;
}
