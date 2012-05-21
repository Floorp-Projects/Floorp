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

  /* ContainerLayer implementation */
  virtual void InsertAfter(Layer* aChild, Layer* aAfter);

  virtual void RemoveChild(Layer* aChild);

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

// This is a bare-bones implementation of a container layer, only
// enough to contain a shadow "window texture".  This impl doesn't
// honor the transform/cliprect/etc. when rendering.
class ShadowContainerLayerD3D10 : public ShadowContainerLayer,
                                  public LayerD3D10
{
public:
  ShadowContainerLayerD3D10(LayerManagerD3D10 *aManager);
  ~ShadowContainerLayerD3D10();

  void InsertAfter(Layer* aChild, Layer* aAfter);

  void RemoveChild(Layer* aChild);

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface);

  /* LayerD3D10 implementation */
  virtual LayerD3D10 *GetFirstChildD3D10();
  virtual Layer* GetLayer() { return this; }
  virtual void RenderLayer();
  virtual void Validate();
  virtual void LayerManagerDestroyed();

private:
    
};

} /* layers */
} /* mozilla */

#endif /* GFX_CONTAINERLAYERD3D10_H */
