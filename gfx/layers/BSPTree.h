/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_BSPTREE_H
#define MOZILLA_LAYERS_BSPTREE_H

#include "mozilla/ArenaAllocator.h"
#include "mozilla/gfx/Polygon.h"
#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

#include <list>

namespace mozilla {
namespace layers {

class Layer;

/**
 * Represents a layer that might have a non-rectangular geometry.
 */
struct LayerPolygon {
  explicit LayerPolygon(Layer* aLayer)
    : layer(aLayer) {}

  LayerPolygon(Layer* aLayer,
               gfx::Polygon&& aGeometry)
    : layer(aLayer), geometry(Some(std::move(aGeometry))) {}

  LayerPolygon(Layer* aLayer,
               nsTArray<gfx::Point4D>&& aPoints,
               const gfx::Point4D& aNormal)
    : layer(aLayer)
  {
    geometry.emplace(std::move(aPoints), aNormal);
  }

  Layer* layer;
  Maybe<gfx::Polygon> geometry;
};

/**
 * Allocate BSPTreeNodes from a memory arena to improve performance with
 * complex scenes.
 * The arena size of 4096 bytes was selected as an arbitrary power of two.
 * Depending on the platform, this size accommodates roughly 100 BSPTreeNodes.
 */
typedef mozilla::ArenaAllocator<4096, 8> BSPTreeArena;

/**
 * Aliases the container type used to store layers within BSPTreeNodes.
 */
typedef std::list<LayerPolygon> LayerList;

/**
 * Represents a node in a BSP tree. The node contains at least one layer with
 * associated geometry that is used as a splitting plane, and at most two child
 * nodes that represent the splitting planes that further subdivide the space.
 */
struct BSPTreeNode {
  explicit BSPTreeNode(nsTArray<LayerList*>& aListPointers)
    : front(nullptr), back(nullptr)
  {
    // Store the layer list pointer to free memory when BSPTree is destroyed.
    aListPointers.AppendElement(&layers);
  }

  const gfx::Polygon& First() const
  {
    MOZ_ASSERT(!layers.empty());
    MOZ_ASSERT(layers.front().geometry);
    return *layers.front().geometry;
  }

  static void* operator new(size_t aSize, BSPTreeArena& mPool)
  {
    return mPool.Allocate(aSize);
  }

  BSPTreeNode* front;
  BSPTreeNode* back;
  LayerList layers;
};

/**
 * BSPTree class takes a list of layers as an input and uses binary space
 * partitioning algorithm to create a tree structure that can be used for
 * depth sorting.

 * Sources for more information:
 * https://en.wikipedia.org/wiki/Binary_space_partitioning
 * ftp://ftp.sgi.com/other/bspfaq/faq/bspfaq.html
 */
class BSPTree {
public:
  /**
   * The constructor modifies layers in the given list.
   */
  explicit BSPTree(std::list<LayerPolygon>& aLayers)
  {
    MOZ_ASSERT(!aLayers.empty());

    mRoot = new (mPool) BSPTreeNode(mListPointers);
    BuildTree(mRoot, aLayers);
  }


  ~BSPTree()
  {
    for (LayerList* listPtr : mListPointers) {
      listPtr->~LayerList();
    }
  }

  /**
   * Builds and returns the back-to-front draw order for the created BSP tree.
   */
  nsTArray<LayerPolygon> GetDrawOrder() const
  {
    nsTArray<LayerPolygon> layers;
    BuildDrawOrder(mRoot, layers);
    return layers;
  }

private:
  BSPTreeArena mPool;
  BSPTreeNode* mRoot;
  nsTArray<LayerList*> mListPointers;

  /**
   * BuildDrawOrder and BuildTree are called recursively. The depth of the
   * recursion depends on the amount of polygons and their intersections.
   */
  void BuildDrawOrder(BSPTreeNode* aNode,
                      nsTArray<LayerPolygon>& aLayers) const;

  void BuildTree(BSPTreeNode* aRoot,
                 std::list<LayerPolygon>& aLayers);
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_LAYERS_BSPTREE_H */
