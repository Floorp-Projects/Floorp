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

#ifndef TX_EXE
#include "nsComponentManagerUtils.h"
#include "nsContentCID.h"
#include "nsContentCreatorFunctions.h"
#include "nsIContent.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMText.h"
#include "txMozillaXMLOutput.h"
#endif

class txStylesheetCompilerState;

static nsresult
convertRtfToNode(txIEvalContext *aContext, txResultTreeFragment *aRtf)
{
    txExecutionState* es = 
        NS_STATIC_CAST(txExecutionState*, aContext->getPrivateContext());
    if (!es) {
        NS_ERROR("Need txExecutionState!");

        return NS_ERROR_UNEXPECTED;
    }

    const txXPathNode& document = es->getSourceDocument();

#ifdef TX_EXE
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    nsIDocument *doc = txXPathNativeNode::getDocument(document);
    nsCOMPtr<nsIDOMDocumentFragment> domFragment;
    nsresult rv = NS_NewDocumentFragment(getter_AddRefs(domFragment),
                                         doc->NodeInfoManager());
    NS_ENSURE_SUCCESS(rv, rv);

    txOutputFormat format;
    txMozillaXMLOutput mozHandler(&format, domFragment, PR_TRUE);

    rv = aRtf->flushToHandler(&mozHandler);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsINode> fragment = do_QueryInterface(domFragment, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // The txResultTreeFragment will own this.
    const txXPathNode* node = txXPathNativeNode::createXPathNode(domFragment,
                                                                 PR_TRUE);
    NS_ENSURE_TRUE(node, NS_ERROR_OUT_OF_MEMORY);

    aRtf->setNode(node);

    return NS_OK;
#endif
}

static nsresult
createTextNode(txIEvalContext *aContext, nsString& aValue,
               txXPathNode* *aResult)
{
    txExecutionState* es = 
        NS_STATIC_CAST(txExecutionState*, aContext->getPrivateContext());
    if (!es) {
        NS_ERROR("Need txExecutionState!");

        return NS_ERROR_UNEXPECTED;
    }

    const txXPathNode& document = es->getSourceDocument();

#ifdef TX_EXE
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    nsIDocument *doc = txXPathNativeNode::getDocument(document);
    nsCOMPtr<nsIContent> text;
    nsresult rv = NS_NewTextNode(getter_AddRefs(text), doc->NodeInfoManager());
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMText> domText = do_QueryInterface(text, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    *aResult = txXPathNativeNode::createXPathNode(domText, PR_TRUE);
    NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

    return NS_OK;
#endif
}

class txEXSLTNodeSetFunctionCall : public FunctionCall
{
public:
    TX_DECL_FUNCTION
};

nsresult
txEXSLTNodeSetFunctionCall::evaluate(txIEvalContext *aContext,
                                     txAExprResult **aResult)
{
    *aResult = nsnull;

    if (!requireParams(1, 1, aContext)) {
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
    }

    txListIterator iter(&params);
    Expr* param1 = NS_STATIC_CAST(Expr*, iter.next());

    nsRefPtr<txAExprResult> exprResult;
    nsresult rv = param1->evaluate(aContext, getter_AddRefs(exprResult));
    NS_ENSURE_SUCCESS(rv, rv);

    if (exprResult->getResultType() == txAExprResult::NODESET) {
        exprResult.swap(*aResult);
    }
    else {
        nsRefPtr<txNodeSet> nodeset;
        rv = aContext->recycler()->getNodeSet(getter_AddRefs(nodeset));
        NS_ENSURE_SUCCESS(rv, rv);

        if (exprResult->getResultType() == txAExprResult::RESULT_TREE_FRAGMENT) {
            txResultTreeFragment *rtf =
                NS_STATIC_CAST(txResultTreeFragment*,
                               NS_STATIC_CAST(txAExprResult*, exprResult));

            const txXPathNode *node = rtf->getNode();
            if (!node) {
                rv = convertRtfToNode(aContext, rtf);
                NS_ENSURE_SUCCESS(rv, rv);

                node = rtf->getNode();
            }

            nodeset->append(*node);
        }
        else {
            nsAutoString value;
            exprResult->stringValue(value);

            nsAutoPtr<txXPathNode> node;
            rv = createTextNode(aContext, value, getter_Transfers(node));
            NS_ENSURE_SUCCESS(rv, rv);

            nodeset->append(*node);
        }

        NS_ADDREF(*aResult = nodeset);
    }

    return NS_OK;
}

Expr::ResultType
txEXSLTNodeSetFunctionCall::getReturnType()
{
    return Expr::NODESET_RESULT;
}

PRBool
txEXSLTNodeSetFunctionCall::isSensitiveTo(ContextSensitivity aContext)
{
    return (aContext & PRIVATE_CONTEXT) || argsSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
nsresult
txEXSLTNodeSetFunctionCall::getNameAtom(nsIAtom **aAtom)
{
    NS_ADDREF(*aAtom = txXSLTAtoms::nodeSet);

    return NS_OK;
}
#endif

class txEXSLTObjectTypeFunctionCall : public FunctionCall
{
public:
    TX_DECL_FUNCTION
};

// Need to update this array if types are added to the ResultType enum in
// txAExprResult.
static const char * const sTypes[] = {
  "node-set",
  "boolean",
  "number",
  "string",
  "RTF"
};

nsresult
txEXSLTObjectTypeFunctionCall::evaluate(txIEvalContext *aContext,
                                        txAExprResult **aResult)
{
    *aResult = nsnull;

    if (!requireParams(1, 1, aContext)) {
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
    }

    txListIterator iter(&params);
    Expr* param1 = NS_STATIC_CAST(Expr*, iter.next());

    nsRefPtr<txAExprResult> exprResult;
    nsresult rv = param1->evaluate(aContext, getter_AddRefs(exprResult));
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<StringResult> strRes;
    rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
    NS_ENSURE_SUCCESS(rv, rv);

    AppendASCIItoUTF16(sTypes[exprResult->getResultType()], strRes->mValue);

    NS_ADDREF(*aResult = strRes);

    return NS_OK;
}

Expr::ResultType
txEXSLTObjectTypeFunctionCall::getReturnType()
{
    return Expr::STRING_RESULT;
}

PRBool
txEXSLTObjectTypeFunctionCall::isSensitiveTo(ContextSensitivity aContext)
{
    return (aContext & PRIVATE_CONTEXT) || argsSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
nsresult
txEXSLTObjectTypeFunctionCall::getNameAtom(nsIAtom **aAtom)
{
    NS_ADDREF(*aAtom = txXSLTAtoms::objectType);

    return NS_OK;
}
#endif

extern nsresult
TX_ConstructEXSLTCommonFunction(nsIAtom *aName,
                                txStylesheetCompilerState* aState,
                                FunctionCall **aResult)
{
    if (aName == txXSLTAtoms::nodeSet) {
        *aResult = new txEXSLTNodeSetFunctionCall();
    }
    else if (aName == txXSLTAtoms::objectType) {
        *aResult = new txEXSLTObjectTypeFunctionCall();
    }
    else {
        return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
    }

    return *aResult ? NS_OK : NS_ERROR_OUT_OF_MEMORY;

}
