/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERD3D11TEXTUREHOSTOGL_H
#define MOZILLA_GFX_RENDERD3D11TEXTUREHOSTOGL_H

#include "RenderTextureHostOGL.h"
#include "GLTypes.h"

struct ID3D11Texture2D;
struct IDXGIKeyedMutex;

namespace mozilla {

namespace wr {

class RenderDXGITextureHostOGL final : public RenderTextureHostOGL
{
public:
  explicit RenderDXGITextureHostOGL(WindowsHandle aHandle,
                                    gfx::SurfaceFormat aFormat,
                                    gfx::IntSize aSize);

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL) override;
  void Unlock() override;

  virtual gfx::IntSize GetSize(uint8_t aChannelIndex) const;
  virtual GLuint GetGLHandle(uint8_t aChannelIndex) const;

private:
  virtual ~RenderDXGITextureHostOGL();

  bool EnsureLockable();

  void DeleteTextureHandle();

  RefPtr<gl::GLContext> mGL;

  WindowsHandle mHandle;
  RefPtr<ID3D11Texture2D> mTexture;
  RefPtr<IDXGIKeyedMutex> mKeyedMutex;

  EGLSurface mSurface;
  EGLStreamKHR mStream;

  // We could use NV12 format for this texture. So, we might have 2 gl texture
  // handles for Y and CbCr data.
  GLuint mTextureHandle[2];

  gfx::SurfaceFormat mFormat;
  gfx::IntSize mSize;

  bool mLocked;
};

class RenderDXGIYCbCrTextureHostOGL final : public RenderTextureHostOGL
{
public:
  explicit RenderDXGIYCbCrTextureHostOGL(WindowsHandle (&aHandles)[3],
                                         gfx::IntSize aSize,
                                         gfx::IntSize aSizeCbCr);

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL) override;
  virtual void Unlock() override;

  virtual gfx::IntSize GetSize(uint8_t aChannelIndex) const;
  virtual GLuint GetGLHandle(uint8_t aChannelIndex) const;

private:
  virtual ~RenderDXGIYCbCrTextureHostOGL();

  bool EnsureLockable();

  void DeleteTextureHandle();

  RefPtr<gl::GLContext> mGL;

  WindowsHandle mHandles[3];
  RefPtr<ID3D11Texture2D> mTextures[3];
  RefPtr<IDXGIKeyedMutex> mKeyedMutexs[3];

  EGLSurface mSurfaces[3];
  EGLStreamKHR mStreams[3];

  // The gl handles for Y, Cb and Cr data.
  GLuint mTextureHandles[3];

  gfx::IntSize mSize;
  gfx::IntSize mSizeCbCr;

  bool mLocked;
};

} // namespace wr
} // namespace mozilla

#endif // MOZILLA_GFX_RENDERD3D11TEXTUREHOSTOGL_H
