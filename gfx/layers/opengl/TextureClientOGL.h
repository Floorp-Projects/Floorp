/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENTOGL_H
#define MOZILLA_GFX_TEXTURECLIENTOGL_H

#include "GLContextTypes.h"             // for SharedTextureHandle, etc
#include "gfxTypes.h"
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureClient.h"  // for TextureClient, etc
#include "nsSurfaceTexture.h"

namespace mozilla {
namespace gl {
class SurfaceStream;
}
}

namespace mozilla {
namespace layers {

class CompositableForwarder;

class EGLImageTextureClient : public TextureClient
{
public:
  EGLImageTextureClient(TextureFlags aFlags,
                        EGLImage aImage,
                        gfx::IntSize aSize,
                        bool aInverted);

  ~EGLImageTextureClient();

  virtual bool IsAllocated() const MOZ_OVERRIDE { return true; }

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return false; }

  virtual gfx::IntSize GetSize() const { return mSize; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  // Useless functions.
  virtual bool Lock(OpenMode mode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool IsLocked() const MOZ_OVERRIDE { return mIsLocked; }

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE
  {
    return gfx::SurfaceFormat::UNKNOWN;
  }

  virtual TemporaryRef<TextureClient>
  CreateSimilar(TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const MOZ_OVERRIDE
  {
    return nullptr;
  }

  virtual bool AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags) MOZ_OVERRIDE
  {
    return false;
  }

protected:
  const EGLImage mImage;
  const gfx::IntSize mSize;
  bool mIsLocked;
};

#ifdef MOZ_WIDGET_ANDROID

class SurfaceTextureClient : public TextureClient
{
public:
  SurfaceTextureClient(TextureFlags aFlags,
                       nsSurfaceTexture* aSurfTex,
                       gfx::IntSize aSize,
                       bool aInverted);

  ~SurfaceTextureClient();

  virtual bool IsAllocated() const MOZ_OVERRIDE { return true; }

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return false; }

  virtual gfx::IntSize GetSize() const { return mSize; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  // Useless functions.
  virtual bool Lock(OpenMode mode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool IsLocked() const MOZ_OVERRIDE { return mIsLocked; }

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE
  {
    return gfx::SurfaceFormat::UNKNOWN;
  }

  virtual TemporaryRef<TextureClient>
  CreateSimilar(TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const MOZ_OVERRIDE
  {
    return nullptr;
  }

  virtual bool AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags) MOZ_OVERRIDE
  {
    return false;
  }

protected:
  const nsRefPtr<nsSurfaceTexture> mSurfTex;
  const gfx::IntSize mSize;
  bool mIsLocked;
};

#endif // MOZ_WIDGET_ANDROID

} // namespace
} // namespace

#endif
