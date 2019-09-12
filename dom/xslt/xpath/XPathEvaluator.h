/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XPathEvaluator_h
#define mozilla_dom_XPathEvaluator_h

#include "mozilla/dom/NonRefcountedDOMObject.h"
#include "nsIWeakReference.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Document.h"

class nsINode;
class txIParseContext;
class txResultRecycler;

namespace mozilla {
namespace dom {

class GlobalObject;
class XPathExpression;
class XPathNSResolver;
class XPathResult;

/**
 * A class for evaluating an XPath expression string
 */
class XPathEvaluator final : public NonRefcountedDOMObject {
 public:
  explicit XPathEvaluator(Document* aDocument = nullptr);
  ~XPathEvaluator();

  // WebIDL API
  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector);
  Document* GetParentObject() {
    nsCOMPtr<Document> doc = do_QueryReferent(mDocument);
    return doc;
  }
  static XPathEvaluator* Constructor(const GlobalObject& aGlobal);
  XPathExpression* CreateExpression(const nsAString& aExpression,
                                    XPathNSResolver* aResolver,
                                    ErrorResult& rv);
  XPathExpression* CreateExpression(const nsAString& aExpression,
                                    nsINode* aResolver, ErrorResult& aRv);
  XPathExpression* CreateExpression(const nsAString& aExpression,
                                    txIParseContext* aContext,
                                    Document* aDocument, ErrorResult& aRv);
  nsINode* CreateNSResolver(nsINode& aNodeResolver) { return &aNodeResolver; }
  already_AddRefed<XPathResult> Evaluate(
      JSContext* aCx, const nsAString& aExpression, nsINode& aContextNode,
      XPathNSResolver* aResolver, uint16_t aType, JS::Handle<JSObject*> aResult,
      ErrorResult& rv);

 private:
  nsWeakPtr mDocument;
  RefPtr<txResultRecycler> mRecycler;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_XPathEvaluator_h */
