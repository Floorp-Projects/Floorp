/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XPathEvaluator.h"
#include "mozilla/Move.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "mozilla/dom/XPathExpression.h"
#include "nsXPathNSResolver.h"
#include "XPathResult.h"
#include "nsContentCID.h"
#include "txExpr.h"
#include "txExprParser.h"
#include "nsError.h"
#include "txURIUtils.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsDOMString.h"
#include "nsNameSpaceManager.h"
#include "nsContentUtils.h"
#include "txIXPathContext.h"
#include "mozilla/dom/XPathEvaluatorBinding.h"
#include "mozilla/dom/BindingUtils.h"

extern nsresult
TX_ResolveFunctionCallXPCOM(const nsCString &aContractID, int32_t aNamespaceID,
                            nsIAtom *aName, nsISupports *aState,
                            FunctionCall **aFunction);

namespace mozilla {
namespace dom {

// txIParseContext implementation
class XPathEvaluatorParseContext : public txIParseContext
{
public:
    XPathEvaluatorParseContext(nsIDOMXPathNSResolver* aResolver,
                               bool aIsCaseSensitive)
        : mResolver(aResolver),
          mLastError(NS_OK),
          mIsCaseSensitive(aIsCaseSensitive)
    {
    }

    nsresult getError()
    {
        return mLastError;
    }

    nsresult resolveNamespacePrefix(nsIAtom* aPrefix, int32_t& aID);
    nsresult resolveFunctionCall(nsIAtom* aName, int32_t aID,
                                 FunctionCall** aFunction);
    bool caseInsensitiveNameTests();
    void SetErrorOffset(uint32_t aOffset);

private:
    nsIDOMXPathNSResolver* mResolver;
    nsresult mLastError;
    bool mIsCaseSensitive;
};

NS_IMPL_ISUPPORTS(XPathEvaluator, nsIDOMXPathEvaluator)

XPathEvaluator::XPathEvaluator(nsIDocument* aDocument)
    : mDocument(do_GetWeakReference(aDocument))
{
}

XPathEvaluator::~XPathEvaluator()
{
}

NS_IMETHODIMP
XPathEvaluator::CreateNSResolver(nsIDOMNode *aNodeResolver,
                                 nsIDOMXPathNSResolver **aResult)
{
    NS_ENSURE_ARG(aNodeResolver);
    if (!nsContentUtils::CanCallerAccess(aNodeResolver))
        return NS_ERROR_DOM_SECURITY_ERR;

    *aResult = new nsXPathNSResolver(aNodeResolver);
    NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
XPathEvaluator::Evaluate(const nsAString & aExpression,
                         nsIDOMNode *aContextNode,
                         nsIDOMXPathNSResolver *aResolver,
                         uint16_t aType,
                         nsISupports *aInResult,
                         nsISupports **aResult)
{
    ErrorResult rv;
    nsAutoPtr<XPathExpression> expression(CreateExpression(aExpression,
                                                           aResolver, rv));
    if (rv.Failed()) {
        return rv.ErrorCode();
    }

    nsCOMPtr<nsINode> node = do_QueryInterface(aContextNode);
    if (!node) {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIXPathResult> inResult = do_QueryInterface(aInResult);
    nsRefPtr<XPathResult> result =
        expression->Evaluate(*node, aType,
                             static_cast<XPathResult*>(inResult.get()), rv);
    if (rv.Failed()) {
        return rv.ErrorCode();
    }

    *aResult = ToSupports(result.forget().take());

    return NS_OK;
}


XPathExpression*
XPathEvaluator::CreateExpression(const nsAString & aExpression,
                                 nsIDOMXPathNSResolver *aResolver,
                                 ErrorResult& aRv)
{
    if (!mRecycler) {
        mRecycler = new txResultRecycler;
    }

    nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
    XPathEvaluatorParseContext pContext(aResolver, !(doc && doc->IsHTML()));

    nsAutoPtr<Expr> expression;
    aRv = txExprParser::createExpr(PromiseFlatString(aExpression), &pContext,
                                   getter_Transfers(expression));
    if (aRv.Failed()) {
        if (aRv.ErrorCode() != NS_ERROR_DOM_NAMESPACE_ERR) {
            aRv.Throw(NS_ERROR_DOM_INVALID_EXPRESSION_ERR);
        }

        return nullptr;
    }

    return new XPathExpression(Move(expression), mRecycler, doc);
}

JSObject*
XPathEvaluator::WrapObject(JSContext* aCx)
{
    return dom::XPathEvaluatorBinding::Wrap(aCx, this);
}

/* static */
already_AddRefed<XPathEvaluator>
XPathEvaluator::Constructor(const GlobalObject& aGlobal,
                            ErrorResult& rv)
{
    nsRefPtr<XPathEvaluator> newObj = new XPathEvaluator(nullptr);
    return newObj.forget();
}

already_AddRefed<nsIDOMXPathNSResolver>
XPathEvaluator::CreateNSResolver(nsINode* aNodeResolver,
                                 ErrorResult& rv)
{
  nsCOMPtr<nsIDOMNode> nodeResolver = do_QueryInterface(aNodeResolver);
  nsCOMPtr<nsIDOMXPathNSResolver> res;
  rv = CreateNSResolver(nodeResolver, getter_AddRefs(res));
  return res.forget();
}

already_AddRefed<XPathResult>
XPathEvaluator::Evaluate(JSContext* aCx, const nsAString& aExpression,
                         nsINode* aContextNode,
                         nsIDOMXPathNSResolver* aResolver, uint16_t aType,
                         JS::Handle<JSObject*> aResult, ErrorResult& rv)
{
  nsCOMPtr<nsIDOMNode> contextNode = do_QueryInterface(aContextNode);
  nsCOMPtr<nsISupports> res;
  rv = Evaluate(aExpression, contextNode, aResolver, aType,
                aResult ? UnwrapDOMObjectToISupports(aResult) : nullptr,
                getter_AddRefs(res));
  return res.forget().downcast<nsIXPathResult>().downcast<XPathResult>();
}


/*
 * Implementation of txIParseContext private to XPathEvaluator, based on a
 * nsIDOMXPathNSResolver
 */

nsresult XPathEvaluatorParseContext::resolveNamespacePrefix
    (nsIAtom* aPrefix, int32_t& aID)
{
    aID = kNameSpaceID_Unknown;

    if (!mResolver) {
        return NS_ERROR_DOM_NAMESPACE_ERR;
    }

    nsAutoString prefix;
    if (aPrefix) {
        aPrefix->ToString(prefix);
    }

    nsVoidableString ns;
    nsresult rv = mResolver->LookupNamespaceURI(prefix, ns);
    NS_ENSURE_SUCCESS(rv, rv);

    if (DOMStringIsNull(ns)) {
        return NS_ERROR_DOM_NAMESPACE_ERR;
    }

    if (ns.IsEmpty()) {
        aID = kNameSpaceID_None;

        return NS_OK;
    }

    // get the namespaceID for the URI
    return nsContentUtils::NameSpaceManager()->RegisterNameSpace(ns, aID);
}

nsresult
XPathEvaluatorParseContext::resolveFunctionCall(nsIAtom* aName,
                                                int32_t aID,
                                                FunctionCall** aFn)
{
    return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
}

bool XPathEvaluatorParseContext::caseInsensitiveNameTests()
{
    return !mIsCaseSensitive;
}

void
XPathEvaluatorParseContext::SetErrorOffset(uint32_t aOffset)
{
}

} // namespace dom
} // namespace mozilla
