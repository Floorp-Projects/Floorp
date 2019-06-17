/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SCOPEDGLHELPERS_H_
#define SCOPEDGLHELPERS_H_

#include "GLDefs.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace gl {

class GLContext;

#ifdef DEBUG
bool IsContextCurrent(GLContext* gl);
#endif

// Wraps glEnable/Disable.
struct ScopedGLState final {
 private:
  GLContext* const mGL;
  const GLenum mCapability;
  bool mOldState;

 public:
  // Use |newState = true| to enable, |false| to disable.
  ScopedGLState(GLContext* aGL, GLenum aCapability, bool aNewState);
  // variant that doesn't change state; simply records existing state to be
  // restored by the destructor
  ScopedGLState(GLContext* aGL, GLenum aCapability);
  ~ScopedGLState();
};

// Saves and restores with GetUserBoundFB and BindUserFB.
struct ScopedBindFramebuffer final {
 private:
  GLContext* const mGL;
  GLuint mOldReadFB;
  GLuint mOldDrawFB;

  void Init();

 public:
  explicit ScopedBindFramebuffer(GLContext* aGL);
  ScopedBindFramebuffer(GLContext* aGL, GLuint aNewFB);
  ~ScopedBindFramebuffer();
};

struct ScopedBindTextureUnit final {
 private:
  GLContext* const mGL;
  GLenum mOldTexUnit;

 public:
  ScopedBindTextureUnit(GLContext* aGL, GLenum aTexUnit);
  ~ScopedBindTextureUnit();
};

struct ScopedTexture final {
 private:
  GLContext* const mGL;
  GLuint mTexture;

 public:
  explicit ScopedTexture(GLContext* aGL);
  ~ScopedTexture();

  GLuint Texture() const { return mTexture; }
  operator GLuint() const { return mTexture; }
};

struct ScopedFramebuffer final {
 private:
  GLContext* const mGL;
  GLuint mFB;

 public:
  explicit ScopedFramebuffer(GLContext* aGL);
  ~ScopedFramebuffer();

  const auto& FB() const { return mFB; }
  operator GLuint() const { return mFB; }
};

struct ScopedRenderbuffer final {
 private:
  GLContext* const mGL;
  GLuint mRB;

 public:
  explicit ScopedRenderbuffer(GLContext* aGL);
  ~ScopedRenderbuffer();

  GLuint RB() { return mRB; }
  operator GLuint() const { return mRB; }
};

struct ScopedBindTexture final {
 private:
  GLContext* const mGL;
  const GLenum mTarget;
  const GLuint mOldTex;

 public:
  ScopedBindTexture(GLContext* aGL, GLuint aNewTex,
                    GLenum aTarget = LOCAL_GL_TEXTURE_2D);
  ~ScopedBindTexture();
};

struct ScopedBindRenderbuffer final {
 private:
  GLContext* const mGL;
  GLuint mOldRB;

 private:
  void Init();

 public:
  explicit ScopedBindRenderbuffer(GLContext* aGL);
  ScopedBindRenderbuffer(GLContext* aGL, GLuint aNewRB);
  ~ScopedBindRenderbuffer();
};

struct ScopedFramebufferForTexture final {
 private:
  GLContext* const mGL;
  bool mComplete;  // True if the framebuffer we create is complete.
  GLuint mFB;

 public:
  ScopedFramebufferForTexture(GLContext* aGL, GLuint aTexture,
                              GLenum aTarget = LOCAL_GL_TEXTURE_2D);
  ~ScopedFramebufferForTexture();

  bool IsComplete() const { return mComplete; }

  GLuint FB() const {
    MOZ_GL_ASSERT(mGL, IsComplete());
    return mFB;
  }
};

struct ScopedFramebufferForRenderbuffer final {
 private:
  GLContext* const mGL;
  bool mComplete;  // True if the framebuffer we create is complete.
  GLuint mFB;

 public:
  ScopedFramebufferForRenderbuffer(GLContext* aGL, GLuint aRB);
  ~ScopedFramebufferForRenderbuffer();

  bool IsComplete() const { return mComplete; }
  GLuint FB() const {
    MOZ_GL_ASSERT(mGL, IsComplete());
    return mFB;
  }
};

struct ScopedViewportRect final {
 private:
  GLContext* const mGL;
  GLint mSavedViewportRect[4];

 public:
  ScopedViewportRect(GLContext* aGL, GLint x, GLint y, GLsizei width,
                     GLsizei height);
  ~ScopedViewportRect();
};

struct ScopedScissorRect final {
 private:
  GLContext* const mGL;
  GLint mSavedScissorRect[4];

 public:
  ScopedScissorRect(GLContext* aGL, GLint x, GLint y, GLsizei width,
                    GLsizei height);
  explicit ScopedScissorRect(GLContext* aGL);
  ~ScopedScissorRect();
};

struct ScopedVertexAttribPointer final {
 private:
  GLContext* const mGL;
  GLuint mAttribIndex;
  GLint mAttribEnabled;
  GLint mAttribSize;
  GLint mAttribStride;
  GLint mAttribType;
  GLint mAttribNormalized;
  GLint mAttribBufferBinding;
  void* mAttribPointer;
  GLuint mBoundBuffer;

 public:
  ScopedVertexAttribPointer(GLContext* aGL, GLuint index, GLint size,
                            GLenum type, realGLboolean normalized,
                            GLsizei stride, GLuint buffer,
                            const GLvoid* pointer);
  explicit ScopedVertexAttribPointer(GLContext* aGL, GLuint index);
  ~ScopedVertexAttribPointer();

 private:
  void WrapImpl(GLuint index);
};

struct ScopedPackState final {
 private:
  GLContext* const mGL;
  GLint mAlignment;

  GLuint mPixelBuffer;
  GLint mRowLength;
  GLint mSkipPixels;
  GLint mSkipRows;

 public:
  explicit ScopedPackState(GLContext* gl);
  ~ScopedPackState();

  // Returns whether the stride was handled successfully.
  bool SetForWidthAndStrideRGBA(GLsizei aWidth, GLsizei aStride);
};

struct ResetUnpackState final {
 private:
  GLContext* const mGL;
  GLuint mAlignment;

  GLuint mPBO;
  GLuint mRowLength;
  GLuint mImageHeight;
  GLuint mSkipPixels;
  GLuint mSkipRows;
  GLuint mSkipImages;

 public:
  explicit ResetUnpackState(GLContext* gl);
  ~ResetUnpackState();
};

struct ScopedBindPBO final {
 private:
  GLContext* const mGL;
  const GLenum mTarget;
  const GLuint mPBO;

 public:
  ScopedBindPBO(GLContext* gl, GLenum target);
  ~ScopedBindPBO();
};

} /* namespace gl */
} /* namespace mozilla */

#endif /* SCOPEDGLHELPERS_H_ */
