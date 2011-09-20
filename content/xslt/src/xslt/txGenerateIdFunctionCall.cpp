/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Keith Visco.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsGkAtoms.h"
#include "txIXPathContext.h"
#include "txNodeSet.h"
#include "txXPathTreeWalker.h"
#include "txXSLTFunctions.h"
#include "txExecutionState.h"

/*
  Implementation of XSLT 1.0 extension function: generate-id
*/

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

    txExecutionState* es = 
        static_cast<txExecutionState*>(aContext->getPrivateContext());
    if (!es) {
        NS_ERROR(
            "called xslt extension function \"current\" with wrong context");
        return NS_ERROR_UNEXPECTED;
    }

    nsresult rv = NS_OK;
    if (mParams.IsEmpty()) {
        StringResult* strRes;
        rv = aContext->recycler()->getStringResult(&strRes);
        NS_ENSURE_SUCCESS(rv, rv);

        txXPathNodeUtils::getXSLTId(aContext->getContextNode(),
                                    es->getSourceDocument(),
                                    strRes->mValue);

        *aResult = strRes;
 
        return NS_OK;
    }

    nsRefPtr<txNodeSet> nodes;
    rv = evaluateToNodeSet(mParams[0], aContext,
                           getter_AddRefs(nodes));
    NS_ENSURE_SUCCESS(rv, rv);

    if (nodes->isEmpty()) {
        aContext->recycler()->getEmptyStringResult(aResult);

        return NS_OK;
    }
    
    StringResult* strRes;
    rv = aContext->recycler()->getStringResult(&strRes);
    NS_ENSURE_SUCCESS(rv, rv);

    txXPathNodeUtils::getXSLTId(nodes->get(0), es->getSourceDocument(),
                                strRes->mValue);

    *aResult = strRes;
 
    return NS_OK;
}

Expr::ResultType
GenerateIdFunctionCall::getReturnType()
{
    return STRING_RESULT;
}

PRBool
GenerateIdFunctionCall::isSensitiveTo(ContextSensitivity aContext)
{
    if (mParams.IsEmpty()) {
        return !!(aContext & NODE_CONTEXT);
    }

    return argsSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
nsresult
GenerateIdFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    *aAtom = nsGkAtoms::generateId;
    NS_ADDREF(*aAtom);
    return NS_OK;
}
#endif
