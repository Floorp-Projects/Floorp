/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "nsError.h"
#include "mozilla/dom/SVGAngle.h"
#include "mozilla/dom/SVGMarkerElement.h"
#include "mozilla/dom/SVGMarkerElementBinding.h"
#include "gfxMatrix.h"
#include "SVGContentUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Marker)

namespace mozilla {
namespace dom {

JSObject*
SVGMarkerElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGMarkerElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::LengthInfo SVGMarkerElement::sLengthInfo[4] =
{
  { &nsGkAtoms::refX, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::refY, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::markerWidth, 3, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::markerHeight, 3, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
};

nsSVGEnumMapping SVGMarkerElement::sUnitsMap[] = {
  {&nsGkAtoms::strokeWidth, SVG_MARKERUNITS_STROKEWIDTH},
  {&nsGkAtoms::userSpaceOnUse, SVG_MARKERUNITS_USERSPACEONUSE},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGMarkerElement::sEnumInfo[1] =
{
  { &nsGkAtoms::markerUnits,
    sUnitsMap,
    SVG_MARKERUNITS_STROKEWIDTH
  }
};

nsSVGElement::AngleInfo SVGMarkerElement::sAngleInfo[1] =
{
  { &nsGkAtoms::orient, 0, SVG_ANGLETYPE_UNSPECIFIED }
};

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

//----------------------------------------------------------------------
// Implementation

nsresult
nsSVGOrientType::SetBaseValue(uint16_t aValue,
                              nsSVGElement *aSVGElement)
{
  if (aValue == SVG_MARKER_ORIENT_AUTO || aValue == SVG_MARKER_ORIENT_ANGLE) {
    SetBaseValue(aValue);
    aSVGElement->SetAttr(
      kNameSpaceID_None, nsGkAtoms::orient, nullptr,
      (aValue == SVG_MARKER_ORIENT_AUTO ?
        NS_LITERAL_STRING("auto") : NS_LITERAL_STRING("0")),
      true);
    return NS_OK;
  }
  return NS_ERROR_DOM_SYNTAX_ERR;
}

already_AddRefed<nsIDOMSVGAnimatedEnumeration>
nsSVGOrientType::ToDOMAnimatedEnum(nsSVGElement *aSVGElement)
{
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> toReturn =
    new DOMAnimatedEnum(this, aSVGElement);
  return toReturn.forget();
}

SVGMarkerElement::SVGMarkerElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGMarkerElementBase(aNodeInfo), mCoordCtx(nullptr)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGMarkerElement)

//----------------------------------------------------------------------

already_AddRefed<nsIDOMSVGAnimatedRect>
SVGMarkerElement::ViewBox()
{
  nsCOMPtr<nsIDOMSVGAnimatedRect> rect;
  mViewBox.ToDOMAnimatedRect(getter_AddRefs(rect), this);
  return rect.forget();
}

already_AddRefed<DOMSVGAnimatedPreserveAspectRatio>
SVGMarkerElement::PreserveAspectRatio()
{
  nsRefPtr<DOMSVGAnimatedPreserveAspectRatio> ratio;
  mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(getter_AddRefs(ratio), this);
  return ratio.forget();
}

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedLength>
SVGMarkerElement::RefX()
{
  return mLengthAttributes[REFX].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGMarkerElement::RefY()
{
  return mLengthAttributes[REFY].ToDOMAnimatedLength(this);
}

already_AddRefed<nsIDOMSVGAnimatedEnumeration>
SVGMarkerElement::MarkerUnits()
{
  return mEnumAttributes[MARKERUNITS].ToDOMAnimatedEnum(this);
}

already_AddRefed<SVGAnimatedLength>
SVGMarkerElement::MarkerWidth()
{
  return mLengthAttributes[MARKERWIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGMarkerElement::MarkerHeight()
{
  return mLengthAttributes[MARKERHEIGHT].ToDOMAnimatedLength(this);
}

already_AddRefed<nsIDOMSVGAnimatedEnumeration>
SVGMarkerElement::OrientType()
{
  return mOrientType.ToDOMAnimatedEnum(this);
}

already_AddRefed<SVGAnimatedAngle>
SVGMarkerElement::OrientAngle()
{
  return mAngleAttributes[ORIENT].ToDOMAnimatedAngle(this);
}

void SVGMarkerElement::SetOrientToAuto()
{
  SetAttr(kNameSpaceID_None, nsGkAtoms::orient, nullptr,
          NS_LITERAL_STRING("auto"), true);
}

void
SVGMarkerElement::SetOrientToAngle(SVGAngle& angle, ErrorResult& rv)
{
  float f = angle.Value();
  if (!NS_finite(f)) {
    rv.Throw(NS_ERROR_DOM_SVG_WRONG_TYPE_ERR);
    return;
  }
  mAngleAttributes[ORIENT].SetBaseValue(f, this, true);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGMarkerElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sLightingEffectsMap,
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap,
    sColorMap,
    sFillStrokeMap,
    sGraphicsMap
  };

  return FindAttributeDependence(name, map) ||
    SVGMarkerElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

bool
SVGMarkerElement::ParseAttribute(int32_t aNameSpaceID, nsIAtom* aName,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::orient) {
    if (aValue.EqualsLiteral("auto")) {
      mOrientType.SetBaseValue(SVG_MARKER_ORIENT_AUTO);
      aResult.SetTo(aValue);
      return true;
    }
    mOrientType.SetBaseValue(SVG_MARKER_ORIENT_ANGLE);
  }
  return SVGMarkerElementBase::ParseAttribute(aNameSpaceID, aName,
                                              aValue, aResult);
}

nsresult
SVGMarkerElement::UnsetAttr(int32_t aNamespaceID, nsIAtom* aName,
                            bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::orient) {
      mOrientType.SetBaseValue(SVG_MARKER_ORIENT_ANGLE);
    }
  }

  return nsSVGElement::UnsetAttr(aNamespaceID, aName, aNotify);
}

//----------------------------------------------------------------------
// nsSVGElement methods

void
SVGMarkerElement::SetParentCoordCtxProvider(SVGSVGElement *aContext)
{
  mCoordCtx = aContext;
  mViewBoxToViewportTransform = nullptr;
}

/* virtual */ bool
SVGMarkerElement::HasValidDimensions() const
{
  return (!mLengthAttributes[MARKERWIDTH].IsExplicitlySet() ||
           mLengthAttributes[MARKERWIDTH].GetAnimValInSpecifiedUnits() > 0) &&
         (!mLengthAttributes[MARKERHEIGHT].IsExplicitlySet() || 
           mLengthAttributes[MARKERHEIGHT].GetAnimValInSpecifiedUnits() > 0);
}

nsSVGElement::LengthAttributesInfo
SVGMarkerElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

nsSVGElement::AngleAttributesInfo
SVGMarkerElement::GetAngleInfo()
{
  return AngleAttributesInfo(mAngleAttributes, sAngleInfo,
                             ArrayLength(sAngleInfo));
}

nsSVGElement::EnumAttributesInfo
SVGMarkerElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGViewBox *
SVGMarkerElement::GetViewBox()
{
  return &mViewBox;
}

SVGAnimatedPreserveAspectRatio *
SVGMarkerElement::GetPreserveAspectRatio()
{
  return &mPreserveAspectRatio;
}

//----------------------------------------------------------------------
// public helpers

gfxMatrix
SVGMarkerElement::GetMarkerTransform(float aStrokeWidth,
                                     float aX, float aY, float aAutoAngle)
{
  gfxFloat scale = mEnumAttributes[MARKERUNITS].GetAnimValue() ==
                     SVG_MARKERUNITS_STROKEWIDTH ? aStrokeWidth : 1.0;

  gfxFloat angle = mOrientType.GetAnimValue() == SVG_MARKER_ORIENT_AUTO ?
                    aAutoAngle :
                    mAngleAttributes[ORIENT].GetAnimValue() * M_PI / 180.0;

  return gfxMatrix(cos(angle) * scale,   sin(angle) * scale,
                   -sin(angle) * scale,  cos(angle) * scale,
                   aX,                    aY);
}

nsSVGViewBoxRect
SVGMarkerElement::GetViewBoxRect()
{
  if (mViewBox.HasRect()) {
    return mViewBox.GetAnimValue();
  }
  return nsSVGViewBoxRect(
           0, 0,
           mLengthAttributes[MARKERWIDTH].GetAnimValue(mCoordCtx),
           mLengthAttributes[MARKERHEIGHT].GetAnimValue(mCoordCtx));
}

gfxMatrix
SVGMarkerElement::GetViewBoxTransform()
{
  if (!mViewBoxToViewportTransform) {
    float viewportWidth =
      mLengthAttributes[MARKERWIDTH].GetAnimValue(mCoordCtx);
    float viewportHeight = 
      mLengthAttributes[MARKERHEIGHT].GetAnimValue(mCoordCtx);
   
    nsSVGViewBoxRect viewbox = GetViewBoxRect();

    NS_ABORT_IF_FALSE(viewbox.width > 0.0f && viewbox.height > 0.0f,
                      "Rendering should be disabled");

    gfxMatrix viewBoxTM =
      SVGContentUtils::GetViewBoxTransform(viewportWidth, viewportHeight,
                                           viewbox.x, viewbox.y,
                                           viewbox.width, viewbox.height,
                                           mPreserveAspectRatio);

    float refX = mLengthAttributes[REFX].GetAnimValue(mCoordCtx);
    float refY = mLengthAttributes[REFY].GetAnimValue(mCoordCtx);

    gfxPoint ref = viewBoxTM.Transform(gfxPoint(refX, refY));

    gfxMatrix TM = viewBoxTM * gfxMatrix().Translate(gfxPoint(-ref.x, -ref.y));

    mViewBoxToViewportTransform = new gfxMatrix(TM);
  }

  return *mViewBoxToViewportTransform;
}

} // namespace dom
} // namespace mozilla
