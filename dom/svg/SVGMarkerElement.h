/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGMarkerElement_h
#define mozilla_dom_SVGMarkerElement_h

#include "nsAutoPtr.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGEnum.h"
#include "nsSVGLength2.h"
#include "SVGOrient.h"
#include "SVGViewBox.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGAnimatedAngle.h"
#include "mozilla/dom/SVGAnimatedEnumeration.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/SVGMarkerElementBinding.h"

class nsSVGMarkerFrame;

nsresult NS_NewSVGMarkerElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {

struct SVGMark;

namespace dom {

// Non-Exposed Marker Orientation Types
static const uint16_t SVG_MARKER_ORIENT_AUTO_START_REVERSE = 3;

typedef SVGElement SVGMarkerElementBase;

class SVGMarkerElement : public SVGMarkerElementBase {
  friend class ::nsSVGMarkerFrame;

 protected:
  friend nsresult(::NS_NewSVGMarkerElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGMarkerElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual JSObject* WrapNode(JSContext* cx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* name) const override;

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const override;

  // public helpers
  gfx::Matrix GetMarkerTransform(float aStrokeWidth, const SVGMark& aMark);
  SVGViewBoxRect GetViewBoxRect();
  gfx::Matrix GetViewBoxTransform();

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<SVGAnimatedRect> ViewBox();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();
  already_AddRefed<SVGAnimatedLength> RefX();
  already_AddRefed<SVGAnimatedLength> RefY();
  already_AddRefed<SVGAnimatedEnumeration> MarkerUnits();
  already_AddRefed<SVGAnimatedLength> MarkerWidth();
  already_AddRefed<SVGAnimatedLength> MarkerHeight();
  already_AddRefed<SVGAnimatedEnumeration> OrientType();
  already_AddRefed<SVGAnimatedAngle> OrientAngle();
  void SetOrientToAuto();
  void SetOrientToAngle(DOMSVGAngle& angle, ErrorResult& rv);

 protected:
  void SetParentCoordCtxProvider(SVGViewportElement* aContext);

  virtual LengthAttributesInfo GetLengthInfo() override;
  virtual EnumAttributesInfo GetEnumInfo() override;
  virtual SVGOrient* GetOrient() override;
  virtual SVGViewBox* GetViewBox() override;
  virtual SVGAnimatedPreserveAspectRatio* GetPreserveAspectRatio() override;

  enum { REFX, REFY, MARKERWIDTH, MARKERHEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { MARKERUNITS };
  SVGEnum mEnumAttributes[1];
  static SVGEnumMapping sUnitsMap[];
  static EnumInfo sEnumInfo[1];

  SVGOrient mOrient;
  SVGViewBox mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;

  SVGViewportElement* mCoordCtx;
  nsAutoPtr<gfx::Matrix> mViewBoxToViewportTransform;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGMarkerElement_h
