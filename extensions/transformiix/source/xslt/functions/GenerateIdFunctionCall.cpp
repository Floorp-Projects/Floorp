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

#include "XSLTFunctions.h"
#include "Names.h"
#ifdef TX_EXE
#include "stdio.h"
#else
#include "prprf.h"
#endif

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
    : FunctionCall(GENERATE_ID_FN)
{}

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
 * @see FunctionCall.h
**/
ExprResult* GenerateIdFunctionCall::evaluate(Node* aContext,
                                             ContextState* aCs) {

    if (!requireParams(0, 1, aCs))
        return new StringResult();

    Node* node = 0;

    // get node to generate id for
    if (params.getLength() == 1) {
        txListIterator iter(&params);
        Expr* param = (Expr*)iter.next();

        ExprResult* exprResult = param->evaluate(aContext, aCs);
        if (!exprResult)
            return 0;

        if (exprResult->getResultType() != ExprResult::NODESET) {
            String err("Invalid argument passed to generate-id(), "
                       "expecting NodeSet");
            aCs->recieveError(err);
            delete exprResult;
            return new StringResult(err);
        }

        NodeSet* nodes = (NodeSet*) exprResult;
        if (nodes->size() > 0) {
            aCs->sortByDocumentOrder(nodes);
            node = nodes->get(0);
        }
        delete exprResult;
    }
    else {
        node = aContext;
    }

    // generate id for selected node
    char buf[22];
    if (node) {
#ifdef TX_EXE
        sprintf(buf, printfFmt, node);
#else
        PR_snprintf(buf, 21, printfFmt, node);
#endif
    }
    else {
        buf[0] = 0;
    }
    return new StringResult(buf);
}
