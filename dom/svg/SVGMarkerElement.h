/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGMARKERELEMENT_H_
#define DOM_SVG_SVGMARKERELEMENT_H_

#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedOrient.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedViewBox.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/SVGMarkerElementBinding.h"
#include "mozilla/UniquePtr.h"

nsresult NS_NewSVGMarkerElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {

struct SVGMark;
class SVGMarkerFrame;

namespace dom {

class DOMSVGAnimatedAngle;
class DOMSVGAnimatedEnumeration;

using SVGMarkerElementBase = SVGElement;

class SVGMarkerElement final : public SVGMarkerElementBase {
  friend class mozilla::SVGMarkerFrame;

 protected:
  friend nsresult(::NS_NewSVGMarkerElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGMarkerElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

 public:
  // SVGSVGElement methods:
  bool HasValidDimensions() const override;

  // public helpers
  gfx::Matrix GetMarkerTransform(float aStrokeWidth, const SVGMark& aMark);
  SVGViewBox GetViewBox();
  gfx::Matrix GetViewBoxTransform();

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<SVGAnimatedRect> ViewBox();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();
  already_AddRefed<DOMSVGAnimatedLength> RefX();
  already_AddRefed<DOMSVGAnimatedLength> RefY();
  already_AddRefed<DOMSVGAnimatedEnumeration> MarkerUnits();
  already_AddRefed<DOMSVGAnimatedLength> MarkerWidth();
  already_AddRefed<DOMSVGAnimatedLength> MarkerHeight();
  already_AddRefed<DOMSVGAnimatedEnumeration> OrientType();
  already_AddRefed<DOMSVGAnimatedAngle> OrientAngle();
  void SetOrientToAuto();
  void SetOrientToAngle(DOMSVGAngle& aAngle);

 protected:
  void SetParentCoordCtxProvider(SVGViewportElement* aContext);

  LengthAttributesInfo GetLengthInfo() override;
  EnumAttributesInfo GetEnumInfo() override;
  SVGAnimatedOrient* GetAnimatedOrient() override;
  SVGAnimatedPreserveAspectRatio* GetAnimatedPreserveAspectRatio() override;
  SVGAnimatedViewBox* GetAnimatedViewBox() override;

  enum { REFX, REFY, MARKERWIDTH, MARKERHEIGHT };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { MARKERUNITS };
  SVGAnimatedEnumeration mEnumAttributes[1];
  static SVGEnumMapping sUnitsMap[];
  static EnumInfo sEnumInfo[1];

  SVGAnimatedOrient mOrient;
  SVGAnimatedViewBox mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;

  SVGViewportElement* mCoordCtx;
  UniquePtr<gfx::Matrix> mViewBoxToViewportTransform;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGMARKERELEMENT_H_
