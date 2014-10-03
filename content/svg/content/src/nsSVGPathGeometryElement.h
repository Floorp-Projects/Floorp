/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGPATHGEOMETRYELEMENT_H__
#define __NS_SVGPATHGEOMETRYELEMENT_H__

#include "mozilla/gfx/2D.h"
#include "SVGGraphicsElement.h"

class gfxMatrix;

struct nsSVGMark {
  enum Type {
    eStart,
    eMid,
    eEnd,

    eTypeCount
  };

  float x, y, angle;
  Type type;
  nsSVGMark(float aX, float aY, float aAngle, Type aType) :
    x(aX), y(aY), angle(aAngle), type(aType) {}
};

typedef mozilla::dom::SVGGraphicsElement nsSVGPathGeometryElementBase;

class nsSVGPathGeometryElement : public nsSVGPathGeometryElementBase
{
protected:
  typedef mozilla::gfx::FillRule FillRule;
  typedef mozilla::gfx::Float Float;
  typedef mozilla::gfx::Path Path;
  typedef mozilla::gfx::PathBuilder PathBuilder;

public:
  explicit nsSVGPathGeometryElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  virtual bool AttributeDefinesGeometry(const nsIAtom *aName);

  /**
   * Returns true if this element's geometry depends on the width or height of its
   * coordinate context (typically the viewport established by its nearest <svg>
   * ancestor). In other words, returns true if one of the attributes for which
   * AttributeDefinesGeometry returns true has a percentage value.
   *
   * This could be moved up to a more general class so it can be used for non-leaf
   * elements, but that would require care and for now there's no need.
   */
  bool GeometryDependsOnCoordCtx();

  virtual bool IsMarkable();
  virtual void GetMarkPoints(nsTArray<nsSVGMark> *aMarks);

  /**
   * Returns a Path that can be used to paint, hit-test or calculate bounds for
   * this element. May return nullptr if there is no [valid] path.
   */
  virtual mozilla::TemporaryRef<Path> BuildPath(PathBuilder* aBuilder = nullptr) = 0;

  virtual mozilla::TemporaryRef<Path> GetPathForLengthOrPositionMeasuring();

  /**
   * Returns a PathBuilder object created using the current computed value of
   * the CSS property 'fill-rule' for this element.
   */
  mozilla::TemporaryRef<PathBuilder> CreatePathBuilder();

  /**
   * Returns the current computed value of the CSS property 'fill-rule' for
   * this element.
   */
  FillRule GetFillRule();
};

#endif
