/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderD3D11TextureHostOGL.h"

namespace mozilla {
namespace wr {

RenderDXGITextureHostOGL::RenderDXGITextureHostOGL(WindowsHandle aHandle,
                                                   gfx::SurfaceFormat aFormat,
                                                   gfx::IntSize aSize)
  : mTextureHandle{ 0, 0 }
  , mFormat(aFormat)
  , mSize(aSize)
{
  MOZ_COUNT_CTOR_INHERITED(RenderDXGITextureHostOGL, RenderTextureHostOGL);
}

RenderDXGITextureHostOGL::~RenderDXGITextureHostOGL()
{
  MOZ_COUNT_DTOR_INHERITED(RenderDXGITextureHostOGL, RenderTextureHostOGL);
}

void
RenderDXGITextureHostOGL::SetGLContext(gl::GLContext* aContext)
{
  MOZ_ASSERT_UNREACHABLE("No implementation for RenderDXGITextureHostOGL.");
}

bool
RenderDXGITextureHostOGL::Lock()
{
  // TODO: Convert the d3d shared-handle to gl handle.
  MOZ_ASSERT_UNREACHABLE("No implementation for RenderDXGITextureHostOGL.");
  return true;
}

void
RenderDXGITextureHostOGL::Unlock()
{
  MOZ_ASSERT_UNREACHABLE("No implementation for RenderDXGITextureHostOGL.");
}

GLuint
RenderDXGITextureHostOGL::GetGLHandle(uint8_t aChannelIndex) const
{
  MOZ_ASSERT(mFormat != gfx::SurfaceFormat::NV12 || aChannelIndex < 2);
  MOZ_ASSERT(mFormat == gfx::SurfaceFormat::NV12 || aChannelIndex < 1);

  // TODO: return the corresponding gl handle for channel index.
  MOZ_ASSERT_UNREACHABLE("No implementation for RenderDXGITextureHostOGL.");
  return 0;
}

gfx::IntSize
RenderDXGITextureHostOGL::GetSize(uint8_t aChannelIndex) const
{
  MOZ_ASSERT(mFormat != gfx::SurfaceFormat::NV12 || aChannelIndex < 2);
  MOZ_ASSERT(mFormat == gfx::SurfaceFormat::NV12 || aChannelIndex < 1);

  if (aChannelIndex == 0) {
    return mSize;
  } else {
    // The CbCr channel size is a half of Y channel size in NV12 format.
    return mSize / 2;
  }
}

} // namespace wr
} // namespace mozilla
