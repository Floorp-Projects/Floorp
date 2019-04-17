/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMSVGAnimatedNumber_h
#define mozilla_dom_DOMSVGAnimatedNumber_h

#include "nsISupports.h"
#include "nsWrapperCache.h"

#include "SVGElement.h"

namespace mozilla {
namespace dom {

class DOMSVGAnimatedNumber : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMSVGAnimatedNumber)

  SVGElement* GetParentObject() const { return mSVGElement; }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

  virtual float BaseVal() = 0;
  virtual void SetBaseVal(float aBaseVal) = 0;
  virtual float AnimVal() = 0;

 protected:
  explicit DOMSVGAnimatedNumber(SVGElement* aSVGElement)
      : mSVGElement(aSVGElement) {}
  virtual ~DOMSVGAnimatedNumber() = default;

  RefPtr<SVGElement> mSVGElement;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_DOMSVGAnimatedNumber_h
