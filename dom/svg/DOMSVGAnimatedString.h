/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMSVGAnimatedString_h
#define mozilla_dom_DOMSVGAnimatedString_h

#include "SVGElement.h"

namespace mozilla {
namespace dom {

class DOMSVGAnimatedString : public nsISupports, public nsWrapperCache {
 public:
  explicit DOMSVGAnimatedString(SVGElement* aSVGElement)
      : mSVGElement(aSVGElement) {}

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  SVGElement* GetParentObject() const { return mSVGElement; }

  virtual void GetBaseVal(nsAString& aResult) = 0;
  virtual void SetBaseVal(const nsAString& aValue) = 0;
  virtual void GetAnimVal(nsAString& aResult) = 0;

  RefPtr<SVGElement> mSVGElement;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_DOMSVGAnimatedString_h
