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
 */

/**
 * StringFunctionCall
 * A representation of the XPath String funtions
**/

#include "ExprResult.h"
#include "FunctionLib.h"
#include "txAtoms.h"
#include "txIXPathContext.h"
#include "XMLDOMUtils.h"
#include "XMLUtils.h"
#include <math.h>

/**
 * Creates a StringFunctionCall of the given type
**/
StringFunctionCall::StringFunctionCall(StringFunctions aType) : mType(aType)
{
}

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* StringFunctionCall::evaluate(txIEvalContext* aContext)
{
    txListIterator iter(&params);
    switch (mType) {
        case CONCAT:
        {
            if (!requireParams(2, aContext))
                return new StringResult(NS_LITERAL_STRING("error"));
                
            nsAutoString resultStr;
            while (iter.hasNext()) {
                evaluateToString((Expr*)iter.next(), aContext, resultStr);
            }
            return new StringResult(resultStr);
        }
        case CONTAINS:
        {
            if (!requireParams(2, 2, aContext))
                return new StringResult(NS_LITERAL_STRING("error"));

            nsAutoString arg1, arg2;
            Expr* arg1Expr = (Expr*)iter.next();
            evaluateToString((Expr*)iter.next(), aContext, arg2);
            if (arg2.IsEmpty())
                return new BooleanResult(PR_TRUE);

            evaluateToString(arg1Expr, aContext, arg1);
            return new BooleanResult(arg1.Find(arg2) >= 0);
        }
        case NORMALIZE_SPACE:
        {
            if (!requireParams(0, 1, aContext))
                return new StringResult(NS_LITERAL_STRING("error"));

            nsAutoString resultStr;
            if (iter.hasNext())
                evaluateToString((Expr*)iter.next(), aContext, resultStr);
            else
                XMLDOMUtils::getNodeValue(aContext->getContextNode(),
                                          resultStr);

            MBool addSpace = MB_FALSE;
            MBool first = MB_TRUE;
            nsAutoString normed;
            normed.SetCapacity(resultStr.Length());
            PRUnichar c;
            PRUint32 src;
            for (src = 0; src < resultStr.Length(); src++) {
                c = resultStr.CharAt(src);
                if (XMLUtils::isWhitespace(c)) {
                    addSpace = MB_TRUE;
                }
                else {
                    if (addSpace && !first)
                        normed.Append(PRUnichar(' '));

                    normed.Append(c);
                    addSpace = MB_FALSE;
                    first = MB_FALSE;
                }
            }
            return new StringResult(normed);
        }
        case STARTS_WITH:
        {
            if (!requireParams(2, 2, aContext))
                return new StringResult(NS_LITERAL_STRING("error"));

            nsAutoString arg1, arg2;
            Expr* arg1Expr = (Expr*)iter.next();
            evaluateToString((Expr*)iter.next(), aContext, arg2);
            if (arg2.IsEmpty())
                return new BooleanResult(PR_TRUE);

            evaluateToString(arg1Expr, aContext, arg1);
            return new BooleanResult(arg1.Find(arg2) == 0);
        }
        case STRING_LENGTH:
        {
            if (!requireParams(0, 1, aContext))
                return new StringResult(NS_LITERAL_STRING("error"));

            nsAutoString resultStr;
            if (iter.hasNext())
                evaluateToString((Expr*)iter.next(), aContext, resultStr);
            else
                XMLDOMUtils::getNodeValue(aContext->getContextNode(),
                                          resultStr);
            return new NumberResult(resultStr.Length());
        }
        case SUBSTRING:
        {
            if (!requireParams(2, 3, aContext))
                return new StringResult(NS_LITERAL_STRING("error"));

            nsAutoString src;
            double start, end;
            evaluateToString((Expr*)iter.next(), aContext, src);
            start = evaluateToNumber((Expr*)iter.next(), aContext);

            // check for NaN or +/-Inf
            if (Double::isNaN(start) ||
                Double::isInfinite(start) ||
                start >= src.Length() + 0.5)
                return new StringResult();

            start = floor(start + 0.5) - 1;
            if (iter.hasNext()) {
                end = start + evaluateToNumber((Expr*)iter.next(),
                                               aContext);
                if (Double::isNaN(end) || end < 0)
                    return new StringResult();
                
                if (end > src.Length())
                    end = src.Length();
                else
                    end = floor(end + 0.5);
            }
            else {
                end = src.Length();
            }

            if (start < 0)
                start = 0;
 
            if (start > end)
                return new StringResult();
            
            return new StringResult(Substring(src, (PRUint32)start,
                                              (PRUint32)end - (PRUint32)start));
        }
        case SUBSTRING_AFTER:
        {
            if (!requireParams(2, 2, aContext))
                return new StringResult(NS_LITERAL_STRING("error"));

            nsAutoString arg1, arg2;
            evaluateToString((Expr*)iter.next(), aContext, arg1);
            evaluateToString((Expr*)iter.next(), aContext, arg2);
            if (arg2.IsEmpty())
                return new StringResult(arg1);

            PRInt32 idx = arg1.Find(arg2);
            if (idx != kNotFound) {
                PRUint32 len = arg2.Length();
                return new StringResult(Substring(arg1, idx + len,
                                                  arg1.Length() - (idx + len)));
            }
            return new StringResult();
        }
        case SUBSTRING_BEFORE:
        {
            if (!requireParams(2, 2, aContext))
                return new StringResult(NS_LITERAL_STRING("error"));

            nsAutoString arg1, arg2;
            Expr* arg1Expr = (Expr*)iter.next();
            evaluateToString((Expr*)iter.next(), aContext, arg2);
            if (arg2.IsEmpty())
                return new StringResult();

            evaluateToString(arg1Expr, aContext, arg1);

            PRInt32 idx = arg1.Find(arg2);
            if (idx != kNotFound) {
                return new StringResult(Substring(arg1, 0, idx));
            }
            return new StringResult();
        }
        case TRANSLATE:
        {
            if (!requireParams(3, 3, aContext))
                return new StringResult(NS_LITERAL_STRING("error"));

            nsAutoString src;
            evaluateToString((Expr*)iter.next(), aContext, src);
            if (src.IsEmpty())
                return new StringResult();
            
            nsAutoString oldChars, newChars, dest;
            evaluateToString((Expr*)iter.next(), aContext, oldChars);
            evaluateToString((Expr*)iter.next(), aContext, newChars);
            PRUint32 i;
            PRInt32 newCharsLength = (PRInt32)newChars.Length();
            for (i = 0; i < src.Length(); i++) {
                PRInt32 idx = oldChars.FindChar(src.CharAt(i));
                if (idx != kNotFound) {
                    if (idx < newCharsLength)
                        dest.Append(newChars.CharAt((PRUint32)idx));
                }
                else {
                    dest.Append(src.CharAt(i));
                }
            }
            return new StringResult(dest);
        }
        case STRING:
        {
            if (!requireParams(0, 1, aContext))
                return new StringResult(NS_LITERAL_STRING("error"));

            nsAutoString resultStr;
            if (iter.hasNext())
                evaluateToString((Expr*)iter.next(), aContext, resultStr);
            else
                XMLDOMUtils::getNodeValue(aContext->getContextNode(),
                                          resultStr);
            return new StringResult(resultStr);
        }
    }

    aContext->receiveError(NS_LITERAL_STRING("Internal error"),
                           NS_ERROR_UNEXPECTED);
    return new StringResult(NS_LITERAL_STRING("error"));
}

nsresult StringFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    switch (mType) {
        case CONCAT:
        {
            *aAtom = txXPathAtoms::concat;
            break;
        }
        case CONTAINS:
        {
            *aAtom = txXPathAtoms::contains;
            break;
        }
        case NORMALIZE_SPACE:
        {
            *aAtom = txXPathAtoms::normalizeSpace;
            break;
        }
        case STARTS_WITH:
        {
            *aAtom = txXPathAtoms::startsWith;
            break;
        }
        case STRING:
        {
            *aAtom = txXPathAtoms::string;
            break;
        }
        case STRING_LENGTH:
        {
            *aAtom = txXPathAtoms::stringLength;
            break;
        }
        case SUBSTRING:
        {
            *aAtom = txXPathAtoms::substring;
            break;
        }
        case SUBSTRING_AFTER:
        {
            *aAtom = txXPathAtoms::substringAfter;
            break;
        }
        case SUBSTRING_BEFORE:
        {
            *aAtom = txXPathAtoms::substringBefore;
            break;
        }
        case TRANSLATE:
        {
            *aAtom = txXPathAtoms::translate;
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
