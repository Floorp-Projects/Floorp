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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsSVGAngle.h"
#include "prdtoa.h"
#include "nsTextFormatter.h"
#include "nsSVGUtils.h"

class DOMSVGAngle : public nsIDOMSVGAngle
{
public:
  NS_DECL_ISUPPORTS

  DOMSVGAngle()
    { mVal.Init(); }
    
  NS_IMETHOD GetUnitType(PRUint16* aResult)
    { *aResult = mVal.mSpecifiedUnitType; return NS_OK; }

  NS_IMETHOD GetValue(float* aResult)
    { *aResult = mVal.GetBaseValue(); return NS_OK; }
  NS_IMETHOD SetValue(float aValue)
    { mVal.SetBaseValue(aValue, nsnull); return NS_OK; }

  NS_IMETHOD GetValueInSpecifiedUnits(float* aResult)
    { *aResult = mVal.mBaseVal; return NS_OK; }
  NS_IMETHOD SetValueInSpecifiedUnits(float aValue)
    { mVal.mBaseVal = aValue; return NS_OK; }

  NS_IMETHOD SetValueAsString(const nsAString& aValue)
    { return mVal.SetBaseValueString(aValue, nsnull, PR_FALSE); }
  NS_IMETHOD GetValueAsString(nsAString& aValue)
    { mVal.GetBaseValueString(aValue); return NS_OK; }

  NS_IMETHOD NewValueSpecifiedUnits(PRUint16 unitType,
                                    float valueInSpecifiedUnits)
    { mVal.NewValueSpecifiedUnits(unitType, valueInSpecifiedUnits, nsnull);
      return NS_OK; }

  NS_IMETHOD ConvertToSpecifiedUnits(PRUint16 unitType)
    { mVal.ConvertToSpecifiedUnits(unitType, nsnull); return NS_OK; }

private:
  nsSVGAngle mVal;
};

NS_IMPL_ADDREF(nsSVGAngle::DOMBaseVal)
NS_IMPL_RELEASE(nsSVGAngle::DOMBaseVal)

NS_IMPL_ADDREF(nsSVGAngle::DOMAnimVal)
NS_IMPL_RELEASE(nsSVGAngle::DOMAnimVal)

NS_IMPL_ADDREF(nsSVGAngle::DOMAnimatedAngle)
NS_IMPL_RELEASE(nsSVGAngle::DOMAnimatedAngle)

NS_IMPL_ADDREF(DOMSVGAngle)
NS_IMPL_RELEASE(DOMSVGAngle)

NS_INTERFACE_MAP_BEGIN(nsSVGAngle::DOMBaseVal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAngle)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAngle)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN(nsSVGAngle::DOMAnimVal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAngle)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAngle)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN(nsSVGAngle::DOMAnimatedAngle)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedAngle)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAnimatedAngle)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN(DOMSVGAngle)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAngle)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAngle)
NS_INTERFACE_MAP_END

static nsIAtom** const unitMap[] =
{
  nsnull, /* SVG_ANGLETYPE_UNKNOWN */
  nsnull, /* SVG_ANGLETYPE_UNSPECIFIED */
  &nsGkAtoms::deg,
  &nsGkAtoms::rad,
  &nsGkAtoms::grad
};

/* Helper functions */

static PRBool
IsValidUnitType(PRUint16 unit)
{
  if (unit > nsIDOMSVGAngle::SVG_ANGLETYPE_UNKNOWN &&
      unit <= nsIDOMSVGAngle::SVG_ANGLETYPE_GRAD)
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
    return nsIDOMSVGAngle::SVG_ANGLETYPE_UNSPECIFIED;
                   
  nsCOMPtr<nsIAtom> unitAtom = do_GetAtom(unitStr);

  for (int i = 0 ; i < NS_ARRAY_LENGTH(unitMap) ; i++) {
    if (unitMap[i] && *unitMap[i] == unitAtom) {
      return i;
    }
  }

  return nsIDOMSVGAngle::SVG_ANGLETYPE_UNKNOWN;
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
    return NS_ERROR_FAILURE;
  
  char *rest;
  *aValue = float(PR_strtod(str, &rest));
  if (rest != str) {
    *aUnitType = GetUnitTypeForString(rest);
    if (IsValidUnitType(*aUnitType)) {
      return NS_OK;
    }
  }
  
  return NS_ERROR_FAILURE;
}

float
nsSVGAngle::GetUnitScaleFactor() const
{
  switch (mSpecifiedUnitType) {
  case nsIDOMSVGAngle::SVG_ANGLETYPE_UNSPECIFIED:
  case nsIDOMSVGAngle::SVG_ANGLETYPE_DEG:
    return static_cast<float>(180.0 / M_PI);
  case nsIDOMSVGAngle::SVG_ANGLETYPE_RAD:
    return 1;
  case nsIDOMSVGAngle::SVG_ANGLETYPE_GRAD:
    return static_cast<float>(100.0 / M_PI);
  default:
    NS_NOTREACHED("Unknown unit type");
    return 0;
  }
}

void
nsSVGAngle::SetBaseValueInSpecifiedUnits(float aValue,
                                         nsSVGElement *aSVGElement)
{
  mBaseVal = aValue;
  aSVGElement->DidChangeAngle(mAttrEnum, PR_TRUE);
}

void
nsSVGAngle::ConvertToSpecifiedUnits(PRUint16 unitType,
                                    nsSVGElement *aSVGElement)
{
  if (!IsValidUnitType(unitType))
    return;

  float valueInUserUnits = mBaseVal / GetUnitScaleFactor();
  mSpecifiedUnitType = PRUint8(unitType);
  SetBaseValue(valueInUserUnits, aSVGElement);
}

void
nsSVGAngle::NewValueSpecifiedUnits(PRUint16 unitType,
                                   float valueInSpecifiedUnits,
                                   nsSVGElement *aSVGElement)
{
  if (!IsValidUnitType(unitType))
    return;

  mBaseVal = mAnimVal = valueInSpecifiedUnits;
  mSpecifiedUnitType = PRUint8(unitType);
  if (aSVGElement) {
    aSVGElement->DidChangeAngle(mAttrEnum, PR_TRUE);
  }
}

nsresult
nsSVGAngle::ToDOMBaseVal(nsIDOMSVGAngle **aResult, nsSVGElement *aSVGElement)
{
  *aResult = new DOMBaseVal(this, aSVGElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

nsresult
nsSVGAngle::ToDOMAnimVal(nsIDOMSVGAngle **aResult, nsSVGElement *aSVGElement)
{
  *aResult = new DOMAnimVal(this, aSVGElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

/* Implementation */

nsresult
nsSVGAngle::SetBaseValueString(const nsAString &aValueAsString,
                               nsSVGElement *aSVGElement,
                               PRBool aDoSetAttr)
{
  float value;
  PRUint16 unitType;
  
  nsresult rv = GetValueFromString(aValueAsString, &value, &unitType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mBaseVal = mAnimVal = value;
  mSpecifiedUnitType = PRUint8(unitType);
  if (aSVGElement) {
    aSVGElement->DidChangeAngle(mAttrEnum, aDoSetAttr);
  }

  return NS_OK;
}

void
nsSVGAngle::GetBaseValueString(nsAString & aValueAsString)
{
  GetValueString(aValueAsString, mBaseVal, mSpecifiedUnitType);
}

void
nsSVGAngle::GetAnimValueString(nsAString & aValueAsString)
{
  GetValueString(aValueAsString, mAnimVal, mSpecifiedUnitType);
}

void
nsSVGAngle::SetBaseValue(float aValue, nsSVGElement *aSVGElement)
{
  mAnimVal = mBaseVal = aValue * GetUnitScaleFactor();
  if (aSVGElement) {
    aSVGElement->DidChangeAngle(mAttrEnum, PR_TRUE);
  }
}

nsresult
nsSVGAngle::ToDOMAnimatedAngle(nsIDOMSVGAnimatedAngle **aResult,
                               nsSVGElement *aSVGElement)
{
  *aResult = new DOMAnimatedAngle(this, aSVGElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

nsresult
NS_NewDOMSVGAngle(nsIDOMSVGAngle** aResult)
{
  *aResult = new DOMSVGAngle;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
