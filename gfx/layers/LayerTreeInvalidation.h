/*-*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYER_TREE_INVALIDATION_H
#define GFX_LAYER_TREE_INVALIDATION_H

#include "nsRegion.h"                   // for nsIntRegion
#include "mozilla/UniquePtr.h"          // for UniquePtr
#include "mozilla/gfx/Point.h"

namespace mozilla {
namespace layers {

class Layer;
class ContainerLayer;

/**
 * Callback for ContainerLayer invalidations.
 *
 * @param aContainer ContainerLayer being invalidated.
 * @param aRegion Invalidated region in the ContainerLayer's coordinate
 * space.
 */
typedef void (*NotifySubDocInvalidationFunc)(ContainerLayer* aLayer,
                                             const nsIntRegion& aRegion);

/**
 * A set of cached layer properties (including those of child layers),
 * used for comparing differences in layer trees.
 */
struct LayerProperties
{
  virtual ~LayerProperties() {}

  /**
   * Copies the current layer tree properties into
   * a new LayerProperties object.
   *
   * @param Layer tree to copy, or nullptr if we have no 
   * initial layer tree.
   */
  static UniquePtr<LayerProperties> CloneFrom(Layer* aRoot);

  /**
   * Clear all invalidation status from this layer tree.
   */
  static void ClearInvalidations(Layer* aRoot);

  /**
   * Compares a set of existing layer tree properties to the current layer
   * tree and generates the changed rectangle.
   *
   * @param aRoot Root layer of the layer tree to compare against.
   * @param aCallback If specified, callback to call when ContainerLayers
   * are invalidated.
   * @return Painted area changed by the layer tree changes.
   */
  virtual nsIntRegion ComputeDifferences(Layer* aRoot,
                                         NotifySubDocInvalidationFunc aCallback,
                                         bool* aGeometryChanged = nullptr) = 0;

  virtual void MoveBy(const gfx::IntPoint& aOffset) = 0;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_LAYER_TREE_INVALIDATON_H */
