/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERPAINTEDLAYER_H
#define GFX_WEBRENDERPAINTEDLAYER_H

#include "Layers.h"
#include "mozilla/layers/ContentClient.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "WebRenderLayerManager.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

class WebRenderPaintedLayer : public WebRenderLayer,
                              public PaintedLayer {
public:
  typedef RotatedContentBuffer::PaintState PaintState;
  typedef RotatedContentBuffer::ContentType ContentType;

  explicit WebRenderPaintedLayer(WebRenderLayerManager* aLayerManager)
    : PaintedLayer(aLayerManager, static_cast<WebRenderLayer*>(this), LayerManager::NONE)
  {
    MOZ_COUNT_CTOR(WebRenderPaintedLayer);
  }

protected:
  virtual ~WebRenderPaintedLayer()
  {
    MOZ_COUNT_DTOR(WebRenderPaintedLayer);
    if (mExternalImageId.isSome()) {
      WrBridge()->DeallocExternalImageId(mExternalImageId.ref());
    }
  }
  WebRenderLayerManager* Manager()
  {
    return static_cast<WebRenderLayerManager*>(mManager);
  }

  Maybe<wr::ExternalImageId> mExternalImageId;

public:
  virtual void InvalidateRegion(const nsIntRegion& aRegion) override
  {
    mInvalidRegion.Add(aRegion);
    mValidRegion.Sub(mValidRegion, mInvalidRegion.GetRegion());
  }

  Layer* GetLayer() override { return this; }
  void RenderLayer(wr::DisplayListBuilder& aBuilder) override;
  RefPtr<ImageContainer> mImageContainer;
  RefPtr<ImageClient> mImageClient;

private:
  bool SetupExternalImages();
  bool UpdateImageClient();
  void CreateWebRenderDisplayList(wr::DisplayListBuilder& aBuilder);
};

} // namespace layers
} // namespace mozilla

#endif // GFX_WEBRENDERPAINTEDLAYER_H
