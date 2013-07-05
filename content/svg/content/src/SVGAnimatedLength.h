/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGAnimatedLength_h
#define mozilla_dom_SVGAnimatedLength_h

#include "mozilla/Attributes.h"
#include "nsSVGElement.h"

class nsSVGLength2;
class nsIDOMSVGLength;

namespace mozilla {
namespace dom {

class SVGAnimatedLength MOZ_FINAL : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(SVGAnimatedLength)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(SVGAnimatedLength)

  SVGAnimatedLength(nsSVGLength2* aVal, nsSVGElement *aSVGElement)
    : mVal(aVal), mSVGElement(aSVGElement)
  { SetIsDOMBinding(); }

  ~SVGAnimatedLength();

  // WebIDL
  nsSVGElement* GetParentObject() { return mSVGElement; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
  already_AddRefed<nsIDOMSVGLength> BaseVal();
  already_AddRefed<nsIDOMSVGLength> AnimVal();

protected:
  nsSVGLength2* mVal; // kept alive because it belongs to content
  nsRefPtr<nsSVGElement> mSVGElement;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGAnimatedLength_h
