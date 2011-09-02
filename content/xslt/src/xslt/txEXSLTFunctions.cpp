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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Peter Van der Beken.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
 *
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

#include "nsIAtom.h"
#include "txAtoms.h"
#include "txExecutionState.h"
#include "txExpr.h"
#include "txIXPathContext.h"
#include "txNodeSet.h"
#include "txOutputFormat.h"
#include "txRtfHandler.h"
#include "txXPathTreeWalker.h"
#include "nsPrintfCString.h"
#include "nsComponentManagerUtils.h"
#include "nsContentCID.h"
#include "nsContentCreatorFunctions.h"
#include "nsIContent.h"
#include "nsIDOMDocumentFragment.h"
#include "txMozillaXMLOutput.h"

class txStylesheetCompilerState;

// ------------------------------------------------------------------
// Utility functions
// ------------------------------------------------------------------

static nsresult
convertRtfToNode(txIEvalContext *aContext, txResultTreeFragment *aRtf)
{
    txExecutionState* es = 
        static_cast<txExecutionState*>(aContext->getPrivateContext());
    if (!es) {
        NS_ERROR("Need txExecutionState!");

        return NS_ERROR_UNEXPECTED;
    }

    const txXPathNode& document = es->getSourceDocument();

    nsIDocument *doc = txXPathNativeNode::getDocument(document);
    nsCOMPtr<nsIDOMDocumentFragment> domFragment;
    nsresult rv = NS_NewDocumentFragment(getter_AddRefs(domFragment),
                                         doc->NodeInfoManager());
    NS_ENSURE_SUCCESS(rv, rv);

    txOutputFormat format;
    txMozillaXMLOutput mozHandler(&format, domFragment, PR_TRUE);

    rv = aRtf->flushToHandler(&mozHandler);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mozHandler.closePrevious(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // The txResultTreeFragment will own this.
    const txXPathNode* node = txXPathNativeNode::createXPathNode(domFragment,
                                                                 PR_TRUE);
    NS_ENSURE_TRUE(node, NS_ERROR_OUT_OF_MEMORY);

    aRtf->setNode(node);

    return NS_OK;
}

static nsresult
createTextNode(txIEvalContext *aContext, nsString& aValue,
               txXPathNode* *aResult)
{
    txExecutionState* es = 
        static_cast<txExecutionState*>(aContext->getPrivateContext());
    if (!es) {
        NS_ERROR("Need txExecutionState!");

        return NS_ERROR_UNEXPECTED;
    }

    const txXPathNode& document = es->getSourceDocument();

    nsIDocument *doc = txXPathNativeNode::getDocument(document);
    nsCOMPtr<nsIContent> text;
    nsresult rv = NS_NewTextNode(getter_AddRefs(text), doc->NodeInfoManager());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = text->SetText(aValue, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    *aResult = txXPathNativeNode::createXPathNode(text, PR_TRUE);
    NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

    return NS_OK;
}

static nsresult
createDocFragment(txIEvalContext *aContext, nsIContent** aResult)
{
    txExecutionState* es = 
        static_cast<txExecutionState*>(aContext->getPrivateContext());
    if (!es) {
        NS_ERROR("Need txExecutionState!");

        return NS_ERROR_UNEXPECTED;
    }

    const txXPathNode& document = es->getSourceDocument();
    nsIDocument *doc = txXPathNativeNode::getDocument(document);
    nsCOMPtr<nsIDOMDocumentFragment> domFragment;
    nsresult rv = NS_NewDocumentFragment(getter_AddRefs(domFragment),
                                         doc->NodeInfoManager());
    NS_ENSURE_SUCCESS(rv, rv);

    return CallQueryInterface(domFragment, aResult);
}

static nsresult
createAndAddToResult(nsIAtom* aName, const nsSubstring& aValue,
                     txNodeSet* aResultSet, nsIContent* aResultHolder)
{
    NS_ASSERTION(aResultHolder->IsNodeOfType(nsINode::eDOCUMENT_FRAGMENT) &&
                 aResultHolder->GetOwnerDoc(),
                 "invalid result-holder");

    nsIDocument* doc = aResultHolder->GetOwnerDoc();
    nsCOMPtr<nsIContent> elem;
    nsresult rv = doc->CreateElem(nsDependentAtomString(aName),
                                  nsnull, kNameSpaceID_None, PR_FALSE,
                                  getter_AddRefs(elem));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIContent> text;
    rv = NS_NewTextNode(getter_AddRefs(text), doc->NodeInfoManager());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = text->SetText(aValue, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = elem->AppendChildTo(text, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aResultHolder->AppendChildTo(elem, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txXPathNode> xpathNode(
          txXPathNativeNode::createXPathNode(elem, PR_TRUE));
    NS_ENSURE_TRUE(xpathNode, NS_ERROR_OUT_OF_MEMORY);

    aResultSet->append(*xpathNode);

    return NS_OK;
}

// Need to update this array if types are added to the ResultType enum in
// txAExprResult.
static const char * const sTypes[] = {
  "node-set",
  "boolean",
  "number",
  "string",
  "RTF"
};

// ------------------------------------------------------------------
// Function implementations
// ------------------------------------------------------------------

struct txEXSLTFunctionDescriptor
{
    PRInt8 mMinParams;
    PRInt8 mMaxParams;
    Expr::ResultType mReturnType;
    nsIAtom** mName;
    PRInt32 mNamespaceID;
    const char* mNamespaceURI;
};

static const char kEXSLTCommonNS[] = "http://exslt.org/common";
static const char kEXSLTSetsNS[] = "http://exslt.org/sets";
static const char kEXSLTStringsNS[] = "http://exslt.org/strings";
static const char kEXSLTMathNS[] = "http://exslt.org/math";
static const char kEXSLTDatesAndTimesNS[] = "http://exslt.org/dates-and-times";

// The order of this table must be the same as the
// txEXSLTFunctionCall::eType enum
static txEXSLTFunctionDescriptor descriptTable[] =
{
    { 1, 1, Expr::NODESET_RESULT, &txXSLTAtoms::nodeSet, 0, kEXSLTCommonNS }, // NODE_SET
    { 1, 1, Expr::STRING_RESULT,  &txXSLTAtoms::objectType, 0, kEXSLTCommonNS }, // OBJECT_TYPE
    { 2, 2, Expr::NODESET_RESULT, &txXSLTAtoms::difference, 0, kEXSLTSetsNS }, // DIFFERENCE
    { 1, 1, Expr::NODESET_RESULT, &txXSLTAtoms::distinct, 0, kEXSLTSetsNS }, // DISTINCT
    { 2, 2, Expr::BOOLEAN_RESULT, &txXSLTAtoms::hasSameNode, 0, kEXSLTSetsNS }, // HAS_SAME_NODE
    { 2, 2, Expr::NODESET_RESULT, &txXSLTAtoms::intersection, 0, kEXSLTSetsNS }, // INTERSECTION
    { 2, 2, Expr::NODESET_RESULT, &txXSLTAtoms::leading, 0, kEXSLTSetsNS }, // LEADING
    { 2, 2, Expr::NODESET_RESULT, &txXSLTAtoms::trailing, 0, kEXSLTSetsNS }, // TRAILING
    { 1, 1, Expr::STRING_RESULT,  &txXSLTAtoms::concat, 0, kEXSLTStringsNS }, // CONCAT
    { 1, 2, Expr::STRING_RESULT,  &txXSLTAtoms::split, 0, kEXSLTStringsNS }, // SPLIT
    { 1, 2, Expr::STRING_RESULT,  &txXSLTAtoms::tokenize, 0, kEXSLTStringsNS }, // TOKENIZE
    { 1, 1, Expr::NUMBER_RESULT,  &txXSLTAtoms::max, 0, kEXSLTMathNS }, // MAX
    { 1, 1, Expr::NUMBER_RESULT,  &txXSLTAtoms::min, 0, kEXSLTMathNS }, // MIN
    { 1, 1, Expr::NODESET_RESULT, &txXSLTAtoms::highest, 0, kEXSLTMathNS }, // HIGHEST
    { 1, 1, Expr::NODESET_RESULT, &txXSLTAtoms::lowest, 0, kEXSLTMathNS }, // LOWEST
    { 0, 0, Expr::STRING_RESULT,  &txXSLTAtoms::dateTime, 0, kEXSLTDatesAndTimesNS }, // DATE_TIME

};

class txEXSLTFunctionCall : public FunctionCall
{
public:
    // The order of this enum must be the same as the descriptTable
    // table above
    enum eType {
        // Set functions
        NODE_SET,
        OBJECT_TYPE,
        DIFFERENCE,
        DISTINCT,
        HAS_SAME_NODE,
        INTERSECTION,
        LEADING,
        TRAILING,
        CONCAT,
        SPLIT,
        TOKENIZE,
        MAX,
        MIN,
        HIGHEST,
        LOWEST,
        DATE_TIME
    };
    
    txEXSLTFunctionCall(eType aType)
      : mType(aType)
    {
    }

    TX_DECL_FUNCTION

private:
    eType mType;
};

nsresult
txEXSLTFunctionCall::evaluate(txIEvalContext *aContext,
                              txAExprResult **aResult)
{
    *aResult = nsnull;
    if (!requireParams(descriptTable[mType].mMinParams,
                       descriptTable[mType].mMaxParams,
                       aContext)) {
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
    }

    nsresult rv = NS_OK;
    switch (mType) {
        case NODE_SET:
        {
            nsRefPtr<txAExprResult> exprResult;
            rv = mParams[0]->evaluate(aContext, getter_AddRefs(exprResult));
            NS_ENSURE_SUCCESS(rv, rv);

            if (exprResult->getResultType() == txAExprResult::NODESET) {
                exprResult.swap(*aResult);
            }
            else {
                nsRefPtr<txNodeSet> resultSet;
                rv = aContext->recycler()->
                    getNodeSet(getter_AddRefs(resultSet));
                NS_ENSURE_SUCCESS(rv, rv);

                if (exprResult->getResultType() ==
                    txAExprResult::RESULT_TREE_FRAGMENT) {
                    txResultTreeFragment *rtf =
                        static_cast<txResultTreeFragment*>
                                   (exprResult.get());

                    const txXPathNode *node = rtf->getNode();
                    if (!node) {
                        rv = convertRtfToNode(aContext, rtf);
                        NS_ENSURE_SUCCESS(rv, rv);

                        node = rtf->getNode();
                    }

                    resultSet->append(*node);
                }
                else {
                    nsAutoString value;
                    exprResult->stringValue(value);

                    nsAutoPtr<txXPathNode> node;
                    rv = createTextNode(aContext, value,
                                        getter_Transfers(node));
                    NS_ENSURE_SUCCESS(rv, rv);

                    resultSet->append(*node);
                }

                NS_ADDREF(*aResult = resultSet);
            }

            return NS_OK;
        }
        case OBJECT_TYPE:
        {
            nsRefPtr<txAExprResult> exprResult;
            nsresult rv = mParams[0]->evaluate(aContext,
                                               getter_AddRefs(exprResult));
            NS_ENSURE_SUCCESS(rv, rv);

            nsRefPtr<StringResult> strRes;
            rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
            NS_ENSURE_SUCCESS(rv, rv);

            AppendASCIItoUTF16(sTypes[exprResult->getResultType()],
                               strRes->mValue);

            NS_ADDREF(*aResult = strRes);

            return NS_OK;
        }
        case DIFFERENCE:
        case INTERSECTION:
        {
            nsRefPtr<txNodeSet> nodes1;
            rv = evaluateToNodeSet(mParams[0], aContext,
                                   getter_AddRefs(nodes1));
            NS_ENSURE_SUCCESS(rv, rv);

            nsRefPtr<txNodeSet> nodes2;
            rv = evaluateToNodeSet(mParams[1], aContext,
                                   getter_AddRefs(nodes2));
            NS_ENSURE_SUCCESS(rv, rv);

            nsRefPtr<txNodeSet> resultSet;
            rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
            NS_ENSURE_SUCCESS(rv, rv);

            PRBool insertOnFound = mType == INTERSECTION;

            PRInt32 searchPos = 0;
            PRInt32 i, len = nodes1->size();
            for (i = 0; i < len; ++i) {
                const txXPathNode& node = nodes1->get(i);
                PRInt32 foundPos = nodes2->indexOf(node, searchPos);
                if (foundPos >= 0) {
                    searchPos = foundPos + 1;
                }

                if ((foundPos >= 0) == insertOnFound) {
                    rv = resultSet->append(node);
                    NS_ENSURE_SUCCESS(rv, rv);
                }
            }

            NS_ADDREF(*aResult = resultSet);

            return NS_OK;
        }
        case DISTINCT:
        {
            nsRefPtr<txNodeSet> nodes;
            rv = evaluateToNodeSet(mParams[0], aContext,
                                   getter_AddRefs(nodes));
            NS_ENSURE_SUCCESS(rv, rv);

            nsRefPtr<txNodeSet> resultSet;
            rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
            NS_ENSURE_SUCCESS(rv, rv);

            nsTHashtable<nsStringHashKey> hash;
            if (!hash.Init()) {
                return NS_ERROR_OUT_OF_MEMORY;
            }

            PRInt32 i, len = nodes->size();
            for (i = 0; i < len; ++i) {
                nsAutoString str;
                const txXPathNode& node = nodes->get(i);
                txXPathNodeUtils::appendNodeValue(node, str);
                if (!hash.GetEntry(str)) {
                    if (!hash.PutEntry(str)) {
                        return NS_ERROR_OUT_OF_MEMORY;
                    }
                    rv = resultSet->append(node);
                    NS_ENSURE_SUCCESS(rv, rv);
                }
            }

            NS_ADDREF(*aResult = resultSet);

            return NS_OK;
        }
        case HAS_SAME_NODE:
        {
            nsRefPtr<txNodeSet> nodes1;
            rv = evaluateToNodeSet(mParams[0], aContext,
                                   getter_AddRefs(nodes1));
            NS_ENSURE_SUCCESS(rv, rv);

            nsRefPtr<txNodeSet> nodes2;
            rv = evaluateToNodeSet(mParams[1], aContext,
                                   getter_AddRefs(nodes2));
            NS_ENSURE_SUCCESS(rv, rv);

            PRBool found = PR_FALSE;
            PRInt32 i, len = nodes1->size();
            for (i = 0; i < len; ++i) {
                if (nodes2->contains(nodes1->get(i))) {
                    found = PR_TRUE;
                    break;
                }
            }

            aContext->recycler()->getBoolResult(found, aResult);

            return NS_OK;
        }
        case LEADING:
        case TRAILING:
        {
            nsRefPtr<txNodeSet> nodes1;
            rv = evaluateToNodeSet(mParams[0], aContext,
                                   getter_AddRefs(nodes1));
            NS_ENSURE_SUCCESS(rv, rv);

            nsRefPtr<txNodeSet> nodes2;
            rv = evaluateToNodeSet(mParams[1], aContext,
                                   getter_AddRefs(nodes2));
            NS_ENSURE_SUCCESS(rv, rv);

            if (nodes2->isEmpty()) {
                *aResult = nodes1;
                NS_ADDREF(*aResult);

                return NS_OK;
            }

            nsRefPtr<txNodeSet> resultSet;
            rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
            NS_ENSURE_SUCCESS(rv, rv);

            PRInt32 end = nodes1->indexOf(nodes2->get(0));
            if (end >= 0) {
                PRInt32 i = 0;
                if (mType == TRAILING) {
                    i = end + 1;
                    end = nodes1->size();
                }
                for (; i < end; ++i) {
                    rv = resultSet->append(nodes1->get(i));
                    NS_ENSURE_SUCCESS(rv, rv);
                }
            }

            NS_ADDREF(*aResult = resultSet);

            return NS_OK;
        }
        case CONCAT:
        {
            nsRefPtr<txNodeSet> nodes;
            rv = evaluateToNodeSet(mParams[0], aContext,
                                   getter_AddRefs(nodes));
            NS_ENSURE_SUCCESS(rv, rv);

            nsAutoString str;
            PRInt32 i, len = nodes->size();
            for (i = 0; i < len; ++i) {
                txXPathNodeUtils::appendNodeValue(nodes->get(i), str);
            }

            return aContext->recycler()->getStringResult(str, aResult);
        }
        case SPLIT:
        case TOKENIZE:
        {
            // Evaluate parameters
            nsAutoString string;
            rv = mParams[0]->evaluateToString(aContext, string);
            NS_ENSURE_SUCCESS(rv, rv);

            nsAutoString pattern;
            if (mParams.Length() == 2) {
                rv = mParams[1]->evaluateToString(aContext, pattern);
                NS_ENSURE_SUCCESS(rv, rv);
            }
            else if (mType == SPLIT) {
                pattern.AssignLiteral(" ");
            }
            else {
                pattern.AssignLiteral("\t\r\n ");
            }

            // Set up holders for the result
            nsCOMPtr<nsIContent> docFrag;
            rv = createDocFragment(aContext, getter_AddRefs(docFrag));
            NS_ENSURE_SUCCESS(rv, rv);

            nsRefPtr<txNodeSet> resultSet;
            rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
            NS_ENSURE_SUCCESS(rv, rv);

            PRUint32 tailIndex;

            // Start splitting
            if (pattern.IsEmpty()) {
                nsString::const_char_iterator start = string.BeginReading();
                nsString::const_char_iterator end = string.EndReading();
                for (; start < end; ++start) {
                    rv = createAndAddToResult(txXSLTAtoms::token,
                                              Substring(start, start + 1),
                                              resultSet, docFrag);
                    NS_ENSURE_SUCCESS(rv, rv);
                }

                tailIndex = string.Length();
            }
            else if (mType == SPLIT) {
                nsAString::const_iterator strStart, strEnd;
                string.BeginReading(strStart);
                string.EndReading(strEnd);
                nsAString::const_iterator start = strStart, end = strEnd;

                while (FindInReadable(pattern, start, end)) {
                    if (start != strStart) {
                        rv = createAndAddToResult(txXSLTAtoms::token,
                                                  Substring(strStart, start),
                                                  resultSet, docFrag);
                        NS_ENSURE_SUCCESS(rv, rv);
                    }
                    strStart = start = end;
                    end = strEnd;
                }

                tailIndex = strStart.get() - string.get();
            }
            else {
                PRInt32 found, start = 0;
                while ((found = string.FindCharInSet(pattern, start)) !=
                       kNotFound) {
                    if (found != start) {
                        rv = createAndAddToResult(txXSLTAtoms::token,
                                                  Substring(string, start,
                                                            found - start),
                                                  resultSet, docFrag);
                        NS_ENSURE_SUCCESS(rv, rv);
                    }
                    start = found + 1;
                }

                tailIndex = start;
            }

            // Add tail if needed
            if (tailIndex != (PRUint32)string.Length()) {
                rv = createAndAddToResult(txXSLTAtoms::token,
                                          Substring(string, tailIndex),
                                          resultSet, docFrag);
                NS_ENSURE_SUCCESS(rv, rv);
            }

            NS_ADDREF(*aResult = resultSet);

            return NS_OK;
        }
        case MAX:
        case MIN:
        {
            nsRefPtr<txNodeSet> nodes;
            rv = evaluateToNodeSet(mParams[0], aContext,
                                   getter_AddRefs(nodes));
            NS_ENSURE_SUCCESS(rv, rv);

            if (nodes->isEmpty()) {
                return aContext->recycler()->
                    getNumberResult(Double::NaN, aResult);
            }

            PRBool findMax = mType == MAX;

            double res = findMax ? txDouble::NEGATIVE_INFINITY :
                                   txDouble::POSITIVE_INFINITY;
            PRInt32 i, len = nodes->size();
            for (i = 0; i < len; ++i) {
                nsAutoString str;
                txXPathNodeUtils::appendNodeValue(nodes->get(i), str);
                double val = Double::toDouble(str);
                if (Double::isNaN(val)) {
                    res = Double::NaN;
                    break;
                }

                if (findMax ? (val > res) : (val < res)) {
                    res = val;
                }
            }

            return aContext->recycler()->getNumberResult(res, aResult);
        }
        case HIGHEST:
        case LOWEST:
        {
            nsRefPtr<txNodeSet> nodes;
            rv = evaluateToNodeSet(mParams[0], aContext,
                                   getter_AddRefs(nodes));
            NS_ENSURE_SUCCESS(rv, rv);

            if (nodes->isEmpty()) {
                NS_ADDREF(*aResult = nodes);

                return NS_OK;
            }

            nsRefPtr<txNodeSet> resultSet;
            rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
            NS_ENSURE_SUCCESS(rv, rv);

            PRBool findMax = mType == HIGHEST;
            double res = findMax ? txDouble::NEGATIVE_INFINITY :
                                   txDouble::POSITIVE_INFINITY;
            PRInt32 i, len = nodes->size();
            for (i = 0; i < len; ++i) {
                nsAutoString str;
                const txXPathNode& node = nodes->get(i);
                txXPathNodeUtils::appendNodeValue(node, str);
                double val = Double::toDouble(str);
                if (Double::isNaN(val)) {
                    resultSet->clear();
                    break;
                }
                if (findMax ? (val > res) : (val < res)) {
                    resultSet->clear();
                    res = val;
                }

                if (res == val) {
                    rv = resultSet->append(node);
                    NS_ENSURE_SUCCESS(rv, rv);
                }
            }

            NS_ADDREF(*aResult = resultSet);

            return NS_OK;
        }
        case DATE_TIME:
        {
            // http://exslt.org/date/functions/date-time/
            // format: YYYY-MM-DDTTHH:MM:SS.sss+00:00
            char formatstr[] = "%04hd-%02ld-%02ldT%02ld:%02ld:%02ld.%03ld%c%02ld:%02ld";
            const size_t max = sizeof("YYYY-MM-DDTHH:MM:SS.sss+00:00");
            
            PRExplodedTime prtime;
            PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &prtime);
            
            PRInt32 offset = (prtime.tm_params.tp_gmt_offset +
              prtime.tm_params.tp_dst_offset) / 60;
              
            PRBool isneg = offset < 0;
            if (isneg) offset = -offset;
            
            StringResult* strRes;
            rv = aContext->recycler()->getStringResult(&strRes);
            NS_ENSURE_SUCCESS(rv, rv);
            
            CopyASCIItoUTF16(nsPrintfCString(max, formatstr,
              prtime.tm_year, prtime.tm_month + 1, prtime.tm_mday,
              prtime.tm_hour, prtime.tm_min, prtime.tm_sec,
              prtime.tm_usec / 10000,
              isneg ? '-' : '+', offset / 60, offset % 60), strRes->mValue);
              
            *aResult = strRes;

            return NS_OK;
        }
    }

    aContext->receiveError(NS_LITERAL_STRING("Internal error"),
                           NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;
}

Expr::ResultType
txEXSLTFunctionCall::getReturnType()
{
    return descriptTable[mType].mReturnType;
}

PRBool
txEXSLTFunctionCall::isSensitiveTo(ContextSensitivity aContext)
{
    if (mType == NODE_SET || mType == SPLIT || mType == TOKENIZE) {
        return (aContext & PRIVATE_CONTEXT) || argsSensitiveTo(aContext);
    }
    return argsSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
nsresult
txEXSLTFunctionCall::getNameAtom(nsIAtom **aAtom)
{
    NS_ADDREF(*aAtom = *descriptTable[mType].mName);
    return NS_OK;
}
#endif

extern nsresult
TX_ConstructEXSLTFunction(nsIAtom *aName,
                          PRInt32 aNamespaceID,
                          txStylesheetCompilerState* aState,
                          FunctionCall **aResult)
{
    PRUint32 i;
    for (i = 0; i < NS_ARRAY_LENGTH(descriptTable); ++i) {
        txEXSLTFunctionDescriptor& desc = descriptTable[i];
        if (aName == *desc.mName && aNamespaceID == desc.mNamespaceID) {
            *aResult = new txEXSLTFunctionCall(
                static_cast<txEXSLTFunctionCall::eType>(i));

            return *aResult ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
        }
    }

    return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
}

extern PRBool
TX_InitEXSLTFunction()
{
    PRUint32 i;
    for (i = 0; i < NS_ARRAY_LENGTH(descriptTable); ++i) {
        txEXSLTFunctionDescriptor& desc = descriptTable[i];
        NS_ConvertASCIItoUTF16 namespaceURI(desc.mNamespaceURI);
        desc.mNamespaceID =
            txNamespaceManager::getNamespaceID(namespaceURI);

        if (desc.mNamespaceID == kNameSpaceID_Unknown) {
            return PR_FALSE;
        }
    }

    return PR_TRUE;
}
