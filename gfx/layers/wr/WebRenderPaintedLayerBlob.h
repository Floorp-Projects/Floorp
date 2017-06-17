/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERPAINTEDLAYERBLOB_H
#define GFX_WEBRENDERPAINTEDLAYERBLOB_H

#include "Layers.h"
#include "mozilla/layers/ContentClient.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderLayer.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

class WebRenderPaintedLayerBlob : public WebRenderLayer,
                                  public PaintedLayer {
public:
  typedef RotatedContentBuffer::PaintState PaintState;
  typedef RotatedContentBuffer::ContentType ContentType;

  explicit WebRenderPaintedLayerBlob(WebRenderLayerManager* aLayerManager)
    : PaintedLayer(aLayerManager, static_cast<WebRenderLayer*>(this), LayerManager::NONE)
  {
    MOZ_COUNT_CTOR(WebRenderPaintedLayerBlob);
  }

protected:
  virtual ~WebRenderPaintedLayerBlob()
  {
    MOZ_COUNT_DTOR(WebRenderPaintedLayerBlob);
    if (mExternalImageId.isSome()) {
      WrBridge()->DeallocExternalImageId(mExternalImageId.ref());
    }
    if (mImageKey.isSome()) {
      WrManager()->AddImageKeyForDiscard(mImageKey.value());
    }
  }

  wr::MaybeExternalImageId mExternalImageId;

public:
  virtual void InvalidateRegion(const nsIntRegion& aRegion) override
  {
    mInvalidRegion.Add(aRegion);
    UpdateValidRegionAfterInvalidRegionChanged();
  }

  Layer* GetLayer() override { return this; }
  void RenderLayer(wr::DisplayListBuilder& aBuilder,
                   const StackingContextHelper& aSc) override;
private:
  RefPtr<ImageContainer> mImageContainer;
  RefPtr<ImageClient> mImageClient;
  Maybe<WrImageKey> mImageKey;
};

} // namespace layers
} // namespace mozilla

#endif // GFX_WEBRENDERPAINTEDLAYERBLOB_H
