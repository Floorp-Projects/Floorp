/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GRALLOCTEXTURECLIENT_H
#define MOZILLA_GFX_GRALLOCTEXTURECLIENT_H
#ifdef MOZ_WIDGET_GONK

#include "mozilla/layers/TextureClient.h"
#include "ISurfaceAllocator.h" // For IsSurfaceDescriptorValid
#include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#include <ui/GraphicBuffer.h>

namespace mozilla {
namespace layers {

class GraphicBufferLocked;

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
                          TextureFlags aFlags = TEXTURE_FLAGS_DEFAULT);
  GrallocTextureClientOGL(CompositableClient* aCompositable,
                          gfx::SurfaceFormat aFormat,
                          TextureFlags aFlags = TEXTURE_FLAGS_DEFAULT);
  GrallocTextureClientOGL(ISurfaceAllocator* aAllocator,
                          gfx::SurfaceFormat aFormat,
                          TextureFlags aFlags = TEXTURE_FLAGS_DEFAULT);

  ~GrallocTextureClientOGL();

  virtual bool Lock(OpenMode aMode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool ImplementsLocking() const MOZ_OVERRIDE { return true; }

  virtual bool IsAllocated() const MOZ_OVERRIDE;

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  virtual TextureClientData* DropTextureData() MOZ_OVERRIDE;

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

  /**
   * These flags are important for performances because they'll let the driver
   * optimize for the right usage.
   * Be sure to specify them before calling Lock.
   */
  void SetGrallocOpenFlags(uint32_t aFlags)
  {
    mGrallocFlags = aFlags;
  }

  virtual uint8_t* GetBuffer() const MOZ_OVERRIDE;

  virtual bool AllocateForSurface(gfx::IntSize aSize,
                                  TextureAllocationFlags aFlags = ALLOC_DEFAULT) MOZ_OVERRIDE;

  virtual bool AllocateForYCbCr(gfx::IntSize aYSize,
                                gfx::IntSize aCbCrSize,
                                StereoMode aStereoMode) MOZ_OVERRIDE;

  bool AllocateForGLRendering(gfx::IntSize aSize);

  bool AllocateGralloc(gfx::IntSize aYSize, uint32_t aAndroidFormat, uint32_t aUsage);

  virtual bool Allocate(uint32_t aSize) MOZ_OVERRIDE;

  virtual size_t GetBufferSize() const MOZ_OVERRIDE;

  void SetGraphicBufferLocked(GraphicBufferLocked* aBufferLocked);

protected:
  ISurfaceAllocator* GetAllocator();

  /**
   * Unfortunately, until bug 879681 is fixed we need to use a GrallocBufferActor.
   */
  GrallocBufferActor* mGrallocActor;

  RefPtr<GraphicBufferLocked> mBufferLocked;

  android::sp<android::GraphicBuffer> mGraphicBuffer;

  RefPtr<ISurfaceAllocator> mAllocator;

  /**
   * Flags that are used when locking the gralloc buffer
   */
  uint32_t mGrallocFlags;
  /**
   * Points to a mapped gralloc buffer between calls to lock and unlock.
   * Should be null outside of the lock-unlock pair.
   */
  uint8_t* mMappedBuffer;
  /**
   * android::GraphicBuffer has a size information. But there are cases
   * that GraphicBuffer's size and actual video's size are different.
   * Extra size member is necessary. See Bug 850566.
   */
  gfx::IntSize mSize;
};

} // namespace layers
} // namespace mozilla

#endif // MOZ_WIDGET_GONK
#endif
