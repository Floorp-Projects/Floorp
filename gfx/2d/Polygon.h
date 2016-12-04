/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_POLYGON_H
#define MOZILLA_GFX_POLYGON_H

#include "Matrix.h"
#include "mozilla/Move.h"
#include "nsTArray.h"
#include "Point.h"
#include "Triangle.h"

#include <initializer_list>

namespace mozilla {
namespace gfx {

template<class Units>
Point4DTyped<Units>
CalculateEdgeIntersect(const Point4DTyped<Units>& aFirst,
                       const Point4DTyped<Units>& aSecond)
{
  static const float w = 0.00001f;
  const float t = (w - aFirst.w) / (aSecond.w - aFirst.w);
  return aFirst + (aSecond - aFirst) * t;
}

template<class Units>
nsTArray<Point4DTyped<Units>>
ClipHomogeneous(const nsTArray<Point4DTyped<Units>>& aPoints)
{
  nsTArray<Point4DTyped<Units>> outPoints;

  const size_t pointCount = aPoints.Length();
  for (size_t i = 0; i < pointCount; ++i) {
    const Point4DTyped<Units>& first = aPoints[i];
    const Point4DTyped<Units>& second = aPoints[(i + 1) % pointCount];

    MOZ_ASSERT(first.w != 0.0f || second.w != 0.0f);

    if (first.w > 0.0f) {
      outPoints.AppendElement(first);
    }

    if ((first.w <= 0.0f) ^ (second.w <= 0.0f)) {
      outPoints.AppendElement(CalculateEdgeIntersect(first, second));
    }
  }

  return outPoints;
}

template<class Units>
nsTArray<Point4DTyped<Units>>
ToPoints4D(const nsTArray<Point3DTyped<Units>>& aPoints)
{
  nsTArray<Point4DTyped<Units>> points;

  for (const Point3DTyped<Units>& point : aPoints) {
    points.AppendElement(Point4DTyped<Units>(point));
  }

  return points;
}

// PolygonTyped stores the points of a convex planar polygon.
template<class Units>
class PolygonTyped {
  typedef Point3DTyped<Units> Point3DType;
  typedef Point4DTyped<Units> Point4DType;

public:
  PolygonTyped() {}

  explicit PolygonTyped(const std::initializer_list<Point3DType>& aPoints)
    : mNormal(DefaultNormal()),
      mPoints(ToPoints4D(nsTArray<Point3DType>(aPoints)))
  {
#ifdef DEBUG
    EnsurePlanarPolygon();
#endif
  }

  explicit PolygonTyped(const nsTArray<Point3DType>& aPoints)
    : mNormal(DefaultNormal()), mPoints(ToPoints4D(aPoints))
  {
#ifdef DEBUG
    EnsurePlanarPolygon();
#endif
  }

  explicit PolygonTyped(const nsTArray<Point4DType>& aPoints,
                          const Point4DType& aNormal = DefaultNormal())
    : mNormal(aNormal), mPoints(aPoints)
  {}

  explicit PolygonTyped(nsTArray<Point4DType>&& aPoints,
                          const Point4DType& aNormal = DefaultNormal())
    : mNormal(aNormal), mPoints(Move(aPoints))
  {}

  RectTyped<Units> BoundingBox() const
  {
    if (mPoints.IsEmpty()) {
      return RectTyped<Units>();
    }

    float minX, maxX, minY, maxY;
    minX = maxX = mPoints[0].x;
    minY = maxY = mPoints[0].y;

    for (const Point4DType& point : mPoints) {
      minX = std::min(point.x, minX);
      maxX = std::max(point.x, maxX);

      minY = std::min(point.y, minY);
      maxY = std::max(point.y, maxY);
    }

    return RectTyped<Units>(minX, minY, maxX - minX, maxY - minY);
  }

  nsTArray<float> CalculateDotProducts(const PolygonTyped<Units>& aPlane,
                                       size_t& aPos, size_t& aNeg) const
  {
    // Point classification might produce incorrect results due to numerical
    // inaccuracies. Using an epsilon value makes the splitting plane "thicker".
    const float epsilon = 0.05f;

    MOZ_ASSERT(!aPlane.GetPoints().IsEmpty());
    const Point4DType& planeNormal = aPlane.GetNormal();
    const Point4DType& planePoint = aPlane[0];

    aPos = aNeg = 0;
    nsTArray<float> dotProducts;

    for (const Point4DType& point : mPoints) {
      float dot = (point - planePoint).DotProduct(planeNormal);

      if (dot > epsilon) {
        aPos++;
      } else if (dot < -epsilon) {
        aNeg++;
      } else {
        // The point is within the thick plane.
        dot = 0.0f;
      }

      dotProducts.AppendElement(dot);
    }

    return dotProducts;
  }

  // Clips the polygon against the given 2D rectangle.
  PolygonTyped<Units> ClipPolygon(const RectTyped<Units>& aRect) const
  {
    if (aRect.IsEmpty()) {
      return PolygonTyped<Units>();
    }

    return ClipPolygon(FromRect(aRect));
  }

  // Clips the polygon against the given polygon in 2D.
  PolygonTyped<Units> ClipPolygon(const PolygonTyped<Units>& aPolygon) const
  {
    const nsTArray<Point4DType>& points = aPolygon.GetPoints();

    if (mPoints.IsEmpty() || points.IsEmpty()) {
      return PolygonTyped<Units>();
    }

    PolygonTyped<Units> polygon(mPoints, mNormal);

    const size_t pointCount = points.Length();
    for (size_t i = 0; i < pointCount; ++i) {
      const Point4DType p1 = points[(i + 1) % pointCount];
      const Point4DType p2 = points[i];

      const Point4DType normal(p2.y - p1.y, p1.x - p2.x, 0.0f, 0.0f);
      const PolygonTyped<Units> plane({p1, p2}, normal);

      ClipPolygonWithPlane(polygon, plane);
    }

    if (polygon.GetPoints().Length() < 3) {
      return PolygonTyped<Units>();
    }

    return polygon;
  }

  static PolygonTyped<Units> FromRect(const RectTyped<Units>& aRect)
  {
    return PolygonTyped<Units> {
      Point3DType(aRect.x, aRect.y, 0.0f),
      Point3DType(aRect.x, aRect.y + aRect.height, 0.0f),
      Point3DType(aRect.x + aRect.width, aRect.y + aRect.height, 0.0f),
      Point3DType(aRect.x + aRect.width, aRect.y, 0.0f)
    };
  }

  const Point4DType& GetNormal() const
  {
    return mNormal;
  }

  const nsTArray<Point4DType>& GetPoints() const
  {
    return mPoints;
  }

  const Point4DType& operator[](size_t aIndex) const
  {
    MOZ_ASSERT(mPoints.Length() > aIndex);
    return mPoints[aIndex];
  }

  void SplitPolygon(const Point4DType& aNormal,
                    const nsTArray<float>& aDots,
                    nsTArray<Point4DType>& aBackPoints,
                    nsTArray<Point4DType>& aFrontPoints) const
  {
    static const auto Sign = [](const float& f) {
      if (f > 0.0f) return 1;
      if (f < 0.0f) return -1;
      return 0;
    };

    const size_t pointCount = mPoints.Length();
    for (size_t i = 0; i < pointCount; ++i) {
      size_t j = (i + 1) % pointCount;

      const Point4DType& a = mPoints[i];
      const Point4DType& b = mPoints[j];
      const float dotA = aDots[i];
      const float dotB = aDots[j];

      // The point is in front of or on the plane.
      if (dotA >= 0) {
        aFrontPoints.AppendElement(a);
      }

      // The point is behind or on the plane.
      if (dotA <= 0) {
        aBackPoints.AppendElement(a);
      }

      // If the sign of the dot products changes between two consecutive
      // vertices, then the plane intersects with the polygon edge.
      // The case where the polygon edge is within the plane is handled above.
      if (Sign(dotA) && Sign(dotB) && Sign(dotA) != Sign(dotB)) {
        // Calculate the line segment and plane intersection point.
        const Point4DType ab = b - a;
        const float dotAB = ab.DotProduct(aNormal);
        const float t = -dotA / dotAB;
        const Point4DType p = a + (ab * t);

        // Add the intersection point to both polygons.
        aBackPoints.AppendElement(p);
        aFrontPoints.AppendElement(p);
      }
    }
  }

  nsTArray<TriangleTyped<Units>> ToTriangles() const
  {
    nsTArray<TriangleTyped<Units>> triangles;

    if (mPoints.Length() < 3) {
      return triangles;
    }

    for (size_t i = 1; i < mPoints.Length() - 1; ++i) {
      TriangleTyped<Units> triangle(Point(mPoints[0].x, mPoints[0].y),
                                    Point(mPoints[i].x, mPoints[i].y),
                                    Point(mPoints[i+1].x, mPoints[i+1].y));
      triangles.AppendElement(Move(triangle));
    }

    return triangles;
  }

  void TransformToLayerSpace(const Matrix4x4Typed<Units, Units>& aTransform)
  {
    TransformPoints(aTransform, true);
    mNormal = DefaultNormal();
  }

  void TransformToScreenSpace(const Matrix4x4Typed<Units, Units>& aTransform)
  {
    TransformPoints(aTransform, false);
    mPoints = ClipHomogeneous(mPoints);

    // Normal vectors should be transformed using inverse transpose.
    mNormal = aTransform.Inverse().Transpose().TransformPoint(mNormal);
  }

private:
  void ClipPolygonWithPlane(PolygonTyped<Units>& aPolygon,
                            const PolygonTyped<Units>& aPlane) const
  {
    size_t pos, neg;
    const nsTArray<float> dots =
      aPolygon.CalculateDotProducts(aPlane, pos, neg);

    nsTArray<Point4DType> backPoints, frontPoints;
    aPolygon.SplitPolygon(aPlane.GetNormal(), dots, backPoints, frontPoints);

    // Only use the points that are behind the clipping plane.
    aPolygon = PolygonTyped<Units>(Move(backPoints), aPolygon.GetNormal());
  }

  static Point4DType DefaultNormal()
  {
    return Point4DType(0.0f, 0.0f, 1.0f, 0.0f);
  }

#ifdef DEBUG
  void EnsurePlanarPolygon() const
  {
    if (mPoints.Length() <= 3) {
      // Polygons with three or less points are guaranteed to be planar.
      return;
    }

    // This normal calculation method works only for planar polygons.
    // The resulting normal vector will point towards the viewer when the
    // polygon has a counter-clockwise winding order from the perspective
    // of the viewer.
    Point3DType normal;
    const Point3DType p0 = mPoints[0].As3DPoint();

    for (size_t i = 1; i < mPoints.Length() - 1; ++i) {
      const Point3DType p1 = mPoints[i].As3DPoint();
      const Point3DType p2 = mPoints[i + 1].As3DPoint();

      normal += (p1 - p0).CrossProduct(p2 - p0);
    }

    // Ensure that at least one component is greater than zero.
    // This avoids division by zero when normalizing the vector.
    bool hasNonZeroComponent = std::abs(normal.x) > 0.0f ||
                               std::abs(normal.y) > 0.0f ||
                               std::abs(normal.z) > 0.0f;

    MOZ_ASSERT(hasNonZeroComponent);

    normal.Normalize();

    // Ensure that the polygon is planar.
    // http://mathworld.wolfram.com/Point-PlaneDistance.html
    const float epsilon = 0.01f;
    for (const Point4DType& point : mPoints) {
      const Point3DType p1 = point.As3DPoint();
      const float d = normal.DotProduct(p1 - p0);

      MOZ_ASSERT(std::abs(d) < epsilon);
    }
  }
#endif

  void TransformPoints(const Matrix4x4Typed<Units, Units>& aTransform,
                       const bool aDivideByW)
  {
    for (Point4DType& point : mPoints) {
      point = aTransform.TransformPoint(point);

      if (aDivideByW && point.w > 0.0f) {
          point = point / point.w;
      }
    }
  }

  Point4DType mNormal;
  nsTArray<Point4DType> mPoints;
};

typedef PolygonTyped<UnknownUnits> Polygon;

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_POLYGON_H */
