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
 * Portions created by Keith Visco (C) 1999, 2000 Keith Visco.
 * All Rights Reserved.
 *
 * Contributor(s):
 *
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */

#include "txAtoms.h"
#include "txIXPathContext.h"
#include "XSLTFunctions.h"
#include "prprf.h"
#include "NodeSet.h"

/*
  Implementation of XSLT 1.0 extension function: generate-id
*/

#ifndef HAVE_64BIT_OS
const char GenerateIdFunctionCall::printfFmt[] = "id0x%08p";
#else
const char GenerateIdFunctionCall::printfFmt[] = "id0x%016p";
#endif

/**
 * Creates a new generate-id function call
**/
GenerateIdFunctionCall::GenerateIdFunctionCall()
{
}

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
 * @see FunctionCall.h
**/
nsresult
GenerateIdFunctionCall::evaluate(txIEvalContext* aContext,
                                 txAExprResult** aResult)
{
    *aResult = nsnull;
    if (!requireParams(0, 1, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

    Node* node = 0;

    // get node to generate id for
    if (params.getLength() == 1) {
        txListIterator iter(&params);
        Expr* param = (Expr*)iter.next();

        nsRefPtr<txAExprResult> exprResult;
        nsresult rv = param->evaluate(aContext, getter_AddRefs(exprResult));
        NS_ENSURE_SUCCESS(rv, rv);

        if (exprResult->getResultType() != txAExprResult::NODESET) {
            NS_NAMED_LITERAL_STRING(err, "Invalid argument passed to generate-id(), expecting NodeSet");
            aContext->receiveError(err, NS_ERROR_XSLT_NODESET_EXPECTED);
            return NS_ERROR_XSLT_NODESET_EXPECTED;
        }

        NodeSet* nodes = NS_STATIC_CAST(NodeSet*,
                                        NS_STATIC_CAST(txAExprResult*,
                                                       exprResult));
        if (nodes->isEmpty()) {
            aContext->recycler()->getEmptyStringResult(aResult);

            return NS_OK;
        }

        node = nodes->get(0);
    }
    else {
        node = aContext->getContextNode();
    }

    // generate id for selected node
    char buf[22];
    PR_snprintf(buf, 21, printfFmt, node);
    return aContext->recycler()->getStringResult(NS_ConvertASCIItoUCS2(buf),
                                                 aResult);
}

nsresult GenerateIdFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    *aAtom = txXSLTAtoms::generateId;
    NS_ADDREF(*aAtom);
    return NS_OK;
}
