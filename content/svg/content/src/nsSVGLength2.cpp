/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *   Jonathan Watt <jonathan.watt@strath.ac.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSVGLength2.h"
#include "prdtoa.h"
#include "nsTextFormatter.h"
#include "nsSVGSVGElement.h"
#include "nsIFrame.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGAttrTearoffTable.h"
#include "nsMathUtils.h"
#ifdef MOZ_SMIL
#include "nsSMILValue.h"
#include "nsSMILFloatType.h"
#endif // MOZ_SMIL

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

static PRBool
IsValidUnitType(PRUint16 unit)
{
  if (unit > nsIDOMSVGLength::SVG_LENGTHTYPE_UNKNOWN &&
      unit <= nsIDOMSVGLength::SVG_LENGTHTYPE_PC)
    return PR_TRUE;

  return PR_FALSE;
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

  for (PRUint32 i = 0 ; i < NS_ARRAY_LENGTH(unitMap) ; i++) {
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
                                           nsSVGElement *aSVGElement)
{
  mBaseVal = aValue;
  mIsBaseSet = PR_TRUE;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
#ifdef MOZ_SMIL
  else {
    aSVGElement->AnimationNeedsResample();
  }
#endif
  aSVGElement->DidChangeLength(mAttrEnum, PR_TRUE);
}

nsresult
nsSVGLength2::ConvertToSpecifiedUnits(PRUint16 unitType,
                                      nsSVGElement *aSVGElement)
{
  if (!IsValidUnitType(unitType))
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

  float valueInUserUnits = 
    mBaseVal / GetUnitScaleFactor(aSVGElement, mSpecifiedUnitType);
  mSpecifiedUnitType = PRUint8(unitType);
  SetBaseValue(valueInUserUnits, aSVGElement);

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

  mBaseVal = valueInSpecifiedUnits;
  mIsBaseSet = PR_TRUE;
  mSpecifiedUnitType = PRUint8(unitType);
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
#ifdef MOZ_SMIL
  else {
    aSVGElement->AnimationNeedsResample();
  }
#endif
  aSVGElement->DidChangeLength(mAttrEnum, PR_TRUE);
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
                                 PRBool aDoSetAttr)
{
  float value;
  PRUint16 unitType;
  
  nsresult rv = GetValueFromString(aValueAsString, &value, &unitType);
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  mBaseVal = value;
  mIsBaseSet = PR_TRUE;
  mSpecifiedUnitType = PRUint8(unitType);
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
#ifdef MOZ_SMIL
  else {
    aSVGElement->AnimationNeedsResample();
  }
#endif

  aSVGElement->DidChangeLength(mAttrEnum, aDoSetAttr);
  return NS_OK;
}

void
nsSVGLength2::GetBaseValueString(nsAString & aValueAsString)
{
  GetValueString(aValueAsString, mBaseVal, mSpecifiedUnitType);
}

void
nsSVGLength2::GetAnimValueString(nsAString & aValueAsString)
{
  GetValueString(aValueAsString, mAnimVal, mSpecifiedUnitType);
}

void
nsSVGLength2::SetBaseValue(float aValue, nsSVGElement *aSVGElement)
{
  SetBaseValueInSpecifiedUnits(aValue * GetUnitScaleFactor(aSVGElement,
                                                           mSpecifiedUnitType),
                               aSVGElement);
}

void
nsSVGLength2::SetAnimValueInSpecifiedUnits(float aValue,
                                           nsSVGElement* aSVGElement)
{
  mAnimVal = aValue;
  mIsAnimated = PR_TRUE;
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

#ifdef MOZ_SMIL
nsISMILAttr*
nsSVGLength2::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return new SMILLength(this, aSVGElement);
}

nsresult
nsSVGLength2::SMILLength::ValueFromString(const nsAString& aStr,
                                 const nsISMILAnimationElement* /*aSrcElement*/,
                                 nsSMILValue& aValue,
                                 PRBool& aPreventCachingOfSandwich) const
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
    mVal->SetAnimValueInSpecifiedUnits(mVal->mBaseVal, mSVGElement);
    mVal->mIsAnimated = PR_FALSE;
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
#endif // MOZ_SMIL
