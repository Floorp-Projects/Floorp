/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SVGPOLYELEMENT_H_
#define NS_SVGPOLYELEMENT_H_

#include "mozilla/Attributes.h"
#include "SVGAnimatedPointList.h"
#include "SVGGeometryElement.h"

namespace mozilla {
class DOMSVGPointList;

namespace dom {

typedef SVGGeometryElement SVGPolyElementBase;

class SVGPolyElement : public SVGPolyElementBase
{
protected:
  explicit SVGPolyElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  virtual ~SVGPolyElement();

public:
  //interfaces

  NS_DECL_ISUPPORTS_INHERITED

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const override;

  virtual SVGAnimatedPointList* GetAnimatedPointList() override {
    return &mPoints;
  }
  virtual nsIAtom* GetPointListAttrName() const override {
    return nsGkAtoms::points;
  }

  // nsSVGElement methods:
  virtual bool HasValidDimensions() const override;

  // SVGGeometryElement methods:
  virtual bool AttributeDefinesGeometry(const nsIAtom *aName) override;
  virtual bool IsMarkable() override { return true; }
  virtual void GetMarkPoints(nsTArray<nsSVGMark> *aMarks) override;
  virtual bool GetGeometryBounds(Rect* aBounds, const StrokeOptions& aStrokeOptions,
                                 const Matrix& aToBoundsSpace,
                                 const Matrix* aToNonScalingStrokeSpace = nullptr) override;

  // WebIDL
  already_AddRefed<mozilla::DOMSVGPointList> Points();
  already_AddRefed<mozilla::DOMSVGPointList> AnimatedPoints();

protected:
  SVGAnimatedPointList mPoints;
};

} // namespace dom
} // namespace mozilla

#endif //NS_SVGPOLYELEMENT_H_
