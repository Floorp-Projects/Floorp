/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BSPTree.h"
#include "mozilla/gfx/Polygon.h"

namespace mozilla {
namespace layers {

LayerPolygon PopFront(std::deque<LayerPolygon>& aLayers)
{
  LayerPolygon layer = Move(aLayers.front());
  aLayers.pop_front();
  return layer;
}

void
BSPTree::BuildDrawOrder(const UniquePtr<BSPTreeNode>& aNode,
                        nsTArray<LayerPolygon>& aLayers) const
{
  const gfx::Point4D& normal = aNode->First().GetNormal();

  UniquePtr<BSPTreeNode> *front = &aNode->front;
  UniquePtr<BSPTreeNode> *back = &aNode->back;

  // Since the goal is to return the draw order from back to front, we reverse
  // the traversal order if the current polygon is facing towards the camera.
  const bool reverseOrder = normal.z > 0.0f;

  if (reverseOrder) {
    std::swap(front, back);
  }

  if (*front) {
    BuildDrawOrder(*front, aLayers);
  }

  for (LayerPolygon& layer : aNode->layers) {
    MOZ_ASSERT(layer.geometry);

    if (layer.geometry->GetPoints().Length() >= 3) {
      aLayers.AppendElement(Move(layer));
    }
  }

  if (*back) {
    BuildDrawOrder(*back, aLayers);
  }
}

void
BSPTree::BuildTree(UniquePtr<BSPTreeNode>& aRoot,
                   std::deque<LayerPolygon>& aLayers)
{
  if (aLayers.empty()) {
    return;
  }

  const gfx::Polygon& plane = aRoot->First();
  std::deque<LayerPolygon> backLayers, frontLayers;

  for (LayerPolygon& layerPolygon : aLayers) {
    const Maybe<gfx::Polygon>& geometry = layerPolygon.geometry;

    size_t pos = 0, neg = 0;
    nsTArray<float> dots = geometry->CalculateDotProducts(plane, pos, neg);

    // Back polygon
    if (pos == 0 && neg > 0) {
      backLayers.push_back(Move(layerPolygon));
    }
    // Front polygon
    else if (pos > 0 && neg == 0) {
      frontLayers.push_back(Move(layerPolygon));
    }
    // Coplanar polygon
    else if (pos == 0 && neg == 0) {
      aRoot->layers.push_back(Move(layerPolygon));
    }
    // Polygon intersects with the splitting plane.
    else if (pos > 0 && neg > 0) {
      nsTArray<gfx::Point4D> backPoints, frontPoints;
      geometry->SplitPolygon(plane.GetNormal(), dots, backPoints, frontPoints);

      const gfx::Point4D& normal = geometry->GetNormal();
      Layer *layer = layerPolygon.layer;

      if (backPoints.Length() >= 3) {
        backLayers.push_back(LayerPolygon(layer, Move(backPoints), normal));
      }

      if (frontPoints.Length() >= 3) {
        frontLayers.push_back(LayerPolygon(layer, Move(frontPoints), normal));
      }
    }
  }

  if (!backLayers.empty()) {
    aRoot->back.reset(new BSPTreeNode(PopFront(backLayers)));
    BuildTree(aRoot->back, backLayers);
  }

  if (!frontLayers.empty()) {
    aRoot->front.reset(new BSPTreeNode(PopFront(frontLayers)));
    BuildTree(aRoot->front, frontLayers);
  }
}

} // namespace layers
} // namespace mozilla
