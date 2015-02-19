/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLContextUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

// Returns one of FLOAT, INT, UNSIGNED_INT.
// Fixed-points (normalized ints) are considered FLOAT.
static GLenum
ValueTypeForFormat(GLenum internalFormat)
{
    switch (internalFormat) {
    // Fixed-point
    case LOCAL_GL_R8:
    case LOCAL_GL_RG8:
    case LOCAL_GL_RGB565:
    case LOCAL_GL_RGB8:
    case LOCAL_GL_RGBA4:
    case LOCAL_GL_RGB5_A1:
    case LOCAL_GL_RGBA8:
    case LOCAL_GL_RGB10_A2:
    case LOCAL_GL_ALPHA8:
    case LOCAL_GL_LUMINANCE8:
    case LOCAL_GL_LUMINANCE8_ALPHA8:
    case LOCAL_GL_SRGB8:
    case LOCAL_GL_SRGB8_ALPHA8:
    case LOCAL_GL_R8_SNORM:
    case LOCAL_GL_RG8_SNORM:
    case LOCAL_GL_RGB8_SNORM:
    case LOCAL_GL_RGBA8_SNORM:

    // Floating-point
    case LOCAL_GL_R16F:
    case LOCAL_GL_RG16F:
    case LOCAL_GL_RGB16F:
    case LOCAL_GL_RGBA16F:
    case LOCAL_GL_ALPHA16F_EXT:
    case LOCAL_GL_LUMINANCE16F_EXT:
    case LOCAL_GL_LUMINANCE_ALPHA16F_EXT:

    case LOCAL_GL_R32F:
    case LOCAL_GL_RG32F:
    case LOCAL_GL_RGB32F:
    case LOCAL_GL_RGBA32F:
    case LOCAL_GL_ALPHA32F_EXT:
    case LOCAL_GL_LUMINANCE32F_EXT:
    case LOCAL_GL_LUMINANCE_ALPHA32F_EXT:

    case LOCAL_GL_R11F_G11F_B10F:
    case LOCAL_GL_RGB9_E5:
        return LOCAL_GL_FLOAT;

    // Int
    case LOCAL_GL_R8I:
    case LOCAL_GL_RG8I:
    case LOCAL_GL_RGB8I:
    case LOCAL_GL_RGBA8I:

    case LOCAL_GL_R16I:
    case LOCAL_GL_RG16I:
    case LOCAL_GL_RGB16I:
    case LOCAL_GL_RGBA16I:

    case LOCAL_GL_R32I:
    case LOCAL_GL_RG32I:
    case LOCAL_GL_RGB32I:
    case LOCAL_GL_RGBA32I:
        return LOCAL_GL_INT;

    // Unsigned int
    case LOCAL_GL_R8UI:
    case LOCAL_GL_RG8UI:
    case LOCAL_GL_RGB8UI:
    case LOCAL_GL_RGBA8UI:

    case LOCAL_GL_R16UI:
    case LOCAL_GL_RG16UI:
    case LOCAL_GL_RGB16UI:
    case LOCAL_GL_RGBA16UI:

    case LOCAL_GL_R32UI:
    case LOCAL_GL_RG32UI:
    case LOCAL_GL_RGB32UI:
    case LOCAL_GL_RGBA32UI:

    case LOCAL_GL_RGB10_A2UI:
        return LOCAL_GL_UNSIGNED_INT;

    default:
        MOZ_CRASH("Bad `internalFormat`.");
    }
}

// -------------------------------------------------------------------------
// Framebuffer objects

static bool
GetFBInfoForBlit(const WebGLFramebuffer* fb, WebGLContext* webgl,
                 const char* const fbInfo, GLsizei* const out_samples,
                 GLenum* const out_colorFormat, GLenum* const out_depthFormat,
                 GLenum* const out_stencilFormat)
{
    auto status = fb->PrecheckFramebufferStatus();
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        webgl->ErrorInvalidOperation("blitFramebuffer: %s is not"
                                     " framebuffer-complete.", fbInfo);
        return false;
    }

    *out_samples = 1; // TODO

    if (fb->ColorAttachment(0).IsDefined()) {
        const auto& attachement = fb->ColorAttachment(0);
        *out_colorFormat = attachement.EffectiveInternalFormat().get();
    } else {
        *out_colorFormat = 0;
    }

    if (fb->DepthStencilAttachment().IsDefined()) {
        const auto& attachement = fb->DepthStencilAttachment();
        *out_depthFormat = attachement.EffectiveInternalFormat().get();
        *out_stencilFormat = *out_depthFormat;
    } else {
        if (fb->DepthAttachment().IsDefined()) {
            const auto& attachement = fb->DepthAttachment();
            *out_depthFormat = attachement.EffectiveInternalFormat().get();
        } else {
            *out_depthFormat = 0;
        }

        if (fb->StencilAttachment().IsDefined()) {
            const auto& attachement = fb->StencilAttachment();
            *out_stencilFormat = attachement.EffectiveInternalFormat().get();
        } else {
            *out_stencilFormat = 0;
        }
    }
    return true;
}

void
WebGL2Context::BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                               GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                               GLbitfield mask, GLenum filter)
{
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

    const GLbitfield depthAndStencilBits = LOCAL_GL_DEPTH_BUFFER_BIT |
                                           LOCAL_GL_STENCIL_BUFFER_BIT;
    if (mask & depthAndStencilBits &&
        filter != LOCAL_GL_NEAREST)
    {
        ErrorInvalidOperation("blitFramebuffer: DEPTH_BUFFER_BIT and"
                              " STENCIL_BUFFER_BIT can only be used with"
                              " NEAREST filtering.");
        return;
    }

    if (mBoundReadFramebuffer == mBoundDrawFramebuffer) {
        // TODO: It's actually more complicated than this. We need to check that
        // the underlying buffers are not the same, not the framebuffers
        // themselves.
        ErrorInvalidOperation("blitFramebuffer: Source and destination must"
                              " differ.");
        return;
    }

    GLsizei srcSamples;
    GLenum srcColorFormat;
    GLenum srcDepthFormat;
    GLenum srcStencilFormat;

    if (mBoundReadFramebuffer) {
        if (!GetFBInfoForBlit(mBoundReadFramebuffer, this, "READ_FRAMEBUFFER",
                              &srcSamples, &srcColorFormat, &srcDepthFormat,
                              &srcStencilFormat))
        {
            return;
        }
    } else {
        srcSamples = 1; // Always 1.

        // TODO: Don't hardcode these.
        srcColorFormat = mOptions.alpha ? LOCAL_GL_RGBA8 : LOCAL_GL_RGB8;

        if (mOptions.depth && mOptions.stencil) {
            srcDepthFormat = LOCAL_GL_DEPTH24_STENCIL8;
            srcStencilFormat = srcDepthFormat;
        } else {
            if (mOptions.depth) {
                srcDepthFormat = LOCAL_GL_DEPTH_COMPONENT16;
            }
            if (mOptions.stencil) {
                srcStencilFormat = LOCAL_GL_STENCIL_INDEX8;
            }
        }
    }

    GLsizei dstSamples;
    GLenum dstColorFormat;
    GLenum dstDepthFormat;
    GLenum dstStencilFormat;

    if (mBoundDrawFramebuffer) {
        if (!GetFBInfoForBlit(mBoundDrawFramebuffer, this, "DRAW_FRAMEBUFFER",
                              &dstSamples, &dstColorFormat, &dstDepthFormat,
                              &dstStencilFormat))
        {
            return;
        }
    } else {
        dstSamples = gl->Screen()->Samples();

        // TODO: Don't hardcode these.
        dstColorFormat = mOptions.alpha ? LOCAL_GL_RGBA8 : LOCAL_GL_RGB8;

        if (mOptions.depth && mOptions.stencil) {
            dstDepthFormat = LOCAL_GL_DEPTH24_STENCIL8;
            dstStencilFormat = dstDepthFormat;
        } else {
            if (mOptions.depth) {
                dstDepthFormat = LOCAL_GL_DEPTH_COMPONENT16;
            }
            if (mOptions.stencil) {
                dstStencilFormat = LOCAL_GL_STENCIL_INDEX8;
            }
        }
    }


    if (mask & LOCAL_GL_COLOR_BUFFER_BIT) {
        const GLenum srcColorType = srcColorFormat ? ValueTypeForFormat(srcColorFormat)
                                                   : 0;
        const GLenum dstColorType = dstColorFormat ? ValueTypeForFormat(dstColorFormat)
                                                   : 0;
        if (dstColorType != srcColorType) {
            ErrorInvalidOperation("blitFramebuffer: Color buffer value type"
                                  " mismatch.");
            return;
        }

        const bool srcIsInt = srcColorType == LOCAL_GL_INT ||
                              srcColorType == LOCAL_GL_UNSIGNED_INT;
        if (srcIsInt && filter != LOCAL_GL_NEAREST) {
            ErrorInvalidOperation("blitFramebuffer: Integer read buffers can only"
                                  " be filtered with NEAREST.");
            return;
        }
    }

    /* GLES 3.0.4, p199:
     *   Calling BlitFramebuffer will result in an INVALID_OPERATION error if
     *   mask includes DEPTH_BUFFER_BIT or STENCIL_BUFFER_BIT, and the source
     *   and destination depth and stencil buffer formats do not match.
     *
     * jgilbert: The wording is such that if only DEPTH_BUFFER_BIT is specified,
     * the stencil formats must match. This seems wrong. It could be a spec bug,
     * or I could be missing an interaction in one of the earlier paragraphs.
     */
    if (mask & LOCAL_GL_DEPTH_BUFFER_BIT &&
        dstDepthFormat != srcDepthFormat)
    {
        ErrorInvalidOperation("blitFramebuffer: Depth buffer formats must match"
                              " if selected.");
        return;
    }

    if (mask & LOCAL_GL_STENCIL_BUFFER_BIT &&
        dstStencilFormat != srcStencilFormat)
    {
        ErrorInvalidOperation("blitFramebuffer: Stencil buffer formats must"
                              " match if selected.");
        return;
    }

    if (dstSamples != 1) {
        ErrorInvalidOperation("blitFramebuffer: DRAW_FRAMEBUFFER may not have"
                              " multiple samples.");
        return;
    }

    if (srcSamples != 1) {
        if (mask & LOCAL_GL_COLOR_BUFFER_BIT &&
            dstColorFormat != srcColorFormat)
        {
            ErrorInvalidOperation("blitFramebuffer: Color buffer formats must"
                                  " match if selected, when reading from a"
                                  " multisampled source.");
            return;
        }

        if (dstX0 != srcX0 ||
            dstX1 != srcX1 ||
            dstY0 != srcY0 ||
            dstY1 != srcY1)
        {
            ErrorInvalidOperation("blitFramebuffer: If the source is"
                                  " multisampled, then the source and dest"
                                  " regions must match exactly.");
            return;
        }
    }

    MakeContextCurrent();
    gl->fBlitFramebuffer(srcX0, srcY0, srcX1, srcY1,
                         dstX0, dstY0, dstX1, dstY1,
                         mask, filter);
}

void
WebGL2Context::FramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::GetInternalformatParameter(JSContext*, GLenum target, GLenum internalformat, GLenum pname, JS::MutableHandleValue retval)
{
    MOZ_CRASH("Not Implemented.");
}

// Map attachments intended for the default buffer, to attachments for a non-
// default buffer.
static void
TranslateDefaultAttachments(const dom::Sequence<GLenum>& in, dom::Sequence<GLenum>* out)
{
    for (size_t i = 0; i < in.Length(); i++) {
        switch (in[i]) {
            case LOCAL_GL_COLOR:
                out->AppendElement(LOCAL_GL_COLOR_ATTACHMENT0);
                break;
            case LOCAL_GL_DEPTH:
                out->AppendElement(LOCAL_GL_DEPTH_ATTACHMENT);
                break;
            case LOCAL_GL_STENCIL:
                out->AppendElement(LOCAL_GL_STENCIL_ATTACHMENT);
                break;
        }
    }
}

void
WebGL2Context::InvalidateFramebuffer(GLenum target, const dom::Sequence<GLenum>& attachments)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();

    if (!ValidateFramebufferTarget(target, "framebufferRenderbuffer"))
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
        MOZ_CRASH("Bad target.");
    }

    for (size_t i = 0; i < attachments.Length(); i++) {
        if (!ValidateFramebufferAttachment(fb, attachments[i],
                                           "invalidateFramebuffer"))
        {
            return;
        }
    }

    if (!fb && !isDefaultFB) {
        dom::Sequence<GLenum> tmpAttachments;
        TranslateDefaultAttachments(attachments, &tmpAttachments);
        gl->fInvalidateFramebuffer(target, tmpAttachments.Length(), tmpAttachments.Elements());
    } else {
        gl->fInvalidateFramebuffer(target, attachments.Length(), attachments.Elements());
    }
}

void
WebGL2Context::InvalidateSubFramebuffer(GLenum target, const dom::Sequence<GLenum>& attachments,
                                        GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();

    if (!ValidateFramebufferTarget(target, "framebufferRenderbuffer"))
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
        MOZ_CRASH("Bad target.");
    }

    for (size_t i = 0; i < attachments.Length(); i++) {
        if (!ValidateFramebufferAttachment(fb, attachments[i],
                                           "invalidateSubFramebuffer"))
        {
            return;
        }
    }

    if (!fb && !isDefaultFB) {
        dom::Sequence<GLenum> tmpAttachments;
        TranslateDefaultAttachments(attachments, &tmpAttachments);
        gl->fInvalidateSubFramebuffer(target, tmpAttachments.Length(), tmpAttachments.Elements(),
                                      x, y, width, height);
    } else {
        gl->fInvalidateSubFramebuffer(target, attachments.Length(), attachments.Elements(),
                                      x, y, width, height);
    }
}

void
WebGL2Context::ReadBuffer(GLenum mode)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();

    if (mBoundReadFramebuffer) {
        bool isColorAttachment = (mode >= LOCAL_GL_COLOR_ATTACHMENT0 &&
                                  mode <= LastColorAttachment());
        if (mode != LOCAL_GL_NONE &&
            !isColorAttachment)
        {
            ErrorInvalidEnumInfo("readBuffer: If READ_FRAMEBUFFER is non-null,"
                                 " `mode` must be COLOR_ATTACHMENTN or NONE."
                                 " Was:", mode);
            return;
        }

        gl->fReadBuffer(mode);
        return;
    }

    // Operating on the default framebuffer.

    if (mode != LOCAL_GL_NONE &&
        mode != LOCAL_GL_BACK)
    {
        ErrorInvalidEnumInfo("readBuffer: If READ_FRAMEBUFFER is null, `mode`"
                             " must be BACK or NONE. Was:", mode);
        return;
    }

    gl->Screen()->SetReadBuffer(mode);
}

void
WebGL2Context::RenderbufferStorageMultisample(GLenum target, GLsizei samples,
                                              GLenum internalFormat,
                                              GLsizei width, GLsizei height)
{
    RenderbufferStorage_base("renderbufferStorageMultisample", target, samples,
                              internalFormat, width, height);
}
