/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CONTAINERLAYERD3D10_H
#define GFX_CONTAINERLAYERD3D10_H

#include "LayerManagerD3D10.h"

namespace mozilla {
namespace layers {

class ContainerLayerD3D10 : public ContainerLayer,
                            public LayerD3D10
{
public:
  ContainerLayerD3D10(LayerManagerD3D10 *aManager);
  ~ContainerLayerD3D10();

  nsIntRect GetVisibleRect() { return mVisibleRegion.GetBounds(); }

  /* LayerD3D10 implementation */
  virtual Layer* GetLayer();

  virtual LayerD3D10* GetFirstChildD3D10();

  virtual void RenderLayer();
  virtual void Validate();

  virtual void LayerManagerDestroyed();

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }
};

} /* layers */
} /* mozilla */

#endif /* GFX_CONTAINERLAYERD3D10_H */
