/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BSPTree.h"
#include "mozilla/gfx/Polygon.h"

namespace mozilla {
namespace layers {

gfx::Polygon3D PopFront(std::deque<gfx::Polygon3D>& aPolygons)
{
  gfx::Polygon3D polygon = std::move(aPolygons.front());
  aPolygons.pop_front();
  return polygon;
}

namespace {

static int sign(float d) {
  if (d > 0) return 1;
  if (d < 0) return -1;

  return 0;
}

}

void
BSPTree::BuildDrawOrder(const UniquePtr<BSPTreeNode>& aNode,
                        nsTArray<gfx::Polygon3D>& aPolygons) const
{
  const gfx::Point3D& normal = aNode->First().GetNormal();

  UniquePtr<BSPTreeNode> *front = &aNode->front;
  UniquePtr<BSPTreeNode> *back = &aNode->back;

  // Since the goal is to return the draw order from back to front, we reverse
  // the traversal order if the current polygon is facing towards the camera.
  const bool reverseOrder = normal.z > 0.0f;

  if (reverseOrder) {
    std::swap(front, back);
  }

  if (*front) {
    BuildDrawOrder(*front, aPolygons);
  }

  for (gfx::Polygon3D& polygon : aNode->polygons) {
    aPolygons.AppendElement(std::move(polygon));
  }

  if (*back) {
    BuildDrawOrder(*back, aPolygons);
  }
}

nsTArray<float>
BSPTree::CalculateDotProduct(const gfx::Polygon3D& aFirst,
                             const gfx::Polygon3D& aSecond,
                             size_t& aPos, size_t& aNeg) const
{
  // Point classification might produce incorrect results due to numerical
  // inaccuracies. Using an epsilon value makes the splitting plane "thicker".
  const float epsilon = 0.05f;

  const gfx::Point3D& normal = aFirst.GetNormal();
  const gfx::Point3D& planePoint = aFirst[0];

  nsTArray<float> dotProducts;

  for (const gfx::Point3D& point : aSecond.GetPoints()) {
    float dot = (point - planePoint).DotProduct(normal);

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

void
BSPTree::BuildTree(UniquePtr<BSPTreeNode>& aRoot,
                   std::deque<gfx::Polygon3D>& aPolygons)
{
  if (aPolygons.empty()) {
    return;
  }

  const gfx::Polygon3D& splittingPlane = aRoot->First();
  std::deque<gfx::Polygon3D> backPolygons, frontPolygons;

  for (gfx::Polygon3D& polygon : aPolygons) {
    size_t pos = 0, neg = 0;
    nsTArray<float> dots = CalculateDotProduct(splittingPlane, polygon,
                                               pos, neg);

    // Back polygon
    if (pos == 0 && neg > 0) {
      backPolygons.push_back(std::move(polygon));
    }
    // Front polygon
    else if (pos > 0 && neg == 0) {
     frontPolygons.push_back(std::move(polygon));
    }
    // Coplanar polygon
    else if (pos == 0 && neg == 0) {
      aRoot->polygons.push_back(std::move(polygon));
    }
    // Polygon intersects with the splitting plane.
    else if (pos > 0 && neg > 0) {
      nsTArray<gfx::Point3D> backPoints, frontPoints;
      SplitPolygon(splittingPlane, polygon, dots, backPoints, frontPoints);

      backPolygons.push_back(gfx::Polygon3D(std::move(backPoints)));
      frontPolygons.push_back(gfx::Polygon3D(std::move(frontPoints)));
    }
  }

  if (!backPolygons.empty()) {
    aRoot->back.reset(new BSPTreeNode(PopFront(backPolygons)));
    BuildTree(aRoot->back, backPolygons);
  }

  if (!frontPolygons.empty()) {
    aRoot->front.reset(new BSPTreeNode(PopFront(frontPolygons)));
    BuildTree(aRoot->front, frontPolygons);
  }
}

void
BSPTree::SplitPolygon(const gfx::Polygon3D& aSplittingPlane,
                      const gfx::Polygon3D& aPolygon,
                      const nsTArray<float>& dots,
                      nsTArray<gfx::Point3D>& backPoints,
                      nsTArray<gfx::Point3D>& frontPoints)
{
  const gfx::Point3D& normal = aSplittingPlane.GetNormal();
  const size_t pointCount = aPolygon.GetPoints().Length();

  for (size_t i = 0; i < pointCount; ++i) {
    size_t j = (i + 1) % pointCount;

    const gfx::Point3D& a = aPolygon[i];
    const gfx::Point3D& b = aPolygon[j];
    const float dotA = dots[i];
    const float dotB = dots[j];

    // The point is in front of the plane.
    if (dotA >= 0) {
      frontPoints.AppendElement(a);
    }

    // The point is behind the plane.
    if (dotA <= 0) {
      backPoints.AppendElement(a);
    }

    // If the sign of the dot product changes between two consecutive vertices,
    // the splitting plane intersects the corresponding polygon edge.
    if (sign(dotA) != sign(dotB)) {

      // Calculate the line segment and plane intersection point.
      const gfx::Point3D ab = b - a;
      const float dotAB = ab.DotProduct(normal);
      const float t = -dotA / dotAB;
      const gfx::Point3D p = a + (ab * t);

      // Add the intersection point to both polygons.
      backPoints.AppendElement(p);
      frontPoints.AppendElement(p);
    }
  }
}

} // namespace layers
} // namespace mozilla
