/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGAngle.h"
#include "SVGAnimatedOrient.h"
#include "mozilla/dom/SVGAngleBinding.h"
#include "mozilla/dom/SVGSVGElement.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(DOMSVGAngle, mSVGElement)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMSVGAngle, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMSVGAngle, Release)

DOMSVGAngle::DOMSVGAngle(SVGSVGElement* aSVGElement)
    : mSVGElement(aSVGElement), mType(DOMSVGAngle::CreatedValue) {
  mVal = new SVGAnimatedOrient();
  mVal->Init();
}

JSObject* DOMSVGAngle::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return SVGAngle_Binding::Wrap(aCx, this, aGivenProto);
}

uint16_t DOMSVGAngle::UnitType() const {
  if (mType == AnimValue) {
    mSVGElement->FlushAnimations();
    return mVal->mAnimValUnit;
  }
  return mVal->mBaseValUnit;
}

float DOMSVGAngle::Value() const {
  if (mType == AnimValue) {
    mSVGElement->FlushAnimations();
    return mVal->GetAnimValue();
  }
  return mVal->GetBaseValue();
}

void DOMSVGAngle::SetValue(float aValue, ErrorResult& rv) {
  if (mType == AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  bool isBaseVal = mType == BaseValue;
  mVal->SetBaseValue(aValue, mVal->mBaseValUnit,
                     isBaseVal ? mSVGElement.get() : nullptr, isBaseVal);
}

float DOMSVGAngle::ValueInSpecifiedUnits() const {
  if (mType == AnimValue) {
    return mVal->mAnimVal;
  }
  return mVal->mBaseVal;
}

void DOMSVGAngle::SetValueInSpecifiedUnits(float aValue, ErrorResult& rv) {
  if (mType == AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  if (mType == BaseValue) {
    mVal->SetBaseValueInSpecifiedUnits(aValue, mSVGElement);
  } else {
    mVal->mBaseVal = aValue;
  }
}

void DOMSVGAngle::NewValueSpecifiedUnits(uint16_t unitType,
                                         float valueInSpecifiedUnits,
                                         ErrorResult& rv) {
  if (mType == AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  rv = mVal->NewValueSpecifiedUnits(
      unitType, valueInSpecifiedUnits,
      mType == BaseValue ? mSVGElement.get() : nullptr);
}

void DOMSVGAngle::ConvertToSpecifiedUnits(uint16_t unitType, ErrorResult& rv) {
  if (mType == AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  rv = mVal->ConvertToSpecifiedUnits(
      unitType, mType == BaseValue ? mSVGElement.get() : nullptr);
}

void DOMSVGAngle::SetValueAsString(const nsAString& aValue, ErrorResult& rv) {
  if (mType == AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  bool isBaseVal = mType == BaseValue;
  rv = mVal->SetBaseValueString(aValue, isBaseVal ? mSVGElement.get() : nullptr,
                                isBaseVal);
}

void DOMSVGAngle::GetValueAsString(nsAString& aValue) {
  if (mType == AnimValue) {
    mSVGElement->FlushAnimations();
    mVal->GetAnimAngleValueString(aValue);
  } else {
    mVal->GetBaseAngleValueString(aValue);
  }
}
