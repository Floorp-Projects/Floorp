/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BSPTree.h"
#include "mozilla/gfx/Polygon.h"

namespace mozilla {
namespace layers {

void
BSPTree::BuildDrawOrder(BSPTreeNode* aNode,
                        nsTArray<LayerPolygon>& aLayers) const
{
  const gfx::Point4D& normal = aNode->First().GetNormal();

  BSPTreeNode* front = aNode->front;
  BSPTreeNode* back = aNode->back;

  // Since the goal is to return the draw order from back to front, we reverse
  // the traversal order if the current polygon is facing towards the camera.
  const bool reverseOrder = normal.z > 0.0f;

  if (reverseOrder) {
    std::swap(front, back);
  }

  if (front) {
    BuildDrawOrder(front, aLayers);
  }

  for (LayerPolygon& layer : aNode->layers) {
    MOZ_ASSERT(layer.geometry);

    if (layer.geometry->GetPoints().Length() >= 3) {
      aLayers.AppendElement(std::move(layer));
    }
  }

  if (back) {
    BuildDrawOrder(back, aLayers);
  }
}

void
BSPTree::BuildTree(BSPTreeNode* aRoot,
                   std::list<LayerPolygon>& aLayers)
{
  MOZ_ASSERT(!aLayers.empty());

  aRoot->layers.push_back(std::move(aLayers.front()));
  aLayers.pop_front();

  if (aLayers.empty()) {
    return;
  }

  const gfx::Polygon& plane = aRoot->First();
  MOZ_ASSERT(!plane.IsEmpty());

  const gfx::Point4D& planeNormal = plane.GetNormal();
  const gfx::Point4D& planePoint = plane.GetPoints()[0];

  std::list<LayerPolygon> backLayers, frontLayers;
  for (LayerPolygon& layerPolygon : aLayers) {
    const nsTArray<gfx::Point4D>& geometry = layerPolygon.geometry->GetPoints();

    // Calculate the plane-point distances for the polygon classification.
    size_t pos = 0, neg = 0;
    nsTArray<float> distances =
      CalculatePointPlaneDistances(geometry, planeNormal, planePoint, pos, neg);

    // Back polygon
    if (pos == 0 && neg > 0) {
      backLayers.push_back(std::move(layerPolygon));
    }
    // Front polygon
    else if (pos > 0 && neg == 0) {
      frontLayers.push_back(std::move(layerPolygon));
    }
    // Coplanar polygon
    else if (pos == 0 && neg == 0) {
      aRoot->layers.push_back(std::move(layerPolygon));
    }
    // Polygon intersects with the splitting plane.
    else if (pos > 0 && neg > 0) {
      nsTArray<gfx::Point4D> backPoints, frontPoints;
      // Clip the polygon against the plane. We reuse the previously calculated
      // distances to find the plane-edge intersections.
      ClipPointsWithPlane(geometry, planeNormal, distances,
                          backPoints, frontPoints);

      const gfx::Point4D& normal = layerPolygon.geometry->GetNormal();
      Layer* layer = layerPolygon.layer;

      if (backPoints.Length() >= 3) {
        backLayers.emplace_back(layer, std::move(backPoints), normal);
      }

      if (frontPoints.Length() >= 3) {
        frontLayers.emplace_back(layer, std::move(frontPoints), normal);
      }
    }
  }

  if (!backLayers.empty()) {
    aRoot->back = new (mPool) BSPTreeNode(mListPointers);
    BuildTree(aRoot->back, backLayers);
  }

  if (!frontLayers.empty()) {
    aRoot->front = new (mPool) BSPTreeNode(mListPointers);
    BuildTree(aRoot->front, frontLayers);
  }
}

} // namespace layers
} // namespace mozilla
