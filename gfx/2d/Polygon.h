/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_POLYGON_H
#define MOZILLA_GFX_POLYGON_H

#include "Matrix.h"
#include "mozilla/InitializerList.h"
#include "mozilla/Move.h"
#include "nsTArray.h"
#include "Point.h"
#include "Triangle.h"

namespace mozilla {
namespace gfx {

// Polygon3DTyped stores the points of a convex planar polygon.
template<class Units>
class Polygon3DTyped {
public:
  Polygon3DTyped() {}

  explicit Polygon3DTyped(const std::initializer_list<Point3DTyped<Units>>& aPoints,
                          Point3DTyped<Units> aNormal =
                            Point3DTyped<Units>(0.0f, 0.0f, 1.0f))
    : mNormal(aNormal), mPoints(aPoints)
  {
#ifdef DEBUG
    EnsurePlanarPolygon();
#endif
  }

  explicit Polygon3DTyped(nsTArray<Point3DTyped<Units>>&& aPoints,
                          Point3DTyped<Units> aNormal =
                            Point3DTyped<Units>(0.0f, 0.0f, 1.0f))
    : mNormal(aNormal), mPoints(Move(aPoints))
  {
#ifdef DEBUG
    EnsurePlanarPolygon();
#endif
  }

  explicit Polygon3DTyped(const nsTArray<Point3DTyped<Units>>& aPoints,
                          Point3DTyped<Units> aNormal =
                            Point3DTyped<Units>(0.0f, 0.0f, 1.0f))
    : mNormal(aNormal), mPoints(aPoints)
  {
#ifdef DEBUG
    EnsurePlanarPolygon();
#endif
  }

  RectTyped<Units> BoundingBox() const
  {
    float minX, maxX, minY, maxY;
    minX = maxX = mPoints[0].x;
    minY = maxY = mPoints[0].y;

    for (const Point3DTyped<Units>& point : mPoints) {
      minX = std::min(point.x, minX);
      maxX = std::max(point.x, maxX);

      minY = std::min(point.y, minY);
      maxY = std::max(point.y, maxY);
    }

    return RectTyped<Units>(minX, minY, maxX - minX, maxY - minY);
  }

  nsTArray<float>
  CalculateDotProducts(const Polygon3DTyped<Units>& aPlane,
                       size_t& aPos, size_t& aNeg) const
  {
    // Point classification might produce incorrect results due to numerical
    // inaccuracies. Using an epsilon value makes the splitting plane "thicker".
    const float epsilon = 0.05f;

    MOZ_ASSERT(!aPlane.GetPoints().IsEmpty());
    const Point3DTyped<Units>& planeNormal = aPlane.GetNormal();
    const Point3DTyped<Units>& planePoint = aPlane[0];

    aPos = aNeg = 0;
    nsTArray<float> dotProducts;
    for (const Point3DTyped<Units>& point : mPoints) {
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
  Polygon3DTyped<Units> ClipPolygon(const RectTyped<Units>& aRect) const
  {
    Polygon3DTyped<Units> polygon(mPoints, mNormal);

    // Left edge
    ClipPolygonWithEdge(polygon, aRect.BottomLeft(), aRect.TopLeft());

    // Bottom edge
    ClipPolygonWithEdge(polygon, aRect.BottomRight(), aRect.BottomLeft());

    // Right edge
    ClipPolygonWithEdge(polygon, aRect.TopRight(), aRect.BottomRight());

    // Top edge
    ClipPolygonWithEdge(polygon, aRect.TopLeft(), aRect.TopRight());

    return polygon;
  }

  const Point3DTyped<Units>& GetNormal() const
  {
    return mNormal;
  }

  const nsTArray<Point3DTyped<Units>>& GetPoints() const
  {
    return mPoints;
  }

  const Point3DTyped<Units>& operator[](size_t aIndex) const
  {
    MOZ_ASSERT(mPoints.Length() > aIndex);
    return mPoints[aIndex];
  }

  void SplitPolygon(const Polygon3DTyped<Units>& aSplittingPlane,
                    const nsTArray<float>& aDots,
                    nsTArray<Point3DTyped<Units>>& aBackPoints,
                    nsTArray<Point3DTyped<Units>>& aFrontPoints) const
  {
    static const auto Sign = [](const float& f) {
      if (f > 0.0f) return 1;
      if (f < 0.0f) return -1;
      return 0;
    };

    const Point3DTyped<Units>& normal = aSplittingPlane.GetNormal();
    const size_t pointCount = mPoints.Length();

    for (size_t i = 0; i < pointCount; ++i) {
      size_t j = (i + 1) % pointCount;

      const Point3DTyped<Units>& a = mPoints[i];
      const Point3DTyped<Units>& b = mPoints[j];
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
        const Point3DTyped<Units> ab = b - a;
        const float dotAB = ab.DotProduct(normal);
        const float t = -dotA / dotAB;
        const Point3DTyped<Units> p = a + (ab * t);

        // Add the intersection point to both polygons.
        aBackPoints.AppendElement(p);
        aFrontPoints.AppendElement(p);
      }
    }
  }

  void TransformToLayerSpace(const Matrix4x4Typed<Units, Units>& aTransform)
  {
    TransformPoints(aTransform);
    mNormal = Point3DTyped<Units>(0.0f, 0.0f, 1.0f);
  }

  void TransformToScreenSpace(const Matrix4x4Typed<Units, Units>& aTransform)
  {
    TransformPoints(aTransform);

    // Normal vectors should be transformed using inverse transpose.
    mNormal = aTransform.Inverse().Transpose().TransformPoint(mNormal);
  }

  nsTArray<TriangleTyped<Units>> Triangulate() const
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

private:
  void ClipPolygonWithEdge(Polygon3DTyped<Units>& aPolygon,
                           const PointTyped<Units>& aFirst,
                           const PointTyped<Units>& aSecond) const
  {
    const Point3DTyped<Units> a(aFirst.x, aFirst.y, 0.0f);
    const Point3DTyped<Units> b(aSecond.x, aSecond.y, 0.0f);
    const Point3DTyped<Units> normal(b.y - a.y, a.x - b.x, 0.0f);
    Polygon3DTyped<Units> plane({a, b}, normal);

    size_t pos, neg;
    nsTArray<float> dots = aPolygon.CalculateDotProducts(plane, pos, neg);

    nsTArray<Point3DTyped<Units>> backPoints, frontPoints;
    aPolygon.SplitPolygon(plane, dots, backPoints, frontPoints);

    // Only use the points that are behind the clipping plane.
    aPolygon = Polygon3DTyped<Units>(Move(backPoints), aPolygon.GetNormal());
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
    Point3DTyped<Units> normal;

    for (size_t i = 1; i < mPoints.Length() - 1; ++i) {
      normal +=
        (mPoints[i] - mPoints[0]).CrossProduct(mPoints[i + 1] - mPoints[0]);
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
    for (const Point3DTyped<Units>& point : mPoints) {
      float d = normal.DotProduct(point - mPoints[0]);
      MOZ_ASSERT(std::abs(d) < epsilon);
    }
  }
#endif
  void TransformPoints(const Matrix4x4Typed<Units, Units>& aTransform)
  {
    for (Point3DTyped<Units>& point : mPoints) {
      point = aTransform.TransformPoint(point);
    }
  }

  Point3DTyped<Units> mNormal;
  nsTArray<Point3DTyped<Units>> mPoints;
};

typedef Polygon3DTyped<UnknownUnits> Polygon3D;

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_POLYGON_H */
