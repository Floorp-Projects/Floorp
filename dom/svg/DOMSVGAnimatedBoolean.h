/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMSVGAnimatedBoolean_h
#define mozilla_dom_DOMSVGAnimatedBoolean_h

#include "SVGAnimatedBoolean.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGElement.h"

namespace mozilla {
namespace dom {

class DOMSVGAnimatedBoolean final : public nsWrapperCache {
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DOMSVGAnimatedBoolean)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DOMSVGAnimatedBoolean)

  DOMSVGAnimatedBoolean(SVGAnimatedBoolean* aVal, SVGElement* aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

  // WebIDL
  SVGElement* GetParentObject() const { return mSVGElement; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  bool BaseVal() const { return mVal->GetBaseValue(); }
  void SetBaseVal(bool aValue) { mVal->SetBaseValue(aValue, mSVGElement); }
  bool AnimVal() const {
    mSVGElement->FlushAnimations();
    return mVal->GetAnimValue();
  }

 protected:
  ~DOMSVGAnimatedBoolean();

  SVGAnimatedBoolean* mVal;  // kept alive because it belongs to content
  RefPtr<SVGElement> mSVGElement;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_DOMSVGAnimatedBoolean_h
