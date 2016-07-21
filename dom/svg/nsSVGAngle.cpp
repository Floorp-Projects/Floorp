/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGAngle.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/SVGMarkerElement.h"
#include "mozilla/Move.h"
#include "nsContentUtils.h" // NS_ENSURE_FINITE
#include "nsSMILValue.h"
#include "nsSVGAttrTearoffTable.h"
#include "nsTextFormatter.h"
#include "SVGAngle.h"
#include "SVGAnimatedAngle.h"
#include "SVGOrientSMILType.h"

using namespace mozilla;
using namespace mozilla::dom;

static nsIAtom** const unitMap[] =
{
  nullptr, /* SVG_ANGLETYPE_UNKNOWN */
  nullptr, /* SVG_ANGLETYPE_UNSPECIFIED */
  &nsGkAtoms::deg,
  &nsGkAtoms::rad,
  &nsGkAtoms::grad
};

static nsSVGAttrTearoffTable<nsSVGAngle, SVGAnimatedAngle>
  sSVGAnimatedAngleTearoffTable;
static nsSVGAttrTearoffTable<nsSVGAngle, SVGAngle>
  sBaseSVGAngleTearoffTable;
static nsSVGAttrTearoffTable<nsSVGAngle, SVGAngle>
  sAnimSVGAngleTearoffTable;

/* Helper functions */

static bool
IsValidUnitType(uint16_t unit)
{
  if (unit > SVG_ANGLETYPE_UNKNOWN &&
      unit <= SVG_ANGLETYPE_GRAD)
    return true;

  return false;
}

static void 
GetUnitString(nsAString& unit, uint16_t unitType)
{
  if (IsValidUnitType(unitType)) {
    if (unitMap[unitType]) {
      (*unitMap[unitType])->ToString(unit);
    }
    return;
  }

  NS_NOTREACHED("Unknown unit type");
  return;
}

static uint16_t
GetUnitTypeForString(const nsAString& unitStr)
{
  if (unitStr.IsEmpty()) 
    return SVG_ANGLETYPE_UNSPECIFIED;
                   
  nsIAtom *unitAtom = NS_GetStaticAtom(unitStr);

  if (unitAtom) {
    for (uint32_t i = 0 ; i < ArrayLength(unitMap) ; i++) {
      if (unitMap[i] && *unitMap[i] == unitAtom) {
        return i;
      }
    }
  }

  return SVG_ANGLETYPE_UNKNOWN;
}

static void
GetValueString(nsAString &aValueAsString, float aValue, uint16_t aUnitType)
{
  char16_t buf[24];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(char16_t),
                            u"%g",
                            (double)aValue);
  aValueAsString.Assign(buf);

  nsAutoString unitString;
  GetUnitString(unitString, aUnitType);
  aValueAsString.Append(unitString);
}

static bool
GetValueFromString(const nsAString& aString,
                   float& aValue,
                   uint16_t* aUnitType)
{
  RangedPtr<const char16_t> iter =
    SVGContentUtils::GetStartRangedPtr(aString);
  const RangedPtr<const char16_t> end =
    SVGContentUtils::GetEndRangedPtr(aString);

  if (!SVGContentUtils::ParseNumber(iter, end, aValue)) {
    return false;
  }

  const nsAString& units = Substring(iter.get(), end.get());
  *aUnitType = GetUnitTypeForString(units);
  return IsValidUnitType(*aUnitType);
}

/* static */ float
nsSVGAngle::GetDegreesPerUnit(uint8_t aUnit)
{
  switch (aUnit) {
  case SVG_ANGLETYPE_UNSPECIFIED:
  case SVG_ANGLETYPE_DEG:
    return 1;
  case SVG_ANGLETYPE_RAD:
    return static_cast<float>(180.0 / M_PI);
  case SVG_ANGLETYPE_GRAD:
    return 90.0f / 100.0f;
  default:
    NS_NOTREACHED("Unknown unit type");
    return 0;
  }
}

void
nsSVGAngle::SetBaseValueInSpecifiedUnits(float aValue,
                                         nsSVGElement *aSVGElement)
{
  if (mBaseVal == aValue) {
    return;
  }

  nsAttrValue emptyOrOldValue = aSVGElement->WillChangeAngle(mAttrEnum);
  mBaseVal = aValue;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
  else {
    aSVGElement->AnimationNeedsResample();
  }
  aSVGElement->DidChangeAngle(mAttrEnum, emptyOrOldValue);
}

nsresult
nsSVGAngle::ConvertToSpecifiedUnits(uint16_t unitType,
                                    nsSVGElement *aSVGElement)
{
  if (!IsValidUnitType(unitType))
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  if (mBaseValUnit == uint8_t(unitType))
    return NS_OK;

  nsAttrValue emptyOrOldValue;
  if (aSVGElement) {
    emptyOrOldValue = aSVGElement->WillChangeAngle(mAttrEnum);
  }

  float valueInUserUnits = mBaseVal * GetDegreesPerUnit(mBaseValUnit);
  mBaseValUnit = uint8_t(unitType);
  // Setting aDoSetAttr to false here will ensure we don't call
  // Will/DidChangeAngle a second time (and dispatch duplicate notifications).
  SetBaseValue(valueInUserUnits, aSVGElement, false);

  if (aSVGElement) {
    aSVGElement->DidChangeAngle(mAttrEnum, emptyOrOldValue);
  }

  return NS_OK;
}

nsresult
nsSVGAngle::NewValueSpecifiedUnits(uint16_t unitType,
                                   float valueInSpecifiedUnits,
                                   nsSVGElement *aSVGElement)
{
  NS_ENSURE_FINITE(valueInSpecifiedUnits, NS_ERROR_ILLEGAL_VALUE);

  if (!IsValidUnitType(unitType))
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

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
  }
  else {
    aSVGElement->AnimationNeedsResample();
  }
  if (aSVGElement) {
    aSVGElement->DidChangeAngle(mAttrEnum, emptyOrOldValue);
  }
  return NS_OK;
}

already_AddRefed<SVGAngle>
nsSVGAngle::ToDOMBaseVal(nsSVGElement *aSVGElement)
{
  RefPtr<SVGAngle> domBaseVal =
    sBaseSVGAngleTearoffTable.GetTearoff(this);
  if (!domBaseVal) {
    domBaseVal = new SVGAngle(this, aSVGElement, SVGAngle::BaseValue);
    sBaseSVGAngleTearoffTable.AddTearoff(this, domBaseVal);
  }

  return domBaseVal.forget();
}

already_AddRefed<SVGAngle>
nsSVGAngle::ToDOMAnimVal(nsSVGElement *aSVGElement)
{
  RefPtr<SVGAngle> domAnimVal =
    sAnimSVGAngleTearoffTable.GetTearoff(this);
  if (!domAnimVal) {
    domAnimVal = new SVGAngle(this, aSVGElement, SVGAngle::AnimValue);
    sAnimSVGAngleTearoffTable.AddTearoff(this, domAnimVal);
  }

  return domAnimVal.forget();
}

SVGAngle::~SVGAngle()
{
  if (mType == BaseValue) {
    sBaseSVGAngleTearoffTable.RemoveTearoff(mVal);
  } else if (mType == AnimValue) {
    sAnimSVGAngleTearoffTable.RemoveTearoff(mVal);
  } else {
    delete mVal;
  }
}

/* Implementation */

nsresult
nsSVGAngle::SetBaseValueString(const nsAString &aValueAsString,
                               nsSVGElement *aSVGElement,
                               bool aDoSetAttr)
{
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
  }
  else {
    aSVGElement->AnimationNeedsResample();
  }

  if (aDoSetAttr) {
    aSVGElement->DidChangeAngle(mAttrEnum, emptyOrOldValue);
  }
  return NS_OK;
}

void
nsSVGAngle::GetBaseValueString(nsAString & aValueAsString) const
{
  GetValueString(aValueAsString, mBaseVal, mBaseValUnit);
}

void
nsSVGAngle::GetAnimValueString(nsAString & aValueAsString) const
{
  GetValueString(aValueAsString, mAnimVal, mAnimValUnit);
}

void
nsSVGAngle::SetBaseValue(float aValue, nsSVGElement *aSVGElement,
                         bool aDoSetAttr)
{
  if (mBaseVal == aValue * GetDegreesPerUnit(mBaseValUnit)) {
    return;
  }
  nsAttrValue emptyOrOldValue;
  if (aSVGElement && aDoSetAttr) {
    emptyOrOldValue = aSVGElement->WillChangeAngle(mAttrEnum);
  }

  mBaseVal = aValue / GetDegreesPerUnit(mBaseValUnit);
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
  else {
    aSVGElement->AnimationNeedsResample();
  }
  if (aSVGElement && aDoSetAttr) {
    aSVGElement->DidChangeAngle(mAttrEnum, emptyOrOldValue);
  }
}

void
nsSVGAngle::SetAnimValue(float aValue, uint8_t aUnit, nsSVGElement *aSVGElement)
{
  if (mIsAnimated && mAnimVal == aValue && mAnimValUnit == aUnit) {
    return;
  }
  mAnimVal = aValue;
  mAnimValUnit = aUnit;
  mIsAnimated = true;
  aSVGElement->DidAnimateAngle(mAttrEnum);
}

already_AddRefed<SVGAnimatedAngle>
nsSVGAngle::ToDOMAnimatedAngle(nsSVGElement *aSVGElement)
{
  RefPtr<SVGAnimatedAngle> domAnimatedAngle =
    sSVGAnimatedAngleTearoffTable.GetTearoff(this);
  if (!domAnimatedAngle) {
    domAnimatedAngle = new SVGAnimatedAngle(this, aSVGElement);
    sSVGAnimatedAngleTearoffTable.AddTearoff(this, domAnimatedAngle);
  }

  return domAnimatedAngle.forget();
}

SVGAnimatedAngle::~SVGAnimatedAngle()
{
  sSVGAnimatedAngleTearoffTable.RemoveTearoff(mVal);
}

nsISMILAttr*
nsSVGAngle::ToSMILAttr(nsSVGElement *aSVGElement)
{
  if (aSVGElement->NodeInfo()->Equals(nsGkAtoms::marker, kNameSpaceID_SVG)) {
    SVGMarkerElement *marker = static_cast<SVGMarkerElement*>(aSVGElement);
    return new SMILOrient(marker->GetOrientType(), this, aSVGElement);
  }
  // SMILOrient would not be useful for general angle attributes (also,
  // "orient" is the only animatable <angle>-valued attribute in SVG 1.1).
  NS_NOTREACHED("Trying to animate unknown angle attribute.");
  return nullptr;
}

nsresult
nsSVGAngle::SMILOrient::ValueFromString(const nsAString& aStr,
                                        const SVGAnimationElement* /*aSrcElement*/,
                                        nsSMILValue& aValue,
                                        bool& aPreventCachingOfSandwich) const
{
  nsSMILValue val(&SVGOrientSMILType::sSingleton);
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
  aValue = Move(val);
  aPreventCachingOfSandwich = false;

  return NS_OK;
}

nsSMILValue
nsSVGAngle::SMILOrient::GetBaseValue() const
{
  nsSMILValue val(&SVGOrientSMILType::sSingleton);
  val.mU.mOrient.mAngle = mAngle->GetBaseValInSpecifiedUnits();
  val.mU.mOrient.mUnit = mAngle->GetBaseValueUnit();
  val.mU.mOrient.mOrientType = mOrientType->GetBaseValue();
  return val;
}

void
nsSVGAngle::SMILOrient::ClearAnimValue()
{
  if (mAngle->mIsAnimated) {
    mOrientType->SetAnimValue(mOrientType->GetBaseValue());
    mAngle->mIsAnimated = false;
    mAngle->mAnimVal = mAngle->mBaseVal;
    mAngle->mAnimValUnit = mAngle->mBaseValUnit;
    mSVGElement->DidAnimateAngle(mAngle->mAttrEnum);
  }
}

nsresult
nsSVGAngle::SMILOrient::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &SVGOrientSMILType::sSingleton,
               "Unexpected type to assign animated value");

  if (aValue.mType == &SVGOrientSMILType::sSingleton) {
    mOrientType->SetAnimValue(aValue.mU.mOrient.mOrientType);
    if (aValue.mU.mOrient.mOrientType == SVG_MARKER_ORIENT_AUTO ||
        aValue.mU.mOrient.mOrientType == SVG_MARKER_ORIENT_AUTO_START_REVERSE) {
      mAngle->SetAnimValue(0.0f, SVG_ANGLETYPE_UNSPECIFIED, mSVGElement);
    } else {
      mAngle->SetAnimValue(aValue.mU.mOrient.mAngle, aValue.mU.mOrient.mUnit, mSVGElement);
    }
  }
  return NS_OK;
}
