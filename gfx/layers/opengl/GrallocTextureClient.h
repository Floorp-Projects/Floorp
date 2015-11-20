/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GRALLOCTEXTURECLIENT_H
#define MOZILLA_GFX_GRALLOCTEXTURECLIENT_H
#ifdef MOZ_WIDGET_GONK

#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/ISurfaceAllocator.h" // For IsSurfaceDescriptorValid
#include "mozilla/layers/FenceUtils.h" // for FenceHandle
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include <ui/GraphicBuffer.h>


namespace android {
class MediaBuffer;
};

namespace mozilla {
namespace gl {
class SharedSurface;
}

namespace layers {

/// A TextureData implementation based on android::GraphicBuffer (also referred to
/// as "gralloc").
///
/// Gralloc lets us map texture data in memory (accessible through pointers)
/// and also use it directly as an OpenGL texture without the cost of texture
/// uploads.
/// Gralloc buffers can also be shared accros processes.
///
/// More info about Gralloc here: https://wiki.mozilla.org/Platform/GFX/Gralloc
///
/// This is only used in Firefox OS
class GrallocTextureData : public TextureData {
public:
  typedef uint32_t AndroidFormat;

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual bool Lock(OpenMode aMode, FenceHandle* aFence) override;

  virtual void Unlock() override;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override;

  virtual bool CanExposeMappedData() const override { return true; }

  virtual bool BorrowMappedData(MappedTextureData& aMap) override;

  virtual bool SupportsMoz2D() const override { return true; }

  virtual bool HasInternalBuffer() const override { return false; }

  virtual bool HasSynchronization() const override { return true; }

  virtual void Deallocate(ISurfaceAllocator*) override;

  virtual void Forget(ISurfaceAllocator*) override;

  static GrallocTextureData* CreateForDrawing(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                              gfx::BackendType aMoz2dBackend,
                                              ISurfaceAllocator* aAllocator);

  static GrallocTextureData* CreateForYCbCr(gfx::IntSize aYSize, gfx::IntSize aCbCrSize,
                                            ISurfaceAllocator* aAllocator);

  static GrallocTextureData* CreateForGLRendering(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                                  ISurfaceAllocator* aAllocator);

  static GrallocTextureData* Create(gfx::IntSize aSize, AndroidFormat aFormat,
                                    gfx::BackendType aMoz2DBackend, uint32_t aUsage,
                                    ISurfaceAllocator* aAllocator);


  static already_AddRefed<TextureClient>
  TextureClientFromSharedSurface(gl::SharedSurface* abstractSurf, TextureFlags flags);

  virtual TextureData*
  CreateSimilar(ISurfaceAllocator* aAllocator,
                TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

  // use TextureClient's default implementation
  virtual bool UpdateFromSurface(gfx::SourceSurface* aSurface) override;

  /// Hold android::MediaBuffer.
  /// MediaBuffer needs to be add refed to keep MediaBuffer alive while the texture
  /// is in use.
  ///
  /// TODO - ideally we should be able to put the MediaBuffer in the texture's
  /// constructor and not expose these methods.
  void SetMediaBuffer(android::MediaBuffer* aMediaBuffer) { mMediaBuffer = aMediaBuffer; }
  android::MediaBuffer* GetMediaBuffer() { return mMediaBuffer; }

  android::sp<android::GraphicBuffer> GetGraphicBuffer() { return mGraphicBuffer; }

  virtual void WaitForFence(FenceHandle* aFence) override;

  ~GrallocTextureData();

protected:
  GrallocTextureData(MaybeMagicGrallocBufferHandle aGrallocHandle,
                     gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                     gfx::BackendType aMoz2DBackend);

  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  gfx::BackendType mMoz2DBackend;

  MaybeMagicGrallocBufferHandle mGrallocHandle;
  android::sp<android::GraphicBuffer> mGraphicBuffer;

  // Points to a mapped gralloc buffer between calls to lock and unlock.
  // Should be null outside of the lock-unlock pair.
  uint8_t* mMappedBuffer;

  android::MediaBuffer* mMediaBuffer;
};

gfx::SurfaceFormat SurfaceFormatForPixelFormat(android::PixelFormat aFormat);

already_AddRefed<TextureClient> CreateGrallocTextureClientForDrawing(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                                                     gfx::BackendType aMoz2dBackend, TextureFlags aFlags,
                                                                     ISurfaceAllocator* aAllocator);

} // namespace layers
} // namespace mozilla

#endif // MOZ_WIDGET_GONK
#endif
