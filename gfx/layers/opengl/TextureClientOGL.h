/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENTOGL_H
#define MOZILLA_GFX_TEXTURECLIENTOGL_H

#include "mozilla/layers/TextureClient.h"
#include "ISurfaceAllocator.h" // For IsSurfaceDescriptorValid

namespace mozilla {
namespace layers {

class TextureClientSharedOGL : public TextureClient
{
public:
  TextureClientSharedOGL(CompositableForwarder* aForwarder, CompositableType aCompositableType);
  ~TextureClientSharedOGL() { ReleaseResources(); }

  virtual bool SupportsType(TextureClientType aType) MOZ_OVERRIDE { return aType == TEXTURE_SHARED_GL; }
  virtual void EnsureAllocated(gfx::IntSize aSize, gfxASurface::gfxContentType aType);
  virtual void ReleaseResources();
  virtual gfxASurface::gfxContentType GetContentType() MOZ_OVERRIDE { return gfxASurface::CONTENT_COLOR_ALPHA; }

protected:
  gl::GLContext* mGL;
  gfx::IntSize mSize;

  friend class CompositingFactory;
};

// Doesn't own the surface descriptor, so we shouldn't delete it
class TextureClientSharedOGLExternal : public TextureClientSharedOGL
{
public:
  TextureClientSharedOGLExternal(CompositableForwarder* aForwarder, CompositableType aCompositableType)
    : TextureClientSharedOGL(aForwarder, aCompositableType)
  {}

  virtual bool SupportsType(TextureClientType aType) MOZ_OVERRIDE { return aType == TEXTURE_SHARED_GL_EXTERNAL; }
  virtual void ReleaseResources() {}
};

class TextureClientStreamOGL : public TextureClient
{
public:
  TextureClientStreamOGL(CompositableForwarder* aForwarder, CompositableType aCompositableType)
    : TextureClient(aForwarder, aCompositableType)
  {}
  ~TextureClientStreamOGL() { ReleaseResources(); }

  virtual bool SupportsType(TextureClientType aType) MOZ_OVERRIDE { return aType == TEXTURE_STREAM_GL; }
  virtual void EnsureAllocated(gfx::IntSize aSize, gfxASurface::gfxContentType aType) { }
  virtual void ReleaseResources() { mDescriptor = SurfaceDescriptor(); }
  virtual gfxASurface::gfxContentType GetContentType() MOZ_OVERRIDE { return gfxASurface::CONTENT_COLOR_ALPHA; }
};

} // namespace
} // namespace

#endif
