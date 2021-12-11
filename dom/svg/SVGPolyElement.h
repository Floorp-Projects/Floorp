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

namespace mozilla {
namespace dom {

class DOMSVGPointList;

using SVGPolyElementBase = SVGGeometryElement;

class SVGPolyElement : public SVGPolyElementBase {
 protected:
  explicit SVGPolyElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  virtual ~SVGPolyElement() = default;

 public:
  // interfaces

  NS_INLINE_DECL_REFCOUNTING_INHERITED(SVGPolyElement, SVGPolyElementBase)

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* name) const override;

  virtual SVGAnimatedPointList* GetAnimatedPointList() override {
    return &mPoints;
  }
  virtual nsStaticAtom* GetPointListAttrName() const override {
    return nsGkAtoms::points;
  }

  // SVGElement methods:
  virtual bool HasValidDimensions() const override;

  // SVGGeometryElement methods:
  virtual bool AttributeDefinesGeometry(const nsAtom* aName) override;
  virtual bool IsMarkable() override { return true; }
  virtual void GetMarkPoints(nsTArray<SVGMark>* aMarks) override;
  virtual bool GetGeometryBounds(
      Rect* aBounds, const StrokeOptions& aStrokeOptions,
      const Matrix& aToBoundsSpace,
      const Matrix* aToNonScalingStrokeSpace = nullptr) override;

  // WebIDL
  already_AddRefed<DOMSVGPointList> Points();
  already_AddRefed<DOMSVGPointList> AnimatedPoints();

 protected:
  SVGAnimatedPointList mPoints;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGPOLYELEMENT_H_
