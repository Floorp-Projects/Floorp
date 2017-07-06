/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/FloatingPoint.h"

#include "txExpr.h"
#include "nsAutoPtr.h"
#include "txNodeSet.h"
#include "nsGkAtoms.h"
#include "txIXPathContext.h"
#include "nsWhitespaceTokenizer.h"
#include "txXPathTreeWalker.h"
#include <math.h>
#include "txStringUtils.h"
#include "txXMLUtils.h"

using namespace mozilla;

struct txCoreFunctionDescriptor
{
    int8_t mMinParams;
    int8_t mMaxParams;
    Expr::ResultType mReturnType;
    nsIAtom** mName;
};

// This must be ordered in the same order as txCoreFunctionCall::eType.
// If you change one, change the other.
static const txCoreFunctionDescriptor descriptTable[] =
{
    { 1, 1, Expr::NUMBER_RESULT,  &nsGkAtoms::count }, // COUNT
    { 1, 1, Expr::NODESET_RESULT, &nsGkAtoms::id }, // ID
    { 0, 0, Expr::NUMBER_RESULT,  &nsGkAtoms::last }, // LAST
    { 0, 1, Expr::STRING_RESULT,  &nsGkAtoms::localName }, // LOCAL_NAME
    { 0, 1, Expr::STRING_RESULT,  &nsGkAtoms::namespaceUri }, // NAMESPACE_URI
    { 0, 1, Expr::STRING_RESULT,  &nsGkAtoms::name }, // NAME
    { 0, 0, Expr::NUMBER_RESULT,  &nsGkAtoms::position }, // POSITION

    { 2, -1, Expr::STRING_RESULT, &nsGkAtoms::concat }, // CONCAT
    { 2, 2, Expr::BOOLEAN_RESULT, &nsGkAtoms::contains }, // CONTAINS
    { 0, 1, Expr::STRING_RESULT,  &nsGkAtoms::normalizeSpace }, // NORMALIZE_SPACE
    { 2, 2, Expr::BOOLEAN_RESULT, &nsGkAtoms::startsWith }, // STARTS_WITH
    { 0, 1, Expr::STRING_RESULT,  &nsGkAtoms::string }, // STRING
    { 0, 1, Expr::NUMBER_RESULT,  &nsGkAtoms::stringLength }, // STRING_LENGTH
    { 2, 3, Expr::STRING_RESULT,  &nsGkAtoms::substring }, // SUBSTRING
    { 2, 2, Expr::STRING_RESULT,  &nsGkAtoms::substringAfter }, // SUBSTRING_AFTER
    { 2, 2, Expr::STRING_RESULT,  &nsGkAtoms::substringBefore }, // SUBSTRING_BEFORE
    { 3, 3, Expr::STRING_RESULT,  &nsGkAtoms::translate }, // TRANSLATE

    { 0, 1, Expr::NUMBER_RESULT,  &nsGkAtoms::number }, // NUMBER
    { 1, 1, Expr::NUMBER_RESULT,  &nsGkAtoms::round }, // ROUND
    { 1, 1, Expr::NUMBER_RESULT,  &nsGkAtoms::floor }, // FLOOR
    { 1, 1, Expr::NUMBER_RESULT,  &nsGkAtoms::ceiling }, // CEILING
    { 1, 1, Expr::NUMBER_RESULT,  &nsGkAtoms::sum }, // SUM

    { 1, 1, Expr::BOOLEAN_RESULT, &nsGkAtoms::boolean }, // BOOLEAN
    { 0, 0, Expr::BOOLEAN_RESULT, &nsGkAtoms::_false }, // _FALSE
    { 1, 1, Expr::BOOLEAN_RESULT, &nsGkAtoms::lang }, // LANG
    { 1, 1, Expr::BOOLEAN_RESULT, &nsGkAtoms::_not }, // _NOT
    { 0, 0, Expr::BOOLEAN_RESULT, &nsGkAtoms::_true } // _TRUE
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
    *aResult = nullptr;

    if (!requireParams(descriptTable[mType].mMinParams,
                       descriptTable[mType].mMaxParams,
                       aContext)) {
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
    }

    nsresult rv = NS_OK;
    switch (mType) {
        case COUNT:
        {
            RefPtr<txNodeSet> nodes;
            rv = evaluateToNodeSet(mParams[0], aContext,
                                   getter_AddRefs(nodes));
            NS_ENSURE_SUCCESS(rv, rv);

            return aContext->recycler()->getNumberResult(nodes->size(),
                                                         aResult);
        }
        case ID:
        {
            RefPtr<txAExprResult> exprResult;
            rv = mParams[0]->evaluate(aContext, getter_AddRefs(exprResult));
            NS_ENSURE_SUCCESS(rv, rv);

            RefPtr<txNodeSet> resultSet;
            rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
            NS_ENSURE_SUCCESS(rv, rv);

            txXPathTreeWalker walker(aContext->getContextNode());

            if (exprResult->getResultType() == txAExprResult::NODESET) {
                txNodeSet* nodes = static_cast<txNodeSet*>
                                              (static_cast<txAExprResult*>
                                                          (exprResult));
                int32_t i;
                for (i = 0; i < nodes->size(); ++i) {
                    nsAutoString idList;
                    txXPathNodeUtils::appendNodeValue(nodes->get(i), idList);
                    nsWhitespaceTokenizer tokenizer(idList);
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
                nsWhitespaceTokenizer tokenizer(idList);
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
            RefPtr<txNodeSet> nodes;
            if (!mParams.IsEmpty()) {
                rv = evaluateToNodeSet(mParams[0], aContext,
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
                    StringResult* strRes = nullptr;
                    rv = aContext->recycler()->getStringResult(&strRes);
                    NS_ENSURE_SUCCESS(rv, rv);

                    *aResult = strRes;
                    txXPathNodeUtils::getLocalName(node, strRes->mValue);

                    return NS_OK;
                }
                case NAMESPACE_URI:
                {
                    StringResult* strRes = nullptr;
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
                        StringResult* strRes = nullptr;
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
                    MOZ_CRASH("Unexpected mType?!");
                }
            }
            MOZ_CRASH("Inner mType switch should have returned!");
        }
        case POSITION:
        {
            return aContext->recycler()->getNumberResult(aContext->position(),
                                                         aResult);
        }

        // String functions

        case CONCAT:
        {
            RefPtr<StringResult> strRes;
            rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
            NS_ENSURE_SUCCESS(rv, rv);

            uint32_t i, len = mParams.Length();
            for (i = 0; i < len; ++i) {
                rv = mParams[i]->evaluateToString(aContext, strRes->mValue);
                NS_ENSURE_SUCCESS(rv, rv);
            }

            NS_ADDREF(*aResult = strRes);

            return NS_OK;
        }
        case CONTAINS:
        {
            nsAutoString arg2;
            rv = mParams[1]->evaluateToString(aContext, arg2);
            NS_ENSURE_SUCCESS(rv, rv);

            if (arg2.IsEmpty()) {
                aContext->recycler()->getBoolResult(true, aResult);
            }
            else {
                nsAutoString arg1;
                rv = mParams[0]->evaluateToString(aContext, arg1);
                NS_ENSURE_SUCCESS(rv, rv);

                aContext->recycler()->getBoolResult(FindInReadable(arg2, arg1),
                                                    aResult);
            }

            return NS_OK;
        }
        case NORMALIZE_SPACE:
        {
            nsAutoString resultStr;
            if (!mParams.IsEmpty()) {
                rv = mParams[0]->evaluateToString(aContext, resultStr);
                NS_ENSURE_SUCCESS(rv, rv);
            }
            else {
                txXPathNodeUtils::appendNodeValue(aContext->getContextNode(),
                                                  resultStr);
            }

            RefPtr<StringResult> strRes;
            rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
            NS_ENSURE_SUCCESS(rv, rv);

            bool addSpace = false;
            bool first = true;
            strRes->mValue.SetCapacity(resultStr.Length());
            char16_t c;
            uint32_t src;
            for (src = 0; src < resultStr.Length(); src++) {
                c = resultStr.CharAt(src);
                if (XMLUtils::isWhitespace(c)) {
                    addSpace = true;
                }
                else {
                    if (addSpace && !first)
                        strRes->mValue.Append(char16_t(' '));

                    strRes->mValue.Append(c);
                    addSpace = false;
                    first = false;
                }
            }
            *aResult = strRes;
            NS_ADDREF(*aResult);

            return NS_OK;
        }
        case STARTS_WITH:
        {
            nsAutoString arg2;
            rv = mParams[1]->evaluateToString(aContext, arg2);
            NS_ENSURE_SUCCESS(rv, rv);

            bool result = false;
            if (arg2.IsEmpty()) {
                result = true;
            }
            else {
                nsAutoString arg1;
                rv = mParams[0]->evaluateToString(aContext, arg1);
                NS_ENSURE_SUCCESS(rv, rv);

                result = StringBeginsWith(arg1, arg2);
            }

            aContext->recycler()->getBoolResult(result, aResult);

            return NS_OK;
        }
        case STRING:
        {
            RefPtr<StringResult> strRes;
            rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
            NS_ENSURE_SUCCESS(rv, rv);

            if (!mParams.IsEmpty()) {
                rv = mParams[0]->evaluateToString(aContext, strRes->mValue);
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
            if (!mParams.IsEmpty()) {
                rv = mParams[0]->evaluateToString(aContext, resultStr);
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
            nsAutoString src;
            rv = mParams[0]->evaluateToString(aContext, src);
            NS_ENSURE_SUCCESS(rv, rv);

            double start;
            rv = evaluateToNumber(mParams[1], aContext, &start);
            NS_ENSURE_SUCCESS(rv, rv);

            // check for NaN or +/-Inf
            if (mozilla::IsNaN(start) ||
                mozilla::IsInfinite(start) ||
                start >= src.Length() + 0.5) {
                aContext->recycler()->getEmptyStringResult(aResult);

                return NS_OK;
            }

            start = floor(start + 0.5) - 1;

            double end;
            if (mParams.Length() == 3) {
                rv = evaluateToNumber(mParams[2], aContext, &end);
                NS_ENSURE_SUCCESS(rv, rv);

                end += start;
                if (mozilla::IsNaN(end) || end < 0) {
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
                  Substring(src, (uint32_t)start, (uint32_t)(end - start)),
                  aResult);
        }
        case SUBSTRING_AFTER:
        {
            nsAutoString arg1;
            rv = mParams[0]->evaluateToString(aContext, arg1);
            NS_ENSURE_SUCCESS(rv, rv);

            nsAutoString arg2;
            rv = mParams[1]->evaluateToString(aContext, arg2);
            NS_ENSURE_SUCCESS(rv, rv);

            if (arg2.IsEmpty()) {
                return aContext->recycler()->getStringResult(arg1, aResult);
            }

            int32_t idx = arg1.Find(arg2);
            if (idx == kNotFound) {
                aContext->recycler()->getEmptyStringResult(aResult);

                return NS_OK;
            }

            const nsAString& result = Substring(arg1, idx + arg2.Length());
            return aContext->recycler()->getStringResult(result, aResult);
        }
        case SUBSTRING_BEFORE:
        {
            nsAutoString arg2;
            rv = mParams[1]->evaluateToString(aContext, arg2);
            NS_ENSURE_SUCCESS(rv, rv);

            if (arg2.IsEmpty()) {
                aContext->recycler()->getEmptyStringResult(aResult);

                return NS_OK;
            }

            nsAutoString arg1;
            rv = mParams[0]->evaluateToString(aContext, arg1);
            NS_ENSURE_SUCCESS(rv, rv);

            int32_t idx = arg1.Find(arg2);
            if (idx == kNotFound) {
                aContext->recycler()->getEmptyStringResult(aResult);

                return NS_OK;
            }

            return aContext->recycler()->getStringResult(StringHead(arg1, idx),
                                                         aResult);
        }
        case TRANSLATE:
        {
            nsAutoString src;
            rv = mParams[0]->evaluateToString(aContext, src);
            NS_ENSURE_SUCCESS(rv, rv);

            if (src.IsEmpty()) {
                aContext->recycler()->getEmptyStringResult(aResult);

                return NS_OK;
            }

            RefPtr<StringResult> strRes;
            rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
            NS_ENSURE_SUCCESS(rv, rv);

            strRes->mValue.SetCapacity(src.Length());

            nsAutoString oldChars, newChars;
            rv = mParams[1]->evaluateToString(aContext, oldChars);
            NS_ENSURE_SUCCESS(rv, rv);

            rv = mParams[2]->evaluateToString(aContext, newChars);
            NS_ENSURE_SUCCESS(rv, rv);

            uint32_t i;
            int32_t newCharsLength = (int32_t)newChars.Length();
            for (i = 0; i < src.Length(); i++) {
                int32_t idx = oldChars.FindChar(src.CharAt(i));
                if (idx != kNotFound) {
                    if (idx < newCharsLength)
                        strRes->mValue.Append(newChars.CharAt((uint32_t)idx));
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
            if (!mParams.IsEmpty()) {
                rv = evaluateToNumber(mParams[0], aContext, &res);
                NS_ENSURE_SUCCESS(rv, rv);
            }
            else {
                nsAutoString resultStr;
                txXPathNodeUtils::appendNodeValue(aContext->getContextNode(),
                                                  resultStr);
                res = txDouble::toDouble(resultStr);
            }
            return aContext->recycler()->getNumberResult(res, aResult);
        }
        case ROUND:
        {
            double dbl;
            rv = evaluateToNumber(mParams[0], aContext, &dbl);
            NS_ENSURE_SUCCESS(rv, rv);

            if (mozilla::IsFinite(dbl)) {
                if (mozilla::IsNegative(dbl) && dbl >= -0.5) {
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
            double dbl;
            rv = evaluateToNumber(mParams[0], aContext, &dbl);
            NS_ENSURE_SUCCESS(rv, rv);

            if (mozilla::IsFinite(dbl) && !mozilla::IsNegativeZero(dbl))
                dbl = floor(dbl);

            return aContext->recycler()->getNumberResult(dbl, aResult);
        }
        case CEILING:
        {
            double dbl;
            rv = evaluateToNumber(mParams[0], aContext, &dbl);
            NS_ENSURE_SUCCESS(rv, rv);

            if (mozilla::IsFinite(dbl)) {
                if (mozilla::IsNegative(dbl) && dbl > -1)
                    dbl *= 0;
                else
                    dbl = ceil(dbl);
            }

            return aContext->recycler()->getNumberResult(dbl, aResult);
        }
        case SUM:
        {
            RefPtr<txNodeSet> nodes;
            nsresult rv = evaluateToNodeSet(mParams[0], aContext,
                                            getter_AddRefs(nodes));
            NS_ENSURE_SUCCESS(rv, rv);

            double res = 0;
            int32_t i;
            for (i = 0; i < nodes->size(); ++i) {
                nsAutoString resultStr;
                txXPathNodeUtils::appendNodeValue(nodes->get(i), resultStr);
                res += txDouble::toDouble(resultStr);
            }
            return aContext->recycler()->getNumberResult(res, aResult);
        }

        // Boolean functions

        case BOOLEAN:
        {
            bool result;
            nsresult rv = mParams[0]->evaluateToBool(aContext, result);
            NS_ENSURE_SUCCESS(rv, rv);

            aContext->recycler()->getBoolResult(result, aResult);

            return NS_OK;
        }
        case _FALSE:
        {
            aContext->recycler()->getBoolResult(false, aResult);

            return NS_OK;
        }
        case LANG:
        {
            txXPathTreeWalker walker(aContext->getContextNode());

            nsAutoString lang;
            bool found;
            do {
                found = walker.getAttr(nsGkAtoms::lang, kNameSpaceID_XML,
                                       lang);
            } while (!found && walker.moveToParent());

            if (!found) {
                aContext->recycler()->getBoolResult(false, aResult);

                return NS_OK;
            }

            nsAutoString arg;
            rv = mParams[0]->evaluateToString(aContext, arg);
            NS_ENSURE_SUCCESS(rv, rv);

            bool result =
                StringBeginsWith(lang, arg,
                                 txCaseInsensitiveStringComparator()) &&
                (lang.Length() == arg.Length() ||
                 lang.CharAt(arg.Length()) == '-');

            aContext->recycler()->getBoolResult(result, aResult);

            return NS_OK;
        }
        case _NOT:
        {
            bool result;
            rv = mParams[0]->evaluateToBool(aContext, result);
            NS_ENSURE_SUCCESS(rv, rv);

            aContext->recycler()->getBoolResult(!result, aResult);

            return NS_OK;
        }
        case _TRUE:
        {
            aContext->recycler()->getBoolResult(true, aResult);

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

bool
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
            return (aContext & NODE_CONTEXT) ||
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
            if (mParams.IsEmpty()) {
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
    return true;
}

// static
bool
txCoreFunctionCall::getTypeFromAtom(nsIAtom* aName, eType& aType)
{
    uint32_t i;
    for (i = 0; i < ArrayLength(descriptTable); ++i) {
        if (aName == *descriptTable[i].mName) {
            aType = static_cast<eType>(i);

            return true;
        }
    }

    return false;
}

#ifdef TX_TO_STRING
nsresult
txCoreFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    NS_ADDREF(*aAtom = *descriptTable[mType].mName);
    return NS_OK;
}
#endif
