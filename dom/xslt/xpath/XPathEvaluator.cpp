/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XPathEvaluator.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsXPathExpression.h"
#include "nsXPathNSResolver.h"
#include "nsXPathResult.h"
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
#include "mozilla/dom/XPathEvaluatorBinding.h"

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

NS_IMPL_ISUPPORTS1(XPathEvaluator, nsIDOMXPathEvaluator)

XPathEvaluator::XPathEvaluator(nsIDocument* aDocument)
    : mDocument(do_GetWeakReference(aDocument))
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
    nsCOMPtr<nsIDOMXPathExpression> expression;
    nsresult rv = CreateExpression(aExpression, aResolver,
                                   getter_AddRefs(expression));
    NS_ENSURE_SUCCESS(rv, rv);

    return expression->Evaluate(aContextNode, aType, aInResult, aResult);
}


NS_IMETHODIMP
XPathEvaluator::CreateExpression(const nsAString & aExpression,
                                 nsIDOMXPathNSResolver *aResolver,
                                 nsIDOMXPathExpression **aResult)
{
    nsresult rv;
    if (!mRecycler) {
        nsRefPtr<txResultRecycler> recycler = new txResultRecycler;
        NS_ENSURE_TRUE(recycler, NS_ERROR_OUT_OF_MEMORY);
        
        rv = recycler->init();
        NS_ENSURE_SUCCESS(rv, rv);
        
        mRecycler = recycler;
    }

    nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
    XPathEvaluatorParseContext pContext(aResolver, !(doc && doc->IsHTML()));

    nsAutoPtr<Expr> expression;
    rv = txExprParser::createExpr(PromiseFlatString(aExpression), &pContext,
                                  getter_Transfers(expression));
    if (NS_FAILED(rv)) {
        if (rv == NS_ERROR_DOM_NAMESPACE_ERR) {
            return NS_ERROR_DOM_NAMESPACE_ERR;
        }

        return NS_ERROR_DOM_INVALID_EXPRESSION_ERR;
    }

    nsCOMPtr<nsIDOMDocument> document = do_QueryReferent(mDocument);

    *aResult = new nsXPathExpression(expression, mRecycler, document);
    if (!*aResult) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(*aResult);
    return NS_OK;
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

already_AddRefed<nsIDOMXPathExpression>
XPathEvaluator::CreateExpression(const nsAString& aExpression,
                                 nsIDOMXPathNSResolver* aResolver,
                                 ErrorResult& rv)
{
  nsCOMPtr<nsIDOMXPathExpression> expr;
  rv = CreateExpression(aExpression, aResolver, getter_AddRefs(expr));
  return expr.forget();
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

already_AddRefed<nsISupports>
XPathEvaluator::Evaluate(const nsAString& aExpression, nsINode* aContextNode,
                         nsIDOMXPathNSResolver* aResolver, uint16_t aType,
                         nsISupports* aResult, ErrorResult& rv)
{
  nsCOMPtr<nsIDOMNode> contextNode = do_QueryInterface(aContextNode);
  nsCOMPtr<nsISupports> res;
  rv = Evaluate(aExpression, contextNode, aResolver, aType,
                aResult, getter_AddRefs(res));
  return res.forget();
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
