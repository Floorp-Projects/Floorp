/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CONTAINERLAYEROGL_H
#define GFX_CONTAINERLAYEROGL_H

#include "LayerManagerOGL.h"            // for LayerOGL
#include "Layers.h"                     // for Layer (ptr only), etc
class gfx3DMatrix;
struct nsIntPoint;

namespace mozilla {
namespace layers {

class ContainerLayerOGL : public ContainerLayer,
                          public LayerOGL
{
public:
  ContainerLayerOGL(LayerManagerOGL *aManager);
  ~ContainerLayerOGL();

  /** LayerOGL implementation */
  Layer* GetLayer() { return this; }

  void Destroy();

  LayerOGL* GetFirstChildOGL();

  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset);

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }

  virtual void CleanupResources();
};

} /* layers */
} /* mozilla */

#endif /* GFX_CONTAINERLAYEROGL_H */
