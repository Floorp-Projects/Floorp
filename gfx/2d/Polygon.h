/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef MOZILLA_GFX_POLYGON_H
#define MOZILLA_GFX_POLYGON_H

#include "Matrix.h"
#include "mozilla/InitializerList.h"
#include "nsTArray.h"
#include "Point.h"

namespace mozilla {
namespace gfx {

// BasePolygon3D stores the points of a convex planar polygon.
template<class Units>
class BasePolygon3D {
public:
  BasePolygon3D() {}

  explicit BasePolygon3D(const std::initializer_list<Point3DTyped<Units>>& aPoints,
                         Point3DTyped<Units> aNormal =
                           Point3DTyped<Units>(0.0f, 0.0f, 1.0f))
    : mNormal(aNormal), mPoints(aPoints)
  {
#ifdef DEBUG
    EnsurePlanarPolygon();
#endif
  }

  explicit BasePolygon3D(nsTArray<Point3DTyped<Units>>&& aPoints,
                         Point3DTyped<Units> aNormal =
                           Point3DTyped<Units>(0.0f, 0.0f, 1.0f))
    : mNormal(aNormal), mPoints(aPoints)
  {
#ifdef DEBUG
    EnsurePlanarPolygon();
#endif
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

  void TransformToLayerSpace(const Matrix4x4Typed<Units, Units>& aInverseTransform)
  {
    TransformPoints(aInverseTransform);
    mNormal = Point3DTyped<Units>(0.0f, 0.0f, 1.0f);
  }

  void TransformToScreenSpace(const Matrix4x4Typed<Units, Units>& aTransform)
  {
    TransformPoints(aTransform);

    // Normal vectors should be transformed using inverse transpose.
    mNormal = aTransform.Inverse().Transpose().TransformPoint(mNormal);
  }

private:

#ifdef DEBUG
  void EnsurePlanarPolygon() const
  {
    MOZ_ASSERT(mPoints.Length() >= 3);

    // This normal calculation method works only for planar polygons.
    // The resulting normal vector will point towards the viewer when the polygon
    // has a counter-clockwise winding order from the perspective of the viewer.
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

typedef BasePolygon3D<UnknownUnits> Polygon3D;

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_POLYGON_H */
