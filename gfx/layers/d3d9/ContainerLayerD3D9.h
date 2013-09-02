/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CONTAINERLAYERD3D9_H
#define GFX_CONTAINERLAYERD3D9_H

#include "Layers.h"
#include "LayerManagerD3D9.h"

namespace mozilla {
namespace layers {
  
class ContainerLayerD3D9 : public ContainerLayer,
                           public LayerD3D9
{
public:
  ContainerLayerD3D9(LayerManagerD3D9 *aManager);
  ~ContainerLayerD3D9();

  nsIntRect GetVisibleRect() { return mVisibleRegion.GetBounds(); }

  /* LayerD3D9 implementation */
  Layer* GetLayer();

  LayerD3D9* GetFirstChildD3D9();

  bool IsEmpty();

  void RenderLayer();

  virtual void LayerManagerDestroyed();

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }
};

} /* layers */
} /* mozilla */

#endif /* GFX_CONTAINERLAYERD3D9_H */
