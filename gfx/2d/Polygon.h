/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef MOZILLA_GFX_POLYGON_H
#define MOZILLA_GFX_POLYGON_H

#include "mozilla/InitializerList.h"
#include "nsTArray.h"
#include "Point.h"

namespace mozilla {
namespace gfx {


// BasePolygon3D stores the points of a convex planar polygon.
template<class Units>
class BasePolygon3D {
public:
  explicit BasePolygon3D(const std::initializer_list<Point3DTyped<Units>>& aPoints)
    : mPoints(aPoints)
  {
    CalculateNormal();
  }

  explicit BasePolygon3D(nsTArray<Point3DTyped<Units>> && aPoints)
    : mPoints(aPoints)
  {
    CalculateNormal();
  }

  const Point3DTyped<Units>& GetNormal() const
  {
    return mNormal;
  }

  const nsTArray<Point3D>& GetPoints() const
  {
    return mPoints;
  }

  const Point3DTyped<Units>& operator[](size_t aIndex) const
  {
    MOZ_ASSERT(mPoints.Length() > aIndex);
    return mPoints[aIndex];
  }

private:
  // Calculates the polygon surface normal.
  // The resulting normal vector will point towards the viewer when the polygon
  // has a counter-clockwise winding order from the perspective of the viewer.
  void CalculateNormal()
  {
    MOZ_ASSERT(mPoints.Length() >= 3);

    // This normal calculation method works only for planar polygons.
    mNormal = (mPoints[1] - mPoints[0]).CrossProduct(mPoints[2] - mPoints[0]);

    const float epsilon = 0.001f;
    if (mNormal.Length() < epsilon) {
      // The first three points were too close.
      // Use more points for better accuracy.
      for (size_t i = 2; i < mPoints.Length() - 1; ++i) {
        mNormal += (mPoints[i] - mPoints[0]).CrossProduct(mPoints[i + 1] - mPoints[0]);
      }
    }

    mNormal.Normalize();

    #ifdef DEBUG
    // Ensure that the polygon is planar.
    // http://mathworld.wolfram.com/Point-PlaneDistance.html
    for (const gfx::Point3DTyped<Units>& point : mPoints) {
      float d = mNormal.DotProduct(point - mPoints[0]);
      MOZ_ASSERT(std::abs(d) < epsilon);
    }
    #endif
  }

  nsTArray<Point3DTyped<Units>> mPoints;
  Point3DTyped<Units> mNormal;
};

typedef BasePolygon3D<UnknownUnits> Polygon3D;

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_POLYGON_H */
