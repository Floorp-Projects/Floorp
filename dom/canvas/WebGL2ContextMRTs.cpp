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
                                   size_t availElemCount, GLuint elemOffset,
                                   GLenum funcType)
{
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
          requiredElements = 4;
          maxDrawBuffer = mGLMaxDrawBuffers - 1;
          break;

    case LOCAL_GL_DEPTH:
    case LOCAL_GL_STENCIL:
          requiredElements = 1;
          maxDrawBuffer = 0;
          break;

    case LOCAL_GL_DEPTH_STENCIL:
          requiredElements = 2;
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
        ErrorInvalidValue("%s: Not enough elements. Require %zu. Given %zu.",
                          funcName, requiredElements, availElemCount);
        return false;
    }

    ////

    MakeContextCurrent();

    const auto& fb = mBoundDrawFramebuffer;
    if (fb) {
        if (!fb->ValidateAndInitAttachments(funcName))
            return false;

        if (!fb->ValidateClearBufferType(funcName, buffer, drawBuffer, funcType))
            return false;
    } else if (buffer == LOCAL_GL_COLOR) {
        if (drawBuffer != 0)
            return true;

        if (mDefaultFB_DrawBuffer0 == LOCAL_GL_NONE)
            return true;

        if (funcType != LOCAL_GL_FLOAT) {
            ErrorInvalidOperation("%s: For default framebuffer, COLOR is always of type"
                                  " FLOAT.",
                                  funcName);
            return false;
        }
    }

    return true;
}

////

void
WebGL2Context::ClearBufferfv(GLenum buffer, GLint drawBuffer, const Float32Arr& src,
                             GLuint srcElemOffset)
{
    const char funcName[] = "clearBufferfv";
    if (IsContextLost())
        return;

    if (buffer != LOCAL_GL_COLOR &&
        buffer != LOCAL_GL_DEPTH)
    {
        ErrorInvalidEnum("%s: buffer must be COLOR or DEPTH.", funcName);
        return;
    }

    if (!ValidateClearBuffer(funcName, buffer, drawBuffer, src.elemCount, srcElemOffset,
                             LOCAL_GL_FLOAT))
    {
        return;
    }

    ScopedDrawCallWrapper wrapper(*this);
    const auto ptr = src.elemBytes + srcElemOffset;
    gl->fClearBufferfv(buffer, drawBuffer, ptr);
}

void
WebGL2Context::ClearBufferiv(GLenum buffer, GLint drawBuffer, const Int32Arr& src,
                             GLuint srcElemOffset)
{
    const char funcName[] = "clearBufferiv";
    if (IsContextLost())
        return;

    if (buffer != LOCAL_GL_COLOR &&
        buffer != LOCAL_GL_STENCIL)
    {
        ErrorInvalidEnum("%s: buffer must be COLOR or STENCIL.", funcName);
        return;
    }

    if (!ValidateClearBuffer(funcName, buffer, drawBuffer, src.elemCount, srcElemOffset,
                             LOCAL_GL_INT))
    {
        return;
    }

    ScopedDrawCallWrapper wrapper(*this);
    const auto ptr = src.elemBytes + srcElemOffset;
    gl->fClearBufferiv(buffer, drawBuffer, ptr);
}

void
WebGL2Context::ClearBufferuiv(GLenum buffer, GLint drawBuffer, const Uint32Arr& src,
                              GLuint srcElemOffset)
{
    const char funcName[] = "clearBufferuiv";
    if (IsContextLost())
        return;

    if (buffer != LOCAL_GL_COLOR)
        return ErrorInvalidEnum("%s: buffer must be COLOR.", funcName);

    if (!ValidateClearBuffer(funcName, buffer, drawBuffer, src.elemCount, srcElemOffset,
                             LOCAL_GL_UNSIGNED_INT))
    {
        return;
    }

    ScopedDrawCallWrapper wrapper(*this);
    const auto ptr = src.elemBytes + srcElemOffset;
    gl->fClearBufferuiv(buffer, drawBuffer, ptr);
}

////

void
WebGL2Context::ClearBufferfi(GLenum buffer, GLint drawBuffer, GLfloat depth,
                             GLint stencil)
{
    const char funcName[] = "clearBufferfi";
    if (IsContextLost())
        return;

    if (buffer != LOCAL_GL_DEPTH_STENCIL)
        return ErrorInvalidEnum("%s: buffer must be DEPTH_STENCIL.", funcName);

    if (!ValidateClearBuffer(funcName, buffer, drawBuffer, 2, 0, 0))
        return;

    ScopedDrawCallWrapper wrapper(*this);
    gl->fClearBufferfi(buffer, drawBuffer, depth, stencil);
}

} // namespace mozilla
