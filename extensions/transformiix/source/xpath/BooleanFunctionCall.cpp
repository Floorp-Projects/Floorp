/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *   -- original author.
 *
 * Marina Mechtcheriakova, mmarina@mindspring.com
 *   -- added lang() implementation
 *
 */

#include "ExprResult.h"
#include "FunctionLib.h"
#include "txAtoms.h"
#include "txIXPathContext.h"
#include "nsReadableUtils.h"
#include "txStringUtils.h"

/**
 * Creates a default BooleanFunctionCall, which always evaluates to False
**/
BooleanFunctionCall::BooleanFunctionCall(BooleanFunctions aType)
    : mType(aType)
{
}

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
nsresult
BooleanFunctionCall::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nsnull;

    txListIterator iter(&params);
    switch (mType) {
        case TX_BOOLEAN:
        {
            if (!requireParams(1, 1, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            aContext->recycler()->getBoolResult(
                evaluateToBoolean((Expr*)iter.next(), aContext), aResult);

            return NS_OK;
        }
        case TX_LANG:
        {
            if (!requireParams(1, 1, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            nsAutoString lang;
            Node* node = aContext->getContextNode();
            while (node) {
                if (node->getNodeType() == Node::ELEMENT_NODE) {
                    Element* elem = (Element*)node;
                    if (elem->getAttr(txXMLAtoms::lang,
                                      kNameSpaceID_XML, lang))
                        break;
                }
                node = node->getParentNode();
            }

            MBool result = MB_FALSE;
            if (node) {
                nsAutoString arg;
                evaluateToString((Expr*)iter.next(), aContext, arg);
                result = arg.Equals(Substring(lang, 0, arg.Length()),
                                    txCaseInsensitiveStringComparator()) &&
                         (lang.Length() == arg.Length() ||
                          lang.CharAt(arg.Length()) == '-');
            }

            aContext->recycler()->getBoolResult(result, aResult);

            return NS_OK;
        }
        case TX_NOT:
        {
            if (!requireParams(1, 1, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            aContext->recycler()->getBoolResult(
                  !evaluateToBoolean((Expr*)iter.next(), aContext), aResult);

            return NS_OK;
        }
        case TX_TRUE:
        {
            if (!requireParams(0, 0, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            aContext->recycler()->getBoolResult(PR_TRUE, aResult);

            return NS_OK;
        }
        case TX_FALSE:
        {
            if (!requireParams(0, 0, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            aContext->recycler()->getBoolResult(PR_FALSE, aResult);

            return NS_OK;
        }
    }

    aContext->receiveError(NS_LITERAL_STRING("Internal error"),
                           NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;
}

nsresult BooleanFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    switch (mType) {
        case TX_BOOLEAN:
        {
            *aAtom = txXPathAtoms::boolean;
            break;
        }
        case TX_LANG:
        {
            *aAtom = txXPathAtoms::lang;
            break;
        }
        case TX_NOT:
        {
            *aAtom = txXPathAtoms::_not;
            break;
        }
        case TX_TRUE:
        {
            *aAtom = txXPathAtoms::_true;
            break;
        }
        case TX_FALSE:
        {
            *aAtom = txXPathAtoms::_false;
            break;
        }
        default:
        {
            *aAtom = 0;
            return NS_ERROR_FAILURE;
        }
    }
    NS_ADDREF(*aAtom);
    return NS_OK;
}
