/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ColorLayerComposite_H
#define GFX_ColorLayerComposite_H

#include "Layers.h"                     // for ColorLayer, etc
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerComposite, etc
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc

struct nsIntPoint;
struct nsIntRect;

namespace mozilla {
namespace layers {

class CompositableHost;

class ColorLayerComposite : public ColorLayer,
                            public LayerComposite
{
public:
  ColorLayerComposite(LayerManagerComposite *aManager)
    : ColorLayer(aManager, nullptr)
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

  virtual void RenderLayer(const nsIntRect& aClipRect) MOZ_OVERRIDE;
  virtual void CleanupResources() MOZ_OVERRIDE {};

  CompositableHost* GetCompositableHost() MOZ_OVERRIDE { return nullptr; }

  virtual LayerComposite* AsLayerComposite() MOZ_OVERRIDE { return this; }

  virtual const char* Name() const MOZ_OVERRIDE { return "ColorLayerComposite"; }
};

} /* layers */
} /* mozilla */
#endif /* GFX_ColorLayerComposite_H */
