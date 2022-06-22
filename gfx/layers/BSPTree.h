/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_BSPTREE_H
#define MOZILLA_LAYERS_BSPTREE_H

#include <list>
#include <utility>

#include "mozilla/ArenaAllocator.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/Polygon.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {

class Layer;

/**
 * Represents a layer that might have a non-rectangular geometry.
 */
template <typename T>
struct BSPPolygon {
  explicit BSPPolygon(T* aData) : data(aData) {}

  BSPPolygon(T* aData, gfx::Polygon&& aGeometry)
      : data(aData), geometry(Some(std::move(aGeometry))) {}

  BSPPolygon(T* aData, nsTArray<gfx::Point4D>&& aPoints,
             const gfx::Point4D& aNormal)
      : data(aData) {
    geometry.emplace(std::move(aPoints), aNormal);
  }

  T* data;
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
template <typename T>
using PolygonList = std::list<BSPPolygon<T>>;

using LayerPolygon = BSPPolygon<Layer>;

/**
 * Represents a node in a BSP tree. The node contains at least one layer with
 * associated geometry that is used as a splitting plane, and at most two child
 * nodes that represent the splitting planes that further subdivide the space.
 */
template <typename T>
struct BSPTreeNode {
  explicit BSPTreeNode(nsTArray<PolygonList<T>*>& aListPointers)
      : front(nullptr), back(nullptr) {
    // Store the layer list pointer to free memory when BSPTree is destroyed.
    aListPointers.AppendElement(&layers);
  }

  const gfx::Polygon& First() const {
    MOZ_ASSERT(!layers.empty());
    MOZ_ASSERT(layers.front().geometry);
    return *layers.front().geometry;
  }

  static void* operator new(size_t aSize, BSPTreeArena& mPool) {
    return mPool.Allocate(aSize);
  }

  BSPTreeNode* front;
  BSPTreeNode* back;
  PolygonList<T> layers;
};

/**
 * BSPTree class takes a list of layers as an input and uses binary space
 * partitioning algorithm to create a tree structure that can be used for
 * depth sorting.

 * Sources for more information:
 * https://en.wikipedia.org/wiki/Binary_space_partitioning
 * ftp://ftp.sgi.com/other/bspfaq/faq/bspfaq.html
 */
template <typename T>
class BSPTree final {
 public:
  /**
   * The constructor modifies layers in the given list.
   */
  explicit BSPTree(std::list<BSPPolygon<T>>& aLayers) {
    MOZ_ASSERT(!aLayers.empty());

    mRoot = new (mPool) BSPTreeNode(mListPointers);
    BuildTree(mRoot, aLayers);
  }

  ~BSPTree() {
    for (PolygonList<T>* listPtr : mListPointers) {
      listPtr->~list();
    }
  }

  /**
   * Builds and returns the back-to-front draw order for the created BSP tree.
   */
  nsTArray<BSPPolygon<T>> GetDrawOrder() const {
    nsTArray<BSPPolygon<T>> layers;
    BuildDrawOrder(mRoot, layers);
    return layers;
  }

 private:
  BSPTreeArena mPool;
  BSPTreeNode<T>* mRoot;
  nsTArray<PolygonList<T>*> mListPointers;

  /**
   * BuildDrawOrder and BuildTree are called recursively. The depth of the
   * recursion depends on the amount of polygons and their intersections.
   */
  void BuildDrawOrder(BSPTreeNode<T>* aNode,
                      nsTArray<BSPPolygon<T>>& aLayers) const;

  void BuildTree(BSPTreeNode<T>* aRoot, PolygonList<T>& aLayers);
};

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_LAYERS_BSPTREE_H */
