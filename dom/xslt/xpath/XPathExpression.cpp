/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Move.h"
#include "XPathExpression.h"
#include "txExpr.h"
#include "txExprResult.h"
#include "txIXPathContext.h"
#include "nsError.h"
#include "nsINode.h"
#include "XPathResult.h"
#include "txURIUtils.h"
#include "txXPathTreeWalker.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Text.h"
#include "mozilla/dom/XPathResultBinding.h"

using mozilla::Move;

namespace mozilla {
namespace dom {

class EvalContextImpl : public txIEvalContext
{
public:
    EvalContextImpl(const txXPathNode& aContextNode,
                    uint32_t aContextPosition, uint32_t aContextSize,
                    txResultRecycler* aRecycler)
        : mContextNode(aContextNode),
          mContextPosition(aContextPosition),
          mContextSize(aContextSize),
          mLastError(NS_OK),
          mRecycler(aRecycler)
    {
    }

    nsresult getError()
    {
        return mLastError;
    }

    TX_DECL_EVAL_CONTEXT;

private:
    const txXPathNode& mContextNode;
    uint32_t mContextPosition;
    uint32_t mContextSize;
    nsresult mLastError;
    RefPtr<txResultRecycler> mRecycler;
};

XPathExpression::XPathExpression(nsAutoPtr<Expr>&& aExpression,
                                 txResultRecycler* aRecycler,
                                 nsIDocument *aDocument)
    : mExpression(Move(aExpression)),
      mRecycler(aRecycler),
      mDocument(do_GetWeakReference(aDocument)),
      mCheckDocument(aDocument != nullptr)
{
}

XPathExpression::~XPathExpression()
{
}

already_AddRefed<XPathResult>
XPathExpression::EvaluateWithContext(JSContext* aCx,
                                     nsINode& aContextNode,
                                     uint32_t aContextPosition,
                                     uint32_t aContextSize,
                                     uint16_t aType,
                                     JS::Handle<JSObject*> aInResult,
                                     ErrorResult& aRv)
{
    RefPtr<XPathResult> inResult;
    if (aInResult) {
        nsresult rv = UNWRAP_OBJECT(XPathResult, aInResult, inResult);
        if (NS_FAILED(rv) && rv != NS_ERROR_XPC_BAD_CONVERT_JS) {
            aRv.Throw(rv);
            return nullptr;
        }
    }

    return EvaluateWithContext(aContextNode, aContextPosition, aContextSize,
                               aType, inResult, aRv);
}

already_AddRefed<XPathResult>
XPathExpression::EvaluateWithContext(nsINode& aContextNode,
                                     uint32_t aContextPosition,
                                     uint32_t aContextSize,
                                     uint16_t aType,
                                     XPathResult* aInResult,
                                     ErrorResult& aRv)
{
    if (aContextPosition > aContextSize) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
    }

    if (aType > XPathResultBinding::FIRST_ORDERED_NODE_TYPE) {
        aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        return nullptr;
    }

    if (!nsContentUtils::LegacyIsCallerNativeCode() &&
        !nsContentUtils::CanCallerAccess(&aContextNode))
    {
        aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
        return nullptr;
    }

    if (mCheckDocument) {
        nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
        if (doc != aContextNode.OwnerDoc()) {
            aRv.Throw(NS_ERROR_DOM_WRONG_DOCUMENT_ERR);
            return nullptr;
        }
    }

    uint16_t nodeType = aContextNode.NodeType();

    if (nodeType == nsINode::TEXT_NODE ||
        nodeType == nsINode::CDATA_SECTION_NODE) {
        Text* textNode = aContextNode.GetAsText();
        MOZ_ASSERT(textNode);

        uint32_t textLength = textNode->Length();
        if (textLength == 0) {
            aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
            return nullptr;
        }

        // XXX Need to get logical XPath text node for CDATASection
        //     and Text nodes.
    }
    else if (nodeType != nsINode::DOCUMENT_NODE &&
             nodeType != nsINode::ELEMENT_NODE &&
             nodeType != nsINode::ATTRIBUTE_NODE &&
             nodeType != nsINode::COMMENT_NODE &&
             nodeType != nsINode::PROCESSING_INSTRUCTION_NODE) {
        aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        return nullptr;
    }

    nsAutoPtr<txXPathNode> contextNode(txXPathNativeNode::createXPathNode(&aContextNode));
    if (!contextNode) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    EvalContextImpl eContext(*contextNode, aContextPosition, aContextSize,
                             mRecycler);
    RefPtr<txAExprResult> exprResult;
    aRv = mExpression->evaluate(&eContext, getter_AddRefs(exprResult));
    if (aRv.Failed()) {
        return nullptr;
    }

    uint16_t resultType = aType;
    if (aType == XPathResult::ANY_TYPE) {
        short exprResultType = exprResult->getResultType();
        switch (exprResultType) {
            case txAExprResult::NUMBER:
                resultType = XPathResult::NUMBER_TYPE;
                break;
            case txAExprResult::STRING:
                resultType = XPathResult::STRING_TYPE;
                break;
            case txAExprResult::BOOLEAN:
                resultType = XPathResult::BOOLEAN_TYPE;
                break;
            case txAExprResult::NODESET:
                resultType = XPathResult::UNORDERED_NODE_ITERATOR_TYPE;
                break;
            case txAExprResult::RESULT_TREE_FRAGMENT:
                aRv.Throw(NS_ERROR_FAILURE);
                return nullptr;
        }
    }

    RefPtr<XPathResult> xpathResult = aInResult;
    if (!xpathResult) {
        xpathResult = new XPathResult(&aContextNode);
    }

    aRv = xpathResult->SetExprResult(exprResult, resultType, &aContextNode);

    return xpathResult.forget();
}

/*
 * Implementation of the txIEvalContext private to XPathExpression
 * EvalContextImpl bases on only one context node and no variables
 */

nsresult
EvalContextImpl::getVariable(int32_t aNamespace,
                             nsAtom* aLName,
                             txAExprResult*& aResult)
{
    aResult = 0;
    return NS_ERROR_INVALID_ARG;
}

nsresult
EvalContextImpl::isStripSpaceAllowed(const txXPathNode& aNode, bool& aAllowed)
{
    aAllowed = false;

    return NS_OK;
}

void*
EvalContextImpl::getPrivateContext()
{
    // we don't have a private context here.
    return nullptr;
}

txResultRecycler*
EvalContextImpl::recycler()
{
    return mRecycler;
}

void
EvalContextImpl::receiveError(const nsAString& aMsg, nsresult aRes)
{
    mLastError = aRes;
    // forward aMsg to console service?
}

const txXPathNode&
EvalContextImpl::getContextNode()
{
    return mContextNode;
}

uint32_t
EvalContextImpl::size()
{
    return mContextSize;
}

uint32_t
EvalContextImpl::position()
{
    return mContextPosition;
}

} // namespace dom
} // namespace mozilla
