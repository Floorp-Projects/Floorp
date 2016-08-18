/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_BSPTREE_H
#define MOZILLA_LAYERS_BSPTREE_H

#include "mozilla/gfx/Polygon.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

#include <deque>

namespace mozilla {
namespace layers {

gfx::Polygon3D PopFront(std::deque<gfx::Polygon3D>& aPolygons);

// Represents a node in a BSP tree. The node contains at least one polygon that
// is used as a splitting plane, and at most two child nodes that represent the
// splitting planes that further subdivide the space.
struct BSPTreeNode {
  explicit BSPTreeNode(gfx::Polygon3D && aPolygon)
  {
    polygons.push_back(std::move(aPolygon));
  }

  const gfx::Polygon3D& First() const
  {
    return polygons[0];
  }

  UniquePtr<BSPTreeNode> front;
  UniquePtr<BSPTreeNode> back;
  std::deque<gfx::Polygon3D> polygons;
};

// BSPTree class takes a list of polygons as an input and uses binary space
// partitioning algorithm to create a tree structure that can be used for
// depth sorting.
//
// Sources for more information:
// https://en.wikipedia.org/wiki/Binary_space_partitioning
// ftp://ftp.sgi.com/other/bspfaq/faq/bspfaq.html
class BSPTree {
public:
  // This constructor takes the ownership of polygons in the given list.
  explicit BSPTree(std::deque<gfx::Polygon3D>& aPolygons)
    : mRoot(new BSPTreeNode(PopFront(aPolygons)))
  {
    BuildTree(mRoot, aPolygons);
  }

  // Returns the root node of the BSP tree.
  const UniquePtr<BSPTreeNode>& GetRoot() const
  {
    return mRoot;
  }

  // Builds and returns the back-to-front draw order for the created BSP tree.
  nsTArray<gfx::Polygon3D> GetDrawOrder() const
  {
    nsTArray<gfx::Polygon3D> polygons;
    BuildDrawOrder(mRoot, polygons);
    return polygons;
  }

private:
  UniquePtr<BSPTreeNode> mRoot;

  void BuildDrawOrder(const UniquePtr<BSPTreeNode>& aNode,
                      nsTArray<gfx::Polygon3D>& aPolygons) const;

  void BuildTree(UniquePtr<BSPTreeNode>& aRoot,
                 std::deque<gfx::Polygon3D>& aPolygons);

  nsTArray<float> CalculateDotProduct(const gfx::Polygon3D& aFirst,
                                      const gfx::Polygon3D& aSecond,
                                      size_t& aPos, size_t& aNeg) const;

  void SplitPolygon(const gfx::Polygon3D& aSplittingPlane,
                    const gfx::Polygon3D& aPolygon,
                    const nsTArray<float>& dots,
                    nsTArray<gfx::Point3D>& backPoints,
                    nsTArray<gfx::Point3D>& frontPoints);
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_LAYERS_BSPTREE_H */
