/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYER_TREE_INVALIDATION_H
#define GFX_LAYER_TREE_INVALIDATION_H

#include "nsRegion.h"           // for nsIntRegion
#include "mozilla/UniquePtr.h"  // for UniquePtr
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
 * space. If null, then the entire region must be invalidated.
 */
typedef void (*NotifySubDocInvalidationFunc)(ContainerLayer* aLayer,
                                             const nsIntRegion* aRegion);

/**
 * A set of cached layer properties (including those of child layers),
 * used for comparing differences in layer trees.
 */
struct LayerProperties {
 protected:
  LayerProperties() = default;

  LayerProperties(const LayerProperties& a) = delete;
  LayerProperties& operator=(const LayerProperties& a) = delete;

 public:
  virtual ~LayerProperties() = default;

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
   * @param aOutRegion Outparam that will contain the painted area changed by
   * the layer tree changes.
   * @param aCallback If specified, callback to call when ContainerLayers
   * are invalidated.
   * @return True on success, false if a calculation overflowed and the entire
   *         layer tree area should be considered invalidated.
   */
  virtual bool ComputeDifferences(Layer* aRoot, nsIntRegion& aOutRegion,
                                  NotifySubDocInvalidationFunc aCallback) = 0;

  virtual void MoveBy(const gfx::IntPoint& aOffset) = 0;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_LAYER_TREE_INVALIDATON_H */
