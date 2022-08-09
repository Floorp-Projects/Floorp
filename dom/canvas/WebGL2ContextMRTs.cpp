/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLFramebuffer.h"

namespace mozilla {

bool WebGL2Context::ValidateClearBuffer(const GLenum buffer,
                                        const GLint drawBuffer,
                                        const webgl::AttribBaseType funcType) {
  GLint maxDrawBuffer;
  switch (buffer) {
    case LOCAL_GL_COLOR:
      maxDrawBuffer = Limits().maxColorDrawBuffers - 1;
      break;

    case LOCAL_GL_DEPTH:
    case LOCAL_GL_STENCIL:
      maxDrawBuffer = 0;
      break;

    case LOCAL_GL_DEPTH_STENCIL:
      maxDrawBuffer = 0;
      break;

    default:
      ErrorInvalidEnumInfo("buffer", buffer);
      return false;
  }

  if (drawBuffer < 0 || drawBuffer > maxDrawBuffer) {
    ErrorInvalidValue(
        "Invalid drawbuffer %d. This buffer only supports"
        " `drawbuffer` values between 0 and %u.",
        drawBuffer, maxDrawBuffer);
    return false;
  }

  ////

  if (!BindCurFBForDraw()) return false;

  const auto& fb = mBoundDrawFramebuffer;
  if (fb) {
    if (!fb->ValidateClearBufferType(buffer, drawBuffer, funcType))
      return false;
  } else if (buffer == LOCAL_GL_COLOR) {
    if (drawBuffer != 0) return true;

    if (mDefaultFB_DrawBuffer0 == LOCAL_GL_NONE) return true;

    if (funcType != webgl::AttribBaseType::Float) {
      ErrorInvalidOperation(
          "For default framebuffer, COLOR is always of type"
          " FLOAT.");
      return false;
    }
  }

  return true;
}

////

void WebGL2Context::ClearBufferTv(GLenum buffer, GLint drawBuffer,
                                  const webgl::TypedQuad& data) {
  const FuncScope funcScope(*this, "clearBufferu?[fi]v");
  if (IsContextLost()) return;

  switch (data.type) {
    case webgl::AttribBaseType::Boolean:
      MOZ_ASSERT(false);
      return;

    case webgl::AttribBaseType::Float:
      if (buffer != LOCAL_GL_COLOR && buffer != LOCAL_GL_DEPTH) {
        ErrorInvalidEnum("`buffer` must be COLOR or DEPTH.");
        return;
      }
      break;

    case webgl::AttribBaseType::Int:
      if (buffer != LOCAL_GL_COLOR && buffer != LOCAL_GL_STENCIL) {
        ErrorInvalidEnum("`buffer` must be COLOR or STENCIL.");
        return;
      }
      break;

    case webgl::AttribBaseType::Uint:
      if (buffer != LOCAL_GL_COLOR) {
        ErrorInvalidEnum("`buffer` must be COLOR.");
        return;
      }
      break;
  }

  if (!ValidateClearBuffer(buffer, drawBuffer, data.type)) {
    return;
  }

  if (!mBoundDrawFramebuffer && buffer == LOCAL_GL_DEPTH && mNeedsFakeNoDepth) {
    return;
  }
  if (!mBoundDrawFramebuffer && buffer == LOCAL_GL_STENCIL &&
      mNeedsFakeNoStencil) {
    return;
  }

  ScopedDrawCallWrapper wrapper(*this);
  switch (data.type) {
    case webgl::AttribBaseType::Boolean:
      MOZ_ASSERT(false);
      return;

    case webgl::AttribBaseType::Float:
      gl->fClearBufferfv(buffer, drawBuffer,
                         reinterpret_cast<const float*>(data.data.data()));
      break;

    case webgl::AttribBaseType::Int:
      gl->fClearBufferiv(buffer, drawBuffer,
                         reinterpret_cast<const int32_t*>(data.data.data()));
      break;

    case webgl::AttribBaseType::Uint:
      gl->fClearBufferuiv(buffer, drawBuffer,
                          reinterpret_cast<const uint32_t*>(data.data.data()));
      break;
  }
}

////

void WebGL2Context::ClearBufferfi(GLenum buffer, GLint drawBuffer,
                                  GLfloat depth, GLint stencil) {
  const FuncScope funcScope(*this, "clearBufferfi");
  if (IsContextLost()) return;

  if (buffer != LOCAL_GL_DEPTH_STENCIL)
    return ErrorInvalidEnum("`buffer` must be DEPTH_STENCIL.");

  const auto ignored = webgl::AttribBaseType::Float;
  if (!ValidateClearBuffer(buffer, drawBuffer, ignored)) return;

  auto driverDepth = depth;
  auto driverStencil = stencil;
  if (!mBoundDrawFramebuffer) {
    if (mNeedsFakeNoDepth) {
      driverDepth = 1.0f;
    } else if (mNeedsFakeNoStencil) {
      driverStencil = 0;
    }
  }

  ScopedDrawCallWrapper wrapper(*this);
  gl->fClearBufferfi(buffer, drawBuffer, driverDepth, driverStencil);
}

}  // namespace mozilla
