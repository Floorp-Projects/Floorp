/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGELLIPSEELEMENT_H_
#define DOM_SVG_SVGELLIPSEELEMENT_H_

#include "nsCSSPropertyID.h"
#include "SVGAnimatedLength.h"
#include "SVGGeometryElement.h"

nsresult NS_NewSVGEllipseElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
class ComputedStyle;

namespace dom {

using SVGEllipseElementBase = SVGGeometryElement;

class SVGEllipseElement final : public SVGEllipseElementBase {
 protected:
  explicit SVGEllipseElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;
  friend nsresult(::NS_NewSVGEllipseElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 public:
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  // SVGSVGElement methods:
  bool HasValidDimensions() const override;

  // SVGGeometryElement methods:
  virtual bool GetGeometryBounds(
      Rect* aBounds, const StrokeOptions& aStrokeOptions,
      const Matrix& aToBoundsSpace,
      const Matrix* aToNonScalingStrokeSpace = nullptr) override;
  already_AddRefed<Path> BuildPath(PathBuilder* aBuilder) override;
  bool IsClosedLoop() const override { return true; }

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  static bool IsLengthChangedViaCSS(const ComputedStyle& aNewStyle,
                                    const ComputedStyle& aOldStyle);
  static nsCSSPropertyID GetCSSPropertyIdForAttrEnum(uint8_t aAttrEnum);

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> Cx();
  already_AddRefed<DOMSVGAnimatedLength> Cy();
  already_AddRefed<DOMSVGAnimatedLength> Rx();
  already_AddRefed<DOMSVGAnimatedLength> Ry();

 protected:
  LengthAttributesInfo GetLengthInfo() override;

  enum { CX, CY, RX, RY };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGELLIPSEELEMENT_H_
