/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XPathEvaluator.h"

#include <utility>

#include "XPathResult.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/XPathEvaluatorBinding.h"
#include "mozilla/dom/XPathExpression.h"
#include "mozilla/dom/XPathNSResolverBinding.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDOMString.h"
#include "nsError.h"
#include "nsNameSpaceManager.h"
#include "txExpr.h"
#include "txExprParser.h"
#include "txIXPathContext.h"
#include "txURIUtils.h"

namespace mozilla::dom {

// txIParseContext implementation
class XPathEvaluatorParseContext : public txIParseContext {
 public:
  XPathEvaluatorParseContext(XPathNSResolver* aResolver, bool aIsCaseSensitive)
      : mResolver(aResolver),
        mResolverNode(nullptr),
        mLastError(NS_OK),
        mIsCaseSensitive(aIsCaseSensitive) {}
  XPathEvaluatorParseContext(nsINode* aResolver, bool aIsCaseSensitive)
      : mResolver(nullptr),
        mResolverNode(aResolver),
        mLastError(NS_OK),
        mIsCaseSensitive(aIsCaseSensitive) {}

  nsresult getError() { return mLastError; }

  int32_t resolveNamespacePrefix(nsAtom* aPrefix) override;
  nsresult resolveFunctionCall(nsAtom* aName, int32_t aID,
                               FunctionCall** aFunction) override;
  bool caseInsensitiveNameTests() override;
  void SetErrorOffset(uint32_t aOffset) override;

 private:
  XPathNSResolver* mResolver;
  nsINode* mResolverNode;
  nsresult mLastError;
  bool mIsCaseSensitive;
};

XPathEvaluator::XPathEvaluator(Document* aDocument) : mDocument(aDocument) {}

XPathEvaluator::~XPathEvaluator() = default;

UniquePtr<XPathExpression> XPathEvaluator::CreateExpression(
    const nsAString& aExpression, XPathNSResolver* aResolver,
    ErrorResult& aRv) {
  nsCOMPtr<Document> doc(mDocument);
  XPathEvaluatorParseContext pContext(aResolver,
                                      !(doc && doc->IsHTMLDocument()));
  return CreateExpression(aExpression, &pContext, doc, aRv);
}

UniquePtr<XPathExpression> XPathEvaluator::CreateExpression(
    const nsAString& aExpression, nsINode* aResolver, ErrorResult& aRv) {
  nsCOMPtr<Document> doc(mDocument);
  XPathEvaluatorParseContext pContext(aResolver,
                                      !(doc && doc->IsHTMLDocument()));
  return CreateExpression(aExpression, &pContext, doc, aRv);
}

UniquePtr<XPathExpression> XPathEvaluator::CreateExpression(
    const nsAString& aExpression, txIParseContext* aContext,
    Document* aDocument, ErrorResult& aRv) {
  if (!mRecycler) {
    mRecycler = new txResultRecycler;
  }

  UniquePtr<Expr> expression;
  aRv = txExprParser::createExpr(PromiseFlatString(aExpression), aContext,
                                 getter_Transfers(expression));
  if (aRv.Failed()) {
    if (!aRv.ErrorCodeIs(NS_ERROR_DOM_NAMESPACE_ERR)) {
      aRv.SuppressException();
      aRv.ThrowSyntaxError("The expression is not a legal expression");
    }

    return nullptr;
  }

  return MakeUnique<XPathExpression>(std::move(expression), mRecycler,
                                     aDocument);
}

bool XPathEvaluator::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto,
                                JS::MutableHandle<JSObject*> aReflector) {
  return dom::XPathEvaluator_Binding::Wrap(aCx, this, aGivenProto, aReflector);
}

/* static */
UniquePtr<XPathEvaluator> XPathEvaluator::Constructor(
    const GlobalObject& aGlobal) {
  return MakeUnique<XPathEvaluator>(nullptr);
}

already_AddRefed<XPathResult> XPathEvaluator::Evaluate(
    JSContext* aCx, const nsAString& aExpression, nsINode& aContextNode,
    XPathNSResolver* aResolver, uint16_t aType, JS::Handle<JSObject*> aResult,
    ErrorResult& rv) {
  UniquePtr<XPathExpression> expression(
      CreateExpression(aExpression, aResolver, rv));
  if (rv.Failed()) {
    return nullptr;
  }
  return expression->Evaluate(aCx, aContextNode, aType, aResult, rv);
}

/*
 * Implementation of txIParseContext private to XPathEvaluator, based on a
 * XPathNSResolver
 */

int32_t XPathEvaluatorParseContext::resolveNamespacePrefix(nsAtom* aPrefix) {
  if (!mResolver && !mResolverNode) {
    return kNameSpaceID_Unknown;
  }

  nsAutoString prefix;
  if (aPrefix) {
    aPrefix->ToString(prefix);
  }

  nsAutoString ns;
  if (mResolver) {
    ErrorResult rv;
    mResolver->LookupNamespaceURI(prefix, ns, rv);
    if (rv.Failed()) {
      rv.SuppressException();
      return kNameSpaceID_Unknown;
    }
  } else {
    if (aPrefix == nsGkAtoms::xml) {
      ns.AssignLiteral("http://www.w3.org/XML/1998/namespace");
    } else {
      mResolverNode->LookupNamespaceURI(prefix, ns);
    }
  }

  if (DOMStringIsNull(ns)) {
    return kNameSpaceID_Unknown;
  }

  if (ns.IsEmpty()) {
    return kNameSpaceID_None;
  }

  // get the namespaceID for the URI
  int32_t id;
  return NS_SUCCEEDED(
             nsNameSpaceManager::GetInstance()->RegisterNameSpace(ns, id))
             ? id
             : kNameSpaceID_Unknown;
}

nsresult XPathEvaluatorParseContext::resolveFunctionCall(nsAtom* aName,
                                                         int32_t aID,
                                                         FunctionCall** aFn) {
  return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
}

bool XPathEvaluatorParseContext::caseInsensitiveNameTests() {
  return !mIsCaseSensitive;
}

void XPathEvaluatorParseContext::SetErrorOffset(uint32_t aOffset) {}

}  // namespace mozilla::dom
