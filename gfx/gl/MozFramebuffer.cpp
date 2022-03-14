/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozFramebuffer.h"

#include "GLContext.h"
#include "mozilla/gfx/Logging.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace gl {

static void DeleteByTarget(GLContext* const gl, const GLenum target,
                           const GLuint name) {
  if (target == LOCAL_GL_RENDERBUFFER) {
    gl->DeleteRenderbuffer(name);
  } else {
    gl->DeleteTexture(name);
  }
}

UniquePtr<MozFramebuffer> MozFramebuffer::Create(GLContext* const gl,
                                                 const gfx::IntSize& size,
                                                 const uint32_t samples,
                                                 const bool depthStencil) {
  if (samples && !gl->IsSupported(GLFeature::framebuffer_multisample))
    return nullptr;

  if (uint32_t(size.width) > gl->MaxTexOrRbSize() ||
      uint32_t(size.height) > gl->MaxTexOrRbSize() ||
      samples > gl->MaxSamples()) {
    return nullptr;
  }

  gl->MakeCurrent();

  GLContext::LocalErrorScope errorScope(*gl);

  GLenum colorTarget;
  GLuint colorName;
  if (samples) {
    colorTarget = LOCAL_GL_RENDERBUFFER;
    colorName = gl->CreateRenderbuffer();
    const ScopedBindRenderbuffer bindRB(gl, colorName);
    gl->fRenderbufferStorageMultisample(colorTarget, samples, LOCAL_GL_RGBA8,
                                        size.width, size.height);
  } else {
    colorTarget = LOCAL_GL_TEXTURE_2D;
    colorName = gl->CreateTexture();
    const ScopedBindTexture bindTex(gl, colorName);
    gl->TexParams_SetClampNoMips();
    gl->fTexImage2D(colorTarget, 0, LOCAL_GL_RGBA, size.width, size.height, 0,
                    LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, nullptr);
  }

  const auto err = errorScope.GetError();
  if (err) {
    if (err != LOCAL_GL_OUT_OF_MEMORY) {
      gfxCriticalNote << "Unexpected error: " << gfx::hexa(err) << ": "
                      << GLContext::GLErrorToString(err);
    }
    DeleteByTarget(gl, colorTarget, colorName);
    return nullptr;
  }

  return CreateImpl(
      gl, size, samples,
      depthStencil ? DepthAndStencilBuffer::Create(gl, size, samples) : nullptr,
      colorTarget, colorName);
}

UniquePtr<MozFramebuffer> MozFramebuffer::CreateForBacking(
    GLContext* const gl, const gfx::IntSize& size, const uint32_t samples,
    bool depthStencil, const GLenum colorTarget, const GLuint colorName) {
  return CreateImpl(
      gl, size, samples,
      depthStencil ? DepthAndStencilBuffer::Create(gl, size, samples) : nullptr,
      colorTarget, colorName);
}

/* static */ UniquePtr<MozFramebuffer>
MozFramebuffer::CreateForBackingWithSharedDepthAndStencil(
    const gfx::IntSize& size, const uint32_t samples, GLenum colorTarget,
    GLuint colorName,
    const RefPtr<DepthAndStencilBuffer>& depthAndStencilBuffer) {
  auto gl = depthAndStencilBuffer->gl();
  if (!gl || !gl->MakeCurrent()) {
    return nullptr;
  }
  return CreateImpl(gl, size, samples, depthAndStencilBuffer, colorTarget,
                    colorName);
}

/* static */ UniquePtr<MozFramebuffer> MozFramebuffer::CreateImpl(
    GLContext* const gl, const gfx::IntSize& size, const uint32_t samples,
    const RefPtr<DepthAndStencilBuffer>& depthAndStencilBuffer,
    const GLenum colorTarget, const GLuint colorName) {
  GLuint fb = gl->CreateFramebuffer();
  const ScopedBindFramebuffer bindFB(gl, fb);

  if (colorTarget == LOCAL_GL_RENDERBUFFER) {
    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_COLOR_ATTACHMENT0, colorTarget,
                                 colorName);
  } else {
    gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                              colorTarget, colorName, 0);
  }

  if (depthAndStencilBuffer) {
    gl->fFramebufferRenderbuffer(
        LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT, LOCAL_GL_RENDERBUFFER,
        depthAndStencilBuffer->mDepthRB);
    gl->fFramebufferRenderbuffer(
        LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT,
        LOCAL_GL_RENDERBUFFER, depthAndStencilBuffer->mStencilRB);
  }

  const auto status = gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
  if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
    gfxCriticalNote << "MozFramebuffer::CreateImpl(size:" << size
                    << ", samples:" << samples
                    << ", depthAndStencil:" << bool(depthAndStencilBuffer)
                    << ", colorTarget:" << gfx::hexa(colorTarget)
                    << ", colorName:" << colorName << "): Incomplete: 0x"
                    << gfx::hexa(status);
    return nullptr;
  }

  return UniquePtr<MozFramebuffer>(new MozFramebuffer(
      gl, size, fb, samples, depthAndStencilBuffer, colorTarget, colorName));
}

/* static */ RefPtr<DepthAndStencilBuffer> DepthAndStencilBuffer::Create(
    GLContext* const gl, const gfx::IntSize& size, const uint32_t samples) {
  const auto fnAllocRB = [&](GLenum format) {
    GLuint rb = gl->CreateRenderbuffer();
    const ScopedBindRenderbuffer bindRB(gl, rb);
    if (samples) {
      gl->fRenderbufferStorageMultisample(LOCAL_GL_RENDERBUFFER, samples,
                                          format, size.width, size.height);
    } else {
      gl->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER, format, size.width,
                               size.height);
    }
    return rb;
  };

  GLuint depthRB, stencilRB;
  {
    GLContext::LocalErrorScope errorScope(*gl);

    if (gl->IsSupported(GLFeature::packed_depth_stencil)) {
      depthRB = fnAllocRB(LOCAL_GL_DEPTH24_STENCIL8);
      stencilRB = depthRB;  // Ignore unused mStencilRB.
    } else {
      depthRB = fnAllocRB(LOCAL_GL_DEPTH_COMPONENT24);
      stencilRB = fnAllocRB(LOCAL_GL_STENCIL_INDEX8);
    }

    const auto err = errorScope.GetError();
    if (err) {
      MOZ_ASSERT(err == LOCAL_GL_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  return new DepthAndStencilBuffer(gl, size, depthRB, stencilRB);
}

////////////////////

MozFramebuffer::MozFramebuffer(
    GLContext* const gl, const gfx::IntSize& size, GLuint fb,
    const uint32_t samples, RefPtr<DepthAndStencilBuffer> depthAndStencilBuffer,
    const GLenum colorTarget, const GLuint colorName)
    : mWeakGL(gl),
      mSize(size),
      mSamples(samples),
      mFB(fb),
      mColorTarget(colorTarget),
      mDepthAndStencilBuffer(std::move(depthAndStencilBuffer)),
      mColorName(colorName) {
  MOZ_ASSERT(mColorTarget);
  MOZ_ASSERT(mColorName);
}

MozFramebuffer::~MozFramebuffer() {
  GLContext* const gl = mWeakGL;
  if (!gl || !gl->MakeCurrent()) {
    return;
  }

  gl->DeleteFramebuffer(mFB);

  DeleteByTarget(gl, mColorTarget, mColorName);
}

bool MozFramebuffer::HasDepth() const {
  return mDepthAndStencilBuffer && mDepthAndStencilBuffer->mDepthRB;
}

bool MozFramebuffer::HasStencil() const {
  return mDepthAndStencilBuffer && mDepthAndStencilBuffer->mStencilRB;
}

DepthAndStencilBuffer::DepthAndStencilBuffer(GLContext* gl,
                                             const gfx::IntSize& size,
                                             GLuint depthRB, GLuint stencilRB)
    : mWeakGL(gl), mSize(size), mDepthRB(depthRB), mStencilRB(stencilRB) {}

DepthAndStencilBuffer::~DepthAndStencilBuffer() {
  GLContext* const gl = mWeakGL;
  if (!gl || !gl->MakeCurrent()) {
    return;
  }

  gl->DeleteRenderbuffer(mDepthRB);
  gl->DeleteRenderbuffer(mStencilRB);
}

}  // namespace gl
}  // namespace mozilla
