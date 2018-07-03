/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "GLScreenBuffer.h"
#include "mozilla/CheckedInt.h"
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

    // --

    const auto fnLikelyOverflow = [](GLint p0, GLint p1) {
        auto checked = CheckedInt<GLint>(p1) - p0;
        checked = -checked; // And check the negation!
        return !checked.isValid();
    };

    if (fnLikelyOverflow(srcX0, srcX1) || fnLikelyOverflow(srcY0, srcY1) ||
        fnLikelyOverflow(dstX0, dstX1) || fnLikelyOverflow(dstY0, dstY1))
    {
        ErrorInvalidValue("blitFramebuffer: Likely-to-overflow large ranges are"
                          " forbidden.");
        return;
    }

    // --

    if (!ValidateAndInitFB("blitFramebuffer: READ_FRAMEBUFFER", mBoundReadFramebuffer) ||
        !ValidateAndInitFB("blitFramebuffer: DRAW_FRAMEBUFFER", mBoundDrawFramebuffer))
    {
        return;
    }

    DoBindFB(mBoundReadFramebuffer, LOCAL_GL_READ_FRAMEBUFFER);
    DoBindFB(mBoundDrawFramebuffer, LOCAL_GL_DRAW_FRAMEBUFFER);

    WebGLFramebuffer::BlitFramebuffer(this,
                                      srcX0, srcY0, srcX1, srcY1,
                                      dstX0, dstY0, dstX1, dstY1,
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

////

static bool
ValidateBackbufferAttachmentEnum(WebGLContext* webgl, const char* funcName,
                                 GLenum attachment)
{
    switch (attachment) {
    case LOCAL_GL_COLOR:
    case LOCAL_GL_DEPTH:
    case LOCAL_GL_STENCIL:
        return true;

    default:
        webgl->ErrorInvalidEnum("%s: attachment: invalid enum value 0x%x.",
                                funcName, attachment);
        return false;
    }
}

static bool
ValidateFramebufferAttachmentEnum(WebGLContext* webgl, const char* funcName,
                                  GLenum attachment)
{
    switch (attachment) {
    case LOCAL_GL_DEPTH_ATTACHMENT:
    case LOCAL_GL_STENCIL_ATTACHMENT:
    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
        return true;
    }

    if (attachment < LOCAL_GL_COLOR_ATTACHMENT0) {
        webgl->ErrorInvalidEnum("%s: attachment: invalid enum value 0x%x.",
                                funcName, attachment);
        return false;
    }

    if (attachment > webgl->LastColorAttachmentEnum()) {
        // That these errors have different types is ridiculous.
        webgl->ErrorInvalidOperation("%s: Too-large LOCAL_GL_COLOR_ATTACHMENTn.",
                                     funcName);
        return false;
    }

    return true;
}

bool
WebGLContext::ValidateInvalidateFramebuffer(const char* funcName, GLenum target,
                                            const dom::Sequence<GLenum>& attachments,
                                            ErrorResult* const out_rv,
                                            std::vector<GLenum>* const scopedVector,
                                            GLsizei* const out_glNumAttachments,
                                            const GLenum** const out_glAttachments)
{
    if (IsContextLost())
        return false;

    if (!ValidateFramebufferTarget(target, funcName))
        return false;

    const WebGLFramebuffer* fb;
    bool isDefaultFB = false;
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

    if (fb) {
        const auto fbStatus = fb->CheckFramebufferStatus(funcName);
        if (fbStatus != LOCAL_GL_FRAMEBUFFER_COMPLETE)
            return false; // Not an error, but don't run forward to driver either.
    } else {
        if (!EnsureDefaultFB(funcName))
            return false;
    }
    DoBindFB(fb, target);

    *out_glNumAttachments = attachments.Length();
    *out_glAttachments = attachments.Elements();

    if (fb) {
        for (const auto& attachment : attachments) {
            if (!ValidateFramebufferAttachmentEnum(this, funcName, attachment))
                return false;
        }
    } else {
        for (const auto& attachment : attachments) {
            if (!ValidateBackbufferAttachmentEnum(this, funcName, attachment))
                return false;
        }

        if (!isDefaultFB) {
            MOZ_ASSERT(scopedVector->empty());
            scopedVector->reserve(attachments.Length());
            for (const auto& attachment : attachments) {
                switch (attachment) {
                case LOCAL_GL_COLOR:
                    scopedVector->push_back(LOCAL_GL_COLOR_ATTACHMENT0);
                    break;

                case LOCAL_GL_DEPTH:
                    scopedVector->push_back(LOCAL_GL_DEPTH_ATTACHMENT);
                    break;

                case LOCAL_GL_STENCIL:
                    scopedVector->push_back(LOCAL_GL_STENCIL_ATTACHMENT);
                    break;

                default:
                    MOZ_CRASH();
                }
            }
            *out_glNumAttachments = scopedVector->size();
            *out_glAttachments = scopedVector->data();
        }
    }

    ////

    if (!fb) {
        mDefaultFB_IsInvalid = true;
        mResolvedDefaultFB = nullptr;
    }
    return true;
}

void
WebGL2Context::InvalidateFramebuffer(GLenum target,
                                     const dom::Sequence<GLenum>& attachments,
                                     ErrorResult& rv)
{
    const char funcName[] = "invalidateSubFramebuffer";

    std::vector<GLenum> scopedVector;
    GLsizei glNumAttachments;
    const GLenum* glAttachments;
    if (!ValidateInvalidateFramebuffer(funcName, target, attachments, &rv, &scopedVector,
                                       &glNumAttachments, &glAttachments))
    {
        return;
    }

    ////

    // Some drivers (like OSX 10.9 GL) just don't support invalidate_framebuffer.
    const bool useFBInvalidation = (mAllowFBInvalidation &&
                                    gl->IsSupported(gl::GLFeature::invalidate_framebuffer));
    if (useFBInvalidation) {
        gl->fInvalidateFramebuffer(target, glNumAttachments, glAttachments);
        return;
    }

    // Use clear instead?
    // No-op for now.
}

void
WebGL2Context::InvalidateSubFramebuffer(GLenum target, const dom::Sequence<GLenum>& attachments,
                                        GLint x, GLint y, GLsizei width, GLsizei height,
                                        ErrorResult& rv)
{
    const char funcName[] = "invalidateSubFramebuffer";

    if (!ValidateNonNegative(funcName, "width", width) ||
        !ValidateNonNegative(funcName, "height", height))
    {
        return;
    }

    std::vector<GLenum> scopedVector;
    GLsizei glNumAttachments;
    const GLenum* glAttachments;
    if (!ValidateInvalidateFramebuffer(funcName, target, attachments, &rv, &scopedVector,
                                       &glNumAttachments, &glAttachments))
    {
        return;
    }

    ////

    // Some drivers (like OSX 10.9 GL) just don't support invalidate_framebuffer.
    const bool useFBInvalidation = (mAllowFBInvalidation &&
                                    gl->IsSupported(gl::GLFeature::invalidate_framebuffer));
    if (useFBInvalidation) {
        gl->fInvalidateSubFramebuffer(target, glNumAttachments, glAttachments, x, y,
                                      width, height);
        return;
    }

    // Use clear instead?
    // No-op for now.
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
        nsCString enumName;
        EnumName(mode, &enumName);
        ErrorInvalidOperation("%s: If READ_FRAMEBUFFER is null, `mode` must be BACK or"
                              " NONE. Was %s.",
                              funcName, enumName.BeginReading());
        return;
    }

    mDefaultFB_ReadBuffer = mode;
}

} // namespace mozilla
