/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XPathExpression_h
#define mozilla_dom_XPathExpression_h

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIWeakReferenceUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/NonRefcountedDOMObject.h"
#include "mozilla/dom/XPathExpressionBinding.h"

class Expr;
class nsIDocument;
class nsINode;
class txResultRecycler;

namespace mozilla {
namespace dom {

class XPathResult;

/**
 * A class for evaluating an XPath expression string
 */
class XPathExpression final : public NonRefcountedDOMObject
{
public:
    XPathExpression(nsAutoPtr<Expr>&& aExpression, txResultRecycler* aRecycler,
                    nsIDocument *aDocument);
    ~XPathExpression();

    bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector)
    {
        return XPathExpression_Binding::Wrap(aCx, this, aGivenProto, aReflector);
    }

    already_AddRefed<XPathResult>
        Evaluate(JSContext* aCx, nsINode& aContextNode, uint16_t aType,
                 JS::Handle<JSObject*> aInResult, ErrorResult& aRv)
    {
        return EvaluateWithContext(aCx, aContextNode, 1, 1, aType, aInResult,
                                   aRv);
    }
    already_AddRefed<XPathResult>
        EvaluateWithContext(JSContext* aCx, nsINode& aContextNode,
                            uint32_t aContextPosition, uint32_t aContextSize,
                            uint16_t aType, JS::Handle<JSObject*> aInResult,
                            ErrorResult& aRv);
    already_AddRefed<XPathResult>
        Evaluate(nsINode& aContextNode, uint16_t aType, XPathResult* aInResult,
                 ErrorResult& aRv)
    {
        return EvaluateWithContext(aContextNode, 1, 1, aType, aInResult, aRv);
    }
    already_AddRefed<XPathResult>
        EvaluateWithContext(nsINode& aContextNode, uint32_t aContextPosition,
                            uint32_t aContextSize, uint16_t aType,
                            XPathResult* aInResult, ErrorResult& aRv);

private:
    nsAutoPtr<Expr> mExpression;
    RefPtr<txResultRecycler> mRecycler;
    nsWeakPtr mDocument;
    bool mCheckDocument;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_XPathExpression_h */
