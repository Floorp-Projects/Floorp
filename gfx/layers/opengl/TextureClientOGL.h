/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENTOGL_H
#define MOZILLA_GFX_TEXTURECLIENTOGL_H

#include "mozilla/layers/TextureClient.h"
#include "ISurfaceAllocator.h" // For IsSurfaceDescriptorValid
#include "GLContext.h" // For SharedTextureHandle

#ifdef MOZ_WIDGET_GONK
#include <ui/GraphicBuffer.h>
#endif

namespace mozilla {
namespace layers {



/**
 * A TextureClient implementation to share TextureMemory that is already
 * on the GPU, for the OpenGL backend.
 */
class SharedTextureClientOGL : public TextureClient
{
public:
  SharedTextureClientOGL();

  ~SharedTextureClientOGL();

  virtual bool IsAllocated() const MOZ_OVERRIDE;

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  void InitWith(gl::SharedTextureHandle aHandle,
                gfx::IntSize aSize,
                bool aIsCrossProcess = false,
                bool aInverted = false);

  gfx::IntSize GetSize() const { return mSize; }

protected:
  gfx::IntSize mSize;
  gl::SharedTextureHandle mHandle;
  bool mIsCrossProcess;
  bool mInverted;
};


class DeprecatedTextureClientSharedOGL : public DeprecatedTextureClient
{
public:
  DeprecatedTextureClientSharedOGL(CompositableForwarder* aForwarder, const TextureInfo& aTextureInfo);
  ~DeprecatedTextureClientSharedOGL() { ReleaseResources(); }

  virtual bool SupportsType(DeprecatedTextureClientType aType) MOZ_OVERRIDE { return aType == TEXTURE_SHARED_GL; }
  virtual bool EnsureAllocated(gfx::IntSize aSize, gfxASurface::gfxContentType aType);
  virtual void ReleaseResources();
  virtual gfxASurface::gfxContentType GetContentType() MOZ_OVERRIDE { return gfxASurface::CONTENT_COLOR_ALPHA; }

protected:
  gl::GLContext* mGL;
  gfx::IntSize mSize;

  friend class CompositingFactory;
};

// Doesn't own the surface descriptor, so we shouldn't delete it
class DeprecatedTextureClientSharedOGLExternal : public DeprecatedTextureClientSharedOGL
{
public:
  DeprecatedTextureClientSharedOGLExternal(CompositableForwarder* aForwarder, const TextureInfo& aTextureInfo)
    : DeprecatedTextureClientSharedOGL(aForwarder, aTextureInfo)
  {}

  virtual bool SupportsType(DeprecatedTextureClientType aType) MOZ_OVERRIDE { return aType == TEXTURE_SHARED_GL_EXTERNAL; }
  virtual void ReleaseResources() {}
};

class DeprecatedTextureClientStreamOGL : public DeprecatedTextureClient
{
public:
  DeprecatedTextureClientStreamOGL(CompositableForwarder* aForwarder, const TextureInfo& aTextureInfo)
    : DeprecatedTextureClient(aForwarder, aTextureInfo)
  {}
  ~DeprecatedTextureClientStreamOGL() { ReleaseResources(); }

  virtual bool SupportsType(DeprecatedTextureClientType aType) MOZ_OVERRIDE { return aType == TEXTURE_STREAM_GL; }
  virtual bool EnsureAllocated(gfx::IntSize aSize, gfxASurface::gfxContentType aType) { return true; }
  virtual void ReleaseResources() { mDescriptor = SurfaceDescriptor(); }
  virtual gfxASurface::gfxContentType GetContentType() MOZ_OVERRIDE { return gfxASurface::CONTENT_COLOR_ALPHA; }
};

} // namespace
} // namespace

#endif
