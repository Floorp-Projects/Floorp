/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERPAINTEDLAYER_H
#define GFX_WEBRENDERPAINTEDLAYER_H

#include "Layers.h"
#include "WebRenderLayerManager.h"

#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "mozilla/layers/TextureWrapperImage.h"

namespace mozilla {
namespace layers {

class WebRenderPaintedLayer : public WebRenderLayer,
                              public PaintedLayer {
public:

  explicit WebRenderPaintedLayer(WebRenderLayerManager* aLayerManager)
    : PaintedLayer(aLayerManager, static_cast<WebRenderLayer*>(this), LayerManager::NONE)
    , mExternalImageId(0)
  {
    MOZ_COUNT_CTOR(WebRenderPaintedLayer);
  }

  virtual void InvalidateRegion(const nsIntRegion& aRegion) override
  {
    mInvalidRegion.Add(aRegion);
    mValidRegion.Sub(mValidRegion, mInvalidRegion.GetRegion());
  }

  Layer* GetLayer() override { return this; }
  void RenderLayer() override;

private:
  virtual ~WebRenderPaintedLayer();

  WebRenderLayerManager* Manager()
  {
    return static_cast<WebRenderLayerManager*>(mManager);
  }

  uint64_t mExternalImageId;
  RefPtr<ImageContainer> mImageContainer;
  RefPtr<ImageClient> mImageClient;
};

} // namespace layers
} // namespace mozilla

#endif // GFX_WEBRENDERPAINTEDLAYER_H
