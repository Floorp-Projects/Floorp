/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is 
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsSVGLength.h"
#include "prdtoa.h"
#include "nsIDOMSVGLocatable.h"
#include "nsIDOMSVGMatrix.h"
#include "nsSVGPoint.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsSVGAtoms.h"
#include "nsIDOMSVGRect.h"
#include "nsSVGValue.h"
#include "nsIWeakReference.h"
#include "nsReadableUtils.h"
#include "nsTextFormatter.h"
#include "nsCRT.h"
#include <math.h>

////////////////////////////////////////////////////////////////////////
// nsSVGLength class

class nsSVGLength : public nsIDOMSVGLength,
                    public nsSVGValue
{
public:
  static nsresult Create(nsIDOMSVGLength** aResult,
                         nsIDOMSVGElement* owner,
                         float value, PRUint16 unit,
                         nsSVGLengthDirection dir);
  
protected:
  nsSVGLength(float value, PRUint16 unit,
                    nsSVGLengthDirection dir);
  nsresult Init(nsIDOMSVGElement* owner);
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGLength interface:
  NS_DECL_NSIDOMSVGLENGTH

  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  
protected:
  // implementation helpers:
  float UserUnitsPerPixel();
  float mmPerPixel();
  float ViewportDimension();
  void  GetUnitString(nsAString& unit);
  PRUint16 GetUnitTypeForString(const char* unitStr);
  PRBool IsValidUnitType(PRUint16 unit);
  
  float mValueInSpecifiedUnits;
  PRUint16 mSpecifiedUnitType;
  nsCOMPtr<nsIWeakReference> mOwnerElementRef;
  nsSVGLengthDirection mDirection;
};


//----------------------------------------------------------------------
// Implementation

nsresult
nsSVGLength::Create(nsIDOMSVGLength** aResult,
                          nsIDOMSVGElement* owner,
                          float value, PRUint16 unit,
                          nsSVGLengthDirection dir)
{
  nsSVGLength *pl = new nsSVGLength(value, unit, dir);
  NS_ENSURE_TRUE(pl, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(pl);
  if (NS_FAILED(pl->Init(owner))) {
    NS_RELEASE(pl);
    return NS_ERROR_FAILURE;
  }
  *aResult = pl;
  return NS_OK;
}


nsSVGLength::nsSVGLength(float value,
                                     PRUint16 unit,
                                     nsSVGLengthDirection dir)
    : mValueInSpecifiedUnits(value),
      mSpecifiedUnitType(unit),
      mDirection(dir)
{
}

nsresult nsSVGLength::Init(nsIDOMSVGElement* owner)
{
  NS_ASSERTION(owner, "need owner");
  mOwnerElementRef = NS_GetWeakReference(owner);
  NS_ENSURE_TRUE(mOwnerElementRef, NS_ERROR_FAILURE);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGLength)
NS_IMPL_RELEASE(nsSVGLength)

NS_INTERFACE_MAP_BEGIN(nsSVGLength)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGLength)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGLength)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGLength::SetValueString(const nsAString& aValue)
{
  return SetValueAsString(aValue);
}

NS_IMETHODIMP
nsSVGLength::GetValueString(nsAString& aValue)
{
  return GetValueAsString(aValue);
}

//----------------------------------------------------------------------
// nsISVGLength methods:

/* readonly attribute unsigned short unitType; */
NS_IMETHODIMP
nsSVGLength::GetUnitType(PRUint16 *aUnitType)
{
  *aUnitType = mSpecifiedUnitType;
  return NS_OK;
}

/* attribute float value; */
NS_IMETHODIMP
nsSVGLength::GetValue(float *aValue)
{
  nsresult rv = NS_OK;
  
  switch (mSpecifiedUnitType) {
    case SVG_LENGTHTYPE_NUMBER:
      *aValue = mValueInSpecifiedUnits;
      break;
    case SVG_LENGTHTYPE_PX:
      *aValue = mValueInSpecifiedUnits * UserUnitsPerPixel();
      break;
    case SVG_LENGTHTYPE_MM:
      *aValue = mValueInSpecifiedUnits / mmPerPixel() * UserUnitsPerPixel();
      break;
    case SVG_LENGTHTYPE_CM:
      *aValue = mValueInSpecifiedUnits * 10.0f / mmPerPixel() * UserUnitsPerPixel();
      break;
    case SVG_LENGTHTYPE_IN:
      *aValue = mValueInSpecifiedUnits * 25.4f / mmPerPixel() * UserUnitsPerPixel();
      break;
    case SVG_LENGTHTYPE_PT:
      *aValue = mValueInSpecifiedUnits * 25.4f/72.0f / mmPerPixel() * UserUnitsPerPixel();
      break;
    case SVG_LENGTHTYPE_PC:
      *aValue = mValueInSpecifiedUnits * 25.4f*12.0f/72.0f / mmPerPixel() * UserUnitsPerPixel();
      break;
    case SVG_LENGTHTYPE_PERCENTAGE:
      *aValue = mValueInSpecifiedUnits * ViewportDimension() / 100.0f;
      break;
    case SVG_LENGTHTYPE_EMS:
    case SVG_LENGTHTYPE_EXS:
      //XXX
      NS_ASSERTION(PR_FALSE, "unit not implemented yet");
    default:
      rv = NS_ERROR_FAILURE;
      break;
  }
  
  return rv;
}

NS_IMETHODIMP
nsSVGLength::SetValue(float aValue)
{
  nsresult rv = NS_OK;
  
  WillModify();

  switch (mSpecifiedUnitType) {
    case SVG_LENGTHTYPE_NUMBER:
      mValueInSpecifiedUnits = aValue;
      break;
    case SVG_LENGTHTYPE_PX:
      mValueInSpecifiedUnits = aValue / UserUnitsPerPixel();
      break;
    case SVG_LENGTHTYPE_MM:
      mValueInSpecifiedUnits = aValue / UserUnitsPerPixel() * mmPerPixel();
      break;
    case SVG_LENGTHTYPE_CM:
      mValueInSpecifiedUnits = aValue / UserUnitsPerPixel() * mmPerPixel() / 10.0f;
      break;
    case SVG_LENGTHTYPE_IN:
      mValueInSpecifiedUnits = aValue / UserUnitsPerPixel() * mmPerPixel() / 25.4f;
      break;
    case SVG_LENGTHTYPE_PT:
      mValueInSpecifiedUnits = aValue / UserUnitsPerPixel() * mmPerPixel() * 72.0f/25.4f;
      break;
    case SVG_LENGTHTYPE_PC:
      mValueInSpecifiedUnits = aValue / UserUnitsPerPixel() * mmPerPixel() * 72.0f/24.4f/12.0f;
      break;
    case SVG_LENGTHTYPE_PERCENTAGE:
      mValueInSpecifiedUnits = aValue * 100.0f / ViewportDimension();
      break;
    case SVG_LENGTHTYPE_EMS:
    case SVG_LENGTHTYPE_EXS:
      //XXX
      NS_ASSERTION(PR_FALSE, "unit not implemented yet");
    default:
      rv = NS_ERROR_FAILURE;
      break;
  }
  
  DidModify();
  return rv;
}

/* attribute float valueInSpecifiedUnits; */
NS_IMETHODIMP
nsSVGLength::GetValueInSpecifiedUnits(float *aValueInSpecifiedUnits)
{
  *aValueInSpecifiedUnits = mValueInSpecifiedUnits;
  return NS_OK;
}
NS_IMETHODIMP
nsSVGLength::SetValueInSpecifiedUnits(float aValueInSpecifiedUnits)
{
  WillModify();
  mValueInSpecifiedUnits = aValueInSpecifiedUnits;
  DidModify();
  return NS_OK;
}

/* attribute DOMString valueAsString; */
NS_IMETHODIMP
nsSVGLength::GetValueAsString(nsAString & aValueAsString)
{
  aValueAsString.Truncate();

  PRUnichar buf[24];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("%g").get(), (double)mValueInSpecifiedUnits);
  aValueAsString.Append(buf);
  
  nsAutoString unitString;
  GetUnitString(unitString);
  aValueAsString.Append(unitString);
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGLength::SetValueAsString(const nsAString & aValueAsString)
{
  nsresult rv = NS_OK;
  
  // XXX how am I supposed to do this ??? 
  // char* str  = aValue.ToNewCString();
  char* str;
  {
    nsAutoString temp(aValueAsString);
    str = ToNewCString(temp);
  }

  char* number = str;
  while (*number && isspace(*number))
    ++number;

  if (*number) {
    char *rest;
    double value = PR_strtod(number, &rest);
    if (rest!=number) {
      PRUint16 unitType = GetUnitTypeForString(nsCRT::strtok(rest, "\x20\x9\xD\xA", &rest));
      if (IsValidUnitType(unitType)){
        WillModify();
        mValueInSpecifiedUnits = (float)value;
        mSpecifiedUnitType     = unitType;
        DidModify();
      }
      else { // parse error
        // not a valid unit type
        // rv = ???
      }
    }
    else { // parse error
    // no number
    // rv = NS_ERROR_???;
    }
  }
  
  nsMemory::Free(str);
    
  return rv;
}

/* void newValueSpecifiedUnits (in unsigned short unitType, in float valueInSpecifiedUnits); */
 NS_IMETHODIMP
nsSVGLength::NewValueSpecifiedUnits(PRUint16 unitType, float valueInSpecifiedUnits)
{
  if (!IsValidUnitType(unitType)) return NS_ERROR_FAILURE;

  WillModify();
  mValueInSpecifiedUnits = valueInSpecifiedUnits;
  mSpecifiedUnitType     = unitType;
  DidModify();
  
  return NS_OK;
}

/* void convertToSpecifiedUnits (in unsigned short unitType); */
NS_IMETHODIMP
nsSVGLength::ConvertToSpecifiedUnits(PRUint16 unitType)
{
  if (!IsValidUnitType(unitType)) return NS_ERROR_FAILURE;

  WillModify();
  
  float valueInUserUnits;
  GetValue(&valueInUserUnits);
  mSpecifiedUnitType = unitType;
  SetValue(valueInUserUnits);
  
  DidModify();
  
  return NS_OK;
}

/* float getTransformedValue (in nsIDOMSVGMatrix matrix); */
NS_IMETHODIMP
nsSVGLength::GetTransformedValue(nsIDOMSVGMatrix *matrix,
                                       float *_retval)
{

// XXX we don't have enough information here. is the length part of a
// coordinate pair (in which case it should transform like a point) or
// is it used like a vector-component (in which case it doesn't
// translate)

  
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}


//----------------------------------------------------------------------
// Implementation helpers:

float nsSVGLength::UserUnitsPerPixel()
{
  float UUPerPx = 1.0f;
  
  // SVG CR 20001102: New: A px unit and a user unit are defined to be
  // equivalent in SVG. Thus, a length of "5px" is the same as a
  // length of "5".
  //
  
  // old way of mapping pixels:
//   if (!mOwnerElement) return UUPerPx;
  
//   nsCOMPtr<nsIDOMSVGLocatable> locatable;
//   locatable = do_QueryInterface(mOwnerElement);
//   if (!locatable) return UUPerPx;
  
//   nsCOMPtr<nsIDOMSVGMatrix> matrix;
//   locatable->GetCTM( getter_AddRefs(matrix) );
//   if (!matrix) return UUPerPx;
  
//   nsCOMPtr<nsIDOMSVGPoint> point, XFormedPoint;
//   nsSVGPoint::Create(1.0, 1.0, getter_AddRefs(point));
//   point->MatrixTransform(matrix, getter_AddRefs(XFormedPoint));
  
//   switch (mDirection) {
//     case eXDirection:
//       XFormedPoint->GetX(&UUPerPx);
//       break;
//     case eYDirection:
//       XFormedPoint->GetY(&UUPerPx);
//       break;
//     case eNoDirection:
//     {
//       float x,y;
//       XFormedPoint->GetX(&x);
//       XFormedPoint->GetY(&y);
//       UUPerPx = (x==y ? x : (x+y)/2);
//       break;
//     }
//   }      

//   if (UUPerPx == 0.0f) {
//     NS_ASSERTION(PR_FALSE, "invalid uu/pixels");
//     UUPerPx = 1e-20f; // some small value
//   }
  
  return UUPerPx;
}

float nsSVGLength::mmPerPixel()
{
  float mmPerPx = 0.28f; // 90dpi by default

  if (!mOwnerElementRef) return mmPerPx;
  nsCOMPtr<nsIDOMSVGElement> ownerElement = do_QueryReferent(mOwnerElementRef);
  if (!ownerElement) return mmPerPx;

  nsCOMPtr<nsIDOMSVGSVGElement> SVGElement;
  ownerElement->GetOwnerSVGElement(getter_AddRefs(SVGElement));
  if (!SVGElement) { // maybe our owner is the svg element...
      SVGElement = do_QueryInterface(ownerElement);
  }
  
  if (!SVGElement) return mmPerPx;
  
  switch (mDirection) {
    case eXDirection:
      SVGElement->GetPixelUnitToMillimeterX(&mmPerPx);
      break;
    case eYDirection:
      SVGElement->GetPixelUnitToMillimeterY(&mmPerPx);
      break;
    case eNoDirection:
    {
      float x,y;
      SVGElement->GetPixelUnitToMillimeterX(&x);
      SVGElement->GetPixelUnitToMillimeterY(&y);
      mmPerPx = (x==y ? x : (x+y)/2);
      break;
    }
  }

  if (mmPerPx == 0.0f) {
    NS_ASSERTION(PR_FALSE, "invalid mm/pixels");
    mmPerPx = 1e-20f; // some small value
  }
  
  return mmPerPx;
}

float nsSVGLength::ViewportDimension()
{
  float d = 1e-20f; 
  
  NS_ASSERTION(mOwnerElementRef, "need owner");
  if (!mOwnerElementRef) return d;
  nsCOMPtr<nsIDOMSVGElement> ownerElement = do_QueryReferent(mOwnerElementRef);
  NS_ASSERTION(ownerElement, "need owner");
  if (!ownerElement) return d;
  
  // find element that establishes the current viewport:
  nsCOMPtr<nsIDOMSVGElement> vpElement;
  ownerElement->GetViewportElement(getter_AddRefs(vpElement));
  if (!vpElement) { // maybe our owner is the outermost svg element...
    vpElement = ownerElement;
  }

  // only 'svg' elements establish explicit viewports ? XXX
  nsCOMPtr<nsIDOMSVGSVGElement> SVGElement = do_QueryInterface(vpElement);
  NS_ASSERTION(SVGElement, "need svg element to obtain vieport");
  if (!SVGElement) return d;
  
  nsCOMPtr<nsIDOMSVGRect> vp;
  SVGElement->GetViewport(getter_AddRefs(vp));
  if (!vp) return d;

  switch (mDirection) {
    case eXDirection:
      vp->GetWidth(&d);
      break;
    case eYDirection:
      vp->GetHeight(&d);
      break;
    case eNoDirection:
    {
      float x,y;
      vp->GetWidth(&x);
      vp->GetHeight(&y);
      d = (float) sqrt(x*x+y*y);
      break;
    }
  }

  NS_ASSERTION(d!=0.0f, "zero viewport w/h?");
  
  if (d == 0.0f)
    d = 1e-20f;
  return d;
}

void nsSVGLength::GetUnitString(nsAString& unit)
{
  nsIAtom* UnitAtom = nsnull;
  
  switch (mSpecifiedUnitType) {
    case SVG_LENGTHTYPE_NUMBER:
      UnitAtom = nsnull;
      break;
    case SVG_LENGTHTYPE_PX:
      UnitAtom = nsSVGAtoms::px;
      break;
    case SVG_LENGTHTYPE_MM:
      UnitAtom = nsSVGAtoms::mm;
      break;
    case SVG_LENGTHTYPE_CM:
      UnitAtom = nsSVGAtoms::cm;
      break;
    case SVG_LENGTHTYPE_IN:
      UnitAtom = nsSVGAtoms::in;
      break;
    case SVG_LENGTHTYPE_PT:
      UnitAtom = nsSVGAtoms::pt;
      break;
    case SVG_LENGTHTYPE_PC:
      UnitAtom = nsSVGAtoms::pc;
      break;
    case SVG_LENGTHTYPE_EMS:
      UnitAtom = nsSVGAtoms::ems;
      break;
    case SVG_LENGTHTYPE_EXS:
      UnitAtom = nsSVGAtoms::exs;
      break;
    case SVG_LENGTHTYPE_PERCENTAGE:
      UnitAtom = nsSVGAtoms::percentage;
      break;
    default:
      NS_ASSERTION(PR_FALSE, "unknown unit");
      break;
  }
  if (!UnitAtom) return;

  UnitAtom->ToString(unit);
}

PRUint16 nsSVGLength::GetUnitTypeForString(const char* unitStr)
{
  if (!unitStr || *unitStr=='\0') return SVG_LENGTHTYPE_NUMBER;
                   
  nsCOMPtr<nsIAtom> unitAtom = do_GetAtom(unitStr);

  if (unitAtom == nsSVGAtoms::px)
    return SVG_LENGTHTYPE_PX;
  else if (unitAtom == nsSVGAtoms::mm)
    return SVG_LENGTHTYPE_MM;
  else if (unitAtom == nsSVGAtoms::cm)
    return SVG_LENGTHTYPE_CM;
  else if (unitAtom == nsSVGAtoms::in)
    return SVG_LENGTHTYPE_IN;
  else if (unitAtom == nsSVGAtoms::pt)
    return SVG_LENGTHTYPE_PT;
  else if (unitAtom == nsSVGAtoms::pc)
    return SVG_LENGTHTYPE_PC;
  else if (unitAtom == nsSVGAtoms::ems)
    return SVG_LENGTHTYPE_EMS;
  else if (unitAtom == nsSVGAtoms::exs)
    return SVG_LENGTHTYPE_EXS;
  else if (unitAtom == nsSVGAtoms::percentage)
    return SVG_LENGTHTYPE_PERCENTAGE;

  return SVG_LENGTHTYPE_UNKNOWN;
}

PRBool nsSVGLength::IsValidUnitType(PRUint16 unit)
{
  if (unit>0 && unit<10)
    return PR_TRUE;

  return PR_FALSE;
}

////////////////////////////////////////////////////////////////////////
// Exported creation functions:

nsresult
NS_NewSVGLength(nsIDOMSVGLength** result,
                nsIDOMSVGElement* owner,
                nsSVGLengthDirection dir,
                float value,
                PRUint16 unit)
{
  return nsSVGLength::Create(result, owner, value, unit, dir);
}
