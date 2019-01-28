/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAngle.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Move.h"
#include "mozilla/SMILValue.h"
#include "mozilla/dom/DOMSVGAngle.h"
#include "mozilla/dom/SVGAnimatedAngle.h"
#include "mozilla/dom/SVGMarkerElement.h"
#include "nsContentUtils.h"
#include "SVGAttrTearoffTable.h"
#include "SVGOrientSMILType.h"
#include "nsTextFormatter.h"

using namespace mozilla::dom;
using namespace mozilla::dom::SVGAngle_Binding;
using namespace mozilla::dom::SVGMarkerElement_Binding;

namespace mozilla {

static const nsStaticAtom* const angleUnitMap[] = {
    nullptr, /* SVG_ANGLETYPE_UNKNOWN */
    nullptr, /* SVG_ANGLETYPE_UNSPECIFIED */
    nsGkAtoms::deg, nsGkAtoms::rad, nsGkAtoms::grad};

static SVGAttrTearoffTable<SVGAngle, SVGAnimatedAngle>
    sSVGAnimatedAngleTearoffTable;
static SVGAttrTearoffTable<SVGAngle, DOMSVGAngle> sBaseSVGAngleTearoffTable;
static SVGAttrTearoffTable<SVGAngle, DOMSVGAngle> sAnimSVGAngleTearoffTable;

/* Helper functions */

static bool IsValidAngleUnitType(uint16_t unit) {
  if (unit > SVG_ANGLETYPE_UNKNOWN && unit <= SVG_ANGLETYPE_GRAD) return true;

  return false;
}

static void GetAngleUnitString(nsAString& unit, uint16_t unitType) {
  if (IsValidAngleUnitType(unitType)) {
    if (angleUnitMap[unitType]) {
      angleUnitMap[unitType]->ToString(unit);
    }
    return;
  }

  MOZ_ASSERT_UNREACHABLE("Unknown unit type");
}

static uint16_t GetAngleUnitTypeForString(const nsAString& unitStr) {
  if (unitStr.IsEmpty()) return SVG_ANGLETYPE_UNSPECIFIED;

  nsStaticAtom* unitAtom = NS_GetStaticAtom(unitStr);

  if (unitAtom) {
    for (uint32_t i = 0; i < ArrayLength(angleUnitMap); i++) {
      if (angleUnitMap[i] == unitAtom) {
        return i;
      }
    }
  }

  return SVG_ANGLETYPE_UNKNOWN;
}

static void GetAngleValueString(nsAString& aValueAsString, float aValue,
                                uint16_t aUnitType) {
  nsTextFormatter::ssprintf(aValueAsString, u"%g", (double)aValue);

  nsAutoString unitString;
  GetAngleUnitString(unitString, aUnitType);
  aValueAsString.Append(unitString);
}

/* static */ bool SVGAngle::GetValueFromString(const nsAString& aString,
                                               float& aValue,
                                               uint16_t* aUnitType) {
  RangedPtr<const char16_t> iter = SVGContentUtils::GetStartRangedPtr(aString);
  const RangedPtr<const char16_t> end =
      SVGContentUtils::GetEndRangedPtr(aString);

  if (!SVGContentUtils::ParseNumber(iter, end, aValue)) {
    return false;
  }

  const nsAString& units = Substring(iter.get(), end.get());
  *aUnitType = GetAngleUnitTypeForString(units);
  return IsValidAngleUnitType(*aUnitType);
}

/* static */ float SVGAngle::GetDegreesPerUnit(uint8_t aUnit) {
  switch (aUnit) {
    case SVG_ANGLETYPE_UNSPECIFIED:
    case SVG_ANGLETYPE_DEG:
      return 1;
    case SVG_ANGLETYPE_RAD:
      return static_cast<float>(180.0 / M_PI);
    case SVG_ANGLETYPE_GRAD:
      return 90.0f / 100.0f;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown unit type");
      return 0;
  }
}

void SVGAngle::SetBaseValueInSpecifiedUnits(float aValue,
                                            SVGElement* aSVGElement) {
  if (mBaseVal == aValue) {
    return;
  }

  nsAttrValue emptyOrOldValue = aSVGElement->WillChangeAngle(mAttrEnum);
  mBaseVal = aValue;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  } else {
    aSVGElement->AnimationNeedsResample();
  }
  aSVGElement->DidChangeAngle(mAttrEnum, emptyOrOldValue);
}

nsresult SVGAngle::ConvertToSpecifiedUnits(uint16_t unitType,
                                           SVGElement* aSVGElement) {
  if (!IsValidAngleUnitType(unitType)) return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  if (mBaseValUnit == uint8_t(unitType)) return NS_OK;

  nsAttrValue emptyOrOldValue;
  if (aSVGElement) {
    emptyOrOldValue = aSVGElement->WillChangeAngle(mAttrEnum);
  }

  float valueInUserUnits = mBaseVal * GetDegreesPerUnit(mBaseValUnit);
  // Setting aDoSetAttr to false here will ensure we don't call
  // Will/DidChangeAngle a second time (and dispatch duplicate notifications).
  SetBaseValue(valueInUserUnits, unitType, aSVGElement, false);

  if (aSVGElement) {
    aSVGElement->DidChangeAngle(mAttrEnum, emptyOrOldValue);
  }

  return NS_OK;
}

nsresult SVGAngle::NewValueSpecifiedUnits(uint16_t unitType,
                                          float valueInSpecifiedUnits,
                                          SVGElement* aSVGElement) {
  NS_ENSURE_FINITE(valueInSpecifiedUnits, NS_ERROR_ILLEGAL_VALUE);

  if (!IsValidAngleUnitType(unitType)) return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  if (mBaseVal == valueInSpecifiedUnits && mBaseValUnit == uint8_t(unitType))
    return NS_OK;

  nsAttrValue emptyOrOldValue;
  if (aSVGElement) {
    emptyOrOldValue = aSVGElement->WillChangeAngle(mAttrEnum);
  }
  mBaseVal = valueInSpecifiedUnits;
  mBaseValUnit = uint8_t(unitType);
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
    mAnimValUnit = mBaseValUnit;
  } else {
    aSVGElement->AnimationNeedsResample();
  }
  if (aSVGElement) {
    aSVGElement->DidChangeAngle(mAttrEnum, emptyOrOldValue);
  }
  return NS_OK;
}

already_AddRefed<DOMSVGAngle> SVGAngle::ToDOMBaseVal(SVGElement* aSVGElement) {
  RefPtr<DOMSVGAngle> domBaseVal = sBaseSVGAngleTearoffTable.GetTearoff(this);
  if (!domBaseVal) {
    domBaseVal = new DOMSVGAngle(this, aSVGElement, DOMSVGAngle::BaseValue);
    sBaseSVGAngleTearoffTable.AddTearoff(this, domBaseVal);
  }

  return domBaseVal.forget();
}

already_AddRefed<DOMSVGAngle> SVGAngle::ToDOMAnimVal(SVGElement* aSVGElement) {
  RefPtr<DOMSVGAngle> domAnimVal = sAnimSVGAngleTearoffTable.GetTearoff(this);
  if (!domAnimVal) {
    domAnimVal = new DOMSVGAngle(this, aSVGElement, DOMSVGAngle::AnimValue);
    sAnimSVGAngleTearoffTable.AddTearoff(this, domAnimVal);
  }

  return domAnimVal.forget();
}

DOMSVGAngle::~DOMSVGAngle() {
  if (mType == BaseValue) {
    sBaseSVGAngleTearoffTable.RemoveTearoff(mVal);
  } else if (mType == AnimValue) {
    sAnimSVGAngleTearoffTable.RemoveTearoff(mVal);
  } else {
    delete mVal;
  }
}

/* Implementation */

nsresult SVGAngle::SetBaseValueString(const nsAString& aValueAsString,
                                      SVGElement* aSVGElement,
                                      bool aDoSetAttr) {
  float value;
  uint16_t unitType;

  if (!GetValueFromString(aValueAsString, value, &unitType)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  if (mBaseVal == value && mBaseValUnit == uint8_t(unitType)) {
    return NS_OK;
  }

  nsAttrValue emptyOrOldValue;
  if (aDoSetAttr) {
    emptyOrOldValue = aSVGElement->WillChangeAngle(mAttrEnum);
  }
  mBaseVal = value;
  mBaseValUnit = uint8_t(unitType);
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
    mAnimValUnit = mBaseValUnit;
  } else {
    aSVGElement->AnimationNeedsResample();
  }

  if (aDoSetAttr) {
    aSVGElement->DidChangeAngle(mAttrEnum, emptyOrOldValue);
  }
  return NS_OK;
}

void SVGAngle::GetBaseValueString(nsAString& aValueAsString) const {
  GetAngleValueString(aValueAsString, mBaseVal, mBaseValUnit);
}

void SVGAngle::GetAnimValueString(nsAString& aValueAsString) const {
  GetAngleValueString(aValueAsString, mAnimVal, mAnimValUnit);
}

void SVGAngle::SetBaseValue(float aValue, uint8_t aUnit,
                            SVGElement* aSVGElement, bool aDoSetAttr) {
  float valueInSpecifiedUnits = aValue / GetDegreesPerUnit(aUnit);
  if (aUnit == mBaseValUnit && mBaseVal == valueInSpecifiedUnits) {
    return;
  }
  nsAttrValue emptyOrOldValue;
  if (aSVGElement && aDoSetAttr) {
    emptyOrOldValue = aSVGElement->WillChangeAngle(mAttrEnum);
  }

  mBaseValUnit = aUnit;
  mBaseVal = valueInSpecifiedUnits;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  } else {
    aSVGElement->AnimationNeedsResample();
  }
  if (aSVGElement && aDoSetAttr) {
    aSVGElement->DidChangeAngle(mAttrEnum, emptyOrOldValue);
  }
}

void SVGAngle::SetAnimValue(float aValue, uint8_t aUnit,
                            SVGElement* aSVGElement) {
  if (mIsAnimated && mAnimVal == aValue && mAnimValUnit == aUnit) {
    return;
  }
  mAnimVal = aValue;
  mAnimValUnit = aUnit;
  mIsAnimated = true;
  aSVGElement->DidAnimateAngle(mAttrEnum);
}

already_AddRefed<SVGAnimatedAngle> SVGAngle::ToDOMAnimatedAngle(
    SVGElement* aSVGElement) {
  RefPtr<SVGAnimatedAngle> domAnimatedAngle =
      sSVGAnimatedAngleTearoffTable.GetTearoff(this);
  if (!domAnimatedAngle) {
    domAnimatedAngle = new SVGAnimatedAngle(this, aSVGElement);
    sSVGAnimatedAngleTearoffTable.AddTearoff(this, domAnimatedAngle);
  }

  return domAnimatedAngle.forget();
}

SVGAnimatedAngle::~SVGAnimatedAngle() {
  sSVGAnimatedAngleTearoffTable.RemoveTearoff(mVal);
}

UniquePtr<SMILAttr> SVGAngle::ToSMILAttr(SVGElement* aSVGElement) {
  if (aSVGElement->NodeInfo()->Equals(nsGkAtoms::marker, kNameSpaceID_SVG)) {
    SVGMarkerElement* marker = static_cast<SVGMarkerElement*>(aSVGElement);
    return MakeUnique<SMILOrient>(marker->GetOrientType(), this, aSVGElement);
  }
  // SMILOrient would not be useful for general angle attributes (also,
  // "orient" is the only animatable <angle>-valued attribute in SVG 1.1).
  MOZ_ASSERT_UNREACHABLE("Trying to animate unknown angle attribute.");
  return nullptr;
}

nsresult SVGAngle::SMILOrient::ValueFromString(
    const nsAString& aStr, const SVGAnimationElement* /*aSrcElement*/,
    SMILValue& aValue, bool& aPreventCachingOfSandwich) const {
  SMILValue val(&SVGOrientSMILType::sSingleton);
  if (aStr.EqualsLiteral("auto")) {
    val.mU.mOrient.mOrientType = SVG_MARKER_ORIENT_AUTO;
  } else if (aStr.EqualsLiteral("auto-start-reverse")) {
    val.mU.mOrient.mOrientType = SVG_MARKER_ORIENT_AUTO_START_REVERSE;
  } else {
    float value;
    uint16_t unitType;
    if (!GetValueFromString(aStr, value, &unitType)) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
    val.mU.mOrient.mAngle = value;
    val.mU.mOrient.mUnit = unitType;
    val.mU.mOrient.mOrientType = SVG_MARKER_ORIENT_ANGLE;
  }
  aValue = std::move(val);
  aPreventCachingOfSandwich = false;

  return NS_OK;
}

SMILValue SVGAngle::SMILOrient::GetBaseValue() const {
  SMILValue val(&SVGOrientSMILType::sSingleton);
  val.mU.mOrient.mAngle = mAngle->GetBaseValInSpecifiedUnits();
  val.mU.mOrient.mUnit = mAngle->GetBaseValueUnit();
  val.mU.mOrient.mOrientType = mOrientType->GetBaseValue();
  return val;
}

void SVGAngle::SMILOrient::ClearAnimValue() {
  if (mAngle->mIsAnimated) {
    mOrientType->SetAnimValue(mOrientType->GetBaseValue());
    mAngle->mIsAnimated = false;
    mAngle->mAnimVal = mAngle->mBaseVal;
    mAngle->mAnimValUnit = mAngle->mBaseValUnit;
    mSVGElement->DidAnimateAngle(mAngle->mAttrEnum);
  }
}

nsresult SVGAngle::SMILOrient::SetAnimValue(const SMILValue& aValue) {
  NS_ASSERTION(aValue.mType == &SVGOrientSMILType::sSingleton,
               "Unexpected type to assign animated value");

  if (aValue.mType == &SVGOrientSMILType::sSingleton) {
    mOrientType->SetAnimValue(aValue.mU.mOrient.mOrientType);
    if (aValue.mU.mOrient.mOrientType == SVG_MARKER_ORIENT_AUTO ||
        aValue.mU.mOrient.mOrientType == SVG_MARKER_ORIENT_AUTO_START_REVERSE) {
      mAngle->SetAnimValue(0.0f, SVG_ANGLETYPE_UNSPECIFIED, mSVGElement);
    } else {
      mAngle->SetAnimValue(aValue.mU.mOrient.mAngle, aValue.mU.mOrient.mUnit,
                           mSVGElement);
    }
  }
  return NS_OK;
}

}  // namespace mozilla
