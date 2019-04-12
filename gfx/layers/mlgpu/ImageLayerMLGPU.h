/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGELAYERMLGPU_H
#define MOZILLA_GFX_IMAGELAYERMLGPU_H

#include "LayerManagerMLGPU.h"
#include "TexturedLayerMLGPU.h"
#include "ImageLayers.h"
#include "mozilla/layers/ImageHost.h"

namespace mozilla {
namespace layers {

class ImageLayerMLGPU final : public ImageLayer, public TexturedLayerMLGPU {
 public:
  explicit ImageLayerMLGPU(LayerManagerMLGPU* aManager);

  Layer* GetLayer() override { return this; }
  HostLayer* AsHostLayer() override { return this; }
  ImageLayerMLGPU* AsImageLayerMLGPU() override { return this; }

  void ComputeEffectiveTransforms(
      const gfx::Matrix4x4& aTransformToSurface) override;
  void SetRenderRegion(LayerIntRegion&& aRegion) override;
  gfx::SamplingFilter GetSamplingFilter() override;
  void ClearCachedResources() override;
  bool IsContentOpaque() override;
  void Disconnect() override;

  Maybe<gfx::Size> GetPictureScale() const override { return mScale; }

  MOZ_LAYER_DECL_NAME("ImageLayerMLGPU", TYPE_IMAGE)

 protected:
  virtual ~ImageLayerMLGPU();

  void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;
  void CleanupResources();

 private:
  Maybe<gfx::Size> mScale;
};

}  // namespace layers
}  // namespace mozilla

#endif
