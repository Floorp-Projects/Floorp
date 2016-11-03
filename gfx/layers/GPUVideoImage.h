/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
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
namespace layers {

// Image class that refers to a decoded video frame within
// the GPU process.
class GPUVideoImage final : public Image {
public:
  GPUVideoImage(dom::VideoDecoderManagerChild* aManager,
                const SurfaceDescriptorGPUVideo& aSD,
                const gfx::IntSize& aSize)
    : Image(nullptr, ImageFormat::GPU_VIDEO)
    , mSize(aSize)
  {
    // Create the TextureClient immediately since the GPUVideoTextureData
    // is responsible for deallocating the SurfaceDescriptor.
    //
    // Use the RECYCLE texture flag, since it's likely that our 'real'
    // TextureData (in the decoder thread of the GPU process) is using
    // it too, and we want to make sure we don't send the delete message
    // until we've stopped being used on the compositor.
    mTextureClient =
      TextureClient::CreateWithData(new GPUVideoTextureData(aManager, aSD, aSize),
                                    TextureFlags::RECYCLE,
                                    ImageBridgeChild::GetSingleton().get());
  }

  ~GPUVideoImage() override {}

  gfx::IntSize GetSize() override { return mSize; }

  // TODO: We really want to be able to support this, but it's complex, since we need to readback
  // in the other process and send it across.
  virtual already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override { return nullptr; }

  virtual TextureClient* GetTextureClient(KnowsCompositor* aForwarder) override
  {
    MOZ_ASSERT(aForwarder == ImageBridgeChild::GetSingleton(), "Must only use GPUVideo on ImageBridge");
    return mTextureClient;
  }

private:
  gfx::IntSize mSize;
  RefPtr<TextureClient> mTextureClient;
};

} // namepace layers
} // namespace mozilla

#endif // GFX_GPU_VIDEO_IMAGE_H
