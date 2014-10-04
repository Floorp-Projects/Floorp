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
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::FillRule FillRule;
  typedef mozilla::gfx::Float Float;
  typedef mozilla::gfx::Path Path;
  typedef mozilla::gfx::PathBuilder PathBuilder;

public:
  explicit nsSVGPathGeometryElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) MOZ_OVERRIDE;

  /**
   * Causes this element to discard any Path object that GetOrBuildPath may
   * have cached.
   */
  virtual void ClearAnyCachedPath() MOZ_OVERRIDE MOZ_FINAL {
    mCachedPath = nullptr;
  }

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
   * this element. May return nullptr if there is no [valid] path. The path
   * that is created may be cached and returned on subsequent calls.
   */
  virtual mozilla::TemporaryRef<Path> GetOrBuildPath(const DrawTarget& aDrawTarget,
                                                     FillRule fillRule);

  /**
   * The same as GetOrBuildPath, but bypasses the cache (neither returns any
   * previously cached Path, nor caches the Path that in does return).
   * this element. May return nullptr if there is no [valid] path.
   */
  virtual mozilla::TemporaryRef<Path> BuildPath(PathBuilder* aBuilder) = 0;

  /**
   * Returns a Path that can be used to measure the length of this elements
   * path, or to find the position at a given distance along it.
   *
   * This is currently equivalent to calling GetOrBuildPath, but it may not be
   * in the future. The reason for this function to be separate from
   * GetOrBuildPath is because SVGPathData::BuildPath inserts small lines into
   * the path if zero length subpaths are encountered, in order to implement
   * the SVG specifications requirements that zero length subpaths should
   * render circles/squares if stroke-linecap is round/square, respectively.
   * In principle these inserted lines could interfere with path measurement,
   * so we keep callers that are looking to do measurement separate in case we
   * run into problems with the inserted lines negatively affecting measuring
   * for content.
   */
  virtual mozilla::TemporaryRef<Path> GetOrBuildPathForMeasuring();

  /**
   * Returns the current computed value of the CSS property 'fill-rule' for
   * this element.
   */
  FillRule GetFillRule();

protected:
  mutable mozilla::RefPtr<Path> mCachedPath;
};

#endif
