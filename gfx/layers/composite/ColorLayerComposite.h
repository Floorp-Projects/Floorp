/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ColorLayerComposite_H
#define GFX_ColorLayerComposite_H

#include "Layers.h"                     // for ColorLayer, etc
#include "mozilla/Attributes.h"         // for override
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerComposite, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc

namespace mozilla {
namespace layers {

class CompositableHost;

class ColorLayerComposite : public ColorLayer,
                            public LayerComposite
{
public:
  explicit ColorLayerComposite(LayerManagerComposite *aManager)
    : ColorLayer(aManager, nullptr)
    , LayerComposite(aManager)
  {
    MOZ_COUNT_CTOR(ColorLayerComposite);
    mImplData = static_cast<LayerComposite*>(this);
  }

protected:
  ~ColorLayerComposite()
  {
    MOZ_COUNT_DTOR(ColorLayerComposite);
    Destroy();
  }

public:
  // LayerComposite Implementation
  virtual Layer* GetLayer() override { return this; }

  virtual void SetLayerManager(LayerManagerComposite* aManager) override
  {
    LayerComposite::SetLayerManager(aManager);
    mManager = aManager;
  }

  virtual void Destroy() override { mDestroyed = true; }

  virtual void RenderLayer(const gfx::IntRect& aClipRect) override;
  virtual void CleanupResources() override {};

  virtual void GenEffectChain(EffectChain& aEffect) override;

  CompositableHost* GetCompositableHost() override { return nullptr; }

  virtual LayerComposite* AsLayerComposite() override { return this; }

  virtual const char* Name() const override { return "ColorLayerComposite"; }
};

} /* layers */
} /* mozilla */
#endif /* GFX_ColorLayerComposite_H */
