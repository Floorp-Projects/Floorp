/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XPathEvaluator.h"

#include <utility>

#include "XPathResult.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/XPathEvaluatorBinding.h"
#include "mozilla/dom/XPathExpression.h"
#include "mozilla/dom/XPathNSResolverBinding.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsDOMString.h"
#include "nsError.h"
#include "nsNameSpaceManager.h"
#include "txExpr.h"
#include "txExprParser.h"
#include "txIXPathContext.h"
#include "txURIUtils.h"

namespace mozilla {
namespace dom {

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

  nsresult resolveNamespacePrefix(nsAtom* aPrefix, int32_t& aID) override;
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

XPathEvaluator::XPathEvaluator(Document* aDocument)
    : mDocument(do_GetWeakReference(aDocument)) {}

XPathEvaluator::~XPathEvaluator() = default;

XPathExpression* XPathEvaluator::CreateExpression(const nsAString& aExpression,
                                                  XPathNSResolver* aResolver,
                                                  ErrorResult& aRv) {
  nsCOMPtr<Document> doc = do_QueryReferent(mDocument);
  XPathEvaluatorParseContext pContext(aResolver,
                                      !(doc && doc->IsHTMLDocument()));
  return CreateExpression(aExpression, &pContext, doc, aRv);
}

XPathExpression* XPathEvaluator::CreateExpression(const nsAString& aExpression,
                                                  nsINode* aResolver,
                                                  ErrorResult& aRv) {
  nsCOMPtr<Document> doc = do_QueryReferent(mDocument);
  XPathEvaluatorParseContext pContext(aResolver,
                                      !(doc && doc->IsHTMLDocument()));
  return CreateExpression(aExpression, &pContext, doc, aRv);
}

XPathExpression* XPathEvaluator::CreateExpression(const nsAString& aExpression,
                                                  txIParseContext* aContext,
                                                  Document* aDocument,
                                                  ErrorResult& aRv) {
  if (!mRecycler) {
    mRecycler = new txResultRecycler;
  }

  UniquePtr<Expr> expression;
  aRv = txExprParser::createExpr(PromiseFlatString(aExpression), aContext,
                                 getter_Transfers(expression));
  if (aRv.Failed()) {
    if (!aRv.ErrorCodeIs(NS_ERROR_DOM_NAMESPACE_ERR)) {
      aRv.SuppressException();
      aRv.Throw(NS_ERROR_DOM_INVALID_EXPRESSION_ERR);
    }

    return nullptr;
  }

  return new XPathExpression(std::move(expression), mRecycler, aDocument);
}

bool XPathEvaluator::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto,
                                JS::MutableHandle<JSObject*> aReflector) {
  return dom::XPathEvaluator_Binding::Wrap(aCx, this, aGivenProto, aReflector);
}

/* static */
XPathEvaluator* XPathEvaluator::Constructor(const GlobalObject& aGlobal) {
  return new XPathEvaluator(nullptr);
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

nsresult XPathEvaluatorParseContext::resolveNamespacePrefix(nsAtom* aPrefix,
                                                            int32_t& aID) {
  aID = kNameSpaceID_Unknown;

  if (!mResolver && !mResolverNode) {
    return NS_ERROR_DOM_NAMESPACE_ERR;
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
      return rv.StealNSResult();
    }
  } else {
    if (aPrefix == nsGkAtoms::xml) {
      ns.AssignLiteral("http://www.w3.org/XML/1998/namespace");
    } else {
      mResolverNode->LookupNamespaceURI(prefix, ns);
    }
  }

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

nsresult XPathEvaluatorParseContext::resolveFunctionCall(nsAtom* aName,
                                                         int32_t aID,
                                                         FunctionCall** aFn) {
  return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
}

bool XPathEvaluatorParseContext::caseInsensitiveNameTests() {
  return !mIsCaseSensitive;
}

void XPathEvaluatorParseContext::SetErrorOffset(uint32_t aOffset) {}

}  // namespace dom
}  // namespace mozilla
