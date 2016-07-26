/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLFramebuffer.h"

namespace mozilla {

bool WebGL2Context::ValidateClearBuffer(const char* info, GLenum buffer, GLint drawbuffer, size_t elemCount)
{
    size_t requiredElements = -1;
    GLint maxDrawbuffer = -1;
    switch (buffer) {
      case LOCAL_GL_COLOR:
      case LOCAL_GL_FRONT:
      case LOCAL_GL_BACK:
      case LOCAL_GL_LEFT:
      case LOCAL_GL_RIGHT:
      case LOCAL_GL_FRONT_AND_BACK:
          requiredElements = 4;
          maxDrawbuffer = mGLMaxDrawBuffers - 1;
          break;

      case LOCAL_GL_DEPTH:
      case LOCAL_GL_STENCIL:
          requiredElements = 1;
          maxDrawbuffer = 0;
          break;

      default:
          ErrorInvalidEnumInfo(info, buffer);
          return false;
    }

    if (drawbuffer < 0 || drawbuffer > maxDrawbuffer) {
        ErrorInvalidValue("%s: invalid drawbuffer %d. This buffer only supports drawbuffer values between 0 and %d",
                          info, drawbuffer, maxDrawbuffer);
        return false;
    }

    if (elemCount < requiredElements) {
        ErrorInvalidValue("%s: Not enough elements. Require %u. Given %u.",
                          info, requiredElements, elemCount);
        return false;
    }
    return true;
}

// Common base functionality. These a good candidates to be moved into WebGLContextUnchecked.cpp
void
WebGL2Context::ClearBufferiv_base(GLenum buffer, GLint drawbuffer, const GLint* value)
{
    const char funcName[] = "clearBufferiv";

    MakeContextCurrent();
    if (mBoundDrawFramebuffer) {
        if (!mBoundDrawFramebuffer->ValidateAndInitAttachments(funcName))
            return;
    }

    gl->fClearBufferiv(buffer, drawbuffer, value);
}

void
WebGL2Context::ClearBufferuiv_base(GLenum buffer, GLint drawbuffer, const GLuint* value)
{
    const char funcName[] = "clearBufferuiv";

    MakeContextCurrent();
    if (mBoundDrawFramebuffer) {
        if (!mBoundDrawFramebuffer->ValidateAndInitAttachments(funcName))
            return;
    }

    gl->fClearBufferuiv(buffer, drawbuffer, value);
}

void
WebGL2Context::ClearBufferfv_base(GLenum buffer, GLint drawbuffer, const GLfloat* value)
{
    const char funcName[] = "clearBufferfv";

    MakeContextCurrent();
    if (mBoundDrawFramebuffer) {
        if (!mBoundDrawFramebuffer->ValidateAndInitAttachments(funcName))
            return;
    }

    gl->fClearBufferfv(buffer, drawbuffer, value);
}

void
WebGL2Context::ClearBufferiv(GLenum buffer, GLint drawbuffer, const dom::Int32Array& value)
{
    if (IsContextLost()) {
        return;
    }

    value.ComputeLengthAndData();
    if (!ValidateClearBuffer("clearBufferiv", buffer, drawbuffer, value.Length())) {
        return;
    }

    ClearBufferiv_base(buffer, drawbuffer, value.Data());
}

void
WebGL2Context::ClearBufferiv(GLenum buffer, GLint drawbuffer, const dom::Sequence<GLint>& value)
{
    if (IsContextLost()) {
        return;
    }

    if (!ValidateClearBuffer("clearBufferiv", buffer, drawbuffer, value.Length())) {
        return;
    }

    ClearBufferiv_base(buffer, drawbuffer, value.Elements());
}

void
WebGL2Context::ClearBufferuiv(GLenum buffer, GLint drawbuffer, const dom::Uint32Array& value)
{
    if (IsContextLost()) {
        return;
    }

    value.ComputeLengthAndData();
    if (!ValidateClearBuffer("clearBufferuiv", buffer, drawbuffer, value.Length())) {
        return;
    }

    ClearBufferuiv_base(buffer, drawbuffer, value.Data());
}

void
WebGL2Context::ClearBufferuiv(GLenum buffer, GLint drawbuffer, const dom::Sequence<GLuint>& value)
{
    if (IsContextLost()) {
        return;
    }

    if (!ValidateClearBuffer("clearBufferuiv", buffer, drawbuffer, value.Length())) {
        return;
    }

    ClearBufferuiv_base(buffer, drawbuffer, value.Elements());
}

void
WebGL2Context::ClearBufferfv(GLenum buffer, GLint drawbuffer, const dom::Float32Array& value)
{
    if (IsContextLost()) {
        return;
    }

    value.ComputeLengthAndData();
    if (!ValidateClearBuffer("clearBufferfv", buffer, drawbuffer, value.Length())) {
        return;
    }

    ClearBufferfv_base(buffer, drawbuffer, value.Data());
}

void
WebGL2Context::ClearBufferfv(GLenum buffer, GLint drawbuffer, const dom::Sequence<GLfloat>& value)
{
    if (IsContextLost()) {
        return;
    }

    if (!ValidateClearBuffer("clearBufferfv", buffer, drawbuffer, value.Length())) {
        return;
    }

    ClearBufferfv_base(buffer, drawbuffer, value.Elements());
}

void
WebGL2Context::ClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    if (IsContextLost()) {
        return;
    }

    if (buffer != LOCAL_GL_DEPTH_STENCIL) {
        return ErrorInvalidEnumInfo("clearBufferfi: buffer", buffer);
    }
    MakeContextCurrent();
    gl->fClearBufferfi(buffer, drawbuffer, depth, stencil);
}

} // namespace mozilla
