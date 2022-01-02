/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/UniquePtr.h"

#include "GLContext.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace gl {

/* ScopedGLState - Wraps glEnable/glDisable. **********************************/

// Use |newState = true| to enable, |false| to disable.
ScopedGLState::ScopedGLState(GLContext* aGL, GLenum aCapability, bool aNewState)
    : mGL(aGL), mCapability(aCapability) {
  mOldState = mGL->fIsEnabled(mCapability);

  // Early out if we're already in the right state.
  if (aNewState == mOldState) return;

  if (aNewState) {
    mGL->fEnable(mCapability);
  } else {
    mGL->fDisable(mCapability);
  }
}

ScopedGLState::ScopedGLState(GLContext* aGL, GLenum aCapability)
    : mGL(aGL), mCapability(aCapability) {
  mOldState = mGL->fIsEnabled(mCapability);
}

ScopedGLState::~ScopedGLState() {
  if (mOldState) {
    mGL->fEnable(mCapability);
  } else {
    mGL->fDisable(mCapability);
  }
}

/* ScopedBindFramebuffer - Saves and restores with GetUserBoundFB and
 * BindUserFB. */

void ScopedBindFramebuffer::Init() {
  if (mGL->IsSupported(GLFeature::split_framebuffer)) {
    mOldReadFB = mGL->GetReadFB();
    mOldDrawFB = mGL->GetDrawFB();
  } else {
    mOldReadFB = mOldDrawFB = mGL->GetFB();
  }
}

ScopedBindFramebuffer::ScopedBindFramebuffer(GLContext* aGL) : mGL(aGL) {
  Init();
}

ScopedBindFramebuffer::ScopedBindFramebuffer(GLContext* aGL, GLuint aNewFB)
    : mGL(aGL) {
  Init();
  mGL->BindFB(aNewFB);
}

ScopedBindFramebuffer::~ScopedBindFramebuffer() {
  if (mOldReadFB == mOldDrawFB) {
    mGL->BindFB(mOldDrawFB);
  } else {
    mGL->BindDrawFB(mOldDrawFB);
    mGL->BindReadFB(mOldReadFB);
  }
}

/* ScopedBindTextureUnit ******************************************************/

ScopedBindTextureUnit::ScopedBindTextureUnit(GLContext* aGL, GLenum aTexUnit)
    : mGL(aGL), mOldTexUnit(0) {
  MOZ_ASSERT(aTexUnit >= LOCAL_GL_TEXTURE0);
  mGL->GetUIntegerv(LOCAL_GL_ACTIVE_TEXTURE, &mOldTexUnit);
  mGL->fActiveTexture(aTexUnit);
}

ScopedBindTextureUnit::~ScopedBindTextureUnit() {
  mGL->fActiveTexture(mOldTexUnit);
}

/* ScopedTexture **************************************************************/

ScopedTexture::ScopedTexture(GLContext* aGL) : mGL(aGL), mTexture(0) {
  mGL->fGenTextures(1, &mTexture);
}

ScopedTexture::~ScopedTexture() { mGL->fDeleteTextures(1, &mTexture); }

/* ScopedFramebuffer
 * **************************************************************/

ScopedFramebuffer::ScopedFramebuffer(GLContext* aGL) : mGL(aGL), mFB(0) {
  mGL->fGenFramebuffers(1, &mFB);
}

ScopedFramebuffer::~ScopedFramebuffer() { mGL->fDeleteFramebuffers(1, &mFB); }

/* ScopedRenderbuffer
 * **************************************************************/

ScopedRenderbuffer::ScopedRenderbuffer(GLContext* aGL) : mGL(aGL), mRB(0) {
  mGL->fGenRenderbuffers(1, &mRB);
}

ScopedRenderbuffer::~ScopedRenderbuffer() {
  mGL->fDeleteRenderbuffers(1, &mRB);
}

/* ScopedBindTexture **********************************************************/

static GLuint GetBoundTexture(GLContext* gl, GLenum texTarget) {
  GLenum bindingTarget;
  switch (texTarget) {
    case LOCAL_GL_TEXTURE_2D:
      bindingTarget = LOCAL_GL_TEXTURE_BINDING_2D;
      break;

    case LOCAL_GL_TEXTURE_CUBE_MAP:
      bindingTarget = LOCAL_GL_TEXTURE_BINDING_CUBE_MAP;
      break;

    case LOCAL_GL_TEXTURE_3D:
      bindingTarget = LOCAL_GL_TEXTURE_BINDING_3D;
      break;

    case LOCAL_GL_TEXTURE_2D_ARRAY:
      bindingTarget = LOCAL_GL_TEXTURE_BINDING_2D_ARRAY;
      break;

    case LOCAL_GL_TEXTURE_RECTANGLE_ARB:
      bindingTarget = LOCAL_GL_TEXTURE_BINDING_RECTANGLE_ARB;
      break;

    case LOCAL_GL_TEXTURE_EXTERNAL:
      bindingTarget = LOCAL_GL_TEXTURE_BINDING_EXTERNAL;
      break;

    default:
      MOZ_CRASH("bad texTarget");
  }

  GLuint ret = 0;
  gl->GetUIntegerv(bindingTarget, &ret);
  return ret;
}

ScopedBindTexture::ScopedBindTexture(GLContext* aGL, GLuint aNewTex,
                                     GLenum aTarget)
    : mGL(aGL), mTarget(aTarget), mOldTex(GetBoundTexture(aGL, aTarget)) {
  mGL->fBindTexture(mTarget, aNewTex);
}

ScopedBindTexture::~ScopedBindTexture() { mGL->fBindTexture(mTarget, mOldTex); }

/* ScopedBindRenderbuffer *****************************************************/

void ScopedBindRenderbuffer::Init() {
  mOldRB = 0;
  mGL->GetUIntegerv(LOCAL_GL_RENDERBUFFER_BINDING, &mOldRB);
}

ScopedBindRenderbuffer::ScopedBindRenderbuffer(GLContext* aGL) : mGL(aGL) {
  Init();
}

ScopedBindRenderbuffer::ScopedBindRenderbuffer(GLContext* aGL, GLuint aNewRB)
    : mGL(aGL) {
  Init();
  mGL->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, aNewRB);
}

ScopedBindRenderbuffer::~ScopedBindRenderbuffer() {
  mGL->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, mOldRB);
}

/* ScopedFramebufferForTexture ************************************************/
ScopedFramebufferForTexture::ScopedFramebufferForTexture(GLContext* aGL,
                                                         GLuint aTexture,
                                                         GLenum aTarget)
    : mGL(aGL), mComplete(false), mFB(0) {
  mGL->fGenFramebuffers(1, &mFB);
  ScopedBindFramebuffer autoFB(aGL, mFB);
  mGL->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                             aTarget, aTexture, 0);

  GLenum status = mGL->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
  if (status == LOCAL_GL_FRAMEBUFFER_COMPLETE) {
    mComplete = true;
  } else {
    mGL->fDeleteFramebuffers(1, &mFB);
    mFB = 0;
  }
}

ScopedFramebufferForTexture::~ScopedFramebufferForTexture() {
  if (!mFB) return;

  mGL->fDeleteFramebuffers(1, &mFB);
  mFB = 0;
}

/* ScopedFramebufferForRenderbuffer *******************************************/

ScopedFramebufferForRenderbuffer::ScopedFramebufferForRenderbuffer(
    GLContext* aGL, GLuint aRB)
    : mGL(aGL), mComplete(false), mFB(0) {
  mGL->fGenFramebuffers(1, &mFB);
  ScopedBindFramebuffer autoFB(aGL, mFB);
  mGL->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                LOCAL_GL_COLOR_ATTACHMENT0,
                                LOCAL_GL_RENDERBUFFER, aRB);

  GLenum status = mGL->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
  if (status == LOCAL_GL_FRAMEBUFFER_COMPLETE) {
    mComplete = true;
  } else {
    mGL->fDeleteFramebuffers(1, &mFB);
    mFB = 0;
  }
}

ScopedFramebufferForRenderbuffer::~ScopedFramebufferForRenderbuffer() {
  if (!mFB) return;

  mGL->fDeleteFramebuffers(1, &mFB);
  mFB = 0;
}

/* ScopedViewportRect *********************************************************/

ScopedViewportRect::ScopedViewportRect(GLContext* aGL, GLint x, GLint y,
                                       GLsizei width, GLsizei height)
    : mGL(aGL) {
  mGL->fGetIntegerv(LOCAL_GL_VIEWPORT, mSavedViewportRect);
  mGL->fViewport(x, y, width, height);
}

ScopedViewportRect::~ScopedViewportRect() {
  mGL->fViewport(mSavedViewportRect[0], mSavedViewportRect[1],
                 mSavedViewportRect[2], mSavedViewportRect[3]);
}

/* ScopedScissorRect **********************************************************/

ScopedScissorRect::ScopedScissorRect(GLContext* aGL, GLint x, GLint y,
                                     GLsizei width, GLsizei height)
    : mGL(aGL) {
  mGL->fGetIntegerv(LOCAL_GL_SCISSOR_BOX, mSavedScissorRect);
  mGL->fScissor(x, y, width, height);
}

ScopedScissorRect::ScopedScissorRect(GLContext* aGL) : mGL(aGL) {
  mGL->fGetIntegerv(LOCAL_GL_SCISSOR_BOX, mSavedScissorRect);
}

ScopedScissorRect::~ScopedScissorRect() {
  mGL->fScissor(mSavedScissorRect[0], mSavedScissorRect[1],
                mSavedScissorRect[2], mSavedScissorRect[3]);
}

/* ScopedVertexAttribPointer **************************************************/

ScopedVertexAttribPointer::ScopedVertexAttribPointer(
    GLContext* aGL, GLuint index, GLint size, GLenum type,
    realGLboolean normalized, GLsizei stride, GLuint buffer,
    const GLvoid* pointer)
    : mGL(aGL),
      mAttribEnabled(0),
      mAttribSize(0),
      mAttribStride(0),
      mAttribType(0),
      mAttribNormalized(0),
      mAttribBufferBinding(0),
      mAttribPointer(nullptr),
      mBoundBuffer(0) {
  WrapImpl(index);
  mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, buffer);
  mGL->fVertexAttribPointer(index, size, type, normalized, stride, pointer);
  mGL->fEnableVertexAttribArray(index);
}

ScopedVertexAttribPointer::ScopedVertexAttribPointer(GLContext* aGL,
                                                     GLuint index)
    : mGL(aGL),
      mAttribEnabled(0),
      mAttribSize(0),
      mAttribStride(0),
      mAttribType(0),
      mAttribNormalized(0),
      mAttribBufferBinding(0),
      mAttribPointer(nullptr),
      mBoundBuffer(0) {
  WrapImpl(index);
}

void ScopedVertexAttribPointer::WrapImpl(GLuint index) {
  mAttribIndex = index;

  /*
   * mGL->fGetVertexAttribiv takes:
   *  VERTEX_ATTRIB_ARRAY_ENABLED
   *  VERTEX_ATTRIB_ARRAY_SIZE,
   *  VERTEX_ATTRIB_ARRAY_STRIDE,
   *  VERTEX_ATTRIB_ARRAY_TYPE,
   *  VERTEX_ATTRIB_ARRAY_NORMALIZED,
   *  VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
   *  CURRENT_VERTEX_ATTRIB
   *
   * CURRENT_VERTEX_ATTRIB is vertex shader state. \o/
   * Others appear to be vertex array state,
   * or alternatively in the internal vertex array state
   * for a buffer object.
   */

  mGL->fGetVertexAttribiv(mAttribIndex, LOCAL_GL_VERTEX_ATTRIB_ARRAY_ENABLED,
                          &mAttribEnabled);
  mGL->fGetVertexAttribiv(mAttribIndex, LOCAL_GL_VERTEX_ATTRIB_ARRAY_SIZE,
                          &mAttribSize);
  mGL->fGetVertexAttribiv(mAttribIndex, LOCAL_GL_VERTEX_ATTRIB_ARRAY_STRIDE,
                          &mAttribStride);
  mGL->fGetVertexAttribiv(mAttribIndex, LOCAL_GL_VERTEX_ATTRIB_ARRAY_TYPE,
                          &mAttribType);
  mGL->fGetVertexAttribiv(mAttribIndex, LOCAL_GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,
                          &mAttribNormalized);
  mGL->fGetVertexAttribiv(mAttribIndex,
                          LOCAL_GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
                          &mAttribBufferBinding);
  mGL->fGetVertexAttribPointerv(
      mAttribIndex, LOCAL_GL_VERTEX_ATTRIB_ARRAY_POINTER, &mAttribPointer);

  // Note that uniform values are program state, so we don't need to rebind
  // those.

  mGL->GetUIntegerv(LOCAL_GL_ARRAY_BUFFER_BINDING, &mBoundBuffer);
}

ScopedVertexAttribPointer::~ScopedVertexAttribPointer() {
  mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mAttribBufferBinding);
  mGL->fVertexAttribPointer(mAttribIndex, mAttribSize, mAttribType,
                            mAttribNormalized, mAttribStride, mAttribPointer);
  if (mAttribEnabled)
    mGL->fEnableVertexAttribArray(mAttribIndex);
  else
    mGL->fDisableVertexAttribArray(mAttribIndex);
  mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mBoundBuffer);
}

////////////////////////////////////////////////////////////////////////
// ScopedPackState

ScopedPackState::ScopedPackState(GLContext* gl)
    : mGL(gl),
      mAlignment(0),
      mPixelBuffer(0),
      mRowLength(0),
      mSkipPixels(0),
      mSkipRows(0) {
  mGL->fGetIntegerv(LOCAL_GL_PACK_ALIGNMENT, &mAlignment);

  if (mAlignment != 4) mGL->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, 4);

  if (!mGL->HasPBOState()) return;

  mGL->fGetIntegerv(LOCAL_GL_PIXEL_PACK_BUFFER_BINDING, (GLint*)&mPixelBuffer);
  mGL->fGetIntegerv(LOCAL_GL_PACK_ROW_LENGTH, &mRowLength);
  mGL->fGetIntegerv(LOCAL_GL_PACK_SKIP_PIXELS, &mSkipPixels);
  mGL->fGetIntegerv(LOCAL_GL_PACK_SKIP_ROWS, &mSkipRows);

  if (mPixelBuffer != 0) mGL->fBindBuffer(LOCAL_GL_PIXEL_PACK_BUFFER, 0);
  if (mRowLength != 0) mGL->fPixelStorei(LOCAL_GL_PACK_ROW_LENGTH, 0);
  if (mSkipPixels != 0) mGL->fPixelStorei(LOCAL_GL_PACK_SKIP_PIXELS, 0);
  if (mSkipRows != 0) mGL->fPixelStorei(LOCAL_GL_PACK_SKIP_ROWS, 0);
}

bool ScopedPackState::SetForWidthAndStrideRGBA(GLsizei aWidth,
                                               GLsizei aStride) {
  MOZ_ASSERT(aStride % 4 == 0, "RGBA data should always be 4-byte aligned");
  MOZ_ASSERT(aStride / 4 >= aWidth, "Stride too small");
  if (aStride / 4 == aWidth) {
    // No special handling needed.
    return true;
  }
  if (mGL->HasPBOState()) {
    // HasPBOState implies support for GL_PACK_ROW_LENGTH.
    mGL->fPixelStorei(LOCAL_GL_PACK_ROW_LENGTH, aStride / 4);
    return true;
  }
  return false;
}

ScopedPackState::~ScopedPackState() {
  mGL->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, mAlignment);

  if (!mGL->HasPBOState()) return;

  mGL->fBindBuffer(LOCAL_GL_PIXEL_PACK_BUFFER, mPixelBuffer);
  mGL->fPixelStorei(LOCAL_GL_PACK_ROW_LENGTH, mRowLength);
  mGL->fPixelStorei(LOCAL_GL_PACK_SKIP_PIXELS, mSkipPixels);
  mGL->fPixelStorei(LOCAL_GL_PACK_SKIP_ROWS, mSkipRows);
}

////////////////////////////////////////////////////////////////////////
// ResetUnpackState

ResetUnpackState::ResetUnpackState(GLContext* gl)
    : mGL(gl),
      mAlignment(0),
      mPBO(0),
      mRowLength(0),
      mImageHeight(0),
      mSkipPixels(0),
      mSkipRows(0),
      mSkipImages(0) {
  const auto fnReset = [&](GLenum pname, GLuint val, GLuint* const out_old) {
    mGL->GetUIntegerv(pname, out_old);
    if (*out_old != val) {
      mGL->fPixelStorei(pname, val);
    }
  };

  // Default is 4, but 1 is more useful.
  fnReset(LOCAL_GL_UNPACK_ALIGNMENT, 1, &mAlignment);

  if (!mGL->HasPBOState()) return;

  mGL->GetUIntegerv(LOCAL_GL_PIXEL_UNPACK_BUFFER_BINDING, &mPBO);
  if (mPBO != 0) mGL->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, 0);

  fnReset(LOCAL_GL_UNPACK_ROW_LENGTH, 0, &mRowLength);
  fnReset(LOCAL_GL_UNPACK_IMAGE_HEIGHT, 0, &mImageHeight);
  fnReset(LOCAL_GL_UNPACK_SKIP_PIXELS, 0, &mSkipPixels);
  fnReset(LOCAL_GL_UNPACK_SKIP_ROWS, 0, &mSkipRows);
  fnReset(LOCAL_GL_UNPACK_SKIP_IMAGES, 0, &mSkipImages);
}

ResetUnpackState::~ResetUnpackState() {
  mGL->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, mAlignment);

  if (!mGL->HasPBOState()) return;

  mGL->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, mPBO);

  mGL->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, mRowLength);
  mGL->fPixelStorei(LOCAL_GL_UNPACK_IMAGE_HEIGHT, mImageHeight);
  mGL->fPixelStorei(LOCAL_GL_UNPACK_SKIP_PIXELS, mSkipPixels);
  mGL->fPixelStorei(LOCAL_GL_UNPACK_SKIP_ROWS, mSkipRows);
  mGL->fPixelStorei(LOCAL_GL_UNPACK_SKIP_IMAGES, mSkipImages);
}

////////////////////////////////////////////////////////////////////////
// ScopedBindPBO

static GLuint GetPBOBinding(GLContext* gl, GLenum target) {
  if (!gl->HasPBOState()) return 0;

  GLenum targetBinding;
  switch (target) {
    case LOCAL_GL_PIXEL_PACK_BUFFER:
      targetBinding = LOCAL_GL_PIXEL_PACK_BUFFER_BINDING;
      break;

    case LOCAL_GL_PIXEL_UNPACK_BUFFER:
      targetBinding = LOCAL_GL_PIXEL_UNPACK_BUFFER_BINDING;
      break;

    default:
      MOZ_CRASH();
  }

  return gl->GetIntAs<GLuint>(targetBinding);
}

ScopedBindPBO::ScopedBindPBO(GLContext* gl, GLenum target)
    : mGL(gl), mTarget(target), mPBO(GetPBOBinding(mGL, mTarget)) {}

ScopedBindPBO::~ScopedBindPBO() {
  if (!mGL->HasPBOState()) return;

  mGL->fBindBuffer(mTarget, mPBO);
}

} /* namespace gl */
} /* namespace mozilla */
