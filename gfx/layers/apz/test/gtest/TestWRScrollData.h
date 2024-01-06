/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_TestWRScrollData_h
#define mozilla_layers_TestWRScrollData_h

#include "mozilla/gfx/MatrixFwd.h"
#include "mozilla/layers/WebRenderScrollData.h"

namespace mozilla {
namespace layers {

class APZUpdater;

// Extends WebRenderScrollData with some methods useful for gtests.
class TestWRScrollData : public WebRenderScrollData {
 public:
  TestWRScrollData() = default;
  TestWRScrollData(TestWRScrollData&& aOther) = default;
  TestWRScrollData& operator=(TestWRScrollData&& aOther) = default;

  /*
   * Create a WebRenderLayerScrollData tree described by |aTreeShape|.
   * |aTreeShape| is expected to be a string where each character is
   * either 'x' to indicate a node in the tree, or a '(' or ')' to indicate
   * the start/end of a subtree.
   *
   * Example "x(x(x(xx)x))" would yield:
   *          x
   *          |
   *          x
   *         / \
   *        x   x
   *       / \
   *      x   x
   *
   * The caller may optionally provide visible rects and/or transforms
   * for the nodes. If provided, the array should contain one element
   * for each node, in the same order as in |aTreeShape|.
   */
  static TestWRScrollData Create(const char* aTreeShape,
                                 const APZUpdater& aUpdater,
                                 const LayerIntRect* aVisibleRects = nullptr,
                                 const gfx::Matrix4x4* aTransforms = nullptr);

  // These methods allow accessing and manipulating layers based on an index
  // representing the order in which they appear in |aTreeShape|.
  WebRenderLayerScrollData* operator[](size_t aLayerIndex);
  const WebRenderLayerScrollData* operator[](size_t aLayerIndex) const;
  void SetScrollMetadata(size_t aLayerIndex,
                         const nsTArray<ScrollMetadata>& aMetadata);

 private:
  std::map<size_t, size_t> mIndexMap;  // Used to implement GetLayer()
};

}  // namespace layers
}  // namespace mozilla

#endif
