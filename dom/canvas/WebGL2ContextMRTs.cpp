/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLFramebuffer.h"

namespace mozilla {

bool
WebGL2Context::ValidateClearBuffer(GLenum buffer, GLint drawBuffer,
                                   size_t availElemCount, GLuint elemOffset,
                                   GLenum funcType)
{
    if (elemOffset > availElemCount) {
        ErrorInvalidValue("Offset too big for list.");
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
          ErrorInvalidEnumInfo("buffer", buffer);
          return false;
    }

    if (drawBuffer < 0 || drawBuffer > maxDrawBuffer) {
        ErrorInvalidValue("Invalid drawbuffer %d. This buffer only supports"
                          " `drawbuffer` values between 0 and %u.",
                          drawBuffer, maxDrawBuffer);
        return false;
    }

    if (availElemCount < requiredElements) {
        ErrorInvalidValue("Not enough elements. Require %zu. Given %zu.",
                          requiredElements, availElemCount);
        return false;
    }

    ////

    if (!BindCurFBForDraw())
        return false;

    const auto& fb = mBoundDrawFramebuffer;
    if (fb) {
        if (!fb->ValidateClearBufferType(buffer, drawBuffer, funcType))
            return false;
    } else if (buffer == LOCAL_GL_COLOR) {
        if (drawBuffer != 0)
            return true;

        if (mDefaultFB_DrawBuffer0 == LOCAL_GL_NONE)
            return true;

        if (funcType != LOCAL_GL_FLOAT) {
            ErrorInvalidOperation("For default framebuffer, COLOR is always of type"
                                  " FLOAT.");
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
    const FuncScope funcScope(*this, "clearBufferfv");
    if (IsContextLost())
        return;

    if (buffer != LOCAL_GL_COLOR &&
        buffer != LOCAL_GL_DEPTH)
    {
        ErrorInvalidEnum("`buffer` must be COLOR or DEPTH.");
        return;
    }

    if (!ValidateClearBuffer(buffer, drawBuffer, src.elemCount, srcElemOffset,
                             LOCAL_GL_FLOAT))
    {
        return;
    }

    if (!mBoundDrawFramebuffer &&
        buffer == LOCAL_GL_DEPTH &&
        mNeedsFakeNoDepth)
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
    const FuncScope funcScope(*this, "clearBufferiv");
    if (IsContextLost())
        return;

    if (buffer != LOCAL_GL_COLOR &&
        buffer != LOCAL_GL_STENCIL)
    {
        ErrorInvalidEnum("`buffer` must be COLOR or STENCIL.");
        return;
    }

    if (!ValidateClearBuffer(buffer, drawBuffer, src.elemCount, srcElemOffset,
                             LOCAL_GL_INT))
    {
        return;
    }

    if (!mBoundDrawFramebuffer &&
        buffer == LOCAL_GL_STENCIL &&
        mNeedsFakeNoStencil)
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
    const FuncScope funcScope(*this, "clearBufferuiv");
    if (IsContextLost())
        return;

    if (buffer != LOCAL_GL_COLOR)
        return ErrorInvalidEnum("`buffer` must be COLOR.");

    if (!ValidateClearBuffer(buffer, drawBuffer, src.elemCount, srcElemOffset,
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
    const FuncScope funcScope(*this, "clearBufferfi");
    if (IsContextLost())
        return;

    if (buffer != LOCAL_GL_DEPTH_STENCIL)
        return ErrorInvalidEnum("`buffer` must be DEPTH_STENCIL.");

    if (!ValidateClearBuffer(buffer, drawBuffer, 2, 0, 0))
        return;

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

} // namespace mozilla
