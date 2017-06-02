/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERIMAGELAYER_H
#define GFX_WEBRENDERIMAGELAYER_H

#include "ImageLayers.h"
#include "mozilla/layers/WebRenderLayer.h"
#include "mozilla/layers/WebRenderLayerManager.h"

namespace mozilla {
namespace layers {

class ImageClient;

class WebRenderImageLayer : public WebRenderLayer,
                            public ImageLayer {
public:
  explicit WebRenderImageLayer(WebRenderLayerManager* aLayerManager);

  virtual already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  virtual void ClearCachedResources() override;

protected:
  virtual ~WebRenderImageLayer();

public:
  Layer* GetLayer() override { return this; }
  void RenderLayer(wr::DisplayListBuilder& aBuilder,
                   const StackingContextHelper& aSc) override;
  Maybe<WrImageMask> RenderMaskLayer(const gfx::Matrix4x4& aTransform) override;

protected:
  CompositableType GetImageClientType();

  void AddWRVideoImage(size_t aChannelNumber);

  wr::MaybeExternalImageId mExternalImageId;
  Maybe<wr::ImageKey> mKey;
  RefPtr<ImageClient> mImageClient;
  CompositableType mImageClientTypeContainer;
  Maybe<wr::PipelineId> mPipelineId;
};

} // namespace layers
} // namespace mozilla

#endif // GFX_WEBRENDERIMAGELAYER_H
