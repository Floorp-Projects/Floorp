/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGAngle.h"
#include "SVGAnimatedOrient.h"
#include "mozilla/dom/SVGAngleBinding.h"
#include "mozilla/dom/SVGSVGElement.h"

namespace mozilla::dom {

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(DOMSVGAngle, mSVGElement)

DOMSVGAngle::DOMSVGAngle(SVGSVGElement* aSVGElement)
    : mSVGElement(aSVGElement), mType(AngleType::CreatedValue) {
  mVal = new SVGAnimatedOrient();
  mVal->Init();
}

JSObject* DOMSVGAngle::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return SVGAngle_Binding::Wrap(aCx, this, aGivenProto);
}

uint16_t DOMSVGAngle::UnitType() const {
  uint16_t unitType;
  if (mType == AngleType::AnimValue) {
    mSVGElement->FlushAnimations();
    unitType = mVal->mAnimValUnit;
  } else {
    unitType = mVal->mBaseValUnit;
  }
  return SVGAnimatedOrient::IsValidUnitType(unitType)
             ? unitType
             : SVGAngle_Binding::SVG_ANGLETYPE_UNKNOWN;
}

float DOMSVGAngle::Value() const {
  if (mType == AngleType::AnimValue) {
    mSVGElement->FlushAnimations();
    return mVal->GetAnimValue();
  }
  return mVal->GetBaseValue();
}

void DOMSVGAngle::SetValue(float aValue, ErrorResult& rv) {
  if (mType == AngleType::AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  bool isBaseVal = mType == AngleType::BaseValue;
  mVal->SetBaseValue(aValue, mVal->mBaseValUnit,
                     isBaseVal ? mSVGElement.get() : nullptr, isBaseVal);
}

float DOMSVGAngle::ValueInSpecifiedUnits() const {
  if (mType == AngleType::AnimValue) {
    return mVal->mAnimVal;
  }
  return mVal->mBaseVal;
}

void DOMSVGAngle::SetValueInSpecifiedUnits(float aValue, ErrorResult& rv) {
  if (mType == AngleType::AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  if (mType == AngleType::BaseValue) {
    mVal->SetBaseValueInSpecifiedUnits(aValue, mSVGElement);
  } else {
    mVal->mBaseVal = aValue;
  }
}

void DOMSVGAngle::NewValueSpecifiedUnits(uint16_t unitType,
                                         float valueInSpecifiedUnits,
                                         ErrorResult& rv) {
  if (mType == AngleType::AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  rv = mVal->NewValueSpecifiedUnits(
      unitType, valueInSpecifiedUnits,
      mType == AngleType::BaseValue ? mSVGElement.get() : nullptr);
}

void DOMSVGAngle::ConvertToSpecifiedUnits(uint16_t unitType, ErrorResult& rv) {
  if (mType == AngleType::AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  rv = mVal->ConvertToSpecifiedUnits(
      unitType, mType == AngleType::BaseValue ? mSVGElement.get() : nullptr);
}

void DOMSVGAngle::SetValueAsString(const nsAString& aValue, ErrorResult& rv) {
  if (mType == AngleType::AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  bool isBaseVal = mType == AngleType::BaseValue;
  rv = mVal->SetBaseValueString(aValue, isBaseVal ? mSVGElement.get() : nullptr,
                                isBaseVal);
}

void DOMSVGAngle::GetValueAsString(nsAString& aValue) {
  if (mType == AngleType::AnimValue) {
    mSVGElement->FlushAnimations();
    mVal->GetAnimAngleValueString(aValue);
  } else {
    mVal->GetBaseAngleValueString(aValue);
  }
}

}  // namespace mozilla::dom
