/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERD3D11TEXTUREHOST_H
#define MOZILLA_GFX_RENDERD3D11TEXTUREHOST_H

#include "GLTypes.h"
#include "RenderTextureHostSWGL.h"

struct ID3D11Texture2D;
struct IDXGIKeyedMutex;

namespace mozilla {

namespace wr {

class RenderDXGITextureHost final : public RenderTextureHostSWGL {
 public:
  RenderDXGITextureHost(WindowsHandle aHandle,
                        Maybe<uint64_t>& aGpuProcessTextureId,
                        uint32_t aArrayIndex, gfx::SurfaceFormat aFormat,
                        gfx::ColorSpace2, gfx::ColorRange aColorRange,
                        gfx::IntSize aSize);

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL,
                           wr::ImageRendering aRendering) override;
  void Unlock() override;
  void ClearCachedResources() override;

  gfx::IntSize GetSize(uint8_t aChannelIndex) const;
  GLuint GetGLHandle(uint8_t aChannelIndex) const;

  bool SyncObjectNeeded() override { return true; }

  RenderDXGITextureHost* AsRenderDXGITextureHost() override { return this; }

  gfx::ColorRange GetColorRange() const { return mColorRange; }

  ID3D11Texture2D* GetD3D11Texture2DWithGL();
  ID3D11Texture2D* GetD3D11Texture2D() { return mTexture; }

  // RenderTextureHostSWGL
  gfx::SurfaceFormat GetFormat() const override { return mFormat; }
  gfx::ColorDepth GetColorDepth() const override {
    if (mFormat == gfx::SurfaceFormat::P010) {
      return gfx::ColorDepth::COLOR_10;
    }
    if (mFormat == gfx::SurfaceFormat::P016) {
      return gfx::ColorDepth::COLOR_16;
    }
    return gfx::ColorDepth::COLOR_8;
  }
  size_t GetPlaneCount() const override;
  bool MapPlane(RenderCompositor* aCompositor, uint8_t aChannelIndex,
                PlaneInfo& aPlaneInfo) override;
  void UnmapPlanes() override;
  gfx::YUVRangedColorSpace GetYUVColorSpace() const override {
    return ToYUVRangedColorSpace(ToYUVColorSpace(mColorSpace), mColorRange);
  }

  bool EnsureD3D11Texture2D(ID3D11Device* aDevice);
  bool LockInternal();

  size_t Bytes() override {
    size_t bytes = 0;

    size_t bpp = GetPlaneCount() > 1
                     ? (GetColorDepth() == gfx::ColorDepth::COLOR_8 ? 1 : 2)
                     : 4;

    for (size_t i = 0; i < GetPlaneCount(); i++) {
      gfx::IntSize size = GetSize(i);
      bytes += size.width * size.height * bpp;
    }
    return bytes;
  }

  uint32_t ArrayIndex() const { return mArrayIndex; }

 private:
  virtual ~RenderDXGITextureHost();

  bool EnsureD3D11Texture2DWithGL();
  bool EnsureLockable(wr::ImageRendering aRendering);

  void DeleteTextureHandle();

  RefPtr<gl::GLContext> mGL;

  WindowsHandle mHandle;
  Maybe<uint64_t> mGpuProcessTextureId;
  RefPtr<ID3D11Texture2D> mTexture;
  uint32_t mArrayIndex = 0;
  RefPtr<IDXGIKeyedMutex> mKeyedMutex;

  // Temporary state between MapPlane and UnmapPlanes.
  RefPtr<ID3D11DeviceContext> mDeviceContext;
  RefPtr<ID3D11Texture2D> mCpuTexture;
  D3D11_MAPPED_SUBRESOURCE mMappedSubresource;

  EGLSurface mSurface;
  EGLStreamKHR mStream;

  // We could use NV12 format for this texture. So, we might have 2 gl texture
  // handles for Y and CbCr data.
  GLuint mTextureHandle[2];

 public:
  const gfx::SurfaceFormat mFormat;
  const gfx::ColorSpace2 mColorSpace;
  const gfx::ColorRange mColorRange;
  const gfx::IntSize mSize;

 private:
  bool mLocked;
};

class RenderDXGIYCbCrTextureHost final : public RenderTextureHostSWGL {
 public:
  explicit RenderDXGIYCbCrTextureHost(WindowsHandle (&aHandles)[3],
                                      gfx::YUVColorSpace aYUVColorSpace,
                                      gfx::ColorDepth aColorDepth,
                                      gfx::ColorRange aColorRange,
                                      gfx::IntSize aSizeY,
                                      gfx::IntSize aSizeCbCr);

  RenderDXGIYCbCrTextureHost* AsRenderDXGIYCbCrTextureHost() override {
    return this;
  }

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL,
                           wr::ImageRendering aRendering) override;
  void Unlock() override;
  void ClearCachedResources() override;

  gfx::IntSize GetSize(uint8_t aChannelIndex) const;
  GLuint GetGLHandle(uint8_t aChannelIndex) const;

  bool SyncObjectNeeded() override { return true; }

  gfx::ColorRange GetColorRange() const { return mColorRange; }

  // RenderTextureHostSWGL
  gfx::SurfaceFormat GetFormat() const override {
    return gfx::SurfaceFormat::YUV;
  }
  gfx::ColorDepth GetColorDepth() const override { return mColorDepth; }
  size_t GetPlaneCount() const override { return 3; }
  bool MapPlane(RenderCompositor* aCompositor, uint8_t aChannelIndex,
                PlaneInfo& aPlaneInfo) override;
  void UnmapPlanes() override;
  gfx::YUVRangedColorSpace GetYUVColorSpace() const override {
    return ToYUVRangedColorSpace(mYUVColorSpace, GetColorRange());
  }

  bool EnsureD3D11Texture2D(ID3D11Device* aDevice);
  bool LockInternal();

  ID3D11Texture2D* GetD3D11Texture2D(uint8_t aChannelIndex) {
    return mTextures[aChannelIndex];
  }

  size_t Bytes() override {
    size_t bytes = 0;

    size_t bpp = mColorDepth == gfx::ColorDepth::COLOR_8 ? 1 : 2;

    for (size_t i = 0; i < GetPlaneCount(); i++) {
      gfx::IntSize size = GetSize(i);
      bytes += size.width * size.height * bpp;
    }
    return bytes;
  }

 private:
  virtual ~RenderDXGIYCbCrTextureHost();

  bool EnsureLockable(wr::ImageRendering aRendering);

  void DeleteTextureHandle();

  RefPtr<gl::GLContext> mGL;

  WindowsHandle mHandles[3];
  RefPtr<ID3D11Texture2D> mTextures[3];
  RefPtr<IDXGIKeyedMutex> mKeyedMutexs[3];

  EGLSurface mSurfaces[3];
  EGLStreamKHR mStreams[3];

  // The gl handles for Y, Cb and Cr data.
  GLuint mTextureHandles[3];

  // Temporary state between MapPlane and UnmapPlanes.
  RefPtr<ID3D11DeviceContext> mDeviceContext;
  RefPtr<ID3D11Texture2D> mCpuTexture[3];

  gfx::YUVColorSpace mYUVColorSpace;
  gfx::ColorDepth mColorDepth;
  gfx::ColorRange mColorRange;
  gfx::IntSize mSizeY;
  gfx::IntSize mSizeCbCr;

  bool mLocked;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERD3D11TEXTUREHOST_H
