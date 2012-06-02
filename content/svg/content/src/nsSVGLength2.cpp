/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsSVGLength2.h"
#include "prdtoa.h"
#include "nsTextFormatter.h"
#include "nsSVGSVGElement.h"
#include "nsIFrame.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGAttrTearoffTable.h"
#include "nsContentUtils.h" // NS_ENSURE_FINITE
#include "nsSMILValue.h"
#include "nsSMILFloatType.h"

using namespace mozilla;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGLength2::DOMBaseVal, mSVGElement)

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGLength2::DOMAnimVal, mSVGElement)

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGLength2::DOMAnimatedLength, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGLength2::DOMBaseVal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGLength2::DOMBaseVal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGLength2::DOMAnimVal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGLength2::DOMAnimVal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGLength2::DOMAnimatedLength)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGLength2::DOMAnimatedLength)

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

DOMCI_DATA(SVGAnimatedLength, nsSVGLength2::DOMAnimatedLength)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGLength2::DOMAnimatedLength)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedLength)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimatedLength)
NS_INTERFACE_MAP_END

static nsIAtom** const unitMap[] =
{
  nsnull, /* SVG_LENGTHTYPE_UNKNOWN */
  nsnull, /* SVG_LENGTHTYPE_NUMBER */
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

static nsSVGAttrTearoffTable<nsSVGLength2, nsIDOMSVGAnimatedLength>
  sSVGAnimatedLengthTearoffTable;
static nsSVGAttrTearoffTable<nsSVGLength2, nsIDOMSVGLength>
  sBaseSVGLengthTearoffTable;
static nsSVGAttrTearoffTable<nsSVGLength2, nsIDOMSVGLength>
  sAnimSVGLengthTearoffTable;

/* Helper functions */

static bool
IsValidUnitType(PRUint16 unit)
{
  if (unit > nsIDOMSVGLength::SVG_LENGTHTYPE_UNKNOWN &&
      unit <= nsIDOMSVGLength::SVG_LENGTHTYPE_PC)
    return true;

  return false;
}

static void
GetUnitString(nsAString& unit, PRUint16 unitType)
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

static PRUint16
GetUnitTypeForString(const char* unitStr)
{
  if (!unitStr || *unitStr == '\0') 
    return nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER;
                   
  nsCOMPtr<nsIAtom> unitAtom = do_GetAtom(unitStr);

  for (PRUint32 i = 0 ; i < ArrayLength(unitMap) ; i++) {
    if (unitMap[i] && *unitMap[i] == unitAtom) {
      return i;
    }
  }

  return nsIDOMSVGLength::SVG_LENGTHTYPE_UNKNOWN;
}

static void
GetValueString(nsAString &aValueAsString, float aValue, PRUint16 aUnitType)
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
                   PRUint16 *aUnitType)
{
  NS_ConvertUTF16toUTF8 value(aValueAsString);
  const char *str = value.get();

  if (NS_IsAsciiWhitespace(*str))
    return NS_ERROR_DOM_SYNTAX_ERR;
  
  char *rest;
  *aValue = float(PR_strtod(str, &rest));
  if (rest != str && NS_finite(*aValue)) {
    *aUnitType = GetUnitTypeForString(rest);
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
nsSVGLength2::GetAxisLength(nsSVGSVGElement *aCtx) const
{
  if (!aCtx)
    return 1;

  return FixAxisLength(aCtx->GetLength(mCtxType));
}

float
nsSVGLength2::GetAxisLength(nsIFrame *aNonSVGFrame) const
{
  gfxRect rect = nsSVGIntegrationUtils::GetSVGRectForNonSVGFrame(aNonSVGFrame);
  float length;
  switch (mCtxType) {
  case nsSVGUtils::X: length = rect.Width(); break;
  case nsSVGUtils::Y: length = rect.Height(); break;
  case nsSVGUtils::XY:
    length = nsSVGUtils::ComputeNormalizedHypotenuse(rect.Width(), rect.Height());
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
                                 PRUint8 aUnitType) const
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
nsSVGLength2::GetUnitScaleFactor(nsSVGSVGElement *aCtx, PRUint8 aUnitType) const
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
nsSVGLength2::GetUnitScaleFactor(nsIFrame *aFrame, PRUint8 aUnitType) const
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
nsSVGLength2::ConvertToSpecifiedUnits(PRUint16 unitType,
                                      nsSVGElement *aSVGElement)
{
  if (!IsValidUnitType(unitType))
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  if (mIsBaseSet && mSpecifiedUnitType == PRUint8(unitType))
    return NS_OK;

  // Even though we're not changing the visual effect this length will have
  // on the document, we still need to send out notifications in case we have
  // mutation listeners, since the actual string value of the attribute will
  // change.
  nsAttrValue emptyOrOldValue = aSVGElement->WillChangeLength(mAttrEnum);

  float valueInUserUnits =
    mBaseVal / GetUnitScaleFactor(aSVGElement, mSpecifiedUnitType);
  mSpecifiedUnitType = PRUint8(unitType);
  // Setting aDoSetAttr to false here will ensure we don't call
  // Will/DidChangeAngle a second time (and dispatch duplicate notifications).
  SetBaseValue(valueInUserUnits, aSVGElement, false);

  aSVGElement->DidChangeLength(mAttrEnum, emptyOrOldValue);

  return NS_OK;
}

nsresult
nsSVGLength2::NewValueSpecifiedUnits(PRUint16 unitType,
                                     float valueInSpecifiedUnits,
                                     nsSVGElement *aSVGElement)
{
  NS_ENSURE_FINITE(valueInSpecifiedUnits, NS_ERROR_ILLEGAL_VALUE);

  if (!IsValidUnitType(unitType))
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  if (mIsBaseSet && mBaseVal == valueInSpecifiedUnits &&
      mSpecifiedUnitType == PRUint8(unitType)) {
    return NS_OK;
  }

  nsAttrValue emptyOrOldValue = aSVGElement->WillChangeLength(mAttrEnum);
  mBaseVal = valueInSpecifiedUnits;
  mIsBaseSet = true;
  mSpecifiedUnitType = PRUint8(unitType);
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
  *aResult = sBaseSVGLengthTearoffTable.GetTearoff(this);
  if (!*aResult) {
    *aResult = new DOMBaseVal(this, aSVGElement);
    if (!*aResult)
      return NS_ERROR_OUT_OF_MEMORY;
    sBaseSVGLengthTearoffTable.AddTearoff(this, *aResult);
  }

  NS_ADDREF(*aResult);
  return NS_OK;
}

nsSVGLength2::DOMBaseVal::~DOMBaseVal()
{
  sBaseSVGLengthTearoffTable.RemoveTearoff(mVal);
}

nsresult
nsSVGLength2::ToDOMAnimVal(nsIDOMSVGLength **aResult, nsSVGElement *aSVGElement)
{
  *aResult = sAnimSVGLengthTearoffTable.GetTearoff(this);
  if (!*aResult) {
    *aResult = new DOMAnimVal(this, aSVGElement);
    if (!*aResult)
      return NS_ERROR_OUT_OF_MEMORY;
    sAnimSVGLengthTearoffTable.AddTearoff(this, *aResult);
  }

  NS_ADDREF(*aResult);
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
  PRUint16 unitType;

  nsresult rv = GetValueFromString(aValueAsString, &value, &unitType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mIsBaseSet && mBaseVal == value &&
      mSpecifiedUnitType == PRUint8(unitType)) {
    return NS_OK;
  }

  nsAttrValue emptyOrOldValue;
  if (aDoSetAttr) {
    emptyOrOldValue = aSVGElement->WillChangeLength(mAttrEnum);
  }
  mBaseVal = value;
  mIsBaseSet = true;
  mSpecifiedUnitType = PRUint8(unitType);
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

nsresult
nsSVGLength2::ToDOMAnimatedLength(nsIDOMSVGAnimatedLength **aResult,
                                  nsSVGElement *aSVGElement)
{
  *aResult = sSVGAnimatedLengthTearoffTable.GetTearoff(this);
  if (!*aResult) {
    *aResult = new DOMAnimatedLength(this, aSVGElement);
    if (!*aResult)
      return NS_ERROR_OUT_OF_MEMORY;
    sSVGAnimatedLengthTearoffTable.AddTearoff(this, *aResult);
  }

  NS_ADDREF(*aResult);
  return NS_OK;
}

nsSVGLength2::DOMAnimatedLength::~DOMAnimatedLength()
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
                                 const nsISMILAnimationElement* /*aSrcElement*/,
                                 nsSMILValue& aValue,
                                 bool& aPreventCachingOfSandwich) const
{
  float value;
  PRUint16 unitType;
  
  nsresult rv = GetValueFromString(aStr, &value, &unitType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsSMILValue val(&nsSMILFloatType::sSingleton);
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
  nsSMILValue val(&nsSMILFloatType::sSingleton);
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
  NS_ASSERTION(aValue.mType == &nsSMILFloatType::sSingleton,
    "Unexpected type to assign animated value");
  if (aValue.mType == &nsSMILFloatType::sSingleton) {
    mVal->SetAnimValue(float(aValue.mU.mDouble), mSVGElement);
  }
  return NS_OK;
}
