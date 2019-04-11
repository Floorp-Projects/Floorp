/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_TexturedLayerMLGPU_h
#define mozilla_gfx_layers_mlgpu_TexturedLayerMLGPU_h

#include "LayerMLGPU.h"
#include "ImageLayers.h"
#include "mozilla/layers/ImageHost.h"

namespace mozilla {
namespace layers {

// This is the base class for canvas and image layers.
class TexturedLayerMLGPU : public LayerMLGPU {
 public:
  TexturedLayerMLGPU* AsTexturedLayerMLGPU() override { return this; }

  virtual gfx::SamplingFilter GetSamplingFilter() = 0;

  bool SetCompositableHost(CompositableHost* aHost) override;
  CompositableHost* GetCompositableHost() override;

  void AssignToView(FrameBuilder* aBuilder, RenderViewMLGPU* aView,
                    Maybe<gfx::Polygon>&& aGeometry) override;

  TextureSource* GetTexture() const { return mTexture; }
  ImageHost* GetImageHost() const { return mHost; }

  // Return the scale factor from the texture source to the picture rect.
  virtual Maybe<gfx::Size> GetPictureScale() const { return Nothing(); }

  // Mask layers aren't prepared like normal layers. They are bound as
  // mask operations are built. Mask layers are never tiled (they are
  // scaled to a lower resolution if too big), so this pathway returns
  // a TextureSource.
  RefPtr<TextureSource> BindAndGetTexture();

 protected:
  explicit TexturedLayerMLGPU(LayerManagerMLGPU* aManager);
  virtual ~TexturedLayerMLGPU();

  void AssignBigImage(FrameBuilder* aBuilder, RenderViewMLGPU* aView,
                      BigImageIterator* aIter,
                      const Maybe<gfx::Polygon>& aGeometry);

  bool OnPrepareToRender(FrameBuilder* aBuilder) override;

 protected:
  RefPtr<ImageHost> mHost;
  RefPtr<TextureSource> mTexture;
  RefPtr<TextureSource> mBigImageTexture;
  gfx::IntRect mPictureRect;
};

// This is a pseudo layer that wraps a tile in an ImageLayer backed by a
// BigImage. Without this, we wouldn't have anything sensible to add to
// RenderPasses. In the future we could potentially consume the source
// layer more intelligently instead (for example, having it compute
// which textures are relevant for a given tile).
class TempImageLayerMLGPU final : public ImageLayer, public TexturedLayerMLGPU {
 public:
  explicit TempImageLayerMLGPU(LayerManagerMLGPU* aManager);

  // Layer
  HostLayer* AsHostLayer() override { return this; }
  gfx::SamplingFilter GetSamplingFilter() override { return mFilter; }
  bool IsContentOpaque() override { return mIsOpaque; }

  void Init(TexturedLayerMLGPU* aSource, const RefPtr<TextureSource>& aTexture,
            const gfx::IntRect& aPictureRect);

  // HostLayer
  Layer* GetLayer() override { return this; }

 protected:
  virtual ~TempImageLayerMLGPU();

 private:
  gfx::SamplingFilter mFilter;
  bool mIsOpaque;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_gfx_layers_mlgpu_TexturedLayerMLGPU_h
