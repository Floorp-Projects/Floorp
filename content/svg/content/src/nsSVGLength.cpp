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
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#include "nsSVGLength.h"
#include "prdtoa.h"
#include "nsIDOMSVGMatrix.h"
#include "nsSVGAtoms.h"
#include "nsSVGValue.h"
#include "nsReadableUtils.h"
#include "nsTextFormatter.h"
#include "nsCRT.h"
#include "nsSVGCoordCtx.h"
#include "nsIDOMSVGNumber.h"
#include "nsISVGValueUtils.h"
#include "nsWeakReference.h"

////////////////////////////////////////////////////////////////////////
// nsSVGLength class

class nsSVGLength : public nsISVGLength,
                    public nsSVGValue,
                    public nsISVGValueObserver,
                    public nsSupportsWeakReference
{
protected:
  friend nsresult NS_NewSVGLength(nsISVGLength** result,
                                  float value,
                                  PRUint16 unit);

  friend nsresult NS_NewSVGLength(nsISVGLength** result,
                                  const nsAString &value);
  
  nsSVGLength(float value, PRUint16 unit);
  nsSVGLength();
  virtual ~nsSVGLength();

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGLength interface:
  NS_DECL_NSIDOMSVGLENGTH

  // nsISVGLength interface:
  NS_IMETHOD SetContext(nsSVGCoordCtx* context);
  
  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
protected:
  // implementation helpers:
  float UserUnitsPerPixel();
  float mmPerPixel();
  float AxisLength();
  void  GetUnitString(nsAString& unit);
  PRUint16 GetUnitTypeForString(const char* unitStr);
  PRBool IsValidUnitType(PRUint16 unit);

  void MaybeAddAsObserver();
  void MaybeRemoveAsObserver();
  
  float mValueInSpecifiedUnits;
  PRUint16 mSpecifiedUnitType;
  nsRefPtr<nsSVGCoordCtx> mContext;
};


//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGLength(nsISVGLength** result,
                float value,
                PRUint16 unit)
{
  nsSVGLength *pl = new nsSVGLength(value, unit);
  NS_ENSURE_TRUE(pl, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(pl);
  *result = pl;
  return NS_OK;
}

nsresult
NS_NewSVGLength(nsISVGLength** result,
                const nsAString &value)
{
  *result = nsnull;
  nsSVGLength *pl = new nsSVGLength();
  NS_ENSURE_TRUE(pl, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(pl);
  if (NS_FAILED(pl->SetValueAsString(value))) {
    NS_RELEASE(pl);
    return NS_ERROR_FAILURE;
  }
  *result = pl;
  return NS_OK;
}  


nsSVGLength::nsSVGLength(float value,
                         PRUint16 unit)
    : mValueInSpecifiedUnits(value),
      mSpecifiedUnitType(unit)
{
  MaybeAddAsObserver();
}

nsSVGLength::nsSVGLength()
{
}

nsSVGLength::~nsSVGLength()
{
  MaybeRemoveAsObserver();
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGLength)
NS_IMPL_RELEASE(nsSVGLength)

NS_INTERFACE_MAP_BEGIN(nsSVGLength)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsISVGLength)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGLength)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
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
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGLength::WillModifySVGObservable(nsISVGValue* observable)
{
  WillModify();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGLength::DidModifySVGObservable(nsISVGValue* observable)
{
  DidModify();
  return NS_OK;
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
      *aValue = mValueInSpecifiedUnits * AxisLength() / 100.0f;
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
      mValueInSpecifiedUnits = aValue * 100.0f / AxisLength();
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
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
                            NS_LITERAL_STRING("%g").get(),
                            (double)mValueInSpecifiedUnits);
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
  
  char *str = ToNewCString(aValueAsString);

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
        rv = NS_ERROR_FAILURE;
        NS_ERROR("invalid length type");
      }
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
nsSVGLength::NewValueSpecifiedUnits(PRUint16 unitType, float valueInSpecifiedUnits)
{
  if (!IsValidUnitType(unitType)) return NS_ERROR_FAILURE;

  bool observer_change = (unitType != mSpecifiedUnitType);
  
  WillModify();
  if (observer_change)
    MaybeRemoveAsObserver();
  mValueInSpecifiedUnits = valueInSpecifiedUnits;
  mSpecifiedUnitType     = unitType;
  if (observer_change)
    MaybeAddAsObserver();
  DidModify();
  
  return NS_OK;
}

/* void convertToSpecifiedUnits (in unsigned short unitType); */
NS_IMETHODIMP
nsSVGLength::ConvertToSpecifiedUnits(PRUint16 unitType)
{
  if (!IsValidUnitType(unitType)) return NS_ERROR_FAILURE;

  bool observer_change = (unitType != mSpecifiedUnitType);

  WillModify();
  if (observer_change)
    MaybeRemoveAsObserver();
  float valueInUserUnits;
  GetValue(&valueInUserUnits);
  mSpecifiedUnitType = unitType;
  SetValue(valueInUserUnits);
  if (observer_change)
    MaybeAddAsObserver();
  DidModify();
  
  return NS_OK;
}

/* float getTransformedValue (in nsIDOMSVGMatrix matrix); */
NS_IMETHODIMP
nsSVGLength::GetTransformedValue(nsIDOMSVGMatrix *matrix,
                                       float *_retval)
{

// XXX we don't have enough information here. is it the length part of
// a coordinate pair (in which case it should transform like a point)
// or is it used like a vector-component (in which case it doesn't
// translate)

  
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}


//----------------------------------------------------------------------
// nsISVGLength methods:
NS_IMETHODIMP
nsSVGLength::SetContext(nsSVGCoordCtx* context)
{
  // XXX should we bracket this inbetween WillModify/DidModify pairs?
  MaybeRemoveAsObserver();
  WillModify();
  mContext = context;
  DidModify();
  MaybeAddAsObserver();
  return NS_OK;
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
  if (!mContext) {
    NS_ERROR("no context in mmPerPixel()");
    return 1.0f;
  }
  
  float mmPerPx = mContext->GetMillimeterPerPixel();
  
  if (mmPerPx == 0.0f) {
    NS_ASSERTION(PR_FALSE, "invalid mm/pixels");
    mmPerPx = 1e-4f; // some small value
  }
  
  return mmPerPx;
}

float nsSVGLength::AxisLength()
{
  if (!mContext) {
    NS_ERROR("no context in AxisLength()");
    return 1.0f;
  }

  nsCOMPtr<nsIDOMSVGNumber> num = mContext->GetLength();
  NS_ASSERTION(num, "null interface");
  float d;
  num->GetValue(&d);
  
  NS_ASSERTION(d!=0.0f, "zero axis length");
  
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
  if (unit>0 && unit<=10)
    return PR_TRUE;

  return PR_FALSE;
}

void nsSVGLength::MaybeAddAsObserver()
{
  if ((mSpecifiedUnitType==SVG_LENGTHTYPE_PERCENTAGE) &&
      mContext) {
    nsCOMPtr<nsIDOMSVGNumber> num = mContext->GetLength();
    NS_ADD_SVGVALUE_OBSERVER(num);
  }
}

void nsSVGLength::MaybeRemoveAsObserver()
{
  if ((mSpecifiedUnitType==SVG_LENGTHTYPE_PERCENTAGE) &&
      mContext) {
    nsCOMPtr<nsIDOMSVGNumber> num = mContext->GetLength();
    NS_REMOVE_SVGVALUE_OBSERVER(num);
  }
}

