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
namespace gl {
class GLBlitHelper;
}
namespace layers {

class IGPUVideoSurfaceManager {
 protected:
  virtual ~IGPUVideoSurfaceManager() = default;

 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual already_AddRefed<gfx::SourceSurface> Readback(
      const SurfaceDescriptorGPUVideo& aSD) = 0;
  virtual void DeallocateSurfaceDescriptor(
      const SurfaceDescriptorGPUVideo& aSD) = 0;
};

// Represents an animated Image that is known to the GPU process.
class GPUVideoImage final : public Image {
  friend class gl::GLBlitHelper;

 public:
  GPUVideoImage(IGPUVideoSurfaceManager* aManager,
                const SurfaceDescriptorGPUVideo& aSD, const gfx::IntSize& aSize,
                const gfx::ColorDepth& aColorDepth,
                gfx::YUVColorSpace aYUVColorSpace,
                gfx::ColorSpace2 aColorPrimaries,
                gfx::TransferFunction aTransferFunction,
                gfx::ColorRange aColorRange)
      : Image(nullptr, ImageFormat::GPU_VIDEO),
        mSize(aSize),
        mColorDepth(aColorDepth),
        mColorSpace(aColorPrimaries),
        mYUVColorSpace(aYUVColorSpace),
        mTransferFunction(aTransferFunction),
        mColorRange(aColorRange) {
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

  GPUVideoImage* AsGPUVideoImage() override { return this; }

  gfx::IntSize GetSize() const override { return mSize; }

  gfx::ColorDepth GetColorDepth() const override { return mColorDepth; }
  gfx::ColorSpace2 GetColorPrimaries() const { return mColorSpace; }
  gfx::YUVColorSpace GetYUVColorSpace() const { return mYUVColorSpace; }
  gfx::TransferFunction GetTransferFunction() const { return mTransferFunction; }
  gfx::ColorRange GetColorRange() const { return mColorRange; }

  Maybe<SurfaceDescriptor> GetDesc() override {
    return GetDescFromTexClient(mTextureClient);
  }

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

  TextureClient* GetTextureClient(KnowsCompositor* aKnowsCompositor) override {
    MOZ_ASSERT(aKnowsCompositor == ImageBridgeChild::GetSingleton(),
               "Must only use GPUVideo on ImageBridge");
    return mTextureClient;
  }

 private:
  gfx::IntSize mSize;
  gfx::ColorDepth mColorDepth;
  gfx::ColorSpace2 mColorSpace;
  gfx::YUVColorSpace mYUVColorSpace;
  RefPtr<TextureClient> mTextureClient;
  gfx::TransferFunction mTransferFunction;
  gfx::ColorRange mColorRange;
};

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_GPU_VIDEO_IMAGE_H
