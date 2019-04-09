/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGEllipseElement_h
#define mozilla_dom_SVGEllipseElement_h

#include "SVGAnimatedLength.h"
#include "SVGGeometryElement.h"

nsresult NS_NewSVGEllipseElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGGeometryElement SVGEllipseElementBase;

class SVGEllipseElement final : public SVGEllipseElementBase {
 protected:
  explicit SVGEllipseElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual JSObject* WrapNode(JSContext* cx,
                             JS::Handle<JSObject*> aGivenProto) override;
  friend nsresult(::NS_NewSVGEllipseElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 public:
  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const override;

  // SVGGeometryElement methods:
  virtual bool GetGeometryBounds(
      Rect* aBounds, const StrokeOptions& aStrokeOptions,
      const Matrix& aToBoundsSpace,
      const Matrix* aToNonScalingStrokeSpace = nullptr) override;
  virtual already_AddRefed<Path> BuildPath(PathBuilder* aBuilder) override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> Cx();
  already_AddRefed<DOMSVGAnimatedLength> Cy();
  already_AddRefed<DOMSVGAnimatedLength> Rx();
  already_AddRefed<DOMSVGAnimatedLength> Ry();

 protected:
  virtual LengthAttributesInfo GetLengthInfo() override;

  enum { CX, CY, RX, RY };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGEllipseElement_h
