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

#include "FunctionLib.h"
#include "txAtoms.h"
#include "txIXPathContext.h"

/**
 * Creates a default BooleanFunctionCall, which always evaluates to False
**/
BooleanFunctionCall::BooleanFunctionCall(BooleanFunctions aType)
    : mType(aType)
{
    switch (aType) {
        case TX_BOOLEAN :
            name = XPathNames::BOOLEAN_FN;
            break;
        case TX_LANG:
            name = XPathNames::LANG_FN;
            break;
        case TX_NOT :
            name = XPathNames::NOT_FN;
            break;
        case TX_TRUE :
            name = XPathNames::TRUE_FN;
            break;
        default:
            name = XPathNames::FALSE_FN;
            break;
    }
} //-- BooleanFunctionCall

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* BooleanFunctionCall::evaluate(txIEvalContext* aContext)
{
    txListIterator iter(&params);

    switch (mType) {
        case TX_BOOLEAN:
        {
            if (!requireParams(1, 1, aContext))
                return new StringResult("error");

            return new BooleanResult(evaluateToBoolean((Expr*)iter.next(),
                                                       aContext));
        }
        case TX_LANG:
        {
            if (!requireParams(1, 1, aContext))
                return new StringResult("error");

            String lang;
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
                String arg;
                evaluateToString((Expr*)iter.next(), aContext, arg);
                arg.toUpperCase(); // case-insensitive comparison
                lang.toUpperCase();
                result = lang.indexOf(arg) == 0 &&
                         (lang.length() == arg.length() ||
                          lang.charAt(arg.length()) == '-');
            }

            return new BooleanResult(result);
        }
        case TX_NOT:
        {
            if (!requireParams(1, 1, aContext))
                return new StringResult("error");

            return new BooleanResult(!evaluateToBoolean((Expr*)iter.next(),
                                                        aContext));
        }
        case TX_TRUE:
        {
            if (!requireParams(0, 0, aContext))
                return new StringResult("error");

            return new BooleanResult(MB_TRUE);
        }
        case TX_FALSE:
        {
            if (!requireParams(0, 0, aContext))
                return new StringResult("error");

            return new BooleanResult(MB_FALSE);
        }
    }

    String err("Internal error");
    aContext->receiveError(err, NS_ERROR_UNEXPECTED);
    return new StringResult("error");
}

