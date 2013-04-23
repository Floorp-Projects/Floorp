/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ColorLayerComposite_H
#define GFX_ColorLayerComposite_H

#include "mozilla/layers/PLayerTransaction.h"
#include "mozilla/layers/ShadowLayers.h"

#include "LayerManagerComposite.h"

namespace mozilla {
namespace layers {


class ColorLayerComposite : public ShadowColorLayer,
                            public LayerComposite
{
public:
  ColorLayerComposite(LayerManagerComposite *aManager)
    : ShadowColorLayer(aManager, nullptr)
    , LayerComposite(aManager)
  {
    MOZ_COUNT_CTOR(ColorLayerComposite);
    mImplData = static_cast<LayerComposite*>(this);
  }
  ~ColorLayerComposite()
  {
    MOZ_COUNT_DTOR(ColorLayerComposite);
    Destroy();
  }

  // LayerComposite Implementation
  virtual Layer* GetLayer() MOZ_OVERRIDE { return this; }

  virtual void Destroy() MOZ_OVERRIDE { mDestroyed = true; }

  virtual void RenderLayer(const nsIntPoint& aOffset,
                           const nsIntRect& aClipRect) MOZ_OVERRIDE;
  virtual void CleanupResources() MOZ_OVERRIDE {};

  CompositableHost* GetCompositableHost() MOZ_OVERRIDE { return nullptr; }

  virtual LayerComposite* AsLayerComposite() MOZ_OVERRIDE { return this; }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() const MOZ_OVERRIDE { return "ColorLayerComposite"; }
#endif
};

} /* layers */
} /* mozilla */
#endif /* GFX_ColorLayerComposite_H */
