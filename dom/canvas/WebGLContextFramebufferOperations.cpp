/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLTexture.h"
#include "WebGLRenderbuffer.h"
#include "WebGLFramebuffer.h"
#include "GLContext.h"
#include "GLScreenBuffer.h"

namespace mozilla {

void
WebGLContext::Clear(GLbitfield mask)
{
    const char funcName[] = "clear";

    if (IsContextLost())
        return;

    MakeContextCurrent();

    uint32_t m = mask & (LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT | LOCAL_GL_STENCIL_BUFFER_BIT);
    if (mask != m)
        return ErrorInvalidValue("%s: invalid mask bits", funcName);

    if (mask == 0) {
        GenerateWarning("Calling gl.clear(0) has no effect.");
    } else if (mRasterizerDiscardEnabled) {
        GenerateWarning("Calling gl.clear() with RASTERIZER_DISCARD enabled has no effects.");
    }

    if (mBoundDrawFramebuffer) {
        if (!mBoundDrawFramebuffer->ValidateAndInitAttachments(funcName))
            return;

        gl->fClear(mask);
        return;
    } else {
        ClearBackbufferIfNeeded();
    }

    // Ok, we're clearing the default framebuffer/screen.
    {
        ScopedMaskWorkaround autoMask(*this);
        gl->fClear(mask);
    }

    Invalidate();
    mShouldPresent = true;
}

static GLfloat
GLClampFloat(GLfloat val)
{
    if (val < 0.0)
        return 0.0;

    if (val > 1.0)
        return 1.0;

    return val;
}

void
WebGLContext::ClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();

    const bool supportsFloatColorBuffers = (IsExtensionEnabled(WebGLExtensionID::EXT_color_buffer_half_float) ||
                                            IsExtensionEnabled(WebGLExtensionID::WEBGL_color_buffer_float));
    if (!supportsFloatColorBuffers) {
        r = GLClampFloat(r);
        g = GLClampFloat(g);
        b = GLClampFloat(b);
        a = GLClampFloat(a);
    }

    gl->fClearColor(r, g, b, a);

    mColorClearValue[0] = r;
    mColorClearValue[1] = g;
    mColorClearValue[2] = b;
    mColorClearValue[3] = a;
}

void
WebGLContext::ClearDepth(GLclampf v)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();
    mDepthClearValue = GLClampFloat(v);
    gl->fClearDepth(mDepthClearValue);
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
    const char funcName[] = "drawBuffers";
    if (IsContextLost())
        return;

    if (!mBoundDrawFramebuffer) {
        // GLES 3.0.4 p186:
        // "If the GL is bound to the default framebuffer, then `n` must be 1 and the
        //  constant must be BACK or NONE. [...] If DrawBuffers is supplied with a
        //  constant other than BACK and NONE, or with a value of `n` other than 1, the
        //  error INVALID_OPERATION is generated."
        if (buffers.Length() != 1) {
            ErrorInvalidOperation("%s: For the default framebuffer, `buffers` must have a"
                                  " length of 1.",
                                  funcName);
            return;
        }

        switch (buffers[0]) {
        case LOCAL_GL_NONE:
        case LOCAL_GL_BACK:
            break;

        default:
            ErrorInvalidOperation("%s: For the default framebuffer, `buffers[0]` must be"
                                  " BACK or NONE.",
                                  funcName);
            return;
        }

        mDefaultFB_DrawBuffer0 = buffers[0];
        gl->Screen()->SetDrawBuffer(buffers[0]);
        return;
    }

    // Framebuffer object (not default framebuffer)

    if (buffers.Length() > mImplMaxDrawBuffers) {
        // "An INVALID_VALUE error is generated if `n` is greater than MAX_DRAW_BUFFERS."
        ErrorInvalidValue("%s: `buffers` must have a length <= MAX_DRAW_BUFFERS.",
                          funcName);
        return;
    }

    for (size_t i = 0; i < buffers.Length(); i++) {
        // "If the GL is bound to a draw framebuffer object, the `i`th buffer listed in
        //  bufs must be COLOR_ATTACHMENTi or NONE. Specifying a buffer out of order,
        //  BACK, or COLOR_ATTACHMENTm where `m` is greater than or equal to the value of
        // MAX_COLOR_ATTACHMENTS, will generate the error INVALID_OPERATION.

        // WEBGL_draw_buffers:
        // "The value of the MAX_COLOR_ATTACHMENTS_WEBGL parameter must be greater than or
        //  equal to that of the MAX_DRAW_BUFFERS_WEBGL parameter."
        // This means that if buffers.Length() isn't larger than MaxDrawBuffers, it won't
        // be larger than MaxColorAttachments.
        if (buffers[i] != LOCAL_GL_NONE &&
            buffers[i] != LOCAL_GL_COLOR_ATTACHMENT0 + i)
        {
            ErrorInvalidOperation("%s: `buffers[i]` must be NONE or COLOR_ATTACHMENTi.",
                                  funcName);
            return;
        }
    }

    MakeContextCurrent();

    const GLenum* ptr = nullptr;
    if (buffers.Length()) {
        ptr = buffers.Elements();
    }

    gl->fDrawBuffers(buffers.Length(), ptr);

    const auto end = ptr + buffers.Length();
    mBoundDrawFramebuffer->mDrawBuffers.assign(ptr, end);
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

} // namespace mozilla
