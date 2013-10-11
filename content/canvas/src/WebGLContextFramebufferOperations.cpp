/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLTexture.h"
#include "WebGLRenderbuffer.h"
#include "WebGLFramebuffer.h"
#include "GLContext.h"

using namespace mozilla;

void
WebGLContext::Clear(GLbitfield mask)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();

    uint32_t m = mask & (LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT | LOCAL_GL_STENCIL_BUFFER_BIT);
    if (mask != m)
        return ErrorInvalidValue("clear: invalid mask bits");

    if (mask == 0) {
        GenerateWarning("Calling gl.clear(0) has no effect.");
    } else if (mRasterizerDiscardEnabled) {
        GenerateWarning("Calling gl.clear() with RASTERIZER_DISCARD enabled has no effects.");
    }

    if (mBoundFramebuffer) {
        if (!mBoundFramebuffer->CheckAndInitializeAttachments())
            return ErrorInvalidFramebufferOperation("clear: incomplete framebuffer");

        gl->fClear(mask);
        return;
    }

    // Ok, we're clearing the default framebuffer/screen.

    bool needsClear = true;
    if (mIsScreenCleared) {
        bool isClearRedundant = true;
        if (mask & LOCAL_GL_COLOR_BUFFER_BIT) {
            if (mColorClearValue[0] != 0.0f ||
                mColorClearValue[1] != 0.0f ||
                mColorClearValue[2] != 0.0f ||
                mColorClearValue[3] != 0.0f)
            {
                isClearRedundant = false;
            }
        }

        if (mask & LOCAL_GL_DEPTH_BUFFER_BIT) {
            if (mDepthClearValue != 1.0f) {
                isClearRedundant = false;
            }
        }

        if (mask & LOCAL_GL_DEPTH_BUFFER_BIT) {
            if (mStencilClearValue != 0) {
                isClearRedundant = false;
            }
        }

        if (isClearRedundant)
            needsClear = false;
    }

    if (needsClear) {
        gl->fClear(mask);
        mIsScreenCleared = false;
    }

    Invalidate();
    mShouldPresent = true;
}

static GLclampf
GLClampFloat(GLclampf val)
{
    if (val < 0.0)
        return 0.0;

    if (val > 1.0)
        return 1.0;

    return val;
}

void
WebGLContext::ClearColor(GLclampf r, GLclampf g,
                             GLclampf b, GLclampf a)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();
    mColorClearValue[0] = GLClampFloat(r);
    mColorClearValue[1] = GLClampFloat(g);
    mColorClearValue[2] = GLClampFloat(b);
    mColorClearValue[3] = GLClampFloat(a);
    gl->fClearColor(r, g, b, a);
}

void
WebGLContext::ClearDepth(GLclampf v)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();
    mDepthClearValue = GLClampFloat(v);
    gl->fClearDepth(v);
}

void
WebGLContext::ClearStencil(GLint v)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();
    mStencilClearValue = v;
    gl->fClearStencil(v);
}

void
WebGLContext::ColorMask(WebGLboolean r, WebGLboolean g, WebGLboolean b, WebGLboolean a)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();
    mColorWriteMask[0] = r;
    mColorWriteMask[1] = g;
    mColorWriteMask[2] = b;
    mColorWriteMask[3] = a;
    gl->fColorMask(r, g, b, a);
}

void
WebGLContext::DepthMask(WebGLboolean b)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();
    mDepthWriteMask = b;
    gl->fDepthMask(b);
}

void
WebGLContext::DrawBuffers(const dom::Sequence<GLenum>& buffers)
{
    const size_t buffersLength = buffers.Length();

    if (buffersLength == 0) {
        return ErrorInvalidValue("drawBuffers: invalid <buffers> (buffers must not be empty)");
    }

    if (mBoundFramebuffer == 0)
    {
        // OK: we are rendering in the default framebuffer

        /* EXT_draw_buffers :
         If the GL is bound to the default framebuffer, then <buffersLength> must be 1
         and the constant must be BACK or NONE. When draw buffer zero is
         BACK, color values are written into the sole buffer for single-
         buffered contexts, or into the back buffer for double-buffered
         contexts. If DrawBuffersEXT is supplied with a constant other than
         BACK and NONE, the error INVALID_OPERATION is generated.
         */
        if (buffersLength != 1) {
            return ErrorInvalidValue("drawBuffers: invalid <buffers> (main framebuffer: buffers.length must be 1)");
        }

        MakeContextCurrent();

        if (buffers[0] == LOCAL_GL_NONE) {
            const GLenum drawBuffersCommand = LOCAL_GL_NONE;
            gl->fDrawBuffers(1, &drawBuffersCommand);
            return;
        }
        else if (buffers[0] == LOCAL_GL_BACK) {
            const GLenum drawBuffersCommand = LOCAL_GL_COLOR_ATTACHMENT0;
            gl->fDrawBuffers(1, &drawBuffersCommand);
            return;
        }
        return ErrorInvalidOperation("drawBuffers: invalid operation (main framebuffer: buffers[0] must be GL_NONE or GL_BACK)");
    }

    // OK: we are rendering in a framebuffer object

    if (buffersLength > size_t(mGLMaxDrawBuffers)) {
        /* EXT_draw_buffers :
         The maximum number of draw buffers is implementation-dependent. The
         number of draw buffers supported can be queried by calling
         GetIntegerv with the symbolic constant MAX_DRAW_BUFFERS_EXT. An
         INVALID_VALUE error is generated if <buffersLength> is greater than
         MAX_DRAW_BUFFERS_EXT.
         */
        return ErrorInvalidValue("drawBuffers: invalid <buffers> (buffers.length > GL_MAX_DRAW_BUFFERS)");
    }

    for (uint32_t i = 0; i < buffersLength; i++)
    {
        /* EXT_draw_buffers :
         If the GL is bound to a draw framebuffer object, the <i>th buffer listed
         in <bufs> must be COLOR_ATTACHMENT<i>_EXT or NONE. Specifying a
         buffer out of order, BACK, or COLOR_ATTACHMENT<m>_EXT where <m> is
         greater than or equal to the value of MAX_COLOR_ATTACHMENTS_EXT,
         will generate the error INVALID_OPERATION.
         */
        /* WEBGL_draw_buffers :
         The value of the MAX_COLOR_ATTACHMENTS_WEBGL parameter must be greater than or equal to that of the MAX_DRAW_BUFFERS_WEBGL parameter.
         */
        if (buffers[i] != LOCAL_GL_NONE &&
            buffers[i] != GLenum(LOCAL_GL_COLOR_ATTACHMENT0 + i)) {
            return ErrorInvalidOperation("drawBuffers: invalid operation (buffers[i] must be GL_NONE or GL_COLOR_ATTACHMENTi)");
        }
    }

    MakeContextCurrent();

    gl->fDrawBuffers(buffersLength, buffers.Elements());
}

void
WebGLContext::StencilMask(GLuint mask)
{
    if (IsContextLost())
        return;

    mStencilWriteMaskFront = mask;
    mStencilWriteMaskBack = mask;

    MakeContextCurrent();
    gl->fStencilMask(mask);
}

void
WebGLContext::StencilMaskSeparate(GLenum face, GLuint mask)
{
    if (IsContextLost())
        return;

    if (!ValidateFaceEnum(face, "stencilMaskSeparate: face"))
        return;

    switch (face) {
        case LOCAL_GL_FRONT_AND_BACK:
            mStencilWriteMaskFront = mask;
            mStencilWriteMaskBack = mask;
            break;
        case LOCAL_GL_FRONT:
            mStencilWriteMaskFront = mask;
            break;
        case LOCAL_GL_BACK:
            mStencilWriteMaskBack = mask;
            break;
    }

    MakeContextCurrent();
    gl->fStencilMaskSeparate(face, mask);
}




