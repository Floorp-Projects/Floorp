/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMSVGAnimatedLength_h
#define mozilla_dom_DOMSVGAnimatedLength_h

#include "mozilla/Attributes.h"
#include "SVGElement.h"

namespace mozilla {

class SVGAnimatedLength;

namespace dom {

class DOMSVGLength;

class DOMSVGAnimatedLength final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DOMSVGAnimatedLength)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DOMSVGAnimatedLength)

  DOMSVGAnimatedLength(SVGAnimatedLength* aVal, SVGElement* aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

  // WebIDL
  SVGElement* GetParentObject() { return mSVGElement; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  already_AddRefed<DOMSVGLength> BaseVal();
  already_AddRefed<DOMSVGLength> AnimVal();

 protected:
  ~DOMSVGAnimatedLength();

  SVGAnimatedLength* mVal;  // kept alive because it belongs to content
  RefPtr<SVGElement> mSVGElement;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_DOMSVGAnimatedLength_h
