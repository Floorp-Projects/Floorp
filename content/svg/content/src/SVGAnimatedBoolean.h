/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsIDOMSVGAnimatedBoolean.h"
#include "nsWrapperCache.h"
#include "nsSVGElement.h"
#include "mozilla/Attributes.h"
#include "nsSVGBoolean.h"

namespace mozilla {
namespace dom {

class SVGAnimatedBoolean MOZ_FINAL : public nsIDOMSVGAnimatedBoolean,
                                     public nsWrapperCache
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(SVGAnimatedBoolean)

  SVGAnimatedBoolean(nsSVGBoolean* aVal, nsSVGElement *aSVGElement)
    : mVal(aVal), mSVGElement(aSVGElement)
  {
    SetIsDOMBinding();
  }
  ~SVGAnimatedBoolean();

  NS_IMETHOD GetBaseVal(bool* aResult)
    { *aResult = BaseVal(); return NS_OK; }
  NS_IMETHOD SetBaseVal(bool aValue)
    { mVal->SetBaseValue(aValue, mSVGElement); return NS_OK; }

  // Script may have modified animation parameters or timeline -- DOM getters
  // need to flush any resample requests to reflect these modifications.
  NS_IMETHOD GetAnimVal(bool* aResult)
  {
    *aResult = AnimVal();
    return NS_OK;
  }

  // WebIDL
  nsSVGElement* GetParentObject() const { return mSVGElement; }
  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope, bool* aTriedToWrap);
  bool BaseVal() const { return mVal->GetBaseValue(); }
  bool AnimVal() const { mSVGElement->FlushAnimations(); return mVal->GetAnimValue(); }

protected:
  nsSVGBoolean* mVal; // kept alive because it belongs to content
  nsRefPtr<nsSVGElement> mSVGElement;
};

} //namespace dom
} //namespace mozilla
