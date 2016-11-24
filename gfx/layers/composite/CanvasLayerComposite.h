/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CanvasLayerComposite_H
#define GFX_CanvasLayerComposite_H

#include "Layers.h"                     // for CanvasLayer, etc
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerComposite, etc
#include "mozilla/layers/LayersTypes.h"  // for LayerRenderState, etc
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nscore.h"                     // for nsACString

namespace mozilla {
namespace layers {

class CompositableHost;
// Canvas layers use ImageHosts (but CanvasClients) because compositing a
// canvas is identical to compositing an image.
class ImageHost;

class CanvasLayerComposite : public CanvasLayer,
                             public LayerComposite
{
public:
  explicit CanvasLayerComposite(LayerManagerComposite* aManager);

protected:
  virtual ~CanvasLayerComposite();

public:
  // CanvasLayer impl
  virtual void Initialize(const Data& aData) override
  {
    NS_RUNTIMEABORT("Incompatibe surface type");
  }

  virtual LayerRenderState GetRenderState() override;

  virtual bool SetCompositableHost(CompositableHost* aHost) override;

  virtual void Disconnect() override
  {
    Destroy();
  }

  virtual void SetLayerManager(HostLayerManager* aManager) override;

  virtual Layer* GetLayer() override;
  virtual void RenderLayer(const gfx::IntRect& aClipRect) override;

  virtual void CleanupResources() override;

  virtual void GenEffectChain(EffectChain& aEffect) override;

  CompositableHost* GetCompositableHost() override;

  virtual HostLayer* AsHostLayer() override { return this; }

  virtual const char* Name() const override { return "CanvasLayerComposite"; }

protected:
  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

private:
  gfx::SamplingFilter GetSamplingFilter();

private:
  RefPtr<CompositableHost> mCompositableHost;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_CanvasLayerComposite_H */
