/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXPathEvaluator.h"
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
#include "nsINameSpaceManager.h"
#include "nsContentUtils.h"
#include "mozilla/dom/XPathEvaluatorBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

// txIParseContext implementation
class nsXPathEvaluatorParseContext : public txIParseContext
{
public:
    nsXPathEvaluatorParseContext(nsIDOMXPathNSResolver* aResolver,
                                 nsTArray<int32_t> *aNamespaceIDs,
                                 nsTArray<nsCString> *aContractIDs,
                                 nsCOMArray<nsISupports> *aState,
                                 bool aIsCaseSensitive)
        : mResolver(aResolver),
          mNamespaceIDs(aNamespaceIDs),
          mContractIDs(aContractIDs),
          mState(aState),
          mLastError(NS_OK),
          mIsCaseSensitive(aIsCaseSensitive)
    {
        NS_ASSERTION(mContractIDs ||
                     (!mNamespaceIDs || mNamespaceIDs->Length() == 0),
                     "Need contract IDs if there are namespaces.");
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
    nsTArray<int32_t> *mNamespaceIDs;
    nsTArray<nsCString> *mContractIDs;
    nsCOMArray<nsISupports> *mState;
    nsresult mLastError;
    bool mIsCaseSensitive;
};

NS_IMPL_AGGREGATED(nsXPathEvaluator)
NS_INTERFACE_MAP_BEGIN_AGGREGATED(nsXPathEvaluator)
    NS_INTERFACE_MAP_ENTRY(nsIDOMXPathEvaluator)
    NS_INTERFACE_MAP_ENTRY(nsIXPathEvaluatorInternal)
NS_INTERFACE_MAP_END

nsXPathEvaluator::nsXPathEvaluator(nsISupports *aOuter)
{
    NS_INIT_AGGREGATED(aOuter);
}

nsresult
nsXPathEvaluator::Init()
{
    nsCOMPtr<nsIDOMDocument> document = do_QueryInterface(fOuter);

    return document ? SetDocument(document) : NS_OK;
}

NS_IMETHODIMP
nsXPathEvaluator::CreateExpression(const nsAString & aExpression,
                                   nsIDOMXPathNSResolver *aResolver,
                                   nsIDOMXPathExpression **aResult)
{
    return CreateExpression(aExpression, aResolver, (nsTArray<int32_t>*)nullptr,
                            nullptr, nullptr, aResult);
}

NS_IMETHODIMP
nsXPathEvaluator::CreateNSResolver(nsIDOMNode *aNodeResolver,
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
nsXPathEvaluator::Evaluate(const nsAString & aExpression,
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
nsXPathEvaluator::SetDocument(nsIDOMDocument* aDocument)
{
    mDocument = do_GetWeakReference(aDocument);
    return NS_OK;
}

NS_IMETHODIMP
nsXPathEvaluator::CreateExpression(const nsAString & aExpression,
                                   nsIDOMXPathNSResolver *aResolver,
                                   nsTArray<nsString> *aNamespaceURIs,
                                   nsTArray<nsCString> *aContractIDs,
                                   nsCOMArray<nsISupports> *aState,
                                   nsIDOMXPathExpression **aResult)
{
    nsTArray<int32_t> namespaceIDs;
    if (aNamespaceURIs) {
        uint32_t count = aNamespaceURIs->Length();

        if (!aContractIDs || aContractIDs->Length() != count) {
            return NS_ERROR_FAILURE;
        }

        if (!namespaceIDs.SetLength(count)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        uint32_t i;
        for (i = 0; i < count; ++i) {
            if (aContractIDs->ElementAt(i).IsEmpty()) {
                return NS_ERROR_FAILURE;
            }

            nsContentUtils::NameSpaceManager()->RegisterNameSpace(aNamespaceURIs->ElementAt(i), namespaceIDs[i]);
        }
    }


    return CreateExpression(aExpression, aResolver, &namespaceIDs, aContractIDs,
                            aState, aResult);
}

nsresult
nsXPathEvaluator::CreateExpression(const nsAString & aExpression,
                                   nsIDOMXPathNSResolver *aResolver,
                                   nsTArray<int32_t> *aNamespaceIDs,
                                   nsTArray<nsCString> *aContractIDs,
                                   nsCOMArray<nsISupports> *aState,
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
    nsXPathEvaluatorParseContext pContext(aResolver, aNamespaceIDs,
                                          aContractIDs, aState,
                                          !(doc && doc->IsHTML()));

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
nsXPathEvaluator::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
    return dom::XPathEvaluatorBinding::Wrap(aCx, aScope, this);
}

/* static */
already_AddRefed<nsXPathEvaluator>
nsXPathEvaluator::Constructor(const GlobalObject& aGlobal, ErrorResult& rv)
{
    nsRefPtr<nsXPathEvaluator> newObj = new nsXPathEvaluator(nullptr);
    newObj->Init();
    return newObj.forget();
}

already_AddRefed<nsIDOMXPathExpression>
nsXPathEvaluator::CreateExpression(const nsAString& aExpression,
                                   nsIDOMXPathNSResolver* aResolver,
                                   ErrorResult& rv)
{
  nsCOMPtr<nsIDOMXPathExpression> expr;
  rv = CreateExpression(aExpression, aResolver, getter_AddRefs(expr));
  return expr.forget();
}

already_AddRefed<nsIDOMXPathNSResolver>
nsXPathEvaluator::CreateNSResolver(nsINode* aNodeResolver,
                                   ErrorResult& rv)
{
  nsCOMPtr<nsIDOMNode> nodeResolver = do_QueryInterface(aNodeResolver);
  nsCOMPtr<nsIDOMXPathNSResolver> res;
  rv = CreateNSResolver(nodeResolver, getter_AddRefs(res));
  return res.forget();
}

already_AddRefed<nsISupports>
nsXPathEvaluator::Evaluate(const nsAString& aExpression, nsINode* aContextNode,
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
 * Implementation of txIParseContext private to nsXPathEvaluator, based on a
 * nsIDOMXPathNSResolver
 */

nsresult nsXPathEvaluatorParseContext::resolveNamespacePrefix
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

extern nsresult
TX_ResolveFunctionCallXPCOM(const nsCString &aContractID, int32_t aNamespaceID,
                            nsIAtom *aName, nsISupports *aState,
                            FunctionCall **aFunction);

nsresult
nsXPathEvaluatorParseContext::resolveFunctionCall(nsIAtom* aName,
                                                  int32_t aID,
                                                  FunctionCall** aFn)
{
    nsresult rv = NS_ERROR_XPATH_UNKNOWN_FUNCTION;

    uint32_t i, count = mNamespaceIDs ? mNamespaceIDs->Length() : 0;
    for (i = 0; i < count; ++i) {
        if (mNamespaceIDs->ElementAt(i) == aID) {
            nsISupports *state = mState ? mState->SafeObjectAt(i) : nullptr;
            rv = TX_ResolveFunctionCallXPCOM(mContractIDs->ElementAt(i), aID,
                                             aName, state, aFn);
            if (NS_SUCCEEDED(rv)) {
                break;
            }
        }
    }

    return rv;
}

bool nsXPathEvaluatorParseContext::caseInsensitiveNameTests()
{
    return !mIsCaseSensitive;
}

void
nsXPathEvaluatorParseContext::SetErrorOffset(uint32_t aOffset)
{
}
