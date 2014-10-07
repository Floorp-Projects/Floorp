/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGAnimatedString_h
#define mozilla_dom_SVGAnimatedString_h

#include "nsSVGElement.h"

namespace mozilla {
namespace dom {

class SVGAnimatedString : public nsISupports,
                          public nsWrapperCache
{
public:
  explicit SVGAnimatedString(nsSVGElement* aSVGElement)
    : mSVGElement(aSVGElement)
  {
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL
  nsSVGElement* GetParentObject() const
  {
    return mSVGElement;
  }

  virtual void GetBaseVal(nsAString& aResult) = 0;
  virtual void SetBaseVal(const nsAString& aValue) = 0;
  virtual void GetAnimVal(nsAString& aResult) = 0;

  nsRefPtr<nsSVGElement> mSVGElement;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGAnimatedString_h
