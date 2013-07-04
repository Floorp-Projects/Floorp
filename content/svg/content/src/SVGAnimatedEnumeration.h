/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGAnimatedEnumeration_h
#define mozilla_dom_SVGAnimatedEnumeration_h

#include "nsIDOMSVGAnimatedEnum.h"
#include "nsWrapperCache.h"

#include "nsSVGElement.h"

namespace mozilla {
namespace dom {

class SVGAnimatedEnumeration : public nsIDOMSVGAnimatedEnumeration
                             , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(SVGAnimatedEnumeration)

  nsSVGElement* GetParentObject() const
  {
    return mSVGElement;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
    MOZ_OVERRIDE MOZ_FINAL;

  virtual uint16_t BaseVal() = 0;
  virtual void SetBaseVal(uint16_t aBaseVal, ErrorResult& aRv) = 0;
  virtual uint16_t AnimVal() = 0;

  NS_IMETHOD GetBaseVal(uint16_t* aResult) MOZ_OVERRIDE MOZ_FINAL
  {
    *aResult = BaseVal();
    return NS_OK;
  }
  NS_IMETHOD SetBaseVal(uint16_t aValue) MOZ_OVERRIDE MOZ_FINAL
  {
    ErrorResult rv;
    SetBaseVal(aValue, rv);
    return rv.ErrorCode();
  }
  NS_IMETHOD GetAnimVal(uint16_t* aResult) MOZ_OVERRIDE MOZ_FINAL
  {
    *aResult = AnimVal();
    return NS_OK;
  }

protected:
  explicit SVGAnimatedEnumeration(nsSVGElement* aSVGElement)
    : mSVGElement(aSVGElement)
  {
    SetIsDOMBinding();
  }
  virtual ~SVGAnimatedEnumeration() {};

  nsRefPtr<nsSVGElement> mSVGElement;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGAnimatedEnumeration_h
