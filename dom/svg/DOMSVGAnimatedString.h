/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_DOMSVGANIMATEDSTRING_H_
#define DOM_SVG_DOMSVGANIMATEDSTRING_H_

#include "mozilla/SVGAnimatedClassOrString.h"
#include "mozilla/dom/SVGElement.h"

namespace mozilla::dom {

class DOMSVGAnimatedString final : public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DOMSVGAnimatedString)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DOMSVGAnimatedString)

  DOMSVGAnimatedString(SVGAnimatedClassOrString* aVal, SVGElement* aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  SVGElement* GetParentObject() const { return mSVGElement; }

  void GetBaseVal(nsAString& aResult) {
    mVal->GetBaseValue(aResult, mSVGElement);
  }
  void SetBaseVal(const nsAString& aValue) {
    mVal->SetBaseValue(aValue, mSVGElement, true);
  }
  void GetAnimVal(nsAString& aResult) {
    mSVGElement->FlushAnimations();
    mVal->GetAnimValue(aResult, mSVGElement);
  }

 private:
  ~DOMSVGAnimatedString() { mVal->RemoveTearoff(); }

  SVGAnimatedClassOrString* mVal;  // kept alive because it belongs to content
  RefPtr<SVGElement> mSVGElement;
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_DOMSVGANIMATEDSTRING_H_
