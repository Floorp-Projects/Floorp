/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsSVGLength2.h"
#include "prdtoa.h"
#include "nsTextFormatter.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "nsIFrame.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGAttrTearoffTable.h"
#include "nsContentUtils.h" // NS_ENSURE_FINITE
#include "nsSMILValue.h"
#include "nsSMILFloatType.h"
#include "nsAttrValueInlines.h"
#include "mozilla/dom/SVGAnimatedLength.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGLength2::DOMBaseVal, mSVGElement)

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGLength2::DOMAnimVal, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGLength2::DOMBaseVal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGLength2::DOMBaseVal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGLength2::DOMAnimVal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGLength2::DOMAnimVal)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGLength2::DOMBaseVal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGLength)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGLength)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGLength2::DOMAnimVal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGLength)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGLength)
NS_INTERFACE_MAP_END

static nsIAtom** const unitMap[] =
{
  nullptr, /* SVG_LENGTHTYPE_UNKNOWN */
  nullptr, /* SVG_LENGTHTYPE_NUMBER */
  &nsGkAtoms::percentage,
  &nsGkAtoms::em,
  &nsGkAtoms::ex,
  &nsGkAtoms::px,
  &nsGkAtoms::cm,
  &nsGkAtoms::mm,
  &nsGkAtoms::in,
  &nsGkAtoms::pt,
  &nsGkAtoms::pc
};

static nsSVGAttrTearoffTable<nsSVGLength2, SVGAnimatedLength>
  sSVGAnimatedLengthTearoffTable;
static nsSVGAttrTearoffTable<nsSVGLength2, nsSVGLength2::DOMBaseVal>
  sBaseSVGLengthTearoffTable;
static nsSVGAttrTearoffTable<nsSVGLength2, nsSVGLength2::DOMAnimVal>
  sAnimSVGLengthTearoffTable;

/* Helper functions */

static bool
IsValidUnitType(uint16_t unit)
{
  if (unit > nsIDOMSVGLength::SVG_LENGTHTYPE_UNKNOWN &&
      unit <= nsIDOMSVGLength::SVG_LENGTHTYPE_PC)
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
    return nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER;
                   
  nsIAtom *unitAtom = NS_GetStaticAtom(unitStr);
  if (unitAtom) {
    for (uint32_t i = 0 ; i < ArrayLength(unitMap) ; i++) {
      if (unitMap[i] && *unitMap[i] == unitAtom) {
        return i;
      }
    }
  }

  return nsIDOMSVGLength::SVG_LENGTHTYPE_UNKNOWN;
}

static void
GetValueString(nsAString &aValueAsString, float aValue, uint16_t aUnitType)
{
  PRUnichar buf[24];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
                            NS_LITERAL_STRING("%g").get(),
                            (double)aValue);
  aValueAsString.Assign(buf);

  nsAutoString unitString;
  GetUnitString(unitString, aUnitType);
  aValueAsString.Append(unitString);
}

static nsresult
GetValueFromString(const nsAString &aValueAsString,
                   float *aValue,
                   uint16_t *aUnitType)
{
  NS_ConvertUTF16toUTF8 value(aValueAsString);
  const char *str = value.get();

  if (NS_IsAsciiWhitespace(*str))
    return NS_ERROR_DOM_SYNTAX_ERR;
  
  char *rest;
  *aValue = float(PR_strtod(str, &rest));
  if (rest != str && NS_finite(*aValue)) {
    *aUnitType = GetUnitTypeForString(
      Substring(aValueAsString, rest - str));
    if (IsValidUnitType(*aUnitType)) {
      return NS_OK;
    }
  }
  
  return NS_ERROR_DOM_SYNTAX_ERR;
}

static float
FixAxisLength(float aLength)
{
  if (aLength == 0.0f) {
    NS_WARNING("zero axis length");
    return 1e-20f;
  }
  return aLength;
}

float
nsSVGLength2::GetAxisLength(SVGSVGElement *aCtx) const
{
  if (!aCtx)
    return 1;

  return FixAxisLength(aCtx->GetLength(mCtxType));
}

float
nsSVGLength2::GetAxisLength(nsIFrame *aNonSVGFrame) const
{
  gfxSize size =
    nsSVGIntegrationUtils::GetSVGCoordContextForNonSVGFrame(aNonSVGFrame);
  float length;
  switch (mCtxType) {
  case SVGContentUtils::X:
    length = size.width;
    break;
  case SVGContentUtils::Y:
    length = size.height;
    break;
  case SVGContentUtils::XY:
    length = SVGContentUtils::ComputeNormalizedHypotenuse(size.width, size.height);
    break;
  default:
    NS_NOTREACHED("Unknown axis type");
    length = 1;
    break;
  }
  return FixAxisLength(length);
}

float
nsSVGLength2::GetUnitScaleFactor(nsSVGElement *aSVGElement,
                                 uint8_t aUnitType) const
{
  switch (aUnitType) {
  case nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER:
  case nsIDOMSVGLength::SVG_LENGTHTYPE_PX:
    return 1;
  case nsIDOMSVGLength::SVG_LENGTHTYPE_EMS:
    return 1 / GetEmLength(aSVGElement);
  case nsIDOMSVGLength::SVG_LENGTHTYPE_EXS:
    return 1 / GetExLength(aSVGElement);
  }

  return GetUnitScaleFactor(aSVGElement->GetCtx(), aUnitType);
}

float
nsSVGLength2::GetUnitScaleFactor(SVGSVGElement *aCtx, uint8_t aUnitType) const
{
  switch (aUnitType) {
  case nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER:
  case nsIDOMSVGLength::SVG_LENGTHTYPE_PX:
    return 1;
  case nsIDOMSVGLength::SVG_LENGTHTYPE_MM:
    return GetMMPerPixel();
  case nsIDOMSVGLength::SVG_LENGTHTYPE_CM:
    return GetMMPerPixel() / 10.0f;
  case nsIDOMSVGLength::SVG_LENGTHTYPE_IN:
    return GetMMPerPixel() / MM_PER_INCH_FLOAT;
  case nsIDOMSVGLength::SVG_LENGTHTYPE_PT:
    return GetMMPerPixel() * POINTS_PER_INCH_FLOAT / MM_PER_INCH_FLOAT;
  case nsIDOMSVGLength::SVG_LENGTHTYPE_PC:
    return GetMMPerPixel() * POINTS_PER_INCH_FLOAT / MM_PER_INCH_FLOAT / 12.0f;
  case nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE:
    return 100.0f / GetAxisLength(aCtx);
  case nsIDOMSVGLength::SVG_LENGTHTYPE_EMS:
    return 1 / GetEmLength(aCtx);
  case nsIDOMSVGLength::SVG_LENGTHTYPE_EXS:
    return 1 / GetExLength(aCtx);
  default:
    NS_NOTREACHED("Unknown unit type");
    return 0;
  }
}

float
nsSVGLength2::GetUnitScaleFactor(nsIFrame *aFrame, uint8_t aUnitType) const
{
  nsIContent* content = aFrame->GetContent();
  if (content->IsSVG())
    return GetUnitScaleFactor(static_cast<nsSVGElement*>(content), aUnitType);

  switch (aUnitType) {
  case nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER:
  case nsIDOMSVGLength::SVG_LENGTHTYPE_PX:
    return 1;
  case nsIDOMSVGLength::SVG_LENGTHTYPE_MM:
    return GetMMPerPixel();
  case nsIDOMSVGLength::SVG_LENGTHTYPE_CM:
    return GetMMPerPixel() / 10.0f;
  case nsIDOMSVGLength::SVG_LENGTHTYPE_IN:
    return GetMMPerPixel() / MM_PER_INCH_FLOAT;
  case nsIDOMSVGLength::SVG_LENGTHTYPE_PT:
    return GetMMPerPixel() * POINTS_PER_INCH_FLOAT / MM_PER_INCH_FLOAT;
  case nsIDOMSVGLength::SVG_LENGTHTYPE_PC:
    return GetMMPerPixel() * POINTS_PER_INCH_FLOAT / MM_PER_INCH_FLOAT / 12.0f;
  case nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE:
    return 100.0f / GetAxisLength(aFrame);
  case nsIDOMSVGLength::SVG_LENGTHTYPE_EMS:
    return 1 / GetEmLength(aFrame);
  case nsIDOMSVGLength::SVG_LENGTHTYPE_EXS:
    return 1 / GetExLength(aFrame);
  default:
    NS_NOTREACHED("Unknown unit type");
    return 0;
  }
}

void
nsSVGLength2::SetBaseValueInSpecifiedUnits(float aValue,
                                           nsSVGElement *aSVGElement,
                                           bool aDoSetAttr)
{
  if (mIsBaseSet && mBaseVal == aValue) {
    return;
  }

  nsAttrValue emptyOrOldValue;
  if (aDoSetAttr) {
    emptyOrOldValue = aSVGElement->WillChangeLength(mAttrEnum);
  }
  mBaseVal = aValue;
  mIsBaseSet = true;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
  else {
    aSVGElement->AnimationNeedsResample();
  }
  if (aDoSetAttr) {
    aSVGElement->DidChangeLength(mAttrEnum, emptyOrOldValue);
  }
}

nsresult
nsSVGLength2::ConvertToSpecifiedUnits(uint16_t unitType,
                                      nsSVGElement *aSVGElement)
{
  if (!IsValidUnitType(unitType))
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  if (mIsBaseSet && mSpecifiedUnitType == uint8_t(unitType))
    return NS_OK;

  // Even though we're not changing the visual effect this length will have
  // on the document, we still need to send out notifications in case we have
  // mutation listeners, since the actual string value of the attribute will
  // change.
  nsAttrValue emptyOrOldValue = aSVGElement->WillChangeLength(mAttrEnum);

  float valueInUserUnits =
    mBaseVal / GetUnitScaleFactor(aSVGElement, mSpecifiedUnitType);
  mSpecifiedUnitType = uint8_t(unitType);
  // Setting aDoSetAttr to false here will ensure we don't call
  // Will/DidChangeAngle a second time (and dispatch duplicate notifications).
  SetBaseValue(valueInUserUnits, aSVGElement, false);

  aSVGElement->DidChangeLength(mAttrEnum, emptyOrOldValue);

  return NS_OK;
}

nsresult
nsSVGLength2::NewValueSpecifiedUnits(uint16_t unitType,
                                     float valueInSpecifiedUnits,
                                     nsSVGElement *aSVGElement)
{
  NS_ENSURE_FINITE(valueInSpecifiedUnits, NS_ERROR_ILLEGAL_VALUE);

  if (!IsValidUnitType(unitType))
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  if (mIsBaseSet && mBaseVal == valueInSpecifiedUnits &&
      mSpecifiedUnitType == uint8_t(unitType)) {
    return NS_OK;
  }

  nsAttrValue emptyOrOldValue = aSVGElement->WillChangeLength(mAttrEnum);
  mBaseVal = valueInSpecifiedUnits;
  mIsBaseSet = true;
  mSpecifiedUnitType = uint8_t(unitType);
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
  else {
    aSVGElement->AnimationNeedsResample();
  }
  aSVGElement->DidChangeLength(mAttrEnum, emptyOrOldValue);
  return NS_OK;
}

nsresult
nsSVGLength2::ToDOMBaseVal(nsIDOMSVGLength **aResult, nsSVGElement *aSVGElement)
{
  nsRefPtr<DOMBaseVal> domBaseVal =
    sBaseSVGLengthTearoffTable.GetTearoff(this);
  if (!domBaseVal) {
    domBaseVal = new DOMBaseVal(this, aSVGElement);
    sBaseSVGLengthTearoffTable.AddTearoff(this, domBaseVal);
  }

  domBaseVal.forget(aResult);
  return NS_OK;
}

nsSVGLength2::DOMBaseVal::~DOMBaseVal()
{
  sBaseSVGLengthTearoffTable.RemoveTearoff(mVal);
}

nsresult
nsSVGLength2::ToDOMAnimVal(nsIDOMSVGLength **aResult, nsSVGElement *aSVGElement)
{
  nsRefPtr<DOMAnimVal> domAnimVal =
    sAnimSVGLengthTearoffTable.GetTearoff(this);
  if (!domAnimVal) {
    domAnimVal = new DOMAnimVal(this, aSVGElement);
    sAnimSVGLengthTearoffTable.AddTearoff(this, domAnimVal);
  }

  domAnimVal.forget(aResult);
  return NS_OK;
}

nsSVGLength2::DOMAnimVal::~DOMAnimVal()
{
  sAnimSVGLengthTearoffTable.RemoveTearoff(mVal);
}

/* Implementation */

nsresult
nsSVGLength2::SetBaseValueString(const nsAString &aValueAsString,
                                 nsSVGElement *aSVGElement,
                                 bool aDoSetAttr)
{
  float value;
  uint16_t unitType;

  nsresult rv = GetValueFromString(aValueAsString, &value, &unitType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mIsBaseSet && mBaseVal == value &&
      mSpecifiedUnitType == uint8_t(unitType)) {
    return NS_OK;
  }

  nsAttrValue emptyOrOldValue;
  if (aDoSetAttr) {
    emptyOrOldValue = aSVGElement->WillChangeLength(mAttrEnum);
  }
  mBaseVal = value;
  mIsBaseSet = true;
  mSpecifiedUnitType = uint8_t(unitType);
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
  else {
    aSVGElement->AnimationNeedsResample();
  }

  if (aDoSetAttr) {
    aSVGElement->DidChangeLength(mAttrEnum, emptyOrOldValue);
  }
  return NS_OK;
}

void
nsSVGLength2::GetBaseValueString(nsAString & aValueAsString) const
{
  GetValueString(aValueAsString, mBaseVal, mSpecifiedUnitType);
}

void
nsSVGLength2::GetAnimValueString(nsAString & aValueAsString) const
{
  GetValueString(aValueAsString, mAnimVal, mSpecifiedUnitType);
}

void
nsSVGLength2::SetBaseValue(float aValue, nsSVGElement *aSVGElement,
                           bool aDoSetAttr)
{
  SetBaseValueInSpecifiedUnits(aValue * GetUnitScaleFactor(aSVGElement,
                                                           mSpecifiedUnitType),
                               aSVGElement, aDoSetAttr);
}

void
nsSVGLength2::SetAnimValueInSpecifiedUnits(float aValue,
                                           nsSVGElement* aSVGElement)
{
  if (mAnimVal == aValue && mIsAnimated) {
    return;
  }
  mAnimVal = aValue;
  mIsAnimated = true;
  aSVGElement->DidAnimateLength(mAttrEnum);
}

void
nsSVGLength2::SetAnimValue(float aValue, nsSVGElement *aSVGElement)
{
  SetAnimValueInSpecifiedUnits(aValue * GetUnitScaleFactor(aSVGElement,
                                                           mSpecifiedUnitType),
                               aSVGElement);
}

already_AddRefed<SVGAnimatedLength>
nsSVGLength2::ToDOMAnimatedLength(nsSVGElement* aSVGElement)
{
  nsRefPtr<SVGAnimatedLength> svgAnimatedLength =
    sSVGAnimatedLengthTearoffTable.GetTearoff(this);
  if (!svgAnimatedLength) {
    svgAnimatedLength = new SVGAnimatedLength(this, aSVGElement);
    sSVGAnimatedLengthTearoffTable.AddTearoff(this, svgAnimatedLength);
  }

  return svgAnimatedLength.forget();
}

SVGAnimatedLength::~SVGAnimatedLength()
{
  sSVGAnimatedLengthTearoffTable.RemoveTearoff(mVal);
}

nsISMILAttr*
nsSVGLength2::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return new SMILLength(this, aSVGElement);
}

nsresult
nsSVGLength2::SMILLength::ValueFromString(const nsAString& aStr,
                                 const SVGAnimationElement* /*aSrcElement*/,
                                 nsSMILValue& aValue,
                                 bool& aPreventCachingOfSandwich) const
{
  float value;
  uint16_t unitType;
  
  nsresult rv = GetValueFromString(aStr, &value, &unitType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsSMILValue val(nsSMILFloatType::Singleton());
  val.mU.mDouble = value / mVal->GetUnitScaleFactor(mSVGElement, unitType);
  aValue = val;
  aPreventCachingOfSandwich =
              (unitType == nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
               unitType == nsIDOMSVGLength::SVG_LENGTHTYPE_EMS ||
               unitType == nsIDOMSVGLength::SVG_LENGTHTYPE_EXS);

  return NS_OK;
}

nsSMILValue
nsSVGLength2::SMILLength::GetBaseValue() const
{
  nsSMILValue val(nsSMILFloatType::Singleton());
  val.mU.mDouble = mVal->GetBaseValue(mSVGElement);
  return val;
}

void
nsSVGLength2::SMILLength::ClearAnimValue()
{
  if (mVal->mIsAnimated) {
    mVal->mIsAnimated = false;
    mVal->mAnimVal = mVal->mBaseVal;
    mSVGElement->DidAnimateLength(mVal->mAttrEnum);
  }  
}

nsresult
nsSVGLength2::SMILLength::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == nsSMILFloatType::Singleton(),
    "Unexpected type to assign animated value");
  if (aValue.mType == nsSMILFloatType::Singleton()) {
    mVal->SetAnimValue(float(aValue.mU.mDouble), mSVGElement);
  }
  return NS_OK;
}
