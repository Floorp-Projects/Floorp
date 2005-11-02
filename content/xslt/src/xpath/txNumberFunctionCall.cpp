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
 *
 * Olivier Gerardin, ogerardin@vo.lu
 *   -- original author.
 *
 * Nisheeth Ranjan, nisheeth@netscape.com
 *   -- implemented rint function, which was not available on Windows.
 *
 */

/*
 * NumberFunctionCall
 * A representation of the XPath Number funtions
 */

#include "FunctionLib.h"
#include <math.h>
#include "NodeSet.h"
#include "txAtoms.h"
#include "txIXPathContext.h"
#include "XMLDOMUtils.h"

/*
 * Creates a NumberFunctionCall of the given type
 */
NumberFunctionCall::NumberFunctionCall(NumberFunctions aType)
    : mType(aType)
{
}

/*
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context Node for evaluation of this Expr
 * @param ps      the ContextState containing the stack information needed
 *                for evaluation
 * @return the result of the evaluation
 */
nsresult
NumberFunctionCall::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nsnull;

    txListIterator iter(&params);
    if (mType == NUMBER) {
        if (!requireParams(0, 1, aContext))
            return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
    }
    else {
        if (!requireParams(1, 1, aContext))
            return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
    }

    switch (mType) {
        case CEILING:
        {
            double dbl = evaluateToNumber((Expr*)iter.next(), aContext);
            if (!Double::isNaN(dbl) && !Double::isInfinite(dbl)) {
                if (Double::isNeg(dbl) && dbl > -1) {
                    dbl *= 0;
                }
                else {
                    dbl = ceil(dbl);
                }
            }

            return aContext->recycler()->getNumberResult(dbl, aResult);
        }
        case FLOOR:
        {
            double dbl = evaluateToNumber((Expr*)iter.next(), aContext);
            if (!Double::isNaN(dbl) &&
                !Double::isInfinite(dbl) &&
                !(dbl == 0 && Double::isNeg(dbl))) {
                dbl = floor(dbl);
            }

            return aContext->recycler()->getNumberResult(dbl, aResult);
        }
        case ROUND:
        {
            double dbl = evaluateToNumber((Expr*)iter.next(), aContext);
            if (!Double::isNaN(dbl) && !Double::isInfinite(dbl)) {
                if (Double::isNeg(dbl) && dbl >= -0.5) {
                    dbl *= 0;
                }
                else {
                    dbl = floor(dbl + 0.5);
                }
            }

            return aContext->recycler()->getNumberResult(dbl, aResult);
        }
        case SUM:
        {
            nsRefPtr<NodeSet> nodes;
            nsresult rv = evaluateToNodeSet((Expr*)iter.next(), aContext,
                                            getter_AddRefs(nodes));
            NS_ENSURE_SUCCESS(rv, rv);

            double res = 0;
            int i;
            for (i = 0; i < nodes->size(); i++) {
                nsAutoString resultStr;
                XMLDOMUtils::getNodeValue(nodes->get(i), resultStr);
                res += Double::toDouble(resultStr);
            }
            return aContext->recycler()->getNumberResult(res, aResult);
        }
        case NUMBER:
        {
            double res;
            if (iter.hasNext()) {
                res = evaluateToNumber((Expr*)iter.next(), aContext);
            }
            else {
                nsAutoString resultStr;
                XMLDOMUtils::getNodeValue(aContext->getContextNode(),
                                          resultStr);
                res = Double::toDouble(resultStr);
            }
            return aContext->recycler()->getNumberResult(res, aResult);
        }
    }

    aContext->receiveError(NS_LITERAL_STRING("Internal error"),
                           NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;
}

nsresult NumberFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    switch (mType) {
        case NUMBER:
        {
            *aAtom = txXPathAtoms::number;
            break;
        }
        case ROUND:
        {
            *aAtom = txXPathAtoms::round;
            break;
        }
        case FLOOR:
        {
            *aAtom = txXPathAtoms::floor;
            break;
        }
        case CEILING:
        {
            *aAtom = txXPathAtoms::ceiling;
            break;
        }
        case SUM:
        {
            *aAtom = txXPathAtoms::sum;
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
