/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGPOLYELEMENT_H_
#define DOM_SVG_SVGPOLYELEMENT_H_

#include "mozilla/Attributes.h"
#include "SVGAnimatedPointList.h"
#include "SVGGeometryElement.h"

namespace mozilla::dom {

class DOMSVGPointList;

using SVGPolyElementBase = SVGGeometryElement;

class SVGPolyElement : public SVGPolyElementBase {
 protected:
  explicit SVGPolyElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  virtual ~SVGPolyElement() = default;

 public:
  // interfaces

  NS_INLINE_DECL_REFCOUNTING_INHERITED(SVGPolyElement, SVGPolyElementBase)

  SVGAnimatedPointList* GetAnimatedPointList() override { return &mPoints; }
  nsStaticAtom* GetPointListAttrName() const override {
    return nsGkAtoms::points;
  }

  // SVGElement methods:
  bool HasValidDimensions() const override;

  // SVGGeometryElement methods:
  bool AttributeDefinesGeometry(const nsAtom* aName) override;
  bool IsMarkable() override { return true; }
  void GetMarkPoints(nsTArray<SVGMark>* aMarks) override;
  bool GetGeometryBounds(
      Rect* aBounds, const StrokeOptions& aStrokeOptions,
      const Matrix& aToBoundsSpace,
      const Matrix* aToNonScalingStrokeSpace = nullptr) override;

  // WebIDL
  already_AddRefed<DOMSVGPointList> Points();
  already_AddRefed<DOMSVGPointList> AnimatedPoints();

 protected:
  SVGAnimatedPointList mPoints;
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGPOLYELEMENT_H_
