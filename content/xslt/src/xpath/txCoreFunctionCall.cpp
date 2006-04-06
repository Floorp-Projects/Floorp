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

#include "txExpr.h"
#include "nsAutoPtr.h"
#include "txNodeSet.h"
#include "txAtoms.h"
#include "txIXPathContext.h"
#include "txTokenizer.h"
#include "txXPathTreeWalker.h"
#include <math.h>
#include "txStringUtils.h"

struct txFunctionDescriptor
{
    PRInt8 mMinParams;
    PRInt8 mMaxParams;
    Expr::ResultType mReturnType;
    nsIAtom** mName;
};

// This must be ordered in the same order as txCoreFunctionCall::eType.
// If you change one, change the other.
static const txFunctionDescriptor descriptTable[] =
{
    { 1, 1, Expr::NUMBER_RESULT,  &txXPathAtoms::count }, // COUNT
    { 1, 1, Expr::NODESET_RESULT, &txXPathAtoms::id }, // ID
    { 0, 0, Expr::NUMBER_RESULT,  &txXPathAtoms::last }, // LAST
    { 0, 1, Expr::STRING_RESULT,  &txXPathAtoms::localName }, // LOCAL_NAME
    { 0, 1, Expr::STRING_RESULT,  &txXPathAtoms::namespaceUri }, // NAMESPACE_URI
    { 0, 1, Expr::STRING_RESULT,  &txXPathAtoms::name }, // NAME
    { 0, 0, Expr::NUMBER_RESULT,  &txXPathAtoms::position }, // POSITION

    { 2, -1, Expr::STRING_RESULT, &txXPathAtoms::concat }, // CONCAT
    { 2, 2, Expr::BOOLEAN_RESULT, &txXPathAtoms::contains }, // CONTAINS
    { 0, 1, Expr::STRING_RESULT,  &txXPathAtoms::normalizeSpace }, // NORMALIZE_SPACE
    { 2, 2, Expr::BOOLEAN_RESULT, &txXPathAtoms::startsWith }, // STARTS_WITH
    { 0, 1, Expr::STRING_RESULT,  &txXPathAtoms::string }, // STRING
    { 0, 1, Expr::NUMBER_RESULT,  &txXPathAtoms::stringLength }, // STRING_LENGTH
    { 2, 3, Expr::STRING_RESULT,  &txXPathAtoms::substring }, // SUBSTRING
    { 2, 2, Expr::STRING_RESULT,  &txXPathAtoms::substringAfter }, // SUBSTRING_AFTER
    { 2, 2, Expr::STRING_RESULT,  &txXPathAtoms::substringBefore }, // SUBSTRING_BEFORE
    { 3, 3, Expr::STRING_RESULT,  &txXPathAtoms::translate }, // TRANSLATE

    { 0, 1, Expr::NUMBER_RESULT,  &txXPathAtoms::number }, // NUMBER
    { 1, 1, Expr::NUMBER_RESULT,  &txXPathAtoms::round }, // ROUND
    { 1, 1, Expr::NUMBER_RESULT,  &txXPathAtoms::floor }, // FLOOR
    { 1, 1, Expr::NUMBER_RESULT,  &txXPathAtoms::ceiling }, // CEILING
    { 1, 1, Expr::NUMBER_RESULT,  &txXPathAtoms::sum }, // SUM

    { 1, 1, Expr::BOOLEAN_RESULT, &txXPathAtoms::boolean }, // BOOLEAN
    { 0, 0, Expr::BOOLEAN_RESULT, &txXPathAtoms::_false }, // _FALSE
    { 1, 1, Expr::BOOLEAN_RESULT, &txXPathAtoms::lang }, // LANG
    { 1, 1, Expr::BOOLEAN_RESULT, &txXPathAtoms::_not }, // _NOT
    { 0, 0, Expr::BOOLEAN_RESULT, &txXPathAtoms::_true } // _TRUE
};


/*
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
 */
nsresult
txCoreFunctionCall::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nsnull;

    if (!requireParams(descriptTable[mType].mMinParams,
                       descriptTable[mType].mMaxParams,
                       aContext)) {
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
    }

    nsresult rv = NS_OK;
    txListIterator iter(&params);

    switch (mType) {
        case COUNT:
        {
            nsRefPtr<txNodeSet> nodes;
            rv = evaluateToNodeSet((Expr*)iter.next(), aContext,
                                   getter_AddRefs(nodes));
            NS_ENSURE_SUCCESS(rv, rv);

            return aContext->recycler()->getNumberResult(nodes->size(),
                                                         aResult);
        }
        case ID:
        {
            nsRefPtr<txAExprResult> exprResult;
            rv = ((Expr*)iter.next())->evaluate(aContext,
                                                getter_AddRefs(exprResult));
            NS_ENSURE_SUCCESS(rv, rv);

            nsRefPtr<txNodeSet> resultSet;
            rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
            NS_ENSURE_SUCCESS(rv, rv);

            txXPathTreeWalker walker(aContext->getContextNode());
            
            if (exprResult->getResultType() == txAExprResult::NODESET) {
                txNodeSet* nodes = NS_STATIC_CAST(txNodeSet*,
                                                  NS_STATIC_CAST(txAExprResult*,
                                                                 exprResult));
                PRInt32 i;
                for (i = 0; i < nodes->size(); ++i) {
                    nsAutoString idList;
                    txXPathNodeUtils::appendNodeValue(nodes->get(i), idList);
                    txTokenizer tokenizer(idList);
                    while (tokenizer.hasMoreTokens()) {
                        if (walker.moveToElementById(tokenizer.nextToken())) {
                            resultSet->add(walker.getCurrentPosition());
                        }
                    }
                }
            }
            else {
                nsAutoString idList;
                exprResult->stringValue(idList);
                txTokenizer tokenizer(idList);
                while (tokenizer.hasMoreTokens()) {
                    if (walker.moveToElementById(tokenizer.nextToken())) {
                        resultSet->add(walker.getCurrentPosition());
                    }
                }
            }

            *aResult = resultSet;
            NS_ADDREF(*aResult);

            return NS_OK;
        }
        case LAST:
        {
            return aContext->recycler()->getNumberResult(aContext->size(),
                                                         aResult);
        }
        case LOCAL_NAME:
        case NAME:
        case NAMESPACE_URI:
        {
            // Check for optional arg
            nsRefPtr<txNodeSet> nodes;
            if (iter.hasNext()) {
                rv = evaluateToNodeSet((Expr*)iter.next(), aContext,
                                       getter_AddRefs(nodes));
                NS_ENSURE_SUCCESS(rv, rv);

                if (nodes->isEmpty()) {
                    aContext->recycler()->getEmptyStringResult(aResult);

                    return NS_OK;
                }
            }

            const txXPathNode& node = nodes ? nodes->get(0) :
                                              aContext->getContextNode();
            switch (mType) {
                case LOCAL_NAME:
                {
                    StringResult* strRes = nsnull;
                    rv = aContext->recycler()->getStringResult(&strRes);
                    NS_ENSURE_SUCCESS(rv, rv);

                    *aResult = strRes;
                    txXPathNodeUtils::getLocalName(node, strRes->mValue);

                    return NS_OK;
                }
                case NAMESPACE_URI:
                {
                    StringResult* strRes = nsnull;
                    rv = aContext->recycler()->getStringResult(&strRes);
                    NS_ENSURE_SUCCESS(rv, rv);

                    *aResult = strRes;
                    txXPathNodeUtils::getNamespaceURI(node, strRes->mValue);

                    return NS_OK;
                }
                case NAME:
                {
                    // XXX Namespace: namespaces have a name
                    if (txXPathNodeUtils::isAttribute(node) ||
                        txXPathNodeUtils::isElement(node) ||
                        txXPathNodeUtils::isProcessingInstruction(node)) {
                        StringResult* strRes = nsnull;
                        rv = aContext->recycler()->getStringResult(&strRes);
                        NS_ENSURE_SUCCESS(rv, rv);

                        *aResult = strRes;
                        txXPathNodeUtils::getNodeName(node, strRes->mValue);
                    }
                    else {
                        aContext->recycler()->getEmptyStringResult(aResult);
                    }

                    return NS_OK;
                }
                default:
                {
                    break;
                }
            }
        }
        case POSITION:
        {
            return aContext->recycler()->getNumberResult(aContext->position(),
                                                         aResult);
        }

        // String functions

        case CONCAT:
        {
            nsRefPtr<StringResult> strRes;
            rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
            NS_ENSURE_SUCCESS(rv, rv);

            while (iter.hasNext()) {
                Expr* param = NS_STATIC_CAST(Expr*, iter.next());
                rv = param->evaluateToString(aContext, strRes->mValue);
                NS_ENSURE_SUCCESS(rv, rv);
            }

            NS_ADDREF(*aResult = strRes);

            return NS_OK;
        }
        case CONTAINS:
        {
            Expr* arg1Expr = NS_STATIC_CAST(Expr*, iter.next());
            Expr* arg2Expr = NS_STATIC_CAST(Expr*, iter.next());

            nsAutoString arg2;
            rv = arg2Expr->evaluateToString(aContext, arg2);
            NS_ENSURE_SUCCESS(rv, rv);

            if (arg2.IsEmpty()) {
                aContext->recycler()->getBoolResult(PR_TRUE, aResult);
            }
            else {
                nsAutoString arg1;
                rv = arg1Expr->evaluateToString(aContext, arg1);
                NS_ENSURE_SUCCESS(rv, rv);

                aContext->recycler()->getBoolResult(FindInReadable(arg2, arg1),
                                                    aResult);
            }

            return NS_OK;
        }
        case NORMALIZE_SPACE:
        {
            nsAutoString resultStr;
            if (iter.hasNext()) {
                Expr* param = NS_STATIC_CAST(Expr*, iter.next());
                rv = param->evaluateToString(aContext, resultStr);
                NS_ENSURE_SUCCESS(rv, rv);
            }
            else {
                txXPathNodeUtils::appendNodeValue(aContext->getContextNode(),
                                                  resultStr);
            }

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
            Expr* arg1Expr = NS_STATIC_CAST(Expr*, iter.next());
            Expr* arg2Expr = NS_STATIC_CAST(Expr*, iter.next());

            nsAutoString arg2;
            rv = arg2Expr->evaluateToString(aContext, arg2);
            NS_ENSURE_SUCCESS(rv, rv);

            PRBool result;
            if (arg2.IsEmpty()) {
                result = PR_TRUE;
            }
            else {
                nsAutoString arg1;
                rv = arg1Expr->evaluateToString(aContext, arg1);
                NS_ENSURE_SUCCESS(rv, rv);

                result = StringBeginsWith(arg1, arg2);
            }

            aContext->recycler()->getBoolResult(result, aResult);

            return NS_OK;
        }
        case STRING:
        {
            nsRefPtr<StringResult> strRes;
            rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
            NS_ENSURE_SUCCESS(rv, rv);

            if (iter.hasNext()) {
                Expr* param = NS_STATIC_CAST(Expr*, iter.next());
                rv = param->evaluateToString(aContext, strRes->mValue);
                NS_ENSURE_SUCCESS(rv, rv);
            }
            else {
                txXPathNodeUtils::appendNodeValue(aContext->getContextNode(),
                                                  strRes->mValue);
            }

            NS_ADDREF(*aResult = strRes);

            return NS_OK;
        }
        case STRING_LENGTH:
        {
            nsAutoString resultStr;
            if (iter.hasNext()) {
                Expr* arg1Expr = NS_STATIC_CAST(Expr*, iter.next());
                rv = arg1Expr->evaluateToString(aContext, resultStr);
                NS_ENSURE_SUCCESS(rv, rv);
            }
            else {
                txXPathNodeUtils::appendNodeValue(aContext->getContextNode(),
                                                  resultStr);
            }
            rv = aContext->recycler()->getNumberResult(resultStr.Length(),
                                                       aResult);
            NS_ENSURE_SUCCESS(rv, rv);

            return NS_OK;
        }
        case SUBSTRING:
        {
            Expr* param = NS_STATIC_CAST(Expr*, iter.next());

            nsAutoString src;
            rv = param->evaluateToString(aContext, src);
            NS_ENSURE_SUCCESS(rv, rv);

            double start = evaluateToNumber(NS_STATIC_CAST(Expr*, iter.next()),
                                            aContext);

            // check for NaN or +/-Inf
            if (Double::isNaN(start) ||
                Double::isInfinite(start) ||
                start >= src.Length() + 0.5) {
                aContext->recycler()->getEmptyStringResult(aResult);

                return NS_OK;
            }

            start = floor(start + 0.5) - 1;

            double end;
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
            Expr* arg1Expr = NS_STATIC_CAST(Expr*, iter.next());
            nsAutoString arg1;
            rv = arg1Expr->evaluateToString(aContext, arg1);
            NS_ENSURE_SUCCESS(rv, rv);

            Expr* arg2Expr = NS_STATIC_CAST(Expr*, iter.next());
            nsAutoString arg2;
            rv = arg2Expr->evaluateToString(aContext, arg2);
            NS_ENSURE_SUCCESS(rv, rv);

            if (arg2.IsEmpty()) {
                return aContext->recycler()->getStringResult(arg1, aResult);
            }

            PRInt32 idx = arg1.Find(arg2);
            if (idx == kNotFound) {
                aContext->recycler()->getEmptyStringResult(aResult);
                
                return NS_OK;
            }

            const nsSubstring& result = Substring(arg1, idx + arg2.Length());
            return aContext->recycler()->getStringResult(result, aResult);
        }
        case SUBSTRING_BEFORE:
        {
            Expr* arg1Expr = NS_STATIC_CAST(Expr*, iter.next());
            Expr* arg2Expr = NS_STATIC_CAST(Expr*, iter.next());

            nsAutoString arg2;
            rv = arg2Expr->evaluateToString(aContext, arg2);
            NS_ENSURE_SUCCESS(rv, rv);

            if (arg2.IsEmpty()) {
                aContext->recycler()->getEmptyStringResult(aResult);

                return NS_OK;
            }

            nsAutoString arg1;
            rv = arg1Expr->evaluateToString(aContext, arg1);
            NS_ENSURE_SUCCESS(rv, rv);

            PRInt32 idx = arg1.Find(arg2);
            if (idx == kNotFound) {
                aContext->recycler()->getEmptyStringResult(aResult);
                
                return NS_OK;
            }

            return aContext->recycler()->getStringResult(StringHead(arg1, idx),
                                                         aResult);
        }
        case TRANSLATE:
        {
            Expr* arg1Expr = NS_STATIC_CAST(Expr*, iter.next());

            nsAutoString src;
            rv = arg1Expr->evaluateToString(aContext, src);
            NS_ENSURE_SUCCESS(rv, rv);

            if (src.IsEmpty()) {
                aContext->recycler()->getEmptyStringResult(aResult);

                return NS_OK;
            }
            
            nsRefPtr<StringResult> strRes;
            rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
            NS_ENSURE_SUCCESS(rv, rv);

            strRes->mValue.SetCapacity(src.Length());

            Expr* arg2Expr = NS_STATIC_CAST(Expr*, iter.next());
            Expr* arg3Expr = NS_STATIC_CAST(Expr*, iter.next());

            nsAutoString oldChars, newChars;
            rv = arg2Expr->evaluateToString(aContext, oldChars);
            NS_ENSURE_SUCCESS(rv, rv);

            rv = arg3Expr->evaluateToString(aContext, newChars);
            NS_ENSURE_SUCCESS(rv, rv);

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

            NS_ADDREF(*aResult = strRes);

            return NS_OK;
        }
        
        // Number functions

        case NUMBER:
        {
            double res;
            if (iter.hasNext()) {
                res = evaluateToNumber((Expr*)iter.next(), aContext);
            }
            else {
                nsAutoString resultStr;
                txXPathNodeUtils::appendNodeValue(aContext->getContextNode(),
                                                  resultStr);
                res = Double::toDouble(resultStr);
            }
            return aContext->recycler()->getNumberResult(res, aResult);
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
        case SUM:
        {
            nsRefPtr<txNodeSet> nodes;
            nsresult rv = evaluateToNodeSet((Expr*)iter.next(), aContext,
                                            getter_AddRefs(nodes));
            NS_ENSURE_SUCCESS(rv, rv);

            double res = 0;
            PRInt32 i;
            for (i = 0; i < nodes->size(); ++i) {
                nsAutoString resultStr;
                txXPathNodeUtils::appendNodeValue(nodes->get(i), resultStr);
                res += Double::toDouble(resultStr);
            }
            return aContext->recycler()->getNumberResult(res, aResult);
        }
        
        // Boolean functions
        
        case BOOLEAN:
        {
            Expr* param = NS_STATIC_CAST(Expr*, iter.next());
            PRBool result;
            nsresult rv = param->evaluateToBool(aContext, result);
            NS_ENSURE_SUCCESS(rv, rv);

            aContext->recycler()->getBoolResult(result, aResult);

            return NS_OK;
        }
        case _FALSE:
        {
            aContext->recycler()->getBoolResult(PR_FALSE, aResult);

            return NS_OK;
        }
        case LANG:
        {
            txXPathTreeWalker walker(aContext->getContextNode());

            nsAutoString lang;
            PRBool found;
            do {
                found = walker.getAttr(txXMLAtoms::lang, kNameSpaceID_XML,
                                       lang);
            } while (!found && walker.moveToParent());

            if (!found) {
                aContext->recycler()->getBoolResult(PR_FALSE, aResult);

                return NS_OK;
            }

            nsAutoString arg;
            Expr* param = NS_STATIC_CAST(Expr*, iter.next());
            nsresult rv = param->evaluateToString(aContext, arg);
            NS_ENSURE_SUCCESS(rv, rv);

            PRBool result = arg.Equals(Substring(lang, 0, arg.Length()),
                                       txCaseInsensitiveStringComparator()) &&
                            (lang.Length() == arg.Length() ||
                             lang.CharAt(arg.Length()) == '-');

            aContext->recycler()->getBoolResult(result, aResult);

            return NS_OK;
        }
        case _NOT:
        {
            Expr* param = NS_STATIC_CAST(Expr*, iter.next());
            PRBool result;
            nsresult rv = param->evaluateToBool(aContext, result);
            NS_ENSURE_SUCCESS(rv, rv);

            aContext->recycler()->getBoolResult(!result, aResult);

            return NS_OK;
        }
        case _TRUE:
        {
            aContext->recycler()->getBoolResult(PR_TRUE, aResult);

            return NS_OK;
        }
    }

    aContext->receiveError(NS_LITERAL_STRING("Internal error"),
                           NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;
}

Expr::ResultType
txCoreFunctionCall::getReturnType()
{
    return descriptTable[mType].mReturnType;
}

PRBool
txCoreFunctionCall::isSensitiveTo(ContextSensitivity aContext)
{
    switch (mType) {
        case COUNT:
        case CONCAT:
        case CONTAINS:
        case STARTS_WITH:
        case SUBSTRING:
        case SUBSTRING_AFTER:
        case SUBSTRING_BEFORE:
        case TRANSLATE:
        case ROUND:
        case FLOOR:
        case CEILING:
        case SUM:
        case BOOLEAN:
        case _NOT:
        case _FALSE:
        case _TRUE:
        {
            return argsSensitiveTo(aContext);
        }
        case ID:
        {
            return (aContext & DOCUMENT_CONTEXT) ||
                   argsSensitiveTo(aContext);
        }
        case LAST:
        {
            return !!(aContext & SIZE_CONTEXT);
        }
        case LOCAL_NAME:
        case NAME:
        case NAMESPACE_URI:
        case NORMALIZE_SPACE:
        case STRING:
        case STRING_LENGTH:
        case NUMBER:
        {
            if (params.isEmpty()) {
                return !!(aContext & NODE_CONTEXT);
            }
            return argsSensitiveTo(aContext);
        }
        case POSITION:
        {
            return !!(aContext & POSITION_CONTEXT);
        }
        case LANG:
        {
            return (aContext & NODE_CONTEXT) ||
                   argsSensitiveTo(aContext);
        }
    }

    NS_NOTREACHED("how'd we get here?");
    return PR_TRUE;
}

// static
PRBool
txCoreFunctionCall::getTypeFromAtom(nsIAtom* aName, eType& aType)
{
    PRUint32 i;
    for (i = 0; i < NS_ARRAY_LENGTH(descriptTable); ++i) {
        if (aName == *descriptTable[i].mName) {
            aType = NS_STATIC_CAST(eType, i);

            return PR_TRUE;
        }
    }

    return PR_FALSE;
}

#ifdef TX_TO_STRING
nsresult
txCoreFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    NS_ADDREF(*aAtom = *descriptTable[mType].mName);
    return NS_OK;
}
#endif
