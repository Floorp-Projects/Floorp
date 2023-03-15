/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGLINEELEMENT_H_
#define DOM_SVG_SVGLINEELEMENT_H_

#include "SVGAnimatedLength.h"
#include "SVGGeometryElement.h"

nsresult NS_NewSVGLineElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

using SVGLineElementBase = SVGGeometryElement;

class SVGLineElement final : public SVGLineElementBase {
 protected:
  explicit SVGLineElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;
  friend nsresult(::NS_NewSVGLineElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  // If the input line has length zero and linecaps aren't butt, adjust |aX2| by
  // a tiny amount to a barely-nonzero-length line that all of our draw targets
  // will render
  void MaybeAdjustForZeroLength(float aX1, float aY1, float& aX2, float aY2);

 public:
  // SVGGeometryElement methods:
  bool IsMarkable() override { return true; }
  void GetMarkPoints(nsTArray<SVGMark>* aMarks) override;
  void GetAsSimplePath(SimplePath* aSimplePath) override;
  already_AddRefed<Path> BuildPath(PathBuilder* aBuilder) override;
  bool GetGeometryBounds(
      Rect* aBounds, const StrokeOptions& aStrokeOptions,
      const Matrix& aToBoundsSpace,
      const Matrix* aToNonScalingStrokeSpace = nullptr) override;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> X1();
  already_AddRefed<DOMSVGAnimatedLength> Y1();
  already_AddRefed<DOMSVGAnimatedLength> X2();
  already_AddRefed<DOMSVGAnimatedLength> Y2();

 protected:
  LengthAttributesInfo GetLengthInfo() override;

  enum { ATTR_X1, ATTR_Y1, ATTR_X2, ATTR_Y2 };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGLINEELEMENT_H_
