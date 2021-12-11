/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderExternalTextureHost.h"

#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/ImageDataSerializer.h"

#include "GLContext.h"

namespace mozilla {
namespace wr {

RenderExternalTextureHost::RenderExternalTextureHost(
    uint8_t* aBuffer, const layers::BufferDescriptor& aDescriptor)
    : mBuffer(aBuffer),
      mDescriptor(aDescriptor),
      mInitialized(false),
      mTextureUpdateNeeded(true) {
  MOZ_COUNT_CTOR_INHERITED(RenderExternalTextureHost, RenderTextureHost);

  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor: {
      const layers::YCbCrDescriptor& ycbcr = mDescriptor.get_YCbCrDescriptor();
      mSize = ycbcr.ySize();
      mFormat = gfx::SurfaceFormat::YUV;
      break;
    }
    case layers::BufferDescriptor::TRGBDescriptor: {
      const layers::RGBDescriptor& rgb = mDescriptor.get_RGBDescriptor();
      mSize = rgb.size();
      mFormat = rgb.format();
      break;
    }
    default:
      gfxCriticalError() << "Bad buffer host descriptor "
                         << (int)mDescriptor.type();
      MOZ_CRASH("GFX: Bad descriptor");
  }
}

RenderExternalTextureHost::~RenderExternalTextureHost() {
  MOZ_COUNT_DTOR_INHERITED(RenderExternalTextureHost, RenderTextureHost);

  if (NS_WARN_IF(!IsReadyForDeletion())) {
    gfxCriticalNote << "RenderExternalTextureHost sync failed";
  }

  DeleteTextures();
}

bool RenderExternalTextureHost::CreateSurfaces() {
  if (!IsYUV()) {
    mSurfaces[0] = gfx::Factory::CreateWrappingDataSourceSurface(
        GetBuffer(),
        layers::ImageDataSerializer::GetRGBStride(
            mDescriptor.get_RGBDescriptor()),
        mSize, mFormat);
  } else {
    const layers::YCbCrDescriptor& desc = mDescriptor.get_YCbCrDescriptor();
    const gfx::SurfaceFormat surfaceFormat =
        SurfaceFormatForColorDepth(desc.colorDepth());

    mSurfaces[0] = gfx::Factory::CreateWrappingDataSourceSurface(
        layers::ImageDataSerializer::GetYChannel(GetBuffer(), desc),
        desc.yStride(), desc.ySize(), surfaceFormat);
    mSurfaces[1] = gfx::Factory::CreateWrappingDataSourceSurface(
        layers::ImageDataSerializer::GetCbChannel(GetBuffer(), desc),
        desc.cbCrStride(), desc.cbCrSize(), surfaceFormat);
    mSurfaces[2] = gfx::Factory::CreateWrappingDataSourceSurface(
        layers::ImageDataSerializer::GetCrChannel(GetBuffer(), desc),
        desc.cbCrStride(), desc.cbCrSize(), surfaceFormat);
  }

  for (size_t i = 0; i < PlaneCount(); ++i) {
    if (NS_WARN_IF(!mSurfaces[i])) {
      gfxCriticalNote << "Surface is null";
      return false;
    }
  }

  return true;
}

void RenderExternalTextureHost::DeleteSurfaces() {
  for (size_t i = 0; i < PlaneCount(); ++i) {
    mSurfaces[i] = nullptr;
  }
}

void RenderExternalTextureHost::DeleteTextures() {
  for (size_t i = 0; i < PlaneCount(); ++i) {
    mTextureSources[i] = nullptr;
    mImages[i] = InvalidToWrExternalImage();
  }
}

bool RenderExternalTextureHost::InitializeIfNeeded() {
  if (mInitialized) {
    return true;
  }

  if (!GetBuffer()) {
    // We hit some problems to get the shmem.
    gfxCriticalNote << "GetBuffer Failed";
    return false;
  }

  if (!CreateSurfaces()) {
    DeleteSurfaces();
    return false;
  }

  mInitialized = true;
  return mInitialized;
}

bool RenderExternalTextureHost::IsReadyForDeletion() {
  if (!mInitialized) {
    return true;
  }

  auto& textureSource = mTextureSources[0];
  if (textureSource) {
    return textureSource->Sync(false);
  }

  return true;
}

wr::WrExternalImage RenderExternalTextureHost::Lock(
    uint8_t aChannelIndex, gl::GLContext* aGL, wr::ImageRendering aRendering) {
  if (mGL.get() != aGL) {
    mGL = aGL;
    mGL->MakeCurrent();
  }

  if (!mGL || !mGL->MakeCurrent()) {
    return InvalidToWrExternalImage();
  }

  if (!InitializeIfNeeded()) {
    return InvalidToWrExternalImage();
  }

  UpdateTextures(aRendering);
  return mImages[aChannelIndex];
}

void RenderExternalTextureHost::PrepareForUse() { mTextureUpdateNeeded = true; }

void RenderExternalTextureHost::Unlock() {}

void RenderExternalTextureHost::UpdateTexture(size_t aIndex) {
  MOZ_ASSERT(mSurfaces[aIndex]);

  auto& texture = mTextureSources[aIndex];

  if (texture) {
    texture->Update(mSurfaces[aIndex]);
  } else {
    texture = new layers::DirectMapTextureSource(mGL, mSurfaces[aIndex]);

    const GLuint handle = texture->GetTextureHandle();
    const gfx::IntSize& size = texture->GetSize();
    mImages[aIndex] =
        NativeTextureToWrExternalImage(handle, 0, 0, size.width, size.height);
  }

  MOZ_ASSERT(mGL->GetError() == LOCAL_GL_NO_ERROR);
}

void RenderExternalTextureHost::UpdateTextures(wr::ImageRendering aRendering) {
  const bool renderingChanged = IsFilterUpdateNecessary(aRendering);

  if (!mTextureUpdateNeeded && !renderingChanged) {
    // Nothing to do here.
    return;
  }

  for (size_t i = 0; i < PlaneCount(); ++i) {
    if (mTextureUpdateNeeded) {
      UpdateTexture(i);
    }

    if (renderingChanged) {
      const GLuint handle = mTextureSources[i]->GetTextureHandle();
      ActivateBindAndTexParameteri(mGL, LOCAL_GL_TEXTURE0,
                                   LOCAL_GL_TEXTURE_RECTANGLE_ARB, handle,
                                   aRendering);
    }
  }

  mTextureSources[0]->MaybeFenceTexture();
  mTextureUpdateNeeded = false;
}

size_t RenderExternalTextureHost::GetPlaneCount() const { return PlaneCount(); }

gfx::SurfaceFormat RenderExternalTextureHost::GetFormat() const {
  return mFormat;
}

gfx::ColorDepth RenderExternalTextureHost::GetColorDepth() const {
  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor:
      return mDescriptor.get_YCbCrDescriptor().colorDepth();
    default:
      return gfx::ColorDepth::COLOR_8;
  }
}

gfx::YUVRangedColorSpace RenderExternalTextureHost::GetYUVColorSpace() const {
  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor:
      return gfx::GetYUVRangedColorSpace(mDescriptor.get_YCbCrDescriptor());
    default:
      return gfx::YUVRangedColorSpace::Default;
  }
}

bool RenderExternalTextureHost::MapPlane(RenderCompositor* aCompositor,
                                         uint8_t aChannelIndex,
                                         PlaneInfo& aPlaneInfo) {
  if (!mBuffer) {
    // We hit some problems to get the shmem.
    gfxCriticalNote << "GetBuffer Failed";
    return false;
  }

  switch (mDescriptor.type()) {
    case layers::BufferDescriptor::TYCbCrDescriptor: {
      const layers::YCbCrDescriptor& desc = mDescriptor.get_YCbCrDescriptor();
      switch (aChannelIndex) {
        case 0:
          aPlaneInfo.mData =
              layers::ImageDataSerializer::GetYChannel(mBuffer, desc);
          aPlaneInfo.mStride = desc.yStride();
          aPlaneInfo.mSize = desc.ySize();
          break;
        case 1:
          aPlaneInfo.mData =
              layers::ImageDataSerializer::GetCbChannel(mBuffer, desc);
          aPlaneInfo.mStride = desc.cbCrStride();
          aPlaneInfo.mSize = desc.cbCrSize();
          break;
        case 2:
          aPlaneInfo.mData =
              layers::ImageDataSerializer::GetCrChannel(mBuffer, desc);
          aPlaneInfo.mStride = desc.cbCrStride();
          aPlaneInfo.mSize = desc.cbCrSize();
          break;
      }
      break;
    }
    default: {
      const layers::RGBDescriptor& desc = mDescriptor.get_RGBDescriptor();
      aPlaneInfo.mData = mBuffer;
      aPlaneInfo.mStride = layers::ImageDataSerializer::GetRGBStride(desc);
      aPlaneInfo.mSize = desc.size();
      break;
    }
  }
  return true;
}

void RenderExternalTextureHost::UnmapPlanes() {}

}  // namespace wr
}  // namespace mozilla
