/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGGeometryElement_h
#define mozilla_dom_SVGGeometryElement_h

#include "mozilla/gfx/2D.h"
#include "SVGGraphicsElement.h"
#include "nsISVGPoint.h"
#include "nsSVGNumber2.h"

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

namespace mozilla {
namespace dom {

class SVGAnimatedNumber;

typedef mozilla::dom::SVGGraphicsElement SVGGeometryElementBase;

class SVGGeometryElement : public SVGGeometryElementBase
{
protected:
  typedef mozilla::gfx::CapStyle CapStyle;
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::FillRule FillRule;
  typedef mozilla::gfx::Float Float;
  typedef mozilla::gfx::Matrix Matrix;
  typedef mozilla::gfx::Path Path;
  typedef mozilla::gfx::Point Point;
  typedef mozilla::gfx::PathBuilder PathBuilder;
  typedef mozilla::gfx::Rect Rect;
  typedef mozilla::gfx::StrokeOptions StrokeOptions;

public:
  explicit SVGGeometryElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) override;

  /**
   * Causes this element to discard any Path object that GetOrBuildPath may
   * have cached.
   */
  virtual void ClearAnyCachedPath() override final {
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
   * A method that can be faster than using a Moz2D Path and calling GetBounds/
   * GetStrokedBounds on it.  It also helps us avoid rounding error for simple
   * shapes and simple transforms where the Moz2D Path backends can fail to
   * produce the clean integer bounds that content authors expect in some cases.
   *
   * If |aToNonScalingStrokeSpace| is non-null then |aBounds|, which is computed
   * in bounds space, has the property that it's the smallest (axis-aligned)
   * rectangular bound containing the image of this shape as stroked in
   * non-scaling-stroke space.  (When all transforms involved are rectilinear
   * the bounds of the image of |aBounds| in non-scaling-stroke space will be
   * tight, but if there are non-rectilinear transforms involved then that may
   * be impossible and this method will return false).
   *
   * If |aToNonScalingStrokeSpace| is non-null then |*aToNonScalingStrokeSpace|
   * must be non-singular.
   */
  virtual bool GetGeometryBounds(Rect* aBounds, const StrokeOptions& aStrokeOptions,
                                 const Matrix& aToBoundsSpace,
                                 const Matrix* aToNonScalingStrokeSpace = nullptr) {
    return false;
  }

  /**
   * For use with GetAsSimplePath.
   */
  class SimplePath
  {
  public:
    SimplePath()
      : mType(NONE)
    {}
    bool IsPath() const {
      return mType != NONE;
    }
    void SetRect(Float x, Float y, Float width, Float height) {
      mX = x; mY = y, mWidthOrX2 = width, mHeightOrY2 = height;
      mType = RECT;
    }
    Rect AsRect() const {
      MOZ_ASSERT(mType == RECT);
      return Rect(mX, mY, mWidthOrX2, mHeightOrY2);
    }
    bool IsRect() const {
      return mType == RECT;
    }
    void SetLine(Float x1, Float y1, Float x2, Float y2) {
      mX = x1, mY = y1, mWidthOrX2 = x2, mHeightOrY2 = y2;
      mType = LINE;
    }
    Point Point1() const {
      MOZ_ASSERT(mType == LINE);
      return Point(mX, mY);
    }
    Point Point2() const {
      MOZ_ASSERT(mType == LINE);
      return Point(mWidthOrX2, mHeightOrY2);
    }
    bool IsLine() const {
      return mType == LINE;
    }
    void Reset() {
      mType = NONE;
    }
  private:
    enum Type {
      NONE, RECT, LINE
    };
    Float mX, mY, mWidthOrX2, mHeightOrY2;
    Type mType;
  };

  /**
   * For some platforms there is significant overhead to creating and painting
   * a Moz2D Path object. For Rects and lines it is better to get the path data
   * using this method and then use the optimized DrawTarget methods for
   * filling/stroking rects and lines.
   */
  virtual void GetAsSimplePath(SimplePath* aSimplePath) {
    aSimplePath->Reset();
  }

  /**
   * Returns a Path that can be used to paint, hit-test or calculate bounds for
   * this element. May return nullptr if there is no [valid] path. The path
   * that is created may be cached and returned on subsequent calls.
   */
  virtual already_AddRefed<Path> GetOrBuildPath(const DrawTarget& aDrawTarget,
                                                FillRule fillRule);

  /**
   * The same as GetOrBuildPath, but bypasses the cache (neither returns any
   * previously cached Path, nor caches the Path that in does return).
   * this element. May return nullptr if there is no [valid] path.
   */
  virtual already_AddRefed<Path> BuildPath(PathBuilder* aBuilder) = 0;

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
  virtual already_AddRefed<Path> GetOrBuildPathForMeasuring();

  /**
   * Returns the current computed value of the CSS property 'fill-rule' for
   * this element.
   */
  FillRule GetFillRule();

  // WebIDL
  already_AddRefed<SVGAnimatedNumber> PathLength();
  float GetTotalLength();
  already_AddRefed<nsISVGPoint>
    GetPointAtLength(float distance, ErrorResult& rv);

protected:
  // nsSVGElement method
  virtual NumberAttributesInfo GetNumberInfo() override;

  nsSVGNumber2 mPathLength;
  static NumberInfo sNumberInfo;
  mutable RefPtr<Path> mCachedPath;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGGeometryElement_h
