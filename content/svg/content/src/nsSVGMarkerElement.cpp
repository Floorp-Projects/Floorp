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

#include "nsIDOMSVGFitToViewBox.h"
#include "nsSVGGraphicElement.h"
#include "nsSVGAtoms.h"
#include "nsSVGAnimatedLength.h"
#include "nsSVGAnimatedAngle.h"
#include "nsSVGAnimatedRect.h"
#include "nsSVGLength.h"
#include "nsSVGEnum.h"
#include "nsSVGAngle.h"
#include "nsSVGRect.h"
#include "nsSVGAnimatedEnumeration.h"
#include "nsIDOMSVGMarkerElement.h"
#include "nsCOMPtr.h"
#include "nsSVGCoordCtxProvider.h"
#include "nsISVGValueUtils.h"
#include "nsSVGAnimatedPreserveAspectRatio.h"
#include "nsSVGPreserveAspectRatio.h"
#include "nsSVGMatrix.h"
#include "nsDOMError.h"

typedef nsSVGGraphicElement nsSVGMarkerElementBase;

class nsSVGMarkerElement : public nsSVGMarkerElementBase,
                           public nsIDOMSVGMarkerElement,
                           public nsIDOMSVGFitToViewBox,
                           public nsSVGCoordCtxProvider
{
protected:
  friend nsresult NS_NewSVGMarkerElement(nsIContent **aResult,
                                         nsINodeInfo *aNodeInfo);
  nsSVGMarkerElement(nsINodeInfo* aNodeInfo);
  virtual ~nsSVGMarkerElement();
  nsresult Init();

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGMARKERELEMENT
  NS_DECL_NSIDOMSVGFITTOVIEWBOX

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGElement::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGElement::)

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);

protected:
  
  nsCOMPtr<nsIDOMSVGAnimatedLength>      mRefX;
  nsCOMPtr<nsIDOMSVGAnimatedLength>      mRefY;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mMarkerUnits;
  nsCOMPtr<nsIDOMSVGAnimatedLength>      mMarkerWidth;
  nsCOMPtr<nsIDOMSVGAnimatedLength>      mMarkerHeight;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mOrientType;
  nsCOMPtr<nsIDOMSVGAnimatedAngle>       mOrientAngle;

  nsCOMPtr<nsIDOMSVGAnimatedRect>        mViewBox;
  nsCOMPtr<nsIDOMSVGAnimatedPreserveAspectRatio> mPreserveAspectRatio;
  nsCOMPtr<nsIDOMSVGMatrix>         mViewBoxToViewportTransform;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Marker)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGMarkerElement,nsSVGElement)
NS_IMPL_RELEASE_INHERITED(nsSVGMarkerElement,nsSVGElement)

NS_INTERFACE_MAP_BEGIN(nsSVGMarkerElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFitToViewBox)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGMarkerElement)
  NS_INTERFACE_MAP_ENTRY(nsSVGCoordCtxProvider)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGMarkerElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGElement)

//----------------------------------------------------------------------
// Implementation

  nsSVGMarkerElement::nsSVGMarkerElement(nsINodeInfo *aNodeInfo)
    : nsSVGMarkerElementBase(aNodeInfo)
{
}

nsSVGMarkerElement::~nsSVGMarkerElement()
{
  if (mPreserveAspectRatio) {
    NS_REMOVE_SVGVALUE_OBSERVER(mPreserveAspectRatio);
  }
  if (mViewBox) {
    NS_REMOVE_SVGVALUE_OBSERVER(mViewBox);
  }
  if (mOrientType) {
    NS_REMOVE_SVGVALUE_OBSERVER(mOrientType);
  }
}

nsresult
nsSVGMarkerElement::Init()
{
  nsresult rv = nsSVGMarkerElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // enumeration mappings
  static struct nsSVGEnumMapping gMarkerUnits[] = {
    {&nsSVGAtoms::strokeWidth, SVG_MARKERUNITS_STROKEWIDTH},
    {&nsSVGAtoms::userSpaceOnUse, SVG_MARKERUNITS_USERSPACEONUSE},
    {nsnull, 0}
  };
  
  static struct nsSVGEnumMapping gOrientType[] = {
    {&nsSVGAtoms::_auto, SVG_MARKER_ORIENT_AUTO},
    {nsnull, 0}
  };

  // Create mapped properties:

  // DOM property: refX
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length), 0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mRefX), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::refX, mRefX);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: refY
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length), 0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mRefY), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::refY, mRefY);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: markerWidth
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length), 3.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mMarkerWidth), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::markerWidth, mMarkerWidth);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: markerHeight
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length), 3.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mMarkerHeight), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::markerHeight, mMarkerHeight);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: markerUnits
  {
    nsCOMPtr<nsISVGEnum> units;
    rv = NS_NewSVGEnum(getter_AddRefs(units), SVG_MARKERUNITS_STROKEWIDTH, gMarkerUnits);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mMarkerUnits), units);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::markerUnits, mMarkerUnits);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: orient (enum)
  {
    nsCOMPtr<nsISVGEnum> orient;
    rv = NS_NewSVGEnum(getter_AddRefs(orient), SVG_MARKER_ORIENT_ANGLE, gOrientType);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mOrientType), orient);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::orient, mOrientType);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: orient (angle)
  {
    nsCOMPtr<nsISVGAngle> angle;
    rv = NS_NewSVGAngle(getter_AddRefs(angle), 0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedAngle(getter_AddRefs(mOrientAngle), angle);
    NS_ENSURE_SUCCESS(rv,rv);
    // Can't map two values to the same attribute, so we take care
    // of keeping mOrientAngle updated in DidModifySVGObservable
  }

  // DOM property: viewBox
  {
    nsCOMPtr<nsIDOMSVGRect> viewbox;
    nsCOMPtr<nsIDOMSVGLength> animWidth, animHeight;
    mMarkerWidth->GetAnimVal(getter_AddRefs(animWidth));
    mMarkerHeight->GetAnimVal(getter_AddRefs(animHeight));
    rv = NS_NewSVGViewBox(getter_AddRefs(viewbox), animWidth, animHeight);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedRect(getter_AddRefs(mViewBox), viewbox);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::viewBox, mViewBox);
    NS_ENSURE_SUCCESS(rv,rv);
    // initialize coordinate context with viewbox:
    SetCoordCtxRect(viewbox);
  }

  // DOM property: preserveAspectRatio
  {
    nsCOMPtr<nsIDOMSVGPreserveAspectRatio> preserveAspectRatio;
    rv = NS_NewSVGPreserveAspectRatio(getter_AddRefs(preserveAspectRatio));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedPreserveAspectRatio(
      getter_AddRefs(mPreserveAspectRatio),
      preserveAspectRatio);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::preserveAspectRatio,
                           mPreserveAspectRatio);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  
  // add observers -------------------------- :
  NS_ADD_SVGVALUE_OBSERVER(mViewBox);
  NS_ADD_SVGVALUE_OBSERVER(mPreserveAspectRatio);
  NS_ADD_SVGVALUE_OBSERVER(mOrientType);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGMarkerElement)

//----------------------------------------------------------------------
// nsIDOMSVGFitToViewBox methods

/* readonly attribute nsIDOMSVGAnimatedRect viewBox; */
  NS_IMETHODIMP nsSVGMarkerElement::GetViewBox(nsIDOMSVGAnimatedRect * *aViewBox)
{
  *aViewBox = mViewBox;
  NS_ADDREF(*aViewBox);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
nsSVGMarkerElement::GetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio * *aPreserveAspectRatio)
{
  *aPreserveAspectRatio = mPreserveAspectRatio;
  NS_ADDREF(*aPreserveAspectRatio);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGMarkerElement methods

/* readonly attribute nsIDOMSVGAnimatedLength refX; */
NS_IMETHODIMP nsSVGMarkerElement::GetRefX(nsIDOMSVGAnimatedLength * *aRefX)
{
  *aRefX = mRefX;
  NS_IF_ADDREF(*aRefX);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength refY; */
NS_IMETHODIMP nsSVGMarkerElement::GetRefY(nsIDOMSVGAnimatedLength * *aRefY)
{
  *aRefY = mRefY;
  NS_IF_ADDREF(*aRefY);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration markerUnits; */
NS_IMETHODIMP nsSVGMarkerElement::GetMarkerUnits(nsIDOMSVGAnimatedEnumeration * *aMarkerUnits)
{
  *aMarkerUnits = mMarkerUnits;
  NS_IF_ADDREF(*aMarkerUnits);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength markerWidth; */
NS_IMETHODIMP nsSVGMarkerElement::GetMarkerWidth(nsIDOMSVGAnimatedLength * *aMarkerWidth)
{
  *aMarkerWidth = mMarkerWidth;
  NS_IF_ADDREF(*aMarkerWidth);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength markerHeight; */
NS_IMETHODIMP nsSVGMarkerElement::GetMarkerHeight(nsIDOMSVGAnimatedLength * *aMarkerHeight)
{
  *aMarkerHeight = mMarkerHeight;
  NS_IF_ADDREF(*aMarkerHeight);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration orientType; */
NS_IMETHODIMP nsSVGMarkerElement::GetOrientType(nsIDOMSVGAnimatedEnumeration * *aOrientType)
{
  *aOrientType = mOrientType;
  NS_IF_ADDREF(*aOrientType);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength orientAngle; */
NS_IMETHODIMP nsSVGMarkerElement::GetOrientAngle(nsIDOMSVGAnimatedAngle * *aOrientAngle)
{
  *aOrientAngle = mOrientAngle;
  NS_IF_ADDREF(*aOrientAngle);
  return NS_OK;
}

/* void setOrientToAuto (); */
NS_IMETHODIMP nsSVGMarkerElement::SetOrientToAuto()
{
  mOrientType->SetBaseVal(SVG_MARKER_ORIENT_AUTO);
  return NS_OK;
}

/* void setOrientToAngle (in nsIDOMSVGAngle angle); */
NS_IMETHODIMP nsSVGMarkerElement::SetOrientToAngle(nsIDOMSVGAngle *angle)
{
  if (!angle)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  mOrientType->SetBaseVal(SVG_MARKER_ORIENT_ANGLE);

  nsIDOMSVGAngle *a;
  mOrientAngle->GetBaseVal(&a);
  float f;
  angle->GetValue(&f);
  a->SetValue(f);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGMarkerElement::GetMarkerTransform(float aStrokeWidth,
                                       float aX, float aY, float aAngle,
                                       nsIDOMSVGMatrix **_retval)
{
  float scale = 1.0;
  PRUint16 val;
  mMarkerUnits->GetAnimVal(&val);
  if (val == SVG_MARKERUNITS_STROKEWIDTH)
    scale = aStrokeWidth;

  mOrientType->GetAnimVal(&val);
  if (val == SVG_MARKER_ORIENT_ANGLE) {
    nsCOMPtr<nsIDOMSVGAngle> a;
    mOrientAngle->GetAnimVal(getter_AddRefs(a));
    a->GetValue(&aAngle);
  }

  nsCOMPtr<nsIDOMSVGMatrix> matrix;
  NS_NewSVGMatrix(getter_AddRefs(matrix),
                  cos(aAngle) * scale,   sin(aAngle) * scale,
                  -sin(aAngle) * scale,  cos(aAngle) * scale,
                  aX,                    aY);
    
  *_retval = matrix;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}


/* nsIDOMSVGMatrix getViewboxToViewportTransform (); */
NS_IMETHODIMP
nsSVGMarkerElement::GetViewboxToViewportTransform(nsIDOMSVGMatrix **_retval)
{
  nsresult rv = NS_OK;

  if (!mViewBoxToViewportTransform) {
    float viewportWidth;
    {
      nsCOMPtr<nsIDOMSVGLength> l;
      mMarkerWidth->GetAnimVal(getter_AddRefs(l));
      l->GetValue(&viewportWidth);
    }
    float viewportHeight;
    {
      nsCOMPtr<nsIDOMSVGLength> l;
      mMarkerHeight->GetAnimVal(getter_AddRefs(l));
      l->GetValue(&viewportHeight);
    }

    float viewboxX, viewboxY, viewboxWidth, viewboxHeight;
    {
      nsCOMPtr<nsIDOMSVGRect> vb;
      mViewBox->GetAnimVal(getter_AddRefs(vb));
      NS_ASSERTION(vb, "could not get viewbox");
      vb->GetX(&viewboxX);
      vb->GetY(&viewboxY);
      vb->GetWidth(&viewboxWidth);
      vb->GetHeight(&viewboxHeight);
    }
    if (viewboxWidth==0.0f || viewboxHeight==0.0f) {
      NS_ERROR("XXX. We shouldn't get here. Viewbox width/height is set to 0. Need to disable display of element as per specs.");
      viewboxWidth = 1.0f;
      viewboxHeight = 1.0f;
    }
    
    PRUint16 align, meetOrSlice;
    {
      nsCOMPtr<nsIDOMSVGPreserveAspectRatio> par;
      mPreserveAspectRatio->GetAnimVal(getter_AddRefs(par));
      NS_ASSERTION(par, "could not get preserveAspectRatio");
      par->GetAlign(&align);
      par->GetMeetOrSlice(&meetOrSlice);
    }

    // default to the defaults
    if (align == nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_UNKNOWN)
      align = nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID;
    if (meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_UNKNOWN)
      align = nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET;
    
    float a, d, e, f;
    a = viewportWidth/viewboxWidth;
    d = viewportHeight/viewboxHeight;
    e = 0.0f;
    f = 0.0f;

    if (align != nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE &&
        a != d) {
      if (meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET &&
          a < d ||
          meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE &&
          d < a) {
        d = a;
      }
      else if (
        meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET &&
        d < a ||
        meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE &&
        a < d) {
        a = d;
      }
      else NS_NOTREACHED("Unknown value for meetOrSlice");
    }

    if (viewboxX) e += -a * viewboxX;
    if (viewboxY) f += -d * viewboxY;

    float refX;
    {
      nsCOMPtr<nsIDOMSVGLength> l;
      mRefX->GetAnimVal(getter_AddRefs(l));
      l->GetValue(&refX);
    }

    float refY;
    {
      nsCOMPtr<nsIDOMSVGLength> l;
      mRefY->GetAnimVal(getter_AddRefs(l));
      l->GetValue(&refY);
    }

    e -= refX * a;
    f -= refY * d;
    
#ifdef DEBUG
    printf("Marker Viewport=(0?,0?,%f,%f)\n", viewportWidth, viewportHeight);
    printf("Marker Viewbox=(%f,%f,%f,%f)\n", viewboxX, viewboxY, viewboxWidth, viewboxHeight);
    printf("Marker Viewbox->Viewport xform [a c e] = [%f,   0, %f]\n", a, e);
    printf("                            [b d f] = [   0,  %f, %f]\n", d, f);
#endif
    
    rv = NS_NewSVGMatrix(getter_AddRefs(mViewBoxToViewportTransform),
                         a,     0.0f,
                         0.0f,  d,
                         e,     f);
  }

  *_retval = mViewBoxToViewportTransform;
  NS_IF_ADDREF(*_retval);
  return rv;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGMarkerElement::WillModifySVGObservable(nsISVGValue* observable,
                                            nsISVGValue::modificationType aModType)
{
#ifdef DEBUG
  printf("markerelement - viewport/viewbox/preserveAspectRatio will be changed\n");
#endif
  return NS_OK;
}


NS_IMETHODIMP
nsSVGMarkerElement::DidModifySVGObservable (nsISVGValue* observable,
                                            nsISVGValue::modificationType aModType)
{
#ifdef DEBUG
  printf("markerelement - viewport/viewbox/preserveAspectRatio have been changed\n");
#endif

  mViewBoxToViewportTransform = nsnull;

  // need to sync mOrientAngle
  nsAutoString value;
  nsresult rv = GetAttribute(NS_LITERAL_STRING("orient"), value);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsISVGValue> target = do_QueryInterface(mOrientAngle);
    target->SetValueString(value);
  }

  return NS_OK;
}
