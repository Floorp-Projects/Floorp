/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLFramebuffer.h"

namespace mozilla {

bool
WebGL2Context::ValidateClearBuffer(const char* funcName, GLenum buffer, GLint drawBuffer,
                                   size_t availElemCount, GLuint elemOffset)
{
    if (IsContextLost())
        return false;

    if (elemOffset > availElemCount) {
        ErrorInvalidValue("%s: Offset too big for list.", funcName);
        return false;
    }
    availElemCount -= elemOffset;

    ////

    size_t requiredElements;
    GLint maxDrawBuffer;
    switch (buffer) {
    case LOCAL_GL_COLOR:
    case LOCAL_GL_FRONT:
    case LOCAL_GL_BACK:
    case LOCAL_GL_LEFT:
    case LOCAL_GL_RIGHT:
    case LOCAL_GL_FRONT_AND_BACK:
          requiredElements = 4;
          maxDrawBuffer = mGLMaxDrawBuffers - 1;
          break;

    case LOCAL_GL_DEPTH:
    case LOCAL_GL_STENCIL:
          requiredElements = 1;
          maxDrawBuffer = 0;
          break;

    default:
          ErrorInvalidEnumInfo(funcName, buffer);
          return false;
    }

    if (drawBuffer < 0 || drawBuffer > maxDrawBuffer) {
        ErrorInvalidValue("%s: Invalid drawbuffer %d. This buffer only supports"
                          " `drawbuffer` values between 0 and %u.",
                          funcName, drawBuffer, maxDrawBuffer);
        return false;
    }

    if (availElemCount < requiredElements) {
        ErrorInvalidValue("%s: Not enough elements. Require %u. Given %u.",
                          funcName, requiredElements, availElemCount);
        return false;
    }

    ////

    MakeContextCurrent();
    if (mBoundDrawFramebuffer) {
        if (!mBoundDrawFramebuffer->ValidateAndInitAttachments(funcName))
            return false;
    }

    return true;
}

////

void
WebGL2Context::ClearBufferfv(GLenum buffer, GLint drawBuffer, const Float32Arr& src,
                             GLuint srcElemOffset)
{
    const char funcName[] = "clearBufferfv";
    if (!ValidateClearBuffer(funcName, buffer, drawBuffer, src.elemCount, srcElemOffset))
        return;

    const auto ptr = src.elemBytes + srcElemOffset;
    gl->fClearBufferfv(buffer, drawBuffer, ptr);
}

void
WebGL2Context::ClearBufferiv(GLenum buffer, GLint drawBuffer, const Int32Arr& src,
                             GLuint srcElemOffset)
{
    const char funcName[] = "clearBufferiv";
    if (!ValidateClearBuffer(funcName, buffer, drawBuffer, src.elemCount, srcElemOffset))
        return;

    const auto ptr = src.elemBytes + srcElemOffset;
    gl->fClearBufferiv(buffer, drawBuffer, ptr);
}

void
WebGL2Context::ClearBufferuiv(GLenum buffer, GLint drawBuffer, const Uint32Arr& src,
                             GLuint srcElemOffset)
{
    const char funcName[] = "clearBufferuiv";
    if (!ValidateClearBuffer(funcName, buffer, drawBuffer, src.elemCount, srcElemOffset))
        return;

    const auto ptr = src.elemBytes + srcElemOffset;
    gl->fClearBufferuiv(buffer, drawBuffer, ptr);
}

////

void
WebGL2Context::ClearBufferfi(GLenum buffer, GLint drawBuffer, GLfloat depth,
                             GLint stencil)
{
    const char funcName[] = "clearBufferfi";
    if (!ValidateClearBuffer(funcName, LOCAL_GL_DEPTH, drawBuffer, 1, 0))
        return;

    if (buffer != LOCAL_GL_DEPTH_STENCIL)
        return ErrorInvalidEnumInfo(funcName, buffer);

    gl->fClearBufferfi(buffer, drawBuffer, depth, stencil);
}

} // namespace mozilla
