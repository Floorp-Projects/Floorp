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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
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

#include "nsXPathExpression.h"
#include "txExpr.h"
#include "txExprResult.h"
#include "nsDOMError.h"
#include "nsIDOMCharacterData.h"
#include "nsDOMClassInfoID.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXPathNamespace.h"
#include "nsXPathResult.h"
#include "nsDOMError.h"
#include "txURIUtils.h"
#include "txXPathTreeWalker.h"

NS_IMPL_CYCLE_COLLECTION_1(nsXPathExpression, mDocument)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXPathExpression)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXPathExpression)

DOMCI_DATA(XPathExpression, nsXPathExpression)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXPathExpression)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXPathExpression)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSXPathExpression)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMXPathExpression)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(XPathExpression)
NS_INTERFACE_MAP_END

nsXPathExpression::nsXPathExpression(nsAutoPtr<Expr>& aExpression,
                                     txResultRecycler* aRecycler,
                                     nsIDOMDocument *aDocument)
    : mExpression(aExpression),
      mRecycler(aRecycler),
      mDocument(aDocument)
{
}

NS_IMETHODIMP
nsXPathExpression::Evaluate(nsIDOMNode *aContextNode,
                            PRUint16 aType,
                            nsISupports *aInResult,
                            nsISupports **aResult)
{
    return EvaluateWithContext(aContextNode, 1, 1, aType, aInResult, aResult);
}

NS_IMETHODIMP
nsXPathExpression::EvaluateWithContext(nsIDOMNode *aContextNode,
                                       PRUint32 aContextPosition,
                                       PRUint32 aContextSize,
                                       PRUint16 aType,
                                       nsISupports *aInResult,
                                       nsISupports **aResult)
{
    nsCOMPtr<nsINode> context = do_QueryInterface(aContextNode);
    NS_ENSURE_ARG(context);

    if (aContextPosition > aContextSize)
        return NS_ERROR_FAILURE;

    if (!nsContentUtils::CanCallerAccess(aContextNode))
        return NS_ERROR_DOM_SECURITY_ERR;

    if (mDocument && mDocument != aContextNode) {
        nsCOMPtr<nsIDOMDocument> contextDocument;
        aContextNode->GetOwnerDocument(getter_AddRefs(contextDocument));

        if (mDocument != contextDocument) {
            return NS_ERROR_DOM_WRONG_DOCUMENT_ERR;
        }
    }

    PRUint16 nodeType = context->NodeType();

    if (nodeType == nsIDOMNode::TEXT_NODE ||
        nodeType == nsIDOMNode::CDATA_SECTION_NODE) {
        nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(aContextNode);
        NS_ENSURE_TRUE(textNode, NS_ERROR_FAILURE);

        if (textNode) {
            PRUint32 textLength;
            textNode->GetLength(&textLength);
            if (textLength == 0)
                return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
        }

        // XXX Need to get logical XPath text node for CDATASection
        //     and Text nodes.
    }
    else if (nodeType != nsIDOMNode::DOCUMENT_NODE &&
             nodeType != nsIDOMNode::ELEMENT_NODE &&
             nodeType != nsIDOMNode::ATTRIBUTE_NODE &&
             nodeType != nsIDOMNode::COMMENT_NODE &&
             nodeType != nsIDOMNode::PROCESSING_INSTRUCTION_NODE &&
             nodeType != nsIDOMXPathNamespace::XPATH_NAMESPACE_NODE) {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }

    NS_ENSURE_ARG(aResult);
    *aResult = nsnull;

    nsAutoPtr<txXPathNode> contextNode(txXPathNativeNode::createXPathNode(aContextNode));
    if (!contextNode) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    EvalContextImpl eContext(*contextNode, aContextPosition, aContextSize,
                             mRecycler);
    nsRefPtr<txAExprResult> exprResult;
    nsresult rv = mExpression->evaluate(&eContext, getter_AddRefs(exprResult));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint16 resultType = aType;
    if (aType == nsIDOMXPathResult::ANY_TYPE) {
        short exprResultType = exprResult->getResultType();
        switch (exprResultType) {
            case txAExprResult::NUMBER:
                resultType = nsIDOMXPathResult::NUMBER_TYPE;
                break;
            case txAExprResult::STRING:
                resultType = nsIDOMXPathResult::STRING_TYPE;
                break;
            case txAExprResult::BOOLEAN:
                resultType = nsIDOMXPathResult::BOOLEAN_TYPE;
                break;
            case txAExprResult::NODESET:
                resultType = nsIDOMXPathResult::UNORDERED_NODE_ITERATOR_TYPE;
                break;
            case txAExprResult::RESULT_TREE_FRAGMENT:
                NS_ERROR("Can't return a tree fragment!");
                return NS_ERROR_FAILURE;
        }
    }

    // We need a result object and it must be our implementation.
    nsCOMPtr<nsIXPathResult> xpathResult = do_QueryInterface(aInResult);
    if (!xpathResult) {
        // Either no aInResult or not one of ours.
        xpathResult = new nsXPathResult();
        NS_ENSURE_TRUE(xpathResult, NS_ERROR_OUT_OF_MEMORY);
    }
    rv = xpathResult->SetExprResult(exprResult, resultType, context);
    NS_ENSURE_SUCCESS(rv, rv);

    return CallQueryInterface(xpathResult, aResult);
}

/*
 * Implementation of the txIEvalContext private to nsXPathExpression
 * EvalContextImpl bases on only one context node and no variables
 */

nsresult
nsXPathExpression::EvalContextImpl::getVariable(PRInt32 aNamespace,
                                                nsIAtom* aLName,
                                                txAExprResult*& aResult)
{
    aResult = 0;
    return NS_ERROR_INVALID_ARG;
}

bool nsXPathExpression::EvalContextImpl::isStripSpaceAllowed(const txXPathNode& aNode)
{
    return false;
}

void* nsXPathExpression::EvalContextImpl::getPrivateContext()
{
    // we don't have a private context here.
    return nsnull;
}

txResultRecycler* nsXPathExpression::EvalContextImpl::recycler()
{
    return mRecycler;
}

void nsXPathExpression::EvalContextImpl::receiveError(const nsAString& aMsg,
                                                      nsresult aRes)
{
    mLastError = aRes;
    // forward aMsg to console service?
}

const txXPathNode& nsXPathExpression::EvalContextImpl::getContextNode()
{
    return mContextNode;
}

PRUint32 nsXPathExpression::EvalContextImpl::size()
{
    return mContextSize;
}

PRUint32 nsXPathExpression::EvalContextImpl::position()
{
    return mContextPosition;
}
