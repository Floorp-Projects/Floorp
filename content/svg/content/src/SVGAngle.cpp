/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAngle.h"
#include "nsSVGAngle.h"
#include "mozilla/dom/SVGAngleBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(SVGAngle, mSVGElement)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(SVGAngle, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(SVGAngle, Release)

JSObject*
SVGAngle::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGAngleBinding::Wrap(aCx, aScope, this);
}

uint16_t
SVGAngle::UnitType() const
{
  if (mType == AnimValue) {
    return mVal->mAnimValUnit;
  }
  return mVal->mBaseValUnit;
}

float
SVGAngle::Value() const
{
  if (mType == AnimValue) {
    return mVal->GetAnimValue();
  }
  return mVal->GetBaseValue();
}

void
SVGAngle::SetValue(float aValue, ErrorResult& rv)
{
  if (mType == AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  bool isBaseVal = mType == BaseValue;
  mVal->SetBaseValue(aValue, isBaseVal ? mSVGElement : nullptr, isBaseVal);
}

float
SVGAngle::ValueInSpecifiedUnits() const
{
  if (mType == AnimValue) {
    return mVal->mAnimVal;
  }
  return mVal->mBaseVal;
}

void
SVGAngle::SetValueInSpecifiedUnits(float aValue, ErrorResult& rv)
{
  if (mType == AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  } else if (mType == BaseValue) {
    mVal->SetBaseValueInSpecifiedUnits(aValue, mSVGElement);
  } else {
    mVal->mBaseVal = aValue;
  }
}

void
SVGAngle::NewValueSpecifiedUnits(uint16_t unitType,
                                 float valueInSpecifiedUnits,
                                 ErrorResult& rv)
{
  if (mType == AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  rv = mVal->NewValueSpecifiedUnits(unitType, valueInSpecifiedUnits,
                                    mType == BaseValue ? mSVGElement : nullptr);
}

void
SVGAngle::ConvertToSpecifiedUnits(uint16_t unitType, ErrorResult& rv)
{
  if (mType == AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  rv = mVal->ConvertToSpecifiedUnits(unitType, mType == BaseValue ? mSVGElement : nullptr);
}

void
SVGAngle::SetValueAsString(const nsAString& aValue, ErrorResult& rv)
{
  if (mType == AnimValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  bool isBaseVal = mType == BaseValue;
  rv = mVal->SetBaseValueString(aValue, isBaseVal ? mSVGElement : nullptr, isBaseVal);
}

void
SVGAngle::GetValueAsString(nsAString& aValue)
{
  if (mType == AnimValue) {
    mVal->GetAnimValueString(aValue);
  } else {
    mVal->GetBaseValueString(aValue);
  }
}

