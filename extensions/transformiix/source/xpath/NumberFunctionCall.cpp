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
#include "XMLDOMUtils.h"
#include <math.h>

/*
 * Creates a NumberFunctionCall of the given type
 */
NumberFunctionCall::NumberFunctionCall(NumberFunctions aType) {
    mType = aType;
    switch (mType) {
    case ROUND:
        name = XPathNames::ROUND_FN;
        break;
    case CEILING:
        name = XPathNames::CEILING_FN;
        break;
    case FLOOR:
        name = XPathNames::FLOOR_FN;
        break;
    case SUM:
        name = XPathNames::SUM_FN;
        break;
    case NUMBER:
        name = XPathNames::NUMBER_FN;
        break;
    }
}

/*
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context Node for evaluation of this Expr
 * @param ps      the ContextState containing the stack information needed
 *                for evaluation
 * @return the result of the evaluation
 */
ExprResult* NumberFunctionCall::evaluate(Node* context, ContextState* cs) {
    ListIterator iter(&params);

    if (mType == NUMBER) {
        if (!requireParams(0, 1, cs))
            return new NumberResult(Double::NaN);
    }
    else {
        if (!requireParams(1, 1, cs))
            return new NumberResult(Double::NaN);
    }

    switch (mType) {
        case CEILING:
        {
            double dbl = evaluateToNumber((Expr*)iter.next(), context, cs);
            if (Double::isNaN(dbl) || Double::isInfinite(dbl))
                return new NumberResult(dbl);

            if (Double::isNeg(dbl) && dbl > -1)
                return new NumberResult(0 * dbl);

            return new NumberResult(ceil(dbl));
        }
        case FLOOR:
        {
            double dbl = evaluateToNumber((Expr*)iter.next(), context, cs);
            if (Double::isNaN(dbl) ||
                Double::isInfinite(dbl) ||
                (dbl == 0 && Double::isNeg(dbl)))
                return new NumberResult(dbl);

            return new NumberResult(floor(dbl));
        }
        case ROUND:
        {
            double dbl = evaluateToNumber((Expr*)iter.next(), context, cs);
            if (Double::isNaN(dbl) || Double::isInfinite(dbl))
                return new NumberResult(dbl);

            if (Double::isNeg(dbl) && dbl >= -0.5)
                return new NumberResult(0 * dbl);

            return new NumberResult(floor(dbl + 0.5));
        }
        case SUM:
        {
            ExprResult* exprResult =
                ((Expr*)iter.next())->evaluate(context, cs);

            if (!exprResult)
                return 0;

            if (exprResult->getResultType() != ExprResult::NODESET) {
                String err("NodeSet expected in call to sum(): ");
                toString(err);
                cs->recieveError(err);
                return new NumberResult(Double::NaN);
            }

            double res = 0;
            NodeSet* nodes = (NodeSet*)exprResult;
            int i;
            for (i = 0; i < nodes->size(); i++) {
                String resultStr;
                XMLDOMUtils::getNodeValue(nodes->get(i), &resultStr);
                res += Double::toDouble(resultStr);
            }
            delete exprResult;

            return new NumberResult(res);
        }
        case NUMBER:
        {
            if (iter.hasNext())
                return new NumberResult(
                    evaluateToNumber((Expr*)iter.next(), context, cs));

            String resultStr;
            XMLDOMUtils::getNodeValue(context, &resultStr);
            return new NumberResult(Double::toDouble(resultStr));
        }
    }
    return new NumberResult(Double::NaN);
}
