/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PaintedLayerComposite_H
#define GFX_PaintedLayerComposite_H

#include "Layers.h"                     // for Layer (ptr only), etc
#include "mozilla/gfx/Rect.h"
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerComposite, etc
#include "mozilla/layers/LayersTypes.h"  // for LayerRenderState, etc
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsRegion.h"                   // for nsIntRegion
#include "nscore.h"                     // for nsACString


namespace mozilla {
namespace layers {

/**
 * PaintedLayers use ContentHosts for their compsositable host.
 * By using different ContentHosts, PaintedLayerComposite support tiled and
 * non-tiled PaintedLayers and single or double buffering.
 */

class CompositableHost;
class ContentHost;

class PaintedLayerComposite : public PaintedLayer,
                              public LayerComposite
{
public:
  explicit PaintedLayerComposite(LayerManagerComposite *aManager);

protected:
  virtual ~PaintedLayerComposite();

public:
  virtual void Disconnect() override;

  virtual LayerRenderState GetRenderState() override;

  CompositableHost* GetCompositableHost() override;

  virtual void Destroy() override;

  virtual Layer* GetLayer() override;

  virtual void SetLayerManager(LayerManagerComposite* aManager) override;

  virtual void RenderLayer(const gfx::IntRect& aClipRect) override;

  virtual void CleanupResources() override;

  virtual void GenEffectChain(EffectChain& aEffect) override;

  virtual bool SetCompositableHost(CompositableHost* aHost) override;

  virtual LayerComposite* AsLayerComposite() override { return this; }

  virtual void InvalidateRegion(const nsIntRegion& aRegion) override
  {
    NS_RUNTIMEABORT("PaintedLayerComposites can't fill invalidated regions");
  }

  void SetValidRegion(const nsIntRegion& aRegion)
  {
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ValidRegion", this));
    mValidRegion = aRegion;
    Mutated();
  }

  MOZ_LAYER_DECL_NAME("PaintedLayerComposite", TYPE_PAINTED)

protected:

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

private:
  gfx::Filter GetEffectFilter() { return gfx::Filter::LINEAR; }

private:
  RefPtr<ContentHost> mBuffer;
};

} /* layers */
} /* mozilla */
#endif /* GFX_PaintedLayerComposite_H */
