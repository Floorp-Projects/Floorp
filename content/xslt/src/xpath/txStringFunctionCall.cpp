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
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Keith Visco <kvisco@ziplink.net> (Original Author)
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

/**
 * StringFunctionCall
 * A representation of the XPath String funtions
**/

#include "ExprResult.h"
#include "FunctionLib.h"
#include "txAtoms.h"
#include "txError.h"
#include "txIXPathContext.h"
#include "XMLUtils.h"
#include "txXPathTreeWalker.h"
#include <math.h>
#include "nsReadableUtils.h"

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
nsresult
StringFunctionCall::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nsnull;

    nsresult rv = NS_OK;
    txListIterator iter(&params);
    switch (mType) {
        case CONCAT:
        {
            if (!requireParams(2, -1, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
                
            nsRefPtr<StringResult> strRes;
            rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
            NS_ENSURE_SUCCESS(rv, rv);

            while (iter.hasNext()) {
                evaluateToString((Expr*)iter.next(), aContext, strRes->mValue);
            }
            *aResult = strRes;
            NS_ADDREF(*aResult);

            return NS_OK;
        }
        case CONTAINS:
        {
            if (!requireParams(2, 2, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            nsAutoString arg1, arg2;
            Expr* arg1Expr = (Expr*)iter.next();
            evaluateToString((Expr*)iter.next(), aContext, arg2);
            if (arg2.IsEmpty()) {
                aContext->recycler()->getBoolResult(PR_TRUE, aResult);
            }
            else {
                evaluateToString(arg1Expr, aContext, arg1);
                aContext->recycler()->getBoolResult(arg1.Find(arg2) >= 0,
                                                    aResult);
            }

            return NS_OK;
        }
        case NORMALIZE_SPACE:
        {
            if (!requireParams(0, 1, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            nsAutoString resultStr;
            if (iter.hasNext())
                evaluateToString((Expr*)iter.next(), aContext, resultStr);
            else
                txXPathNodeUtils::appendNodeValue(aContext->getContextNode(),
                                                  resultStr);

            nsRefPtr<StringResult> strRes;
            rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
            NS_ENSURE_SUCCESS(rv, rv);

            MBool addSpace = MB_FALSE;
            MBool first = MB_TRUE;
            strRes->mValue.SetCapacity(resultStr.Length());
            PRUnichar c;
            PRUint32 src;
            for (src = 0; src < resultStr.Length(); src++) {
                c = resultStr.CharAt(src);
                if (XMLUtils::isWhitespace(c)) {
                    addSpace = MB_TRUE;
                }
                else {
                    if (addSpace && !first)
                        strRes->mValue.Append(PRUnichar(' '));

                    strRes->mValue.Append(c);
                    addSpace = MB_FALSE;
                    first = MB_FALSE;
                }
            }
            *aResult = strRes;
            NS_ADDREF(*aResult);

            return NS_OK;
        }
        case STARTS_WITH:
        {
            if (!requireParams(2, 2, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            nsAutoString arg1, arg2;
            Expr* arg1Expr = (Expr*)iter.next();
            evaluateToString((Expr*)iter.next(), aContext, arg2);
            if (arg2.IsEmpty()) {
                aContext->recycler()->getBoolResult(PR_TRUE, aResult);
            }
            else {
                evaluateToString(arg1Expr, aContext, arg1);
                aContext->recycler()->getBoolResult(
                      StringBeginsWith(arg1, arg2), aResult);
            }

            return NS_OK;
        }
        case STRING_LENGTH:
        {
            if (!requireParams(0, 1, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            nsAutoString resultStr;
            if (iter.hasNext())
                evaluateToString((Expr*)iter.next(), aContext, resultStr);
            else
                txXPathNodeUtils::appendNodeValue(aContext->getContextNode(),
                                                  resultStr);
            rv = aContext->recycler()->getNumberResult(resultStr.Length(),
                                                       aResult);
            NS_ENSURE_SUCCESS(rv, rv);

            return NS_OK;
        }
        case SUBSTRING:
        {
            if (!requireParams(2, 3, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            nsAutoString src;
            double start, end;
            evaluateToString((Expr*)iter.next(), aContext, src);
            start = evaluateToNumber((Expr*)iter.next(), aContext);

            // check for NaN or +/-Inf
            if (Double::isNaN(start) ||
                Double::isInfinite(start) ||
                start >= src.Length() + 0.5) {
                aContext->recycler()->getEmptyStringResult(aResult);

                return NS_OK;
            }

            start = floor(start + 0.5) - 1;
            if (iter.hasNext()) {
                end = start + evaluateToNumber((Expr*)iter.next(),
                                               aContext);
                if (Double::isNaN(end) || end < 0) {
                    aContext->recycler()->getEmptyStringResult(aResult);

                    return NS_OK;
                }
                
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
 
            if (start > end) {
                aContext->recycler()->getEmptyStringResult(aResult);
                
                return NS_OK;
            }

            return aContext->recycler()->getStringResult(
                  Substring(src, (PRUint32)start, (PRUint32)(end - start)),
                  aResult);
        }
        case SUBSTRING_AFTER:
        {
            if (!requireParams(2, 2, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            nsAutoString arg1, arg2;
            evaluateToString((Expr*)iter.next(), aContext, arg1);
            evaluateToString((Expr*)iter.next(), aContext, arg2);
            if (arg2.IsEmpty()) {
                return aContext->recycler()->getStringResult(arg1, aResult);
            }

            PRInt32 idx = arg1.Find(arg2);
            if (idx == kNotFound) {
                aContext->recycler()->getEmptyStringResult(aResult);
                
                return NS_OK;
            }

            PRUint32 len = arg2.Length();
            return aContext->recycler()->getStringResult(
                  Substring(arg1, idx + len, arg1.Length() - (idx + len)),
                  aResult);
        }
        case SUBSTRING_BEFORE:
        {
            if (!requireParams(2, 2, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            nsAutoString arg1, arg2;
            Expr* arg1Expr = (Expr*)iter.next();
            evaluateToString((Expr*)iter.next(), aContext, arg2);
            if (arg2.IsEmpty()) {
                aContext->recycler()->getEmptyStringResult(aResult);

                return NS_OK;
            }

            evaluateToString(arg1Expr, aContext, arg1);

            PRInt32 idx = arg1.Find(arg2);
            if (idx == kNotFound) {
                aContext->recycler()->getEmptyStringResult(aResult);
                
                return NS_OK;
            }

            return aContext->recycler()->
                getStringResult(Substring(arg1, 0, idx), aResult);
        }
        case TRANSLATE:
        {
            if (!requireParams(3, 3, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            nsAutoString src;
            evaluateToString((Expr*)iter.next(), aContext, src);
            if (src.IsEmpty()) {
                aContext->recycler()->getEmptyStringResult(aResult);

                return NS_OK;
            }
            
            nsRefPtr<StringResult> strRes;
            rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
            NS_ENSURE_SUCCESS(rv, rv);

            strRes->mValue.SetCapacity(src.Length());
            nsAutoString oldChars, newChars;
            evaluateToString((Expr*)iter.next(), aContext, oldChars);
            evaluateToString((Expr*)iter.next(), aContext, newChars);
            PRUint32 i;
            PRInt32 newCharsLength = (PRInt32)newChars.Length();
            for (i = 0; i < src.Length(); i++) {
                PRInt32 idx = oldChars.FindChar(src.CharAt(i));
                if (idx != kNotFound) {
                    if (idx < newCharsLength)
                        strRes->mValue.Append(newChars.CharAt((PRUint32)idx));
                }
                else {
                    strRes->mValue.Append(src.CharAt(i));
                }
            }
            *aResult = strRes;
            NS_ADDREF(*aResult);

            return NS_OK;
        }
        case STRING:
        {
            if (!requireParams(0, 1, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            nsRefPtr<StringResult> strRes;
            rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
            NS_ENSURE_SUCCESS(rv, rv);

            if (iter.hasNext())
                evaluateToString((Expr*)iter.next(), aContext, strRes->mValue);
            else
                txXPathNodeUtils::appendNodeValue(aContext->getContextNode(),
                                                  strRes->mValue);
            *aResult = strRes;
            NS_ADDREF(*aResult);

            return NS_OK;
        }
    }

    aContext->receiveError(NS_LITERAL_STRING("Internal error"),
                           NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;
}

#ifdef TX_TO_STRING
nsresult
StringFunctionCall::getNameAtom(nsIAtom** aAtom)
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
#endif
