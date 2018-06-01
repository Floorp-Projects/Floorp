/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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

/**
 * Calculates the w = 0 intersection point for the edge defined by
 * |aFirst| and |aSecond|.
 */
template<class Units>
Point4DTyped<Units>
CalculateEdgeIntersect(const Point4DTyped<Units>& aFirst,
                       const Point4DTyped<Units>& aSecond)
{
  static const float w = 0.00001f;
  const float t = (w - aFirst.w) / (aSecond.w - aFirst.w);
  return aFirst + (aSecond - aFirst) * t;
}

/**
 * Clips the polygon defined by |aPoints| so that there are no points with
 * w <= 0.
 */
template<class Units>
nsTArray<Point4DTyped<Units>>
ClipPointsAtInfinity(const nsTArray<Point4DTyped<Units>>& aPoints)
{
  nsTArray<Point4DTyped<Units>> outPoints(aPoints.Length());

  const size_t pointCount = aPoints.Length();
  for (size_t i = 0; i < pointCount; ++i) {
    const Point4DTyped<Units>& first = aPoints[i];
    const Point4DTyped<Units>& second = aPoints[(i + 1) % pointCount];

    if (!first.w || !second.w) {
      // Skip edges at infinity.
      continue;
    }

    if (first.w > 0.0f) {
      outPoints.AppendElement(first);
    }

    if ((first.w <= 0.0f) ^ (second.w <= 0.0f)) {
      outPoints.AppendElement(CalculateEdgeIntersect(first, second));
    }
  }

  return outPoints;
}

/**
 * Calculates the distances between the points in |aPoints| and the plane
 * defined by |aPlaneNormal| and |aPlanePoint|.
 */
template<class Units>
nsTArray<float>
CalculatePointPlaneDistances(const nsTArray<Point4DTyped<Units>>& aPoints,
                             const Point4DTyped<Units>& aPlaneNormal,
                             const Point4DTyped<Units>& aPlanePoint,
                             size_t& aPos, size_t& aNeg)
{
  // Point classification might produce incorrect results due to numerical
  // inaccuracies. Using an epsilon value makes the splitting plane "thicker".
  const float epsilon = 0.05f;

  aPos = aNeg = 0;
  nsTArray<float> distances(aPoints.Length());

  for (const Point4DTyped<Units>& point : aPoints) {
    float dot = (point - aPlanePoint).DotProduct(aPlaneNormal);

    if (dot > epsilon) {
      aPos++;
    } else if (dot < -epsilon) {
      aNeg++;
    } else {
      // The point is within the thick plane.
      dot = 0.0f;
    }

    distances.AppendElement(dot);
  }

  return distances;
}

/**
 * Clips the polygon defined by |aPoints|. The clipping uses previously
 * calculated plane to point distances and the plane normal |aNormal|.
 * The result of clipping is stored in |aBackPoints| and |aFrontPoints|.
 */
template<class Units>
void
ClipPointsWithPlane(const nsTArray<Point4DTyped<Units>>& aPoints,
                    const Point4DTyped<Units>& aNormal,
                    const nsTArray<float>& aDots,
                    nsTArray<Point4DTyped<Units>>& aBackPoints,
                    nsTArray<Point4DTyped<Units>>& aFrontPoints)
{
  static const auto Sign = [](const float& f) {
    if (f > 0.0f) return 1;
    if (f < 0.0f) return -1;
    return 0;
  };

  const size_t pointCount = aPoints.Length();
  for (size_t i = 0; i < pointCount; ++i) {
    size_t j = (i + 1) % pointCount;

    const Point4DTyped<Units>& a = aPoints[i];
    const Point4DTyped<Units>& b = aPoints[j];
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
      const Point4DTyped<Units> ab = b - a;
      const float dotAB = ab.DotProduct(aNormal);
      const float t = -dotA / dotAB;
      const Point4DTyped<Units> p = a + (ab * t);

      // Add the intersection point to both polygons.
      aBackPoints.AppendElement(p);
      aFrontPoints.AppendElement(p);
    }
  }
}

/**
 * PolygonTyped stores the points of a convex planar polygon.
 */
template<class Units>
class PolygonTyped {
  typedef Point3DTyped<Units> Point3DType;
  typedef Point4DTyped<Units> Point4DType;

public:
  PolygonTyped() {}

  explicit PolygonTyped(const nsTArray<Point4DType>& aPoints,
                        const Point4DType& aNormal = DefaultNormal())
    : mNormal(aNormal), mPoints(aPoints) {}

  explicit PolygonTyped(nsTArray<Point4DType>&& aPoints,
                        const Point4DType& aNormal = DefaultNormal())
    : mNormal(aNormal), mPoints(std::move(aPoints)) {}

  explicit PolygonTyped(const std::initializer_list<Point4DType>& aPoints,
                        const Point4DType& aNormal = DefaultNormal())
    : mNormal(aNormal), mPoints(aPoints)
  {
#ifdef DEBUG
    EnsurePlanarPolygon();
#endif
  }

  /**
   * Returns the smallest 2D rectangle that can fully cover the polygon.
   */
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

  /**
   * Clips the polygon against the given 2D rectangle.
   */
  PolygonTyped<Units> ClipPolygon(const RectTyped<Units>& aRect) const
  {
    if (aRect.IsEmpty()) {
      return PolygonTyped<Units>();
    }

    return ClipPolygon(FromRect(aRect));
  }

  /**
   * Clips this polygon against |aPolygon| in 2D and returns a new polygon.
   */
  PolygonTyped<Units> ClipPolygon(const PolygonTyped<Units>& aPolygon) const
  {
    const nsTArray<Point4DType>& points = aPolygon.GetPoints();

    if (mPoints.IsEmpty() || points.IsEmpty()) {
      return PolygonTyped<Units>();
    }

    nsTArray<Point4DType> clippedPoints(mPoints);

    size_t pos, neg;
    nsTArray<Point4DType> backPoints(4), frontPoints(4);

    // Iterate over all the edges of the clipping polygon |aPolygon| and clip
    // this polygon against the edges.
    const size_t pointCount = points.Length();
    for (size_t i = 0; i < pointCount; ++i) {
      const Point4DType p1 = points[(i + 1) % pointCount];
      const Point4DType p2 = points[i];

      // Calculate the normal for the edge defined by |p1| and |p2|.
      const Point4DType normal(p2.y - p1.y, p1.x - p2.x, 0.0f, 0.0f);

      // Calculate the distances between the points of the polygon and the
      // plane defined by |aPolygon|.
      const nsTArray<float> distances =
        CalculatePointPlaneDistances(clippedPoints, normal, p1, pos, neg);

      backPoints.ClearAndRetainStorage();
      frontPoints.ClearAndRetainStorage();

      // Clip the polygon points using the previously calculated distances.
      ClipPointsWithPlane(clippedPoints, normal, distances,
                          backPoints, frontPoints);

      // Only use the points behind the clipping plane.
      clippedPoints = std::move(backPoints);

      if (clippedPoints.Length() < 3) {
        // The clipping created a polygon with no area.
        return PolygonTyped<Units>();
      }
    }

    return PolygonTyped<Units>(std::move(clippedPoints), mNormal);
  }

  /**
   * Returns a new polygon containing the bounds of the given 2D rectangle.
   */
  static PolygonTyped<Units> FromRect(const RectTyped<Units>& aRect)
  {
    nsTArray<Point4DType> points {
      Point4DType(aRect.X(), aRect.Y(), 0.0f, 1.0f),
      Point4DType(aRect.X(), aRect.YMost(), 0.0f, 1.0f),
      Point4DType(aRect.XMost(), aRect.YMost(), 0.0f, 1.0f),
      Point4DType(aRect.XMost(), aRect.Y(), 0.0f, 1.0f)
    };

    return PolygonTyped<Units>(std::move(points));
  }

  const Point4DType& GetNormal() const
  {
    return mNormal;
  }

  const nsTArray<Point4DType>& GetPoints() const
  {
    return mPoints;
  }

  bool IsEmpty() const
  {
    // If the polygon has less than three points, it has no visible area.
    return mPoints.Length() < 3;
  }

  /**
   * Returns a list of triangles covering the polygon.
   */
  nsTArray<TriangleTyped<Units>> ToTriangles() const
  {
    nsTArray<TriangleTyped<Units>> triangles;

    if (IsEmpty()) {
      return triangles;
    }

    // This fan triangulation method only works for convex polygons.
    for (size_t i = 1; i < mPoints.Length() - 1; ++i) {
      TriangleTyped<Units> triangle(Point(mPoints[0].x, mPoints[0].y),
                                    Point(mPoints[i].x, mPoints[i].y),
                                    Point(mPoints[i + 1].x, mPoints[i + 1].y));
      triangles.AppendElement(std::move(triangle));
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
    MOZ_ASSERT(!aTransform.IsSingular());

    TransformPoints(aTransform, false);

    // Perspective projection transformation might produce points with w <= 0,
    // so we need to clip these points.
    mPoints = ClipPointsAtInfinity(mPoints);

    // Normal vectors should be transformed using inverse transpose.
    mNormal = aTransform.Inverse().Transpose().TransformPoint(mNormal);
  }

private:
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
