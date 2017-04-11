/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_BSPTREE_H
#define MOZILLA_LAYERS_BSPTREE_H

#include "mozilla/gfx/Polygon.h"
#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

#include <deque>

namespace mozilla {
namespace layers {

class Layer;

// Represents a layer that might have a non-rectangular geometry.
struct LayerPolygon {
  explicit LayerPolygon(Layer *aLayer)
    : layer(aLayer) {}

  LayerPolygon(Layer *aLayer,
               gfx::Polygon&& aGeometry)
    : layer(aLayer), geometry(Some(aGeometry)) {}

  LayerPolygon(Layer *aLayer,
               nsTArray<gfx::Point4D>&& aPoints,
               const gfx::Point4D& aNormal)
    : layer(aLayer), geometry(Some(gfx::Polygon(Move(aPoints), aNormal))) {}

  Layer *layer;
  Maybe<gfx::Polygon> geometry;
};

LayerPolygon PopFront(std::deque<LayerPolygon>& aLayers);

// Represents a node in a BSP tree. The node contains at least one layer with
// associated geometry that is used as a splitting plane, and at most two child
// nodes that represent the splitting planes that further subdivide the space.
struct BSPTreeNode {
  explicit BSPTreeNode(LayerPolygon&& layer)
  {
    layers.push_back(Move(layer));
  }

  const gfx::Polygon& First() const
  {
    MOZ_ASSERT(layers[0].geometry);
    return *layers[0].geometry;
  }

  UniquePtr<BSPTreeNode> front;
  UniquePtr<BSPTreeNode> back;
  std::deque<LayerPolygon> layers;
};

// BSPTree class takes a list of layers as an input and uses binary space
// partitioning algorithm to create a tree structure that can be used for
// depth sorting.
//
// Sources for more information:
// https://en.wikipedia.org/wiki/Binary_space_partitioning
// ftp://ftp.sgi.com/other/bspfaq/faq/bspfaq.html
class BSPTree {
public:
  // This constructor takes the ownership of layers in the given list.
  explicit BSPTree(std::deque<LayerPolygon>& aLayers)
  {
    MOZ_ASSERT(!aLayers.empty());
    mRoot.reset(new BSPTreeNode(PopFront(aLayers)));

    BuildTree(mRoot, aLayers);
  }

  // Returns the root node of the BSP tree.
  const UniquePtr<BSPTreeNode>& GetRoot() const
  {
    return mRoot;
  }

  // Builds and returns the back-to-front draw order for the created BSP tree.
  nsTArray<LayerPolygon> GetDrawOrder() const
  {
    nsTArray<LayerPolygon> layers;
    BuildDrawOrder(mRoot, layers);
    return layers;
  }

private:
  UniquePtr<BSPTreeNode> mRoot;

  // BuildDrawOrder and BuildTree are called recursively. The depth of the
  // recursion depends on the amount of polygons and their intersections.
  void BuildDrawOrder(const UniquePtr<BSPTreeNode>& aNode,
                      nsTArray<LayerPolygon>& aLayers) const;
  void BuildTree(UniquePtr<BSPTreeNode>& aRoot,
                 std::deque<LayerPolygon>& aLayers);
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_LAYERS_BSPTREE_H */
