/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ThebesLayerComposite_H
#define GFX_ThebesLayerComposite_H

#include "Layers.h"                     // for Layer (ptr only), etc
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerComposite, etc
#include "mozilla/layers/LayersTypes.h"  // for LayerRenderState, etc
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsRegion.h"                   // for nsIntRegion
#include "nscore.h"                     // for nsACString

struct nsIntPoint;
struct nsIntRect;


namespace mozilla {
namespace layers {

/**
 * Thebes layers use ContentHosts for their compsositable host.
 * By using different ContentHosts, ThebesLayerComposite support tiled and
 * non-tiled Thebes layers and single or double buffering.
 */

class CompositableHost;
class ContentHost;
class TiledLayerComposer;

class ThebesLayerComposite : public ThebesLayer,
                             public LayerComposite
{
public:
  ThebesLayerComposite(LayerManagerComposite *aManager);

protected:
  virtual ~ThebesLayerComposite();

public:
  virtual void Disconnect() MOZ_OVERRIDE;

  virtual LayerRenderState GetRenderState() MOZ_OVERRIDE;

  CompositableHost* GetCompositableHost() MOZ_OVERRIDE;

  virtual void Destroy() MOZ_OVERRIDE;

  virtual Layer* GetLayer() MOZ_OVERRIDE;

  virtual TiledLayerComposer* GetTiledLayerComposer() MOZ_OVERRIDE;

  virtual void RenderLayer(const nsIntRect& aClipRect) MOZ_OVERRIDE;

  virtual void CleanupResources() MOZ_OVERRIDE;

  virtual void GenEffectChain(EffectChain& aEffect) MOZ_OVERRIDE;

  virtual bool SetCompositableHost(CompositableHost* aHost) MOZ_OVERRIDE;

  virtual LayerComposite* AsLayerComposite() MOZ_OVERRIDE { return this; }

  void EnsureTiled() { mRequiresTiledProperties = true; }

  virtual void InvalidateRegion(const nsIntRegion& aRegion)
  {
    NS_RUNTIMEABORT("ThebesLayerComposites can't fill invalidated regions");
  }

  void SetValidRegion(const nsIntRegion& aRegion)
  {
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ValidRegion", this));
    mValidRegion = aRegion;
    Mutated();
  }

  MOZ_LAYER_DECL_NAME("ThebesLayerComposite", TYPE_THEBES)

protected:

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) MOZ_OVERRIDE;

private:
  gfx::Filter GetEffectFilter() { return gfx::Filter::LINEAR; }

  CSSToScreenScale GetEffectiveResolution();

private:
  RefPtr<ContentHost> mBuffer;
  bool mRequiresTiledProperties;
};

} /* layers */
} /* mozilla */
#endif /* GFX_ThebesLayerComposite_H */
