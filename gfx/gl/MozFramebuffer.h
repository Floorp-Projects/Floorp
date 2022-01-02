/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_FRAMEBUFFER_H_
#define MOZ_FRAMEBUFFER_H_

#include "gfx2DGlue.h"
#include "GLConsts.h"
#include "GLContextTypes.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"

namespace mozilla {
namespace gl {

class DepthAndStencilBuffer final : public SupportsWeakPtr {
  const WeakPtr<GLContext> mWeakGL;
  const gfx::IntSize mSize;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DepthAndStencilBuffer)

  const GLuint mDepthRB;
  const GLuint mStencilRB;

  static RefPtr<DepthAndStencilBuffer> Create(GLContext* const gl,
                                              const gfx::IntSize& size,
                                              const uint32_t samples);

  RefPtr<GLContext> gl() const { return mWeakGL.get(); }

  // 4 bytes per pixel (24-bit depth + 8-bit stencil).
  uint64_t EstimateMemory() const {
    return static_cast<uint64_t>(mSize.width) * 4 * mSize.height;
  }

 protected:
  DepthAndStencilBuffer(GLContext* gl, const gfx::IntSize& size, GLuint depthRB,
                        GLuint stencilRB);
  ~DepthAndStencilBuffer();
};

class MozFramebuffer final {
  const WeakPtr<GLContext> mWeakGL;

 public:
  const gfx::IntSize mSize;
  const uint32_t mSamples;
  const GLuint mFB;
  const GLenum mColorTarget;

 private:
  const RefPtr<DepthAndStencilBuffer> mDepthAndStencilBuffer;
  const GLuint mColorName;

 public:
  // Create a new framebuffer with the specified properties.
  static UniquePtr<MozFramebuffer> Create(GLContext* gl,
                                          const gfx::IntSize& size,
                                          uint32_t samples, bool depthStencil);

  // Create a new framebuffer backed by an existing texture or buffer.
  // Assumes that gl is the current context.
  static UniquePtr<MozFramebuffer> CreateForBacking(
      GLContext* gl, const gfx::IntSize& size, uint32_t samples,
      bool depthStencil, GLenum colorTarget, GLuint colorName);

  // Create a new framebuffer backed by an existing texture or buffer.
  // Use the same GLContext, size, and samples as framebufferToShareWith.
  // The new framebuffer will share its depth and stencil buffer with
  // framebufferToShareWith. The depth and stencil buffers will be destroyed
  // once the last MozFramebuffer using them is destroyed.
  static UniquePtr<MozFramebuffer> CreateForBackingWithSharedDepthAndStencil(
      const gfx::IntSize& size, const uint32_t samples, GLenum colorTarget,
      GLuint colorName,
      const RefPtr<DepthAndStencilBuffer>& depthAndStencilBuffer);

 private:
  MozFramebuffer(GLContext* gl, const gfx::IntSize& size, GLuint fb,
                 uint32_t samples,
                 RefPtr<DepthAndStencilBuffer> depthAndStencilBuffer,
                 GLenum colorTarget, GLuint colorName);

  // gl must be the current context when this is called.
  static UniquePtr<MozFramebuffer> CreateImpl(
      GLContext* const gl, const gfx::IntSize& size, const uint32_t samples,
      const RefPtr<DepthAndStencilBuffer>& depthAndStencilBuffer,
      const GLenum colorTarget, const GLuint colorName);

 public:
  ~MozFramebuffer();

  GLuint ColorTex() const {
    if (mColorTarget == LOCAL_GL_RENDERBUFFER) return 0;
    return mColorName;
  }
  const auto& GetDepthAndStencilBuffer() const {
    return mDepthAndStencilBuffer;
  }
  bool HasDepth() const;
  bool HasStencil() const;
};

}  // namespace gl
}  // namespace mozilla

#endif  // MOZ_FRAMEBUFFER_H_
