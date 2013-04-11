/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_COLORLAYEROGL_H
#define GFX_COLORLAYEROGL_H

#include "mozilla/layers/PLayers.h"
#include "mozilla/layers/ShadowLayers.h"

#include "LayerManagerOGL.h"

namespace mozilla {
namespace layers {

class THEBES_API ColorLayerOGL : public ColorLayer,
                                 public LayerOGL
{
public:
  ColorLayerOGL(LayerManagerOGL *aManager)
    : ColorLayer(aManager, NULL)
    , LayerOGL(aManager)
  { 
    mImplData = static_cast<LayerOGL*>(this);
  }
  ~ColorLayerOGL() { Destroy(); }

  // LayerOGL Implementation
  virtual Layer* GetLayer() { return this; }

  virtual void Destroy() { mDestroyed = true; }

  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset);
  virtual void CleanupResources() {};
};

} /* layers */
} /* mozilla */
#endif /* GFX_COLORLAYEROGL_H */
