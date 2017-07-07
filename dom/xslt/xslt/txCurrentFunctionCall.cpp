/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGkAtoms.h"
#include "txXSLTFunctions.h"
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
nsresult
CurrentFunctionCall::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nullptr;

    if (!requireParams(0, 0, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

    txExecutionState* es =
        static_cast<txExecutionState*>(aContext->getPrivateContext());
    if (!es) {
        NS_ERROR(
            "called xslt extension function \"current\" with wrong context");
        return NS_ERROR_UNEXPECTED;
    }
    return aContext->recycler()->getNodeSet(
           es->getEvalContext()->getContextNode(), aResult);
}

Expr::ResultType
CurrentFunctionCall::getReturnType()
{
    return NODESET_RESULT;
}

bool
CurrentFunctionCall::isSensitiveTo(ContextSensitivity aContext)
{
    return !!(aContext & PRIVATE_CONTEXT);
}

#ifdef TX_TO_STRING
nsresult
CurrentFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    *aAtom = nsGkAtoms::current;
    NS_ADDREF(*aAtom);
    return NS_OK;
}
#endif
