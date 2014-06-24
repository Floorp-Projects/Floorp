/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XPathEvaluator_h
#define mozilla_dom_XPathEvaluator_h

#include "nsIDOMXPathEvaluator.h"
#include "nsIWeakReference.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsIDocument.h"

class nsINode;
class txResultRecycler;

namespace mozilla {
namespace dom {

class GlobalObject;
class XPathResult;

/**
 * A class for evaluating an XPath expression string
 */
class XPathEvaluator MOZ_FINAL : public nsIDOMXPathEvaluator
{
    ~XPathEvaluator();

public:
    XPathEvaluator(nsIDocument* aDocument = nullptr);

    NS_DECL_ISUPPORTS

    // nsIDOMXPathEvaluator interface
    NS_DECL_NSIDOMXPATHEVALUATOR

    // WebIDL API
    JSObject* WrapObject(JSContext* aCx);
    nsIDocument* GetParentObject()
    {
        nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
        return doc;
    }
    static already_AddRefed<XPathEvaluator>
        Constructor(const GlobalObject& aGlobal, ErrorResult& rv);
    already_AddRefed<nsIDOMXPathExpression>
        CreateExpression(const nsAString& aExpression,
                         nsIDOMXPathNSResolver* aResolver,
                         ErrorResult& rv);
    already_AddRefed<nsIDOMXPathNSResolver>
        CreateNSResolver(nsINode* aNodeResolver, ErrorResult& rv);
    already_AddRefed<XPathResult>
        Evaluate(JSContext* aCx, const nsAString& aExpression,
                 nsINode* aContextNode, nsIDOMXPathNSResolver* aResolver,
                 uint16_t aType, JS::Handle<JSObject*> aResult,
                 ErrorResult& rv);
private:
    nsWeakPtr mDocument;
    nsRefPtr<txResultRecycler> mRecycler;
};

inline nsISupports*
ToSupports(XPathEvaluator* e)
{
    return static_cast<nsIDOMXPathEvaluator*>(e);
}

/* d0a75e02-b5e7-11d5-a7f2-df109fb8a1fc */
#define TRANSFORMIIX_XPATH_EVALUATOR_CID   \
{ 0xd0a75e02, 0xb5e7, 0x11d5, { 0xa7, 0xf2, 0xdf, 0x10, 0x9f, 0xb8, 0xa1, 0xfc } }

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_XPathEvaluator_h */
