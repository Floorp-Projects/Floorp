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
#include "nsGkAtoms.h"
#include "nsSVGValue.h"
#include "nsReadableUtils.h"
#include "nsTextFormatter.h"
#include "nsCRT.h"
#include "nsIDOMSVGNumber.h"
#include "nsISVGValueUtils.h"
#include "nsWeakReference.h"
#include "nsContentUtils.h"
#include "nsSVGUtils.h"
#include <math.h>

////////////////////////////////////////////////////////////////////////
// nsSVGAngle class

class nsSVGAngle : public nsIDOMSVGAngle,
                   public nsSVGValue,
                   public nsISVGValueObserver
{
protected:
  friend nsresult NS_NewSVGAngle(nsIDOMSVGAngle** result,
                                 float value,
                                 PRUint16 unit);

  friend nsresult NS_NewSVGAngle(nsIDOMSVGAngle** result,
                                 const nsAString &value);
  
  nsSVGAngle(float value, PRUint16 unit);
  nsSVGAngle();
  virtual ~nsSVGAngle();

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGAngle interface:
  NS_DECL_NSIDOMSVGANGLE

  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     modificationType aModType);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
protected:
  // implementation helpers:
  void  GetUnitString(nsAString& unit);
  PRUint16 GetUnitTypeForString(const char* unitStr);
  PRBool IsValidUnitType(PRUint16 unit);

  float mValueInSpecifiedUnits;
  PRUint8 mSpecifiedUnitType;
  PRPackedBool mIsAuto;
};


//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGAngle(nsIDOMSVGAngle** result,
               float value,
               PRUint16 unit)
{
  nsSVGAngle *pl = new nsSVGAngle(value, unit);
  NS_ENSURE_TRUE(pl, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(pl);
  *result = pl;
  return NS_OK;
}

nsresult
NS_NewSVGAngle(nsIDOMSVGAngle** result,
               const nsAString &value)
{
  *result = nsnull;
  nsSVGAngle *pl = new nsSVGAngle();
  NS_ENSURE_TRUE(pl, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(pl);
  if (NS_FAILED(pl->SetValueAsString(value))) {
    NS_RELEASE(pl);
    return NS_ERROR_FAILURE;
  }
  *result = pl;
  return NS_OK;
}  


nsSVGAngle::nsSVGAngle(float value,
                       PRUint16 unit)
  : mValueInSpecifiedUnits(value),
    mIsAuto(PR_FALSE)
{
  NS_ASSERTION(unit == SVG_ANGLETYPE_UNKNOWN || IsValidUnitType(unit), "unknown unit");
  mSpecifiedUnitType = unit;
}

nsSVGAngle::nsSVGAngle()
{
}

nsSVGAngle::~nsSVGAngle()
{
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGAngle)
NS_IMPL_RELEASE(nsSVGAngle)

NS_INTERFACE_MAP_BEGIN(nsSVGAngle)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAngle)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAngle)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGAngle::SetValueString(const nsAString& aValue)
{
  return SetValueAsString(aValue);
}

NS_IMETHODIMP
nsSVGAngle::GetValueString(nsAString& aValue)
{
  return GetValueAsString(aValue);
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGAngle::WillModifySVGObservable(nsISVGValue* observable,
                                    modificationType aModType)
{
  WillModify(aModType);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAngle::DidModifySVGObservable(nsISVGValue* observable,
                                   modificationType aModType)
{
  DidModify(aModType);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGAngle methods:

/* readonly attribute unsigned short unitType; */
NS_IMETHODIMP
nsSVGAngle::GetUnitType(PRUint16 *aUnitType)
{
  *aUnitType = mSpecifiedUnitType;
  return NS_OK;
}

/* attribute float value; */
NS_IMETHODIMP
nsSVGAngle::GetValue(float *aValue)
{
  nsresult rv = NS_OK;
  
  switch (mSpecifiedUnitType) {
  case SVG_ANGLETYPE_UNSPECIFIED:
  case SVG_ANGLETYPE_DEG:
    *aValue = float((mValueInSpecifiedUnits * M_PI) / 180.0);
    break;
  case SVG_ANGLETYPE_RAD:
    *aValue = mValueInSpecifiedUnits;
    break;
  case SVG_ANGLETYPE_GRAD:
    *aValue = float((mValueInSpecifiedUnits * M_PI) / 100.0);
    break;
  default:
    rv = NS_ERROR_FAILURE;
    break;
  }
  return rv;
}

NS_IMETHODIMP
nsSVGAngle::SetValue(float aValue)
{
  nsresult rv;
  
  switch (mSpecifiedUnitType) {
  case SVG_ANGLETYPE_UNSPECIFIED:
  case SVG_ANGLETYPE_DEG:
    rv = SetValueInSpecifiedUnits(float((aValue * 180.0) / M_PI));
    break;
  case SVG_ANGLETYPE_RAD:
    rv = SetValueInSpecifiedUnits(aValue);
    break;
  case SVG_ANGLETYPE_GRAD:
    rv = SetValueInSpecifiedUnits(float((aValue * 100.0) / M_PI));
    break;
  default:
    rv = NS_ERROR_FAILURE;
    break;
  }

  return rv;
}

/* attribute float valueInSpecifiedUnits; */
NS_IMETHODIMP
nsSVGAngle::GetValueInSpecifiedUnits(float *aValueInSpecifiedUnits)
{
  *aValueInSpecifiedUnits = mValueInSpecifiedUnits;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAngle::SetValueInSpecifiedUnits(float aValueInSpecifiedUnits)
{
  WillModify();
  mIsAuto                = PR_FALSE;
  mValueInSpecifiedUnits = aValueInSpecifiedUnits;
  DidModify();
  return NS_OK;
}

/* attribute DOMString valueAsString; */
NS_IMETHODIMP
nsSVGAngle::GetValueAsString(nsAString & aValueAsString)
{
  if (mIsAuto) {
    aValueAsString.AssignLiteral("auto");
    return NS_OK;
  }
  PRUnichar buf[24];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
                            NS_LITERAL_STRING("%g").get(),
                            (double)mValueInSpecifiedUnits);
  aValueAsString.Assign(buf);
  
  nsAutoString unitString;
  GetUnitString(unitString);
  aValueAsString.Append(unitString);
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAngle::SetValueAsString(const nsAString & aValueAsString)
{
  if (aValueAsString.EqualsLiteral("auto")) {
    WillModify();
    mIsAuto = PR_TRUE;
    DidModify();
    return NS_OK;
  }
  nsresult rv = NS_OK;
  
  char *str = ToNewCString(aValueAsString);

  char* number = str;
  while (*number && isspace(*number))
    ++number;

  if (*number) {
    char *rest;
    double value = PR_strtod(number, &rest);
    if (rest!=number) {
      PRUint16 unitType = GetUnitTypeForString(nsCRT::strtok(rest, "\x20\x9\xD\xA", &rest));
      rv = NewValueSpecifiedUnits(unitType, (float)value);
    }
    else { // parse error
      // no number
      rv = NS_ERROR_FAILURE;
    }
  }
  
  nsMemory::Free(str);
    
  return rv;
}

/* void newValueSpecifiedUnits (in unsigned short unitType, in float valueInSpecifiedUnits); */
NS_IMETHODIMP
nsSVGAngle::NewValueSpecifiedUnits(PRUint16 unitType, float valueInSpecifiedUnits)
{
  if (!IsValidUnitType(unitType)) return NS_ERROR_FAILURE;

  WillModify();
  mIsAuto                = PR_FALSE;
  mValueInSpecifiedUnits = valueInSpecifiedUnits;
  mSpecifiedUnitType     = unitType;
  DidModify();
  
  return NS_OK;
}

/* void convertToSpecifiedUnits (in unsigned short unitType); */
NS_IMETHODIMP
nsSVGAngle::ConvertToSpecifiedUnits(PRUint16 unitType)
{
  if (!IsValidUnitType(unitType)) return NS_ERROR_FAILURE;

  float valueInUserUnits;
  GetValue(&valueInUserUnits);
  mSpecifiedUnitType = unitType;
  SetValue(valueInUserUnits);
  
  return NS_OK;
}


//----------------------------------------------------------------------
// Implementation helpers:


void nsSVGAngle::GetUnitString(nsAString& unit)
{
  nsIAtom* UnitAtom = nsnull;
  
  switch (mSpecifiedUnitType) {
  case SVG_ANGLETYPE_UNSPECIFIED:
    UnitAtom = nsnull;
    break;
  case SVG_ANGLETYPE_DEG:
    UnitAtom = nsGkAtoms::deg;
    break;
  case SVG_ANGLETYPE_GRAD:
    UnitAtom = nsGkAtoms::grad;
    break;
  case SVG_ANGLETYPE_RAD:
    UnitAtom = nsGkAtoms::rad;
    break;
  default:
    NS_ASSERTION(PR_FALSE, "unknown unit");
    break;
  }
  if (!UnitAtom) return;

  UnitAtom->ToString(unit);
}

PRUint16 nsSVGAngle::GetUnitTypeForString(const char* unitStr)
{
  if (!unitStr || *unitStr=='\0') return SVG_ANGLETYPE_UNSPECIFIED;
                   
  nsCOMPtr<nsIAtom> unitAtom = do_GetAtom(unitStr);

  if (unitAtom == nsGkAtoms::deg)
    return SVG_ANGLETYPE_DEG;
  else if (unitAtom == nsGkAtoms::grad)
    return SVG_ANGLETYPE_GRAD;
  else if (unitAtom == nsGkAtoms::rad)
    return SVG_ANGLETYPE_RAD;

  return SVG_ANGLETYPE_UNKNOWN;
}

PRBool nsSVGAngle::IsValidUnitType(PRUint16 unit)
{
  if (unit > SVG_ANGLETYPE_UNKNOWN && unit <= SVG_ANGLETYPE_GRAD)
    return PR_TRUE;

  return PR_FALSE;
}
