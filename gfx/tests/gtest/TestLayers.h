/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifndef GFX_TEST_LAYERS_H
#define GFX_TEST_LAYERS_H

#include "Layers.h"
#include "nsTArray.h"

/* Create layer tree from a simple layer tree description syntax.
 * Each index is either the first letter of the layer type or
 * a '(',')' to indicate the start/end of the child layers.
 * The aim of this function is to remove hard to read
 * layer tree creation code.
 *
 * Example "c(c(c(tt)t))" would yield:
 *          c
 *          |
 *          c
 *         / \
 *        c   t
 *       / \
 *      t   t
 */
already_AddRefed<mozilla::layers::Layer> CreateLayerTree(
    const char* aLayerTreeDescription,
    nsIntRegion* aVisibleRegions,
    const mozilla::gfx::Matrix4x4* aTransforms,
    RefPtr<mozilla::layers::LayerManager>& aLayerManager,
    nsTArray<RefPtr<mozilla::layers::Layer> >& aLayersOut);


#endif

