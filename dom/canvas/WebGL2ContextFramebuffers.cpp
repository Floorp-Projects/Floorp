/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "GLScreenBuffer.h"
#include "WebGLContextUtils.h"
#include "WebGLFormats.h"
#include "WebGLFramebuffer.h"

namespace mozilla {

void
WebGL2Context::BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                               GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                               GLbitfield mask, GLenum filter)
{
    if (IsContextLost())
        return;

    const GLbitfield validBits = LOCAL_GL_COLOR_BUFFER_BIT |
                                 LOCAL_GL_DEPTH_BUFFER_BIT |
                                 LOCAL_GL_STENCIL_BUFFER_BIT;
    if ((mask | validBits) != validBits) {
        ErrorInvalidValue("blitFramebuffer: Invalid bit set in mask.");
        return;
    }

    switch (filter) {
    case LOCAL_GL_NEAREST:
    case LOCAL_GL_LINEAR:
        break;
    default:
        ErrorInvalidEnumInfo("blitFramebuffer: Bad `filter`:", filter);
        return;
    }

    ////

    const auto& readFB = mBoundReadFramebuffer;
    if (readFB &&
        !readFB->ValidateAndInitAttachments("blitFramebuffer's READ_FRAMEBUFFER"))
    {
        return;
    }

    const auto& drawFB = mBoundDrawFramebuffer;
    if (drawFB &&
        !drawFB->ValidateAndInitAttachments("blitFramebuffer's DRAW_FRAMEBUFFER"))
    {
        return;
    }

    ////

    WebGLFramebuffer::BlitFramebuffer(this,
                                      readFB, srcX0, srcY0, srcX1, srcY1,
                                      drawFB, dstX0, dstY0, dstX1, dstY1,
                                      mask, filter);
}

void
WebGL2Context::FramebufferTextureLayer(GLenum target, GLenum attachment,
                                       WebGLTexture* texture, GLint level, GLint layer)
{
    const char funcName[] = "framebufferTextureLayer";
    if (IsContextLost())
        return;

    if (!ValidateFramebufferTarget(target, funcName))
        return;

    WebGLFramebuffer* fb;
    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
    case LOCAL_GL_DRAW_FRAMEBUFFER:
        fb = mBoundDrawFramebuffer;
        break;

    case LOCAL_GL_READ_FRAMEBUFFER:
        fb = mBoundReadFramebuffer;
        break;

    default:
        MOZ_CRASH("GFX: Bad target.");
    }

    if (!fb)
        return ErrorInvalidOperation("%s: Cannot modify framebuffer 0.", funcName);

    fb->FramebufferTextureLayer(funcName, attachment, texture, level, layer);
}

JS::Value
WebGL2Context::GetFramebufferAttachmentParameter(JSContext* cx,
                                                 GLenum target,
                                                 GLenum attachment,
                                                 GLenum pname,
                                                 ErrorResult& out_error)
{
    return WebGLContext::GetFramebufferAttachmentParameter(cx, target, attachment, pname,
                                                           out_error);
}

// Map attachments intended for the default buffer, to attachments for a non-
// default buffer.
static bool
TranslateDefaultAttachments(const dom::Sequence<GLenum>& in, dom::Sequence<GLenum>* out)
{
    for (size_t i = 0; i < in.Length(); i++) {
        switch (in[i]) {
            case LOCAL_GL_COLOR:
                if (!out->AppendElement(LOCAL_GL_COLOR_ATTACHMENT0, fallible)) {
                    return false;
                }
                break;

            case LOCAL_GL_DEPTH:
                if (!out->AppendElement(LOCAL_GL_DEPTH_ATTACHMENT, fallible)) {
                    return false;
                }
                break;

            case LOCAL_GL_STENCIL:
                if (!out->AppendElement(LOCAL_GL_STENCIL_ATTACHMENT, fallible)) {
                    return false;
                }
                break;
        }
    }

    return true;
}

void
WebGL2Context::InvalidateFramebuffer(GLenum target,
                                     const dom::Sequence<GLenum>& attachments,
                                     ErrorResult& rv)
{
    const char funcName[] = "invalidateSubFramebuffer";

    if (IsContextLost())
        return;

    MakeContextCurrent();

    if (!ValidateFramebufferTarget(target, funcName))
        return;

    const WebGLFramebuffer* fb;
    bool isDefaultFB;
    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
    case LOCAL_GL_DRAW_FRAMEBUFFER:
        fb = mBoundDrawFramebuffer;
        isDefaultFB = gl->Screen()->IsDrawFramebufferDefault();
        break;

    case LOCAL_GL_READ_FRAMEBUFFER:
        fb = mBoundReadFramebuffer;
        isDefaultFB = gl->Screen()->IsReadFramebufferDefault();
        break;

    default:
        MOZ_CRASH("GFX: Bad target.");
    }

    const bool badColorAttachmentIsInvalidOp = true;
    for (size_t i = 0; i < attachments.Length(); i++) {
        if (!ValidateFramebufferAttachment(fb, attachments[i], funcName,
                                           badColorAttachmentIsInvalidOp))
        {
            return;
        }
    }

    // InvalidateFramebuffer is a hint to the driver. Should be OK to
    // skip calls if not supported, for example by OSX 10.9 GL
    // drivers.
    if (!gl->IsSupported(gl::GLFeature::invalidate_framebuffer))
        return;

    if (!fb && !isDefaultFB) {
        dom::Sequence<GLenum> tmpAttachments;
        if (!TranslateDefaultAttachments(attachments, &tmpAttachments)) {
            rv.Throw(NS_ERROR_OUT_OF_MEMORY);
            return;
        }

        gl->fInvalidateFramebuffer(target, tmpAttachments.Length(),
                                   tmpAttachments.Elements());
    } else {
        gl->fInvalidateFramebuffer(target, attachments.Length(), attachments.Elements());
    }
}

void
WebGL2Context::InvalidateSubFramebuffer(GLenum target, const dom::Sequence<GLenum>& attachments,
                                        GLint x, GLint y, GLsizei width, GLsizei height,
                                        ErrorResult& rv)
{
    const char funcName[] = "invalidateSubFramebuffer";

    if (IsContextLost())
        return;

    MakeContextCurrent();

    if (!ValidateFramebufferTarget(target, funcName))
        return;

    if (width < 0 || height < 0) {
        ErrorInvalidValue("%s: width and height must be >= 0.", funcName);
        return;
    }

    const WebGLFramebuffer* fb;
    bool isDefaultFB;
    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
    case LOCAL_GL_DRAW_FRAMEBUFFER:
        fb = mBoundDrawFramebuffer;
        isDefaultFB = gl->Screen()->IsDrawFramebufferDefault();
        break;

    case LOCAL_GL_READ_FRAMEBUFFER:
        fb = mBoundReadFramebuffer;
        isDefaultFB = gl->Screen()->IsReadFramebufferDefault();
        break;

    default:
        MOZ_CRASH("GFX: Bad target.");
    }

    const bool badColorAttachmentIsInvalidOp = true;
    for (size_t i = 0; i < attachments.Length(); i++) {
        if (!ValidateFramebufferAttachment(fb, attachments[i], funcName,
                                           badColorAttachmentIsInvalidOp))
        {
            return;
        }
    }

    // InvalidateFramebuffer is a hint to the driver. Should be OK to
    // skip calls if not supported, for example by OSX 10.9 GL
    // drivers.
    if (!gl->IsSupported(gl::GLFeature::invalidate_framebuffer))
        return;

    if (!fb && !isDefaultFB) {
        dom::Sequence<GLenum> tmpAttachments;
        if (!TranslateDefaultAttachments(attachments, &tmpAttachments)) {
            rv.Throw(NS_ERROR_OUT_OF_MEMORY);
            return;
        }

        gl->fInvalidateSubFramebuffer(target, tmpAttachments.Length(),
                                      tmpAttachments.Elements(), x, y, width, height);
    } else {
        gl->fInvalidateSubFramebuffer(target, attachments.Length(),
                                      attachments.Elements(), x, y, width, height);
    }
}

void
WebGL2Context::ReadBuffer(GLenum mode)
{
    const char funcName[] = "readBuffer";
    if (IsContextLost())
        return;

    if (mBoundReadFramebuffer) {
        mBoundReadFramebuffer->ReadBuffer(funcName, mode);
        return;
    }

    // Operating on the default framebuffer.
    if (mode != LOCAL_GL_NONE &&
        mode != LOCAL_GL_BACK)
    {
        ErrorInvalidOperation("%s: If READ_FRAMEBUFFER is null, `mode` must be BACK or"
                              " NONE. Was %s",
                              funcName, EnumName(mode));
        return;
    }

    gl->Screen()->SetReadBuffer(mode);
}

} // namespace mozilla
