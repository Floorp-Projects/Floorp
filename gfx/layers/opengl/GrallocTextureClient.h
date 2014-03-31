/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GRALLOCTEXTURECLIENT_H
#define MOZILLA_GFX_GRALLOCTEXTURECLIENT_H
#ifdef MOZ_WIDGET_GONK

#include "mozilla/layers/TextureClient.h"
#include "ISurfaceAllocator.h" // For IsSurfaceDescriptorValid
#include "mozilla/layers/FenceUtils.h" // for FenceHandle
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include <ui/GraphicBuffer.h>


namespace android {
class MediaBuffer;
};

namespace mozilla {
namespace layers {

/**
 * A TextureClient implementation based on android::GraphicBuffer (also referred to
 * as "gralloc").
 *
 * Gralloc lets us map texture data in memory (accessible through pointers)
 * and also use it directly as an OpenGL texture without the cost of texture
 * uploads.
 * Gralloc buffers can also be shared accros processes.
 *
 * More info about Gralloc here: https://wiki.mozilla.org/Platform/GFX/Gralloc
 *
 * This is only used in Firefox OS
 */
class GrallocTextureClientOGL : public BufferTextureClient
{
public:
  GrallocTextureClientOGL(GrallocBufferActor* aActor,
                          gfx::IntSize aSize,
                          gfx::BackendType aMoz2dBackend,
                          TextureFlags aFlags = TEXTURE_FLAGS_DEFAULT);
  GrallocTextureClientOGL(ISurfaceAllocator* aAllocator,
                          gfx::SurfaceFormat aFormat,
                          gfx::BackendType aMoz2dBackend,
                          TextureFlags aFlags = TEXTURE_FLAGS_DEFAULT);

  ~GrallocTextureClientOGL();

  virtual bool Lock(OpenMode aMode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool ImplementsLocking() const MOZ_OVERRIDE { return true; }

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return false; }

  virtual bool IsAllocated() const MOZ_OVERRIDE;

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  virtual bool UpdateSurface(gfxASurface* aSurface) MOZ_OVERRIDE;

  virtual TextureClientData* DropTextureData() MOZ_OVERRIDE;

  virtual void SetReleaseFenceHandle(FenceHandle aReleaseFenceHandle) MOZ_OVERRIDE;

  virtual void WaitReleaseFence() MOZ_OVERRIDE;

  void InitWith(GrallocBufferActor* aActor, gfx::IntSize aSize);

  void SetTextureFlags(TextureFlags aFlags) { AddFlags(aFlags); }

  gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  android::GraphicBuffer* GetGraphicBuffer()
  {
    return mGraphicBuffer.get();
  }

  android::PixelFormat GetPixelFormat()
  {
    return mGraphicBuffer->getPixelFormat();
  }

  virtual uint8_t* GetBuffer() const MOZ_OVERRIDE;

  virtual TemporaryRef<gfx::DrawTarget> GetAsDrawTarget() MOZ_OVERRIDE;

  virtual already_AddRefed<gfxASurface> GetAsSurface() MOZ_OVERRIDE;

  virtual bool AllocateForSurface(gfx::IntSize aSize,
                                  TextureAllocationFlags aFlags = ALLOC_DEFAULT) MOZ_OVERRIDE;

  virtual bool AllocateForYCbCr(gfx::IntSize aYSize,
                                gfx::IntSize aCbCrSize,
                                StereoMode aStereoMode) MOZ_OVERRIDE;

  bool AllocateForGLRendering(gfx::IntSize aSize);

  bool AllocateGralloc(gfx::IntSize aYSize, uint32_t aAndroidFormat, uint32_t aUsage);

  virtual bool Allocate(uint32_t aSize) MOZ_OVERRIDE;

  virtual size_t GetBufferSize() const MOZ_OVERRIDE;

  /**
   * Hold android::MediaBuffer.
   * MediaBuffer needs to be add refed to keep MediaBuffer alive
   * during TextureClient is in use.
   */
  void SetMediaBuffer(android::MediaBuffer* aMediaBuffer)
  {
    mMediaBuffer = aMediaBuffer;
  }

  android::MediaBuffer* GetMediaBuffer()
  {
    return mMediaBuffer;
  }

protected:
  /**
   * Unfortunately, until bug 879681 is fixed we need to use a GrallocBufferActor.
   */
  GrallocBufferActor* mGrallocActor;

  android::sp<android::GraphicBuffer> mGraphicBuffer;

  /**
   * Points to a mapped gralloc buffer between calls to lock and unlock.
   * Should be null outside of the lock-unlock pair.
   */
  uint8_t* mMappedBuffer;

  RefPtr<gfx::DrawTarget> mDrawTarget;

  /**
   * android::GraphicBuffer has a size information. But there are cases
   * that GraphicBuffer's size and actual video's size are different.
   * Extra size member is necessary. See Bug 850566.
   */
  gfx::IntSize mSize;

  android::MediaBuffer* mMediaBuffer;
};

} // namespace layers
} // namespace mozilla

#endif // MOZ_WIDGET_GONK
#endif
