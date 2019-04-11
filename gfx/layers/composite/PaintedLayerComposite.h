/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PaintedLayerComposite_H
#define GFX_PaintedLayerComposite_H

#include "Layers.h"  // for Layer (ptr only), etc
#include "mozilla/gfx/Rect.h"
#include "mozilla/Attributes.h"                    // for override
#include "mozilla/RefPtr.h"                        // for RefPtr
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerComposite, etc
#include "mozilla/layers/LayersTypes.h"            // for LayerRenderState, etc
#include "nsRegion.h"                              // for nsIntRegion
#include "nscore.h"                                // for nsACString

namespace mozilla {
namespace layers {

/**
 * PaintedLayers use ContentHosts for their compsositable host.
 * By using different ContentHosts, PaintedLayerComposite support tiled and
 * non-tiled PaintedLayers and single or double buffering.
 */

class CompositableHost;
class ContentHost;

class PaintedLayerComposite : public PaintedLayer, public LayerComposite {
 public:
  explicit PaintedLayerComposite(LayerManagerComposite* aManager);

 protected:
  virtual ~PaintedLayerComposite();

 public:
  void Disconnect() override;

  CompositableHost* GetCompositableHost() override;

  void Destroy() override;

  Layer* GetLayer() override;

  void SetLayerManager(HostLayerManager* aManager) override;

  void RenderLayer(const gfx::IntRect& aClipRect,
                   const Maybe<gfx::Polygon>& aGeometry) override;

  void CleanupResources() override;

  bool IsOpaque() override;

  void GenEffectChain(EffectChain& aEffect) override;

  bool SetCompositableHost(CompositableHost* aHost) override;

  HostLayer* AsHostLayer() override { return this; }

  void InvalidateRegion(const nsIntRegion& aRegion) override {
    MOZ_CRASH("PaintedLayerComposites can't fill invalidated regions");
  }

  const gfx::TiledIntRegion& GetInvalidRegion() override;

  MOZ_LAYER_DECL_NAME("PaintedLayerComposite", TYPE_PAINTED)

 protected:
  virtual void PrintInfo(std::stringstream& aStream,
                         const char* aPrefix) override;

 private:
  gfx::SamplingFilter GetSamplingFilter() {
    return gfx::SamplingFilter::LINEAR;
  }

 private:
  RefPtr<ContentHost> mBuffer;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_PaintedLayerComposite_H */
