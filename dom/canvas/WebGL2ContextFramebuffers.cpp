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

static bool
GetFBInfoForBlit(const WebGLFramebuffer* fb, const char* const fbInfo,
                 GLsizei* const out_samples,
                 const webgl::FormatInfo** const out_colorFormat,
                 const webgl::FormatInfo** const out_depthFormat,
                 const webgl::FormatInfo** const out_stencilFormat)
{
    *out_samples = 1; // TODO
    *out_colorFormat = nullptr;
    *out_depthFormat = nullptr;
    *out_stencilFormat = nullptr;

    if (fb->ColorAttachment(0).IsDefined()) {
        const auto& attachment = fb->ColorAttachment(0);
        *out_colorFormat = attachment.Format()->format;
    }

    if (fb->DepthStencilAttachment().IsDefined()) {
        const auto& attachment = fb->DepthStencilAttachment();
        *out_depthFormat = attachment.Format()->format;
        *out_stencilFormat = *out_depthFormat;
    } else {
        if (fb->DepthAttachment().IsDefined()) {
            const auto& attachment = fb->DepthAttachment();
            *out_depthFormat = attachment.Format()->format;
        }

        if (fb->StencilAttachment().IsDefined()) {
            const auto& attachment = fb->StencilAttachment();
            *out_stencilFormat = attachment.Format()->format;
        }
    }
    return true;
}

static void
GetBackbufferFormats(const WebGLContextOptions& options,
                     const webgl::FormatInfo** const out_color,
                     const webgl::FormatInfo** const out_depth,
                     const webgl::FormatInfo** const out_stencil)
{
    const auto effFormat = options.alpha ? webgl::EffectiveFormat::RGBA8
                                          : webgl::EffectiveFormat::RGB8;
    *out_color = webgl::GetFormat(effFormat);

    *out_depth = nullptr;
    *out_stencil = nullptr;
    if (options.depth && options.stencil) {
        *out_depth = webgl::GetFormat(webgl::EffectiveFormat::DEPTH24_STENCIL8);
        *out_stencil = *out_depth;
    } else {
        if (options.depth) {
            *out_depth = webgl::GetFormat(webgl::EffectiveFormat::DEPTH_COMPONENT16);
        }
        if (options.stencil) {
            *out_stencil = webgl::GetFormat(webgl::EffectiveFormat::STENCIL_INDEX8);
        }
    }
}

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

    if (!mBoundReadFramebuffer->ValidateAndInitAttachments("blitFramebuffer's READ_FRAMEBUFFER") ||
        !mBoundDrawFramebuffer->ValidateAndInitAttachments("blitFramebuffer's DRAW_FRAMEBUFFER"))
    {
        return;
    }

    GLsizei srcSamples;
    const webgl::FormatInfo* srcColorFormat = nullptr;
    const webgl::FormatInfo* srcDepthFormat = nullptr;
    const webgl::FormatInfo* srcStencilFormat = nullptr;

    if (mBoundReadFramebuffer) {
        if (!GetFBInfoForBlit(mBoundReadFramebuffer, "READ_FRAMEBUFFER", &srcSamples,
                              &srcColorFormat, &srcDepthFormat, &srcStencilFormat))
        {
            return;
        }
    } else {
        srcSamples = 1; // Always 1.

        GetBackbufferFormats(mOptions, &srcColorFormat, &srcDepthFormat,
                             &srcStencilFormat);
    }

    GLsizei dstSamples;
    const webgl::FormatInfo* dstColorFormat = nullptr;
    const webgl::FormatInfo* dstDepthFormat = nullptr;
    const webgl::FormatInfo* dstStencilFormat = nullptr;

    if (mBoundDrawFramebuffer) {
        if (!GetFBInfoForBlit(mBoundDrawFramebuffer, "DRAW_FRAMEBUFFER", &dstSamples,
                              &dstColorFormat, &dstDepthFormat, &dstStencilFormat))
        {
            return;
        }
    } else {
        dstSamples = gl->Screen()->Samples();

        GetBackbufferFormats(mOptions, &dstColorFormat, &dstDepthFormat,
                             &dstStencilFormat);
    }

    if (mask & LOCAL_GL_COLOR_BUFFER_BIT) {
        const auto fnSignlessType = [](const webgl::FormatInfo* format)
                                    -> webgl::ComponentType
        {
            if (!format)
                return webgl::ComponentType::None;

            switch (format->componentType) {
            case webgl::ComponentType::UInt:
                return webgl::ComponentType::Int;

            case webgl::ComponentType::NormUInt:
                return webgl::ComponentType::NormInt;

            default:
                return format->componentType;
            }
        };

        const auto srcType = fnSignlessType(srcColorFormat);
        const auto dstType = fnSignlessType(dstColorFormat);

        if (srcType != dstType) {
            ErrorInvalidOperation("blitFramebuffer: Color buffer format component type"
                                  " mismatch.");
            return;
        }

        const bool srcIsInt = (srcType == webgl::ComponentType::Int);
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

static bool
ValidateTextureLayerAttachment(GLenum attachment)
{
    if (LOCAL_GL_COLOR_ATTACHMENT0 < attachment &&
        attachment <= LOCAL_GL_COLOR_ATTACHMENT15)
    {
        return true;
    }

    switch (attachment) {
    case LOCAL_GL_DEPTH_ATTACHMENT:
    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
    case LOCAL_GL_STENCIL_ATTACHMENT:
        return true;
    }

    return false;
}

void
WebGL2Context::FramebufferTextureLayer(GLenum target, GLenum attachment,
                                       WebGLTexture* texture, GLint level, GLint layer)
{
    if (IsContextLost())
        return;

    if (!ValidateFramebufferTarget(target, "framebufferTextureLayer"))
        return;

    if (!ValidateTextureLayerAttachment(attachment))
        return ErrorInvalidEnumInfo("framebufferTextureLayer: attachment:", attachment);

    if (texture) {
        if (texture->IsDeleted()) {
            return ErrorInvalidValue("framebufferTextureLayer: texture must be a valid "
                                     "texture object.");
        }

        if (level < 0)
            return ErrorInvalidValue("framebufferTextureLayer: layer must be >= 0.");

        switch (texture->Target().get()) {
        case LOCAL_GL_TEXTURE_3D:
            if (uint32_t(layer) >= mImplMax3DTextureSize) {
                return ErrorInvalidValue("framebufferTextureLayer: layer must be < "
                                         "MAX_3D_TEXTURE_SIZE");
            }
            break;

        case LOCAL_GL_TEXTURE_2D_ARRAY:
            if (uint32_t(layer) >= mImplMaxArrayTextureLayers) {
                return ErrorInvalidValue("framebufferTextureLayer: layer must be < "
                                         "MAX_ARRAY_TEXTURE_LAYERS");
            }
            break;

        default:
            return ErrorInvalidOperation("framebufferTextureLayer: texture must be an "
                                         "existing 3D texture, or a 2D texture array.");
        }
    }

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
        MOZ_CRASH("Bad target.");
    }

    if (!fb) {
        return ErrorInvalidOperation("framebufferTextureLayer: cannot modify"
                                     " framebuffer 0.");
    }

    fb->FramebufferTextureLayer(attachment, texture, level, layer);
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
        MOZ_CRASH("Bad target.");
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
        MOZ_CRASH("Bad target.");
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
    if (IsContextLost())
        return;

    const bool isColorAttachment = (mode >= LOCAL_GL_COLOR_ATTACHMENT0 &&
                                    mode <= LastColorAttachmentEnum());

    if (mode != LOCAL_GL_NONE && mode != LOCAL_GL_BACK && !isColorAttachment) {
        ErrorInvalidEnum("readBuffer: `mode` must be one of NONE, BACK, or "
                         "COLOR_ATTACHMENTi. Was %s",
                         EnumName(mode));
        return;
    }

    if (mBoundReadFramebuffer) {
        if (mode != LOCAL_GL_NONE &&
            !isColorAttachment)
        {
            ErrorInvalidOperation("readBuffer: If READ_FRAMEBUFFER is non-null, `mode` "
                                  "must be COLOR_ATTACHMENTi or NONE. Was %s",
                                  EnumName(mode));
            return;
        }

        MakeContextCurrent();
        gl->fReadBuffer(mode);
        return;
    }

    // Operating on the default framebuffer.
    if (mode != LOCAL_GL_NONE &&
        mode != LOCAL_GL_BACK)
    {
        ErrorInvalidOperation("readBuffer: If READ_FRAMEBUFFER is null, `mode`"
                              " must be BACK or NONE. Was %s",
                              EnumName(mode));
        return;
    }

    gl->Screen()->SetReadBuffer(mode);
}

} // namespace mozilla
