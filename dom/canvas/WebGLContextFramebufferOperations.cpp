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

    uint32_t m = mask & (LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT | LOCAL_GL_STENCIL_BUFFER_BIT);
    if (mask != m)
        return ErrorInvalidValue("%s: invalid mask bits", funcName);

    if (mask == 0) {
        GenerateWarning("Calling gl.clear(0) has no effect.");
    } else if (mRasterizerDiscardEnabled) {
        GenerateWarning("Calling gl.clear() with RASTERIZER_DISCARD enabled has no effects.");
    }

    if (mask & LOCAL_GL_COLOR_BUFFER_BIT && mBoundDrawFramebuffer) {
        if (mask & LOCAL_GL_COLOR_BUFFER_BIT) {
            for (const auto& cur : mBoundDrawFramebuffer->ColorDrawBuffers()) {
                if (!cur->IsDefined())
                    continue;

                switch (cur->Format()->format->componentType) {
                case webgl::ComponentType::Float:
                case webgl::ComponentType::NormInt:
                case webgl::ComponentType::NormUInt:
                    break;

                default:
                    ErrorInvalidOperation("%s: Color draw buffers must be floating-point"
                                          " or fixed-point. (normalized (u)ints)",
                                          funcName);
                    return;
                }
            }
        }
    }

    if (!BindCurFBForDraw(funcName))
        return;

    auto driverMask = mask;
    if (!mBoundDrawFramebuffer) {
        if (mNeedsFakeNoDepth) {
            driverMask &= ~LOCAL_GL_DEPTH_BUFFER_BIT;
        }
        if (mNeedsFakeNoStencil) {
            driverMask &= ~LOCAL_GL_STENCIL_BUFFER_BIT;
        }
    }

    const ScopedDrawCallWrapper wrapper(*this);
    gl->fClear(driverMask);
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

    const bool supportsFloatColorBuffers = (IsExtensionEnabled(WebGLExtensionID::EXT_color_buffer_float) ||
                                            IsExtensionEnabled(WebGLExtensionID::EXT_color_buffer_half_float) ||
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

    mDepthClearValue = GLClampFloat(v);
    gl->fClearDepth(mDepthClearValue);
}

void
WebGLContext::ClearStencil(GLint v)
{
    if (IsContextLost())
        return;

    mStencilClearValue = v;
    gl->fClearStencil(v);
}

void
WebGLContext::ColorMask(WebGLboolean r, WebGLboolean g, WebGLboolean b, WebGLboolean a)
{
    if (IsContextLost())
        return;

    gl->fColorMask(r, g, b, a);
    mColorWriteMask = uint8_t(bool(r)) << 0 |
                      uint8_t(bool(g)) << 1 |
                      uint8_t(bool(b)) << 2 |
                      uint8_t(bool(a)) << 3;
}

void
WebGLContext::DepthMask(WebGLboolean b)
{
    if (IsContextLost())
        return;

    mDepthWriteMask = b;
    gl->fDepthMask(b);
}

void
WebGLContext::DrawBuffers(const dom::Sequence<GLenum>& buffers)
{
    const char funcName[] = "drawBuffers";
    if (IsContextLost())
        return;

    if (mBoundDrawFramebuffer) {
        mBoundDrawFramebuffer->DrawBuffers(funcName, buffers);
        return;
    }

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
    // Don't actually set it.
}

void
WebGLContext::StencilMask(GLuint mask)
{
    if (IsContextLost())
        return;

    mStencilWriteMaskFront = mask;
    mStencilWriteMaskBack = mask;

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

    gl->fStencilMaskSeparate(face, mask);
}

} // namespace mozilla
