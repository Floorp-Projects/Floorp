/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GPU_VIDEO_IMAGE_H
#define GFX_GPU_VIDEO_IMAGE_H

#include "mozilla/RefPtr.h"
#include "ImageContainer.h"
#include "mozilla/layers/GPUVideoTextureClient.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/ImageBridgeChild.h"

namespace mozilla {
namespace dom {
class VideoDecoderManagerChild;
}
namespace gl {
class GLBlitHelper;
}
namespace layers {

// Image class that refers to a decoded video frame within
// the GPU process.
class GPUVideoImage final : public Image {
  friend class gl::GLBlitHelper;

 public:
  GPUVideoImage(VideoDecoderManagerChild* aManager,
                const SurfaceDescriptorGPUVideo& aSD, const gfx::IntSize& aSize)
      : Image(nullptr, ImageFormat::GPU_VIDEO), mSize(aSize) {
    // Create the TextureClient immediately since the GPUVideoTextureData
    // is responsible for deallocating the SurfaceDescriptor.
    //
    // Use the RECYCLE texture flag, since it's likely that our 'real'
    // TextureData (in the decoder thread of the GPU process) is using
    // it too, and we want to make sure we don't send the delete message
    // until we've stopped being used on the compositor.
    mTextureClient = TextureClient::CreateWithData(
        new GPUVideoTextureData(aManager, aSD, aSize), TextureFlags::RECYCLE,
        ImageBridgeChild::GetSingleton().get());
  }

  virtual ~GPUVideoImage() = default;

  gfx::IntSize GetSize() const override { return mSize; }

 private:
  GPUVideoTextureData* GetData() const {
    if (!mTextureClient) {
      return nullptr;
    }
    TextureData* data = mTextureClient->GetInternalData();
    if (!data) {
      return nullptr;
    }
    return data->AsGPUVideoTextureData();
  }

 public:
  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override {
    GPUVideoTextureData* data = GetData();
    if (!data) {
      return nullptr;
    }
    return data->GetAsSourceSurface();
  }

  TextureClient* GetTextureClient(KnowsCompositor* aForwarder) override {
    MOZ_ASSERT(aForwarder == ImageBridgeChild::GetSingleton(),
               "Must only use GPUVideo on ImageBridge");
    return mTextureClient;
  }

 private:
  gfx::IntSize mSize;
  RefPtr<TextureClient> mTextureClient;
};

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_GPU_VIDEO_IMAGE_H
