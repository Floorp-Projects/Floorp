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

#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "nsISVGValueUtils.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "nsSVGMatrix.h"
#include "nsDOMError.h"
#include "nsSVGUtils.h"
#include "nsSVGMarkerElement.h"
#include "gfxMatrix.h"
#include "nsContentUtils.h" // NS_ENSURE_FINITE

using namespace mozilla;

nsSVGElement::LengthInfo nsSVGMarkerElement::sLengthInfo[4] =
{
  { &nsGkAtoms::refX, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::refY, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
  { &nsGkAtoms::markerWidth, 3, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::markerHeight, 3, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
};

nsSVGEnumMapping nsSVGMarkerElement::sUnitsMap[] = {
  {&nsGkAtoms::strokeWidth, nsIDOMSVGMarkerElement::SVG_MARKERUNITS_STROKEWIDTH},
  {&nsGkAtoms::userSpaceOnUse, nsIDOMSVGMarkerElement::SVG_MARKERUNITS_USERSPACEONUSE},
  {nsnull, 0}
};

nsSVGElement::EnumInfo nsSVGMarkerElement::sEnumInfo[1] =
{
  { &nsGkAtoms::markerUnits,
    sUnitsMap,
    nsIDOMSVGMarkerElement::SVG_MARKERUNITS_STROKEWIDTH
  }
};

nsSVGElement::AngleInfo nsSVGMarkerElement::sAngleInfo[1] =
{
  { &nsGkAtoms::orient, 0, nsIDOMSVGAngle::SVG_ANGLETYPE_UNSPECIFIED }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Marker)

//----------------------------------------------------------------------
// nsISupports methods

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGOrientType::DOMAnimatedEnum, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGOrientType::DOMAnimatedEnum)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGOrientType::DOMAnimatedEnum)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGOrientType::DOMAnimatedEnum)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedEnumeration)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimatedEnumeration)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF_INHERITED(nsSVGMarkerElement,nsSVGMarkerElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGMarkerElement,nsSVGMarkerElementBase)

DOMCI_NODE_DATA(SVGMarkerElement, nsSVGMarkerElement)

NS_INTERFACE_TABLE_HEAD(nsSVGMarkerElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGMarkerElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGFitToViewBox,
                           nsIDOMSVGMarkerElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGMarkerElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGMarkerElementBase)

//----------------------------------------------------------------------
// Implementation

nsresult
nsSVGOrientType::SetBaseValue(PRUint16 aValue,
                              nsSVGElement *aSVGElement)
{
  if (aValue == nsIDOMSVGMarkerElement::SVG_MARKER_ORIENT_AUTO ||
      aValue == nsIDOMSVGMarkerElement::SVG_MARKER_ORIENT_ANGLE) {
    SetBaseValue(aValue);
    aSVGElement->SetAttr(
      kNameSpaceID_None, nsGkAtoms::orient, nsnull,
      (aValue ==nsIDOMSVGMarkerElement::SVG_MARKER_ORIENT_AUTO ?
        NS_LITERAL_STRING("auto") : NS_LITERAL_STRING("0")),
      PR_TRUE);
    return NS_OK;
  }
  return NS_ERROR_DOM_SYNTAX_ERR;
}

nsresult
nsSVGOrientType::ToDOMAnimatedEnum(nsIDOMSVGAnimatedEnumeration **aResult,
                                   nsSVGElement *aSVGElement)
{
  *aResult = new DOMAnimatedEnum(this, aSVGElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

nsSVGMarkerElement::nsSVGMarkerElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGMarkerElementBase(aNodeInfo), mCoordCtx(nsnull)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGMarkerElement)

//----------------------------------------------------------------------
// nsIDOMSVGFitToViewBox methods

/* readonly attribute nsIDOMSVGAnimatedRect viewBox; */
  NS_IMETHODIMP nsSVGMarkerElement::GetViewBox(nsIDOMSVGAnimatedRect * *aViewBox)
{
  return mViewBox.ToDOMAnimatedRect(aViewBox, this);
}

/* readonly attribute nsIDOMSVGAnimatedPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
nsSVGMarkerElement::GetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio
                                           **aPreserveAspectRatio)
{
  return mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(aPreserveAspectRatio, this);
}

//----------------------------------------------------------------------
// nsIDOMSVGMarkerElement methods

/* readonly attribute nsIDOMSVGAnimatedLength refX; */
NS_IMETHODIMP nsSVGMarkerElement::GetRefX(nsIDOMSVGAnimatedLength * *aRefX)
{
  return mLengthAttributes[REFX].ToDOMAnimatedLength(aRefX, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength refY; */
NS_IMETHODIMP nsSVGMarkerElement::GetRefY(nsIDOMSVGAnimatedLength * *aRefY)
{
  return mLengthAttributes[REFY].ToDOMAnimatedLength(aRefY, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration markerUnits; */
NS_IMETHODIMP nsSVGMarkerElement::GetMarkerUnits(nsIDOMSVGAnimatedEnumeration * *aMarkerUnits)
{
  return mEnumAttributes[MARKERUNITS].ToDOMAnimatedEnum(aMarkerUnits, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength markerWidth; */
NS_IMETHODIMP nsSVGMarkerElement::GetMarkerWidth(nsIDOMSVGAnimatedLength * *aMarkerWidth)
{
  return mLengthAttributes[MARKERWIDTH].ToDOMAnimatedLength(aMarkerWidth, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength markerHeight; */
NS_IMETHODIMP nsSVGMarkerElement::GetMarkerHeight(nsIDOMSVGAnimatedLength * *aMarkerHeight)
{
  return mLengthAttributes[MARKERHEIGHT].ToDOMAnimatedLength(aMarkerHeight, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration orientType; */
NS_IMETHODIMP nsSVGMarkerElement::GetOrientType(nsIDOMSVGAnimatedEnumeration * *aOrientType)
{
  return mOrientType.ToDOMAnimatedEnum(aOrientType, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength orientAngle; */
NS_IMETHODIMP nsSVGMarkerElement::GetOrientAngle(nsIDOMSVGAnimatedAngle * *aOrientAngle)
{
  return mAngleAttributes[ORIENT].ToDOMAnimatedAngle(aOrientAngle, this);
}

/* void setOrientToAuto (); */
NS_IMETHODIMP nsSVGMarkerElement::SetOrientToAuto()
{
  SetAttr(kNameSpaceID_None, nsGkAtoms::orient, nsnull,
          NS_LITERAL_STRING("auto"), PR_TRUE);
  return NS_OK;
}

/* void setOrientToAngle (in nsIDOMSVGAngle angle); */
NS_IMETHODIMP nsSVGMarkerElement::SetOrientToAngle(nsIDOMSVGAngle *angle)
{
  if (!angle)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  float f;
  nsresult rv = angle->GetValue(&f);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_FINITE(f, NS_ERROR_DOM_SVG_WRONG_TYPE_ERR);
  mAngleAttributes[ORIENT].SetBaseValue(f, this);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
nsSVGMarkerElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sLightingEffectsMap,
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap
  };

  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGMarkerElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

PRBool
nsSVGMarkerElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                            nsAString &aResult) const
{
  if (aNameSpaceID == kNameSpaceID_None &&
      aName == nsGkAtoms::orient &&
      mOrientType.GetBaseValue() == SVG_MARKER_ORIENT_AUTO) {
    aResult.AssignLiteral("auto");
    return PR_TRUE;
  }
  return nsSVGMarkerElementBase::GetAttr(aNameSpaceID, aName, aResult);
}

PRBool
nsSVGMarkerElement::ParseAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                   const nsAString& aValue,
                                   nsAttrValue& aResult)
{
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::orient) {
    if (aValue.EqualsLiteral("auto")) {
      mOrientType.SetBaseValue(SVG_MARKER_ORIENT_AUTO);
      aResult.SetTo(aValue);
      return PR_TRUE;
    }
    mOrientType.SetBaseValue(SVG_MARKER_ORIENT_ANGLE);
  }
  return nsSVGMarkerElementBase::ParseAttribute(aNameSpaceID, aName,
                                                aValue, aResult);
}

nsresult
nsSVGMarkerElement::UnsetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                              PRBool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::viewBox && mCoordCtx) {
      mViewBox.SetBaseValue(0, 0, mLengthAttributes[MARKERWIDTH].GetAnimValue(mCoordCtx),
                            mLengthAttributes[MARKERHEIGHT].GetAnimValue(mCoordCtx),
                            this, PR_FALSE);
      return nsGenericElement::UnsetAttr(aNamespaceID, aName, aNotify);
    } else if (aName == nsGkAtoms::orient) {
      mOrientType.SetBaseValue(SVG_MARKER_ORIENT_ANGLE);
    }
  }

  return nsSVGMarkerElementBase::UnsetAttr(aNamespaceID, aName, aNotify);
}

//----------------------------------------------------------------------
// nsSVGElement methods

void
nsSVGMarkerElement::DidChangeLength(PRUint8 aAttrEnum, PRBool aDoSetAttr)
{
  nsSVGMarkerElementBase::DidChangeLength(aAttrEnum, aDoSetAttr);

  mViewBoxToViewportTransform = nsnull;

  if (mCoordCtx && !HasAttr(kNameSpaceID_None, nsGkAtoms::viewBox) &&
      (aAttrEnum == MARKERWIDTH || aAttrEnum == MARKERHEIGHT)) {
    mViewBox.SetBaseValue(0, 0, mLengthAttributes[MARKERWIDTH].GetAnimValue(mCoordCtx),
                          mLengthAttributes[MARKERHEIGHT].GetAnimValue(mCoordCtx),
                          this, PR_FALSE);
  }
}

void
nsSVGMarkerElement::DidChangeViewBox(PRBool aDoSetAttr)
{
  nsSVGMarkerElementBase::DidChangeViewBox(aDoSetAttr);

  mViewBoxToViewportTransform = nsnull;
}

void
nsSVGMarkerElement::DidChangePreserveAspectRatio(PRBool aDoSetAttr)
{
  nsSVGMarkerElementBase::DidChangePreserveAspectRatio(aDoSetAttr);

  mViewBoxToViewportTransform = nsnull;
}

void 
nsSVGMarkerElement::SetParentCoordCtxProvider(nsSVGSVGElement *aContext)
{
  mCoordCtx = aContext;
  mViewBoxToViewportTransform = nsnull;

  if (mCoordCtx && !HasAttr(kNameSpaceID_None, nsGkAtoms::viewBox)) {
    mViewBox.SetBaseValue(0, 0, mLengthAttributes[MARKERWIDTH].GetAnimValue(mCoordCtx),
                          mLengthAttributes[MARKERHEIGHT].GetAnimValue(mCoordCtx),
                          this, PR_FALSE);
  }
}

nsSVGElement::LengthAttributesInfo
nsSVGMarkerElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              NS_ARRAY_LENGTH(sLengthInfo));
}

nsSVGElement::AngleAttributesInfo
nsSVGMarkerElement::GetAngleInfo()
{
  return AngleAttributesInfo(mAngleAttributes, sAngleInfo,
                             NS_ARRAY_LENGTH(sAngleInfo));
}

nsSVGElement::EnumAttributesInfo
nsSVGMarkerElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}

nsSVGViewBox *
nsSVGMarkerElement::GetViewBox()
{
  return &mViewBox;
}

SVGAnimatedPreserveAspectRatio *
nsSVGMarkerElement::GetPreserveAspectRatio()
{
  return &mPreserveAspectRatio;
}

//----------------------------------------------------------------------
// public helpers

gfxMatrix
nsSVGMarkerElement::GetMarkerTransform(float aStrokeWidth,
                                       float aX, float aY, float aAutoAngle)
{
  float scale = 1.0;
  if (mEnumAttributes[MARKERUNITS].GetAnimValue() ==
      SVG_MARKERUNITS_STROKEWIDTH)
    scale = aStrokeWidth;

  float angle = mOrientType.GetAnimValue() == SVG_MARKER_ORIENT_AUTO ?
                aAutoAngle :
                mAngleAttributes[ORIENT].GetAnimValue() * M_PI / 180.0;

  return gfxMatrix(cos(angle) * scale,   sin(angle) * scale,
                   -sin(angle) * scale,  cos(angle) * scale,
                   aX,                    aY);
}

gfxMatrix
nsSVGMarkerElement::GetViewBoxTransform()
{
  if (!mViewBoxToViewportTransform) {
    float viewportWidth =
      mLengthAttributes[MARKERWIDTH].GetAnimValue(mCoordCtx);
    float viewportHeight = 
      mLengthAttributes[MARKERHEIGHT].GetAnimValue(mCoordCtx);
   
    const nsSVGViewBoxRect& viewbox = mViewBox.GetAnimValue(); 

    if (viewbox.width <= 0.0f || viewbox.height <= 0.0f) {
      return gfxMatrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); // invalid - don't paint element
    }

    float refX = mLengthAttributes[REFX].GetAnimValue(mCoordCtx);
    float refY = mLengthAttributes[REFY].GetAnimValue(mCoordCtx);

    gfxMatrix viewBoxTM =
      nsSVGUtils::GetViewBoxTransform(this,
                                      viewportWidth, viewportHeight,
                                      viewbox.x, viewbox.y,
                                      viewbox.width, viewbox.height,
                                      mPreserveAspectRatio);

    gfxPoint ref = viewBoxTM.Transform(gfxPoint(refX, refY));

    gfxMatrix TM = viewBoxTM * gfxMatrix().Translate(gfxPoint(-ref.x, -ref.y));

    mViewBoxToViewportTransform = NS_NewSVGMatrix(TM);
  }

  return nsSVGUtils::ConvertSVGMatrixToThebes(mViewBoxToViewportTransform);
}


