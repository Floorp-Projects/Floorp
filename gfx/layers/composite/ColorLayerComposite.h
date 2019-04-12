/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ColorLayerComposite_H
#define GFX_ColorLayerComposite_H

#include "Layers.h"                                // for ColorLayer, etc
#include "mozilla/Attributes.h"                    // for override
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerComposite, etc
#include "nsISupportsImpl.h"                       // for MOZ_COUNT_CTOR, etc

namespace mozilla {
namespace layers {

class CompositableHost;

class ColorLayerComposite : public ColorLayer, public LayerComposite {
 public:
  explicit ColorLayerComposite(LayerManagerComposite* aManager)
      : ColorLayer(aManager, nullptr), LayerComposite(aManager) {
    MOZ_COUNT_CTOR(ColorLayerComposite);
    mImplData = static_cast<LayerComposite*>(this);
  }

 protected:
  virtual ~ColorLayerComposite() {
    MOZ_COUNT_DTOR(ColorLayerComposite);
    Destroy();
  }

 public:
  // LayerComposite Implementation
  Layer* GetLayer() override { return this; }

  void SetLayerManager(HostLayerManager* aManager) override {
    LayerComposite::SetLayerManager(aManager);
    mManager = aManager;
  }

  void Destroy() override { mDestroyed = true; }

  void RenderLayer(const gfx::IntRect& aClipRect,
                   const Maybe<gfx::Polygon>& aGeometry) override;

  void CleanupResources() override{};

  void GenEffectChain(EffectChain& aEffect) override;

  CompositableHost* GetCompositableHost() override { return nullptr; }

  HostLayer* AsHostLayer() override { return this; }

  const char* Name() const override { return "ColorLayerComposite"; }
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_ColorLayerComposite_H */
