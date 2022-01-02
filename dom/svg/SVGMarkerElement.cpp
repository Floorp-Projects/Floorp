/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGMarkerElement.h"

#include "nsGkAtoms.h"
#include "DOMSVGAngle.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "nsError.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/SVGGeometryElement.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/dom/SVGMarkerElementBinding.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/RefPtr.h"
#include "SVGContentUtils.h"

using namespace mozilla::gfx;
using namespace mozilla::dom::SVGMarkerElement_Binding;

NS_IMPL_NS_NEW_SVG_ELEMENT(Marker)

namespace mozilla {
namespace dom {

using namespace SVGAngle_Binding;

JSObject* SVGMarkerElement::WrapNode(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return SVGMarkerElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::LengthInfo SVGMarkerElement::sLengthInfo[4] = {
    {nsGkAtoms::refX, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::X},
    {nsGkAtoms::refY, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::Y},
    {nsGkAtoms::markerWidth, 3, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::X},
    {nsGkAtoms::markerHeight, 3, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::Y},
};

SVGEnumMapping SVGMarkerElement::sUnitsMap[] = {
    {nsGkAtoms::strokeWidth, SVG_MARKERUNITS_STROKEWIDTH},
    {nsGkAtoms::userSpaceOnUse, SVG_MARKERUNITS_USERSPACEONUSE},
    {nullptr, 0}};

SVGElement::EnumInfo SVGMarkerElement::sEnumInfo[1] = {
    {nsGkAtoms::markerUnits, sUnitsMap, SVG_MARKERUNITS_STROKEWIDTH}};

//----------------------------------------------------------------------
// Implementation

SVGMarkerElement::SVGMarkerElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGMarkerElementBase(std::move(aNodeInfo)), mCoordCtx(nullptr) {}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGMarkerElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedRect> SVGMarkerElement::ViewBox() {
  return mViewBox.ToSVGAnimatedRect(this);
}

already_AddRefed<DOMSVGAnimatedPreserveAspectRatio>
SVGMarkerElement::PreserveAspectRatio() {
  return mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(this);
}

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedLength> SVGMarkerElement::RefX() {
  return mLengthAttributes[REFX].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGMarkerElement::RefY() {
  return mLengthAttributes[REFY].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedEnumeration> SVGMarkerElement::MarkerUnits() {
  return mEnumAttributes[MARKERUNITS].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGMarkerElement::MarkerWidth() {
  return mLengthAttributes[MARKERWIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGMarkerElement::MarkerHeight() {
  return mLengthAttributes[MARKERHEIGHT].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedEnumeration> SVGMarkerElement::OrientType() {
  return mOrient.ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedAngle> SVGMarkerElement::OrientAngle() {
  return mOrient.ToDOMAnimatedAngle(this);
}

void SVGMarkerElement::SetOrientToAuto() {
  mOrient.SetBaseType(SVG_MARKER_ORIENT_AUTO, this, IgnoreErrors());
}

void SVGMarkerElement::SetOrientToAngle(DOMSVGAngle& angle, ErrorResult& rv) {
  float f = angle.Value();
  if (!IsFinite(f)) {
    rv.ThrowTypeError("Unknown or invalid type");
    return;
  }
  mOrient.SetBaseValue(f, angle.UnitType(), this, true);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGMarkerElement::IsAttributeMapped(const nsAtom* name) const {
  static const MappedAttributeEntry* const map[] = {sFEFloodMap,
                                                    sFiltersMap,
                                                    sFontSpecificationMap,
                                                    sGradientStopMap,
                                                    sLightingEffectsMap,
                                                    sMarkersMap,
                                                    sTextContentElementsMap,
                                                    sViewportsMap,
                                                    sColorMap,
                                                    sFillStrokeMap,
                                                    sGraphicsMap};

  return FindAttributeDependence(name, map) ||
         SVGMarkerElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// SVGElement methods

void SVGMarkerElement::SetParentCoordCtxProvider(SVGViewportElement* aContext) {
  mCoordCtx = aContext;
  mViewBoxToViewportTransform = nullptr;
}

/* virtual */
bool SVGMarkerElement::HasValidDimensions() const {
  return (!mLengthAttributes[MARKERWIDTH].IsExplicitlySet() ||
          mLengthAttributes[MARKERWIDTH].GetAnimValInSpecifiedUnits() > 0) &&
         (!mLengthAttributes[MARKERHEIGHT].IsExplicitlySet() ||
          mLengthAttributes[MARKERHEIGHT].GetAnimValInSpecifiedUnits() > 0);
}

SVGElement::LengthAttributesInfo SVGMarkerElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

SVGElement::EnumAttributesInfo SVGMarkerElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

SVGAnimatedOrient* SVGMarkerElement::GetAnimatedOrient() { return &mOrient; }

SVGAnimatedViewBox* SVGMarkerElement::GetAnimatedViewBox() { return &mViewBox; }

SVGAnimatedPreserveAspectRatio*
SVGMarkerElement::GetAnimatedPreserveAspectRatio() {
  return &mPreserveAspectRatio;
}

//----------------------------------------------------------------------
// public helpers

gfx::Matrix SVGMarkerElement::GetMarkerTransform(float aStrokeWidth,
                                                 const SVGMark& aMark) {
  float scale =
      mEnumAttributes[MARKERUNITS].GetAnimValue() == SVG_MARKERUNITS_STROKEWIDTH
          ? aStrokeWidth
          : 1.0f;

  float angle;
  switch (mOrient.GetAnimType()) {
    case SVG_MARKER_ORIENT_AUTO:
      angle = aMark.angle;
      break;
    case SVG_MARKER_ORIENT_AUTO_START_REVERSE:
      angle = aMark.angle + (aMark.type == SVGMark::eStart ? M_PI : 0.0f);
      break;
    default:  // SVG_MARKER_ORIENT_ANGLE
      angle = mOrient.GetAnimValue() * M_PI / 180.0f;
      break;
  }

  return gfx::Matrix(cos(angle) * scale, sin(angle) * scale,
                     -sin(angle) * scale, cos(angle) * scale, aMark.x, aMark.y);
}

SVGViewBox SVGMarkerElement::GetViewBox() {
  if (mViewBox.HasRect()) {
    return mViewBox.GetAnimValue();
  }
  return SVGViewBox(0, 0,
                    mLengthAttributes[MARKERWIDTH].GetAnimValue(mCoordCtx),
                    mLengthAttributes[MARKERHEIGHT].GetAnimValue(mCoordCtx));
}

gfx::Matrix SVGMarkerElement::GetViewBoxTransform() {
  if (!mViewBoxToViewportTransform) {
    float viewportWidth =
        mLengthAttributes[MARKERWIDTH].GetAnimValue(mCoordCtx);
    float viewportHeight =
        mLengthAttributes[MARKERHEIGHT].GetAnimValue(mCoordCtx);

    SVGViewBox viewbox = GetViewBox();

    MOZ_ASSERT(viewbox.width > 0.0f && viewbox.height > 0.0f,
               "Rendering should be disabled");

    gfx::Matrix viewBoxTM = SVGContentUtils::GetViewBoxTransform(
        viewportWidth, viewportHeight, viewbox.x, viewbox.y, viewbox.width,
        viewbox.height, mPreserveAspectRatio);

    float refX = mLengthAttributes[REFX].GetAnimValue(mCoordCtx);
    float refY = mLengthAttributes[REFY].GetAnimValue(mCoordCtx);

    gfx::Point ref = viewBoxTM.TransformPoint(gfx::Point(refX, refY));

    Matrix TM = viewBoxTM;
    TM.PostTranslate(-ref.x, -ref.y);

    mViewBoxToViewportTransform = MakeUnique<gfx::Matrix>(TM);
  }

  return *mViewBoxToViewportTransform;
}

}  // namespace dom
}  // namespace mozilla
