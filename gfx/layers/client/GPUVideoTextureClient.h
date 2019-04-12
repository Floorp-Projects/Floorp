/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GPUVIDEOTEXTURECLIENT_H
#define MOZILLA_GFX_GPUVIDEOTEXTURECLIENT_H

#include "mozilla/layers/TextureClient.h"

namespace mozilla {
namespace gfx {
class SourceSurface;
}
class VideoDecoderManagerChild;

namespace layers {

class GPUVideoTextureData : public TextureData {
 public:
  GPUVideoTextureData(VideoDecoderManagerChild* aManager,
                      const SurfaceDescriptorGPUVideo& aSD,
                      const gfx::IntSize& aSize);
  virtual ~GPUVideoTextureData();

  void FillInfo(TextureData::Info& aInfo) const override;

  bool Lock(OpenMode) override { return true; };

  void Unlock() override{};

  bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  void Deallocate(LayersIPCChannel* aAllocator) override;

  void Forget(LayersIPCChannel* aAllocator) override;

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface();

  GPUVideoTextureData* AsGPUVideoTextureData() override { return this; }

 protected:
  RefPtr<VideoDecoderManagerChild> mManager;
  SurfaceDescriptorGPUVideo mSD;
  gfx::IntSize mSize;

 public:
  const decltype(mSD)& SD() const { return mSD; }
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_GPUVIDEOTEXTURECLIENT_H
