/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include <algorithm>
#include "angle/ShaderLang.h"
#include "CanvasUtils.h"
#include "GLContext.h"
#include "jsfriendapi.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "WebGLBuffer.h"
#include "WebGLContextUtils.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShader.h"
#include "WebGLTexture.h"
#include "WebGLUniformLocation.h"
#include "WebGLValidateStrings.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

#if defined(MOZ_WIDGET_COCOA)
#include "nsCocoaFeatures.h"
#endif

namespace mozilla {

/**
 * Return the block size for format.
 */
static void
BlockSizeFor(GLenum format, GLint* const out_blockWidth,
             GLint* const out_blockHeight)
{
    MOZ_ASSERT(out_blockWidth && out_blockHeight);

    switch (format) {
    case LOCAL_GL_ATC_RGB:
    case LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA:
    case LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA:
    case LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        *out_blockWidth = 4;
        *out_blockHeight = 4;
        break;

    case LOCAL_GL_ETC1_RGB8_OES:
        // 4x4 blocks, but no 4-multiple requirement.
        break;

    default:
        break;
    }
}

static bool
IsCompressedFunc(WebGLTexImageFunc func)
{
    return func == WebGLTexImageFunc::CompTexImage ||
           func == WebGLTexImageFunc::CompTexSubImage;
}

/**
 * Same as ErrorInvalidEnum but uses WebGLContext::EnumName to print displayable
 * name for \a glenum.
 */
static void
ErrorInvalidEnumWithName(WebGLContext* ctx, const char* msg, GLenum glenum,
                         WebGLTexImageFunc func, WebGLTexDimensions dims)
{
    const char* name = WebGLContext::EnumName(glenum);
    if (name) {
        ctx->ErrorInvalidEnum("%s: %s %s", InfoFrom(func, dims), msg, name);
    } else {
        ctx->ErrorInvalidEnum("%s: %s 0x%04x", InfoFrom(func, dims), msg,
                              glenum);
    }
}

/**
 * Return true if the format is valid for source calls.
 */
static bool
IsAllowedFromSource(GLenum format, WebGLTexImageFunc func)
{
    switch (format) {
    case LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1:
    case LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1:
    case LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1:
    case LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1:
        return (func == WebGLTexImageFunc::CompTexImage ||
                func == WebGLTexImageFunc::CompTexSubImage);

    case LOCAL_GL_ATC_RGB:
    case LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA:
    case LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA:
    case LOCAL_GL_ETC1_RGB8_OES:
        return func == WebGLTexImageFunc::CompTexImage;
    }

    return true;
}

/**
 * Returns true if func is a CopyTexImage variant.
 */
static bool
IsCopyFunc(WebGLTexImageFunc func)
{
    return (func == WebGLTexImageFunc::CopyTexImage ||
            func == WebGLTexImageFunc::CopyTexSubImage);
}

/**
 * Returns true if func is a SubImage variant.
 */
static bool
IsSubFunc(WebGLTexImageFunc func)
{
    return (func == WebGLTexImageFunc::TexSubImage ||
            func == WebGLTexImageFunc::CopyTexSubImage ||
            func == WebGLTexImageFunc::CompTexSubImage);
}

/**
 * returns true is target is a texture cube map target.
 */
static bool
IsTexImageCubemapTarget(GLenum texImageTarget)
{
    return (texImageTarget >= LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X &&
            texImageTarget <= LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
}

bool
WebGLContext::ValidateBlendEquationEnum(GLenum mode, const char* info)
{
    switch (mode) {
    case LOCAL_GL_FUNC_ADD:
    case LOCAL_GL_FUNC_SUBTRACT:
    case LOCAL_GL_FUNC_REVERSE_SUBTRACT:
        return true;

    case LOCAL_GL_MIN:
    case LOCAL_GL_MAX:
        if (IsExtensionEnabled(WebGLExtensionID::EXT_blend_minmax))
            return true;

        break;

    default:
        break;
    }

    ErrorInvalidEnumInfo(info, mode);
    return false;
}

bool
WebGLContext::ValidateBlendFuncDstEnum(GLenum factor, const char* info)
{
    switch (factor) {
    case LOCAL_GL_ZERO:
    case LOCAL_GL_ONE:
    case LOCAL_GL_SRC_COLOR:
    case LOCAL_GL_ONE_MINUS_SRC_COLOR:
    case LOCAL_GL_DST_COLOR:
    case LOCAL_GL_ONE_MINUS_DST_COLOR:
    case LOCAL_GL_SRC_ALPHA:
    case LOCAL_GL_ONE_MINUS_SRC_ALPHA:
    case LOCAL_GL_DST_ALPHA:
    case LOCAL_GL_ONE_MINUS_DST_ALPHA:
    case LOCAL_GL_CONSTANT_COLOR:
    case LOCAL_GL_ONE_MINUS_CONSTANT_COLOR:
    case LOCAL_GL_CONSTANT_ALPHA:
    case LOCAL_GL_ONE_MINUS_CONSTANT_ALPHA:
        return true;

    default:
        ErrorInvalidEnumInfo(info, factor);
        return false;
    }
}

bool
WebGLContext::ValidateBlendFuncSrcEnum(GLenum factor, const char* info)
{
    if (factor == LOCAL_GL_SRC_ALPHA_SATURATE)
        return true;

    return ValidateBlendFuncDstEnum(factor, info);
}

bool
WebGLContext::ValidateBlendFuncEnumsCompatibility(GLenum sfactor,
                                                  GLenum dfactor,
                                                  const char* info)
{
    bool sfactorIsConstantColor = sfactor == LOCAL_GL_CONSTANT_COLOR ||
                                  sfactor == LOCAL_GL_ONE_MINUS_CONSTANT_COLOR;
    bool sfactorIsConstantAlpha = sfactor == LOCAL_GL_CONSTANT_ALPHA ||
                                  sfactor == LOCAL_GL_ONE_MINUS_CONSTANT_ALPHA;
    bool dfactorIsConstantColor = dfactor == LOCAL_GL_CONSTANT_COLOR ||
                                  dfactor == LOCAL_GL_ONE_MINUS_CONSTANT_COLOR;
    bool dfactorIsConstantAlpha = dfactor == LOCAL_GL_CONSTANT_ALPHA ||
                                  dfactor == LOCAL_GL_ONE_MINUS_CONSTANT_ALPHA;
    if ( (sfactorIsConstantColor && dfactorIsConstantAlpha) ||
         (dfactorIsConstantColor && sfactorIsConstantAlpha) )
    {
        ErrorInvalidOperation("%s are mutually incompatible, see section 6.8 in"
                              " the WebGL 1.0 spec", info);
        return false;
    }

    return true;
}

bool
WebGLContext::ValidateDataOffsetSize(WebGLintptr offset, WebGLsizeiptr size, WebGLsizeiptr bufferSize, const char* info)
{
    if (offset < 0) {
        ErrorInvalidValue("%s: offset must be positive", info);
        return false;
    }

    if (size < 0) {
        ErrorInvalidValue("%s: size must be positive", info);
        return false;
    }

    // *** Careful *** WebGLsizeiptr is always 64-bits but GLsizeiptr
    // is like intptr_t. On some platforms it is 32-bits.
    CheckedInt<GLsizeiptr> neededBytes = CheckedInt<GLsizeiptr>(offset) + size;
    if (!neededBytes.isValid() || neededBytes.value() > bufferSize) {
        ErrorInvalidValue("%s: invalid range", info);
        return false;
    }

    return true;
}

/**
 * Check data ranges [readOffset, readOffset + size] and [writeOffset,
 * writeOffset + size] for overlap.
 *
 * It is assumed that offset and size have already been validated with
 * ValidateDataOffsetSize().
 */
bool
WebGLContext::ValidateDataRanges(WebGLintptr readOffset, WebGLintptr writeOffset, WebGLsizeiptr size, const char* info)
{
    MOZ_ASSERT((CheckedInt<WebGLsizeiptr>(readOffset) + size).isValid());
    MOZ_ASSERT((CheckedInt<WebGLsizeiptr>(writeOffset) + size).isValid());

    bool separate = (readOffset + size < writeOffset || writeOffset + size < readOffset);
    if (!separate)
        ErrorInvalidValue("%s: ranges [readOffset, readOffset + size) and [writeOffset, writeOffset + size) overlap");

    return separate;
}

bool
WebGLContext::ValidateTextureTargetEnum(GLenum target, const char* info)
{
    switch (target) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP:
        return true;

    case LOCAL_GL_TEXTURE_3D:
        if (IsWebGL2())
            return true;

        break;

    default:
        break;
    }

    ErrorInvalidEnumInfo(info, target);
    return false;
}

bool
WebGLContext::ValidateComparisonEnum(GLenum target, const char* info)
{
    switch (target) {
    case LOCAL_GL_NEVER:
    case LOCAL_GL_LESS:
    case LOCAL_GL_LEQUAL:
    case LOCAL_GL_GREATER:
    case LOCAL_GL_GEQUAL:
    case LOCAL_GL_EQUAL:
    case LOCAL_GL_NOTEQUAL:
    case LOCAL_GL_ALWAYS:
        return true;

    default:
        ErrorInvalidEnumInfo(info, target);
        return false;
    }
}

bool
WebGLContext::ValidateStencilOpEnum(GLenum action, const char* info)
{
    switch (action) {
    case LOCAL_GL_KEEP:
    case LOCAL_GL_ZERO:
    case LOCAL_GL_REPLACE:
    case LOCAL_GL_INCR:
    case LOCAL_GL_INCR_WRAP:
    case LOCAL_GL_DECR:
    case LOCAL_GL_DECR_WRAP:
    case LOCAL_GL_INVERT:
        return true;

    default:
        ErrorInvalidEnumInfo(info, action);
        return false;
    }
}

bool
WebGLContext::ValidateFaceEnum(GLenum face, const char* info)
{
    switch (face) {
    case LOCAL_GL_FRONT:
    case LOCAL_GL_BACK:
    case LOCAL_GL_FRONT_AND_BACK:
        return true;

    default:
        ErrorInvalidEnumInfo(info, face);
        return false;
    }
}

bool
WebGLContext::ValidateDrawModeEnum(GLenum mode, const char* info)
{
    switch (mode) {
    case LOCAL_GL_TRIANGLES:
    case LOCAL_GL_TRIANGLE_STRIP:
    case LOCAL_GL_TRIANGLE_FAN:
    case LOCAL_GL_POINTS:
    case LOCAL_GL_LINE_STRIP:
    case LOCAL_GL_LINE_LOOP:
    case LOCAL_GL_LINES:
        return true;

    default:
        ErrorInvalidEnumInfo(info, mode);
        return false;
    }
}

/**
 * Return true if the framebuffer attachment is valid. Attachment must
 * be one of depth/stencil/depth_stencil/color attachment.
 */
bool
WebGLContext::ValidateFramebufferAttachment(const WebGLFramebuffer* fb, GLenum attachment,
                                            const char* funcName)
{
    if (!fb) {
        switch (attachment) {
        case LOCAL_GL_COLOR:
        case LOCAL_GL_DEPTH:
        case LOCAL_GL_STENCIL:
            return true;

        default:
            ErrorInvalidEnum("%s: attachment: invalid enum value 0x%x.",
                             funcName, attachment);
            return false;
        }
    }

    if (attachment == LOCAL_GL_DEPTH_ATTACHMENT ||
        attachment == LOCAL_GL_STENCIL_ATTACHMENT ||
        attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
    {
        return true;
    }

    GLenum colorAttachCount = 1;
    if (IsExtensionEnabled(WebGLExtensionID::WEBGL_draw_buffers))
        colorAttachCount = mGLMaxColorAttachments;

    if (attachment >= LOCAL_GL_COLOR_ATTACHMENT0 &&
        attachment < GLenum(LOCAL_GL_COLOR_ATTACHMENT0 + colorAttachCount))
    {
        return true;
    }

    ErrorInvalidEnum("%s: attachment: invalid enum value 0x%x.", funcName,
                     attachment);
    return false;
}

/**
 * Return true if pname is valid for GetSamplerParameter calls.
 */
bool
WebGLContext::ValidateSamplerParameterName(GLenum pname, const char* info)
{
    switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
    case LOCAL_GL_TEXTURE_MAG_FILTER:
    case LOCAL_GL_TEXTURE_WRAP_S:
    case LOCAL_GL_TEXTURE_WRAP_T:
    case LOCAL_GL_TEXTURE_WRAP_R:
    case LOCAL_GL_TEXTURE_MIN_LOD:
    case LOCAL_GL_TEXTURE_MAX_LOD:
    case LOCAL_GL_TEXTURE_COMPARE_MODE:
    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
        return true;

    default:
        ErrorInvalidEnum("%s: invalid pname: %s", info, EnumName(pname));
        return false;
    }
}

/**
 * Return true if pname and param are valid combination for SamplerParameter calls.
 */
bool
WebGLContext::ValidateSamplerParameterParams(GLenum pname, const WebGLIntOrFloat& param, const char* info)
{
    const GLenum p = param.AsInt();

    switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
        switch (p) {
        case LOCAL_GL_NEAREST:
        case LOCAL_GL_LINEAR:
        case LOCAL_GL_NEAREST_MIPMAP_NEAREST:
        case LOCAL_GL_NEAREST_MIPMAP_LINEAR:
        case LOCAL_GL_LINEAR_MIPMAP_NEAREST:
        case LOCAL_GL_LINEAR_MIPMAP_LINEAR:
            return true;

        default:
            ErrorInvalidEnum("%s: invalid param: %s", info, EnumName(p));
            return false;
        }

    case LOCAL_GL_TEXTURE_MAG_FILTER:
        switch (p) {
        case LOCAL_GL_NEAREST:
        case LOCAL_GL_LINEAR:
            return true;

        default:
            ErrorInvalidEnum("%s: invalid param: %s", info, EnumName(p));
            return false;
        }

    case LOCAL_GL_TEXTURE_WRAP_S:
    case LOCAL_GL_TEXTURE_WRAP_T:
    case LOCAL_GL_TEXTURE_WRAP_R:
        switch (p) {
        case LOCAL_GL_CLAMP_TO_EDGE:
        case LOCAL_GL_REPEAT:
        case LOCAL_GL_MIRRORED_REPEAT:
            return true;

        default:
            ErrorInvalidEnum("%s: invalid param: %s", info, EnumName(p));
            return false;
        }

    case LOCAL_GL_TEXTURE_MIN_LOD:
    case LOCAL_GL_TEXTURE_MAX_LOD:
        return true;

    case LOCAL_GL_TEXTURE_COMPARE_MODE:
        switch (param.AsInt()) {
        case LOCAL_GL_NONE:
        case LOCAL_GL_COMPARE_REF_TO_TEXTURE:
            return true;

        default:
            ErrorInvalidEnum("%s: invalid param: %s", info, EnumName(p));
            return false;
        }

    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
        switch (p) {
        case LOCAL_GL_LEQUAL:
        case LOCAL_GL_GEQUAL:
        case LOCAL_GL_LESS:
        case LOCAL_GL_GREATER:
        case LOCAL_GL_EQUAL:
        case LOCAL_GL_NOTEQUAL:
        case LOCAL_GL_ALWAYS:
        case LOCAL_GL_NEVER:
            return true;

        default:
            ErrorInvalidEnum("%s: invalid param: %s", info, EnumName(p));
            return false;
        }

    default:
        ErrorInvalidEnum("%s: invalid pname: %s", info, EnumName(pname));
        return false;
    }
}


/**
 * Return true if format is a valid texture image format for source,
 * taking into account enabled WebGL extensions.
 */
bool
WebGLContext::ValidateTexImageFormat(GLenum format, WebGLTexImageFunc func,
                                     WebGLTexDimensions dims)
{
    /* Core WebGL texture formats */
    if (format == LOCAL_GL_ALPHA ||
        format == LOCAL_GL_RGB ||
        format == LOCAL_GL_RGBA ||
        format == LOCAL_GL_LUMINANCE ||
        format == LOCAL_GL_LUMINANCE_ALPHA)
    {
        return true;
    }

    /* WebGL2 new formats */
    if (format == LOCAL_GL_RED ||
        format == LOCAL_GL_RG ||
        format == LOCAL_GL_RED_INTEGER ||
        format == LOCAL_GL_RG_INTEGER ||
        format == LOCAL_GL_RGB_INTEGER ||
        format == LOCAL_GL_RGBA_INTEGER)
    {
        if (IsWebGL2())
            return true;

        ErrorInvalidEnum("%s: Invalid format %s: Requires WebGL version 2.0 or"
                         " newer.", InfoFrom(func, dims), EnumName(format));
        return false;
    }

    /* WEBGL_depth_texture added formats */
    if (format == LOCAL_GL_DEPTH_COMPONENT ||
        format == LOCAL_GL_DEPTH_STENCIL)
    {
        if (!IsExtensionEnabled(WebGLExtensionID::WEBGL_depth_texture)) {
            ErrorInvalidEnum("%s: Invalid format %s: Requires that"
                             " WEBGL_depth_texture is enabled.",
                             InfoFrom(func, dims), EnumName(format));
            return false;
        }

        // If WEBGL_depth_texture is enabled, then it is not allowed to be used
        // with the copyTexImage, or copyTexSubImage methods, and it is not
        // allowed with texSubImage in WebGL1.
        if ((func == WebGLTexImageFunc::TexSubImage && !IsWebGL2()) ||
            func == WebGLTexImageFunc::CopyTexImage ||
            func == WebGLTexImageFunc::CopyTexSubImage)
        {
            ErrorInvalidOperation("%s: format %s is not supported",
                                  InfoFrom(func, dims), EnumName(format));
            return false;
        }

        return true;
    }

    // Needs to be below the depth_texture check because an invalid operation
    // error needs to be generated instead of invalid enum.
    // Only core formats are valid for CopyTex[Sub]Image.
    // TODO: Revisit this once color_buffer_[half_]float lands.
    if (IsCopyFunc(func)) {
        ErrorInvalidEnumWithName(this, "invalid format", format, func, dims);
        return false;
    }

    // EXT_sRGB added formats
    if (format == LOCAL_GL_SRGB ||
        format == LOCAL_GL_SRGB_ALPHA)
    {
        if (IsExtensionEnabled(WebGLExtensionID::EXT_sRGB))
            return true;

        ErrorInvalidEnum("%s: Invalid format %s: Requires that EXT_sRGB is"
                         " enabled.", InfoFrom(func, dims),
                         WebGLContext::EnumName(format));
        return false;
    }

    ErrorInvalidEnumWithName(this, "invalid format", format, func, dims);
    return false;
}

/**
 * Check if the given texture target is valid for TexImage.
 */
bool
WebGLContext::ValidateTexImageTarget(GLenum target, WebGLTexImageFunc func,
                                     WebGLTexDimensions dims)
{
    switch (dims) {
    case WebGLTexDimensions::Tex2D:
        if (target == LOCAL_GL_TEXTURE_2D ||
            IsTexImageCubemapTarget(target))
        {
            return true;
        }

        ErrorInvalidEnumWithName(this, "invalid target", target, func, dims);
        return false;

    case WebGLTexDimensions::Tex3D:
        if (target == LOCAL_GL_TEXTURE_3D)
        {
            return true;
        }

        ErrorInvalidEnumWithName(this, "invalid target", target, func, dims);
        return false;

    default:
        MOZ_ASSERT(false, "ValidateTexImageTarget: Invalid dims");
    }

    return false;
}

/**
 * Return true if type is a valid texture image type for source,
 * taking into account enabled WebGL extensions.
 */
bool
WebGLContext::ValidateTexImageType(GLenum type, WebGLTexImageFunc func,
                                   WebGLTexDimensions dims)
{
    /* Core WebGL texture types */
    if (type == LOCAL_GL_UNSIGNED_BYTE ||
        type == LOCAL_GL_UNSIGNED_SHORT_5_6_5 ||
        type == LOCAL_GL_UNSIGNED_SHORT_4_4_4_4 ||
        type == LOCAL_GL_UNSIGNED_SHORT_5_5_5_1)
    {
        return true;
    }

    /* WebGL2 new types */
    if (type == LOCAL_GL_BYTE ||
        type == LOCAL_GL_SHORT ||
        type == LOCAL_GL_INT ||
        type == LOCAL_GL_FLOAT_32_UNSIGNED_INT_24_8_REV ||
        type == LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV ||
        type == LOCAL_GL_UNSIGNED_INT_10F_11F_11F_REV ||
        type == LOCAL_GL_UNSIGNED_INT_5_9_9_9_REV)
    {
        if (IsWebGL2())
            return true;

        ErrorInvalidEnum("%s: Invalid type %s: Requires WebGL version 2.0 or"
                         " newer.", InfoFrom(func, dims),
                         WebGLContext::EnumName(type));
        return false;
    }

    /* OES_texture_float added types */
    if (type == LOCAL_GL_FLOAT) {
        if (IsExtensionEnabled(WebGLExtensionID::OES_texture_float))
            return true;

        ErrorInvalidEnum("%s: Invalid type %s: Requires that OES_texture_float"
                         " is enabled.",
                         InfoFrom(func, dims), WebGLContext::EnumName(type));
        return false;
    }

    /* OES_texture_half_float add types */
    if (type == LOCAL_GL_HALF_FLOAT) {
        if (IsExtensionEnabled(WebGLExtensionID::OES_texture_half_float))
            return true;

        ErrorInvalidEnum("%s: Invalid type %s: Requires that"
                         " OES_texture_half_float is enabled.",
                         InfoFrom(func, dims), WebGLContext::EnumName(type));
        return false;
    }

    /* WEBGL_depth_texture added types */
    if (type == LOCAL_GL_UNSIGNED_SHORT ||
        type == LOCAL_GL_UNSIGNED_INT ||
        type == LOCAL_GL_UNSIGNED_INT_24_8)
    {
        if (IsExtensionEnabled(WebGLExtensionID::WEBGL_depth_texture))
            return true;

        ErrorInvalidEnum("%s: Invalid type %s: Requires that"
                         " WEBGL_depth_texture is enabled.",
                         InfoFrom(func, dims), WebGLContext::EnumName(type));
        return false;
    }

    ErrorInvalidEnumWithName(this, "invalid type", type, func, dims);
    return false;
}

/**
 * Validate texture image sizing extra constraints for
 * CompressedTex(Sub)?Image.
 */
// TODO: WebGL 2
bool
WebGLContext::ValidateCompTexImageSize(GLint level, GLenum format,
                                       GLint xoffset, GLint yoffset,
                                       GLsizei width, GLsizei height,
                                       GLsizei levelWidth, GLsizei levelHeight,
                                       WebGLTexImageFunc func,
                                       WebGLTexDimensions dims)
{
    // Negative parameters must already have been handled above
    MOZ_ASSERT(xoffset >= 0 && yoffset >= 0 &&
               width >= 0 && height >= 0);

    if (xoffset + width > (GLint) levelWidth) {
        ErrorInvalidValue("%s: xoffset + width must be <= levelWidth.",
                          InfoFrom(func, dims));
        return false;
    }

    if (yoffset + height > (GLint) levelHeight) {
        ErrorInvalidValue("%s: yoffset + height must be <= levelHeight.",
                          InfoFrom(func, dims));
        return false;
    }

    GLint blockWidth = 1;
    GLint blockHeight = 1;
    BlockSizeFor(format, &blockWidth, &blockHeight);

    // If blockWidth || blockHeight != 1, then the compressed format had
    // block-based constraints to be checked. (For example, PVRTC is compressed
    // but isn't a block-based format)
    if (blockWidth != 1 || blockHeight != 1) {
        // Offsets must be multiple of block size.
        if (xoffset % blockWidth != 0) {
            ErrorInvalidOperation("%s: xoffset must be multiple of %d.",
                                  InfoFrom(func, dims), blockWidth);
            return false;
        }

        if (yoffset % blockHeight != 0) {
            ErrorInvalidOperation("%s: yoffset must be multiple of %d.",
                                  InfoFrom(func, dims), blockHeight);
            return false;
        }

        /* The size must be a multiple of blockWidth and blockHeight, or must be
         * using offset+size that exactly hits the edge. Important for small
         * mipmap levels.
         *
         * From the WEBGL_compressed_texture_s3tc spec:
         *     When level equals zero width and height must be a multiple of 4.
         *     When level is greater than 0 width and height must be 0, 1, 2 or
         *     a multiple of 4. If they are not an INVALID_OPERATION error is
         *     generated."
         */
        if (level == 0) {
            if (width % blockWidth != 0) {
                ErrorInvalidOperation("%s: Width of level 0 must be a multiple"
                                      " of %d.", InfoFrom(func, dims),
                                      blockWidth);
                return false;
            }

            if (height % blockHeight != 0) {
                ErrorInvalidOperation("%s: Height of level 0 must be a multiple"
                                      " of %d.", InfoFrom(func, dims),
                                      blockHeight);
                return false;
            }
        } else if (level > 0) {
            if (width % blockWidth != 0 && width > 2) {
                ErrorInvalidOperation("%s: Width of level %d must be a multiple"
                                      " of %d, or be 0, 1, or 2.",
                                      InfoFrom(func, dims), level, blockWidth);
                return false;
            }

            if (height % blockHeight != 0 && height > 2) {
                ErrorInvalidOperation("%s: Height of level %d must be a"
                                      " multiple of %d, or be 0, 1, or 2.",
                                      InfoFrom(func, dims), level, blockHeight);
                return false;
            }
        }

        if (IsSubFunc(func)) {
            if ((xoffset % blockWidth) != 0) {
                ErrorInvalidOperation("%s: xoffset must be a multiple of %d.",
                                      InfoFrom(func, dims), blockWidth);
                return false;
            }

            if (yoffset % blockHeight != 0) {
                ErrorInvalidOperation("%s: yoffset must be a multiple of %d.",
                                      InfoFrom(func, dims), blockHeight);
                return false;
            }
        }
    }

    switch (format) {
    case LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1:
    case LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1:
    case LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1:
    case LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1:
        if (!IsPOTAssumingNonnegative(width) ||
            !IsPOTAssumingNonnegative(height))
        {
            ErrorInvalidValue("%s: Width and height must be powers of two.",
                              InfoFrom(func, dims));
            return false;
        }
    }

    return true;
}

/**
 * Return true if the enough data is present to satisfy compressed
 * texture format constraints.
 */
bool
WebGLContext::ValidateCompTexImageDataSize(GLint level, GLenum format,
                                           GLsizei width, GLsizei height,
                                           uint32_t byteLength,
                                           WebGLTexImageFunc func,
                                           WebGLTexDimensions dims)
{
    // negative width and height must already have been handled above
    MOZ_ASSERT(width >= 0 && height >= 0);

    CheckedUint32 required_byteLength = 0;

    switch (format) {
    case LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case LOCAL_GL_ATC_RGB:
    case LOCAL_GL_ETC1_RGB8_OES:
        required_byteLength = ((CheckedUint32(width) + 3) / 4) *
                              ((CheckedUint32(height) + 3) / 4) * 8;
        break;

    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA:
    case LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA:
        required_byteLength = ((CheckedUint32(width) + 3) / 4) *
                              ((CheckedUint32(height) + 3) / 4) * 16;
        break;

    case LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1:
    case LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1:
        required_byteLength = CheckedUint32(std::max(width, 8)) *
                              CheckedUint32(std::max(height, 8)) / 2;
        break;

    case LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1:
    case LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1:
        required_byteLength = CheckedUint32(std::max(width, 16)) *
                              CheckedUint32(std::max(height, 8)) / 4;
        break;
    }

    if (!required_byteLength.isValid() ||
        required_byteLength.value() != byteLength)
    {
        ErrorInvalidValue("%s: Data size does not match dimensions.",
                          InfoFrom(func, dims));
        return false;
    }

    return true;
}

/**
 * Validate the width, height, and depth of a texture image, \return
 * true is valid, false otherwise.
 * Used by all the (Compressed|Copy)?Tex(Sub)?Image functions.
 * Target and level must have been validated before calling.
 */
bool
WebGLContext::ValidateTexImageSize(TexImageTarget texImageTarget, GLint level,
                                   GLint width, GLint height, GLint depth,
                                   WebGLTexImageFunc func,
                                   WebGLTexDimensions dims)
{
    MOZ_ASSERT(level >= 0, "level should already be validated");

    /* Bug 966630: maxTextureSize >> level runs into "undefined"
     * behaviour depending on ISA. For example, on Intel shifts
     * amounts are mod 64 (in 64-bit mode on 64-bit dest) and mod 32
     * otherwise. This means 16384 >> 0x10000001 == 8192 which isn't
     * what would be expected. Make the required behaviour explicit by
     * clamping to a shift of 31 bits if level is greater than that
     * ammount. This will give 0 that if (!maxAllowedSize) is
     * expecting.
     */

    if (level > 31)
        level = 31;

    auto texTarget = TexImageTargetToTexTarget(texImageTarget);
    const GLuint maxTexImageSize = MaxTextureSizeForTarget(texTarget) >> level;

    const bool isCubemapTarget = IsTexImageCubemapTarget(texImageTarget.get());
    const bool isSub = IsSubFunc(func);

    if (!isSub && isCubemapTarget && (width != height)) {
        /* GL ES Version 2.0.25 - 3.7.1 Texture Image Specification
         *   "When the target parameter to TexImage2D is one of the
         *   six cube map two-dimensional image targets, the error
         *   INVALID_VALUE is generated if the width and height
         *   parameters are not equal."
         */
        ErrorInvalidValue("%s: For cube maps, width must equal height.",
                          InfoFrom(func, dims));
        return false;
    }

    if (texImageTarget == LOCAL_GL_TEXTURE_2D || isCubemapTarget) {
        /* GL ES Version 2.0.25 - 3.7.1 Texture Image Specification
         *   "If wt and ht are the specified image width and height,
         *   and if either wt or ht are less than zero, then the error
         *   INVALID_VALUE is generated."
         */
        if (width < 0) {
            ErrorInvalidValue("%s: Width must be >= 0.", InfoFrom(func, dims));
            return false;
        }

        if (height < 0) {
            ErrorInvalidValue("%s: Height must be >= 0.", InfoFrom(func, dims));
            return false;
        }

        /* GL ES Version 2.0.25 - 3.7.1 Texture Image Specification
         *   "The maximum allowable width and height of a
         *   two-dimensional texture image must be at least 2**(k−lod)
         *   for image arrays of level zero through k, where k is the
         *   log base 2 of MAX_TEXTURE_SIZE. and lod is the
         *   level-of-detail of the image array. It may be zero for
         *   image arrays of any level-of-detail greater than k. The
         *   error INVALID_VALUE is generated if the specified image
         *   is too large to be stored under any conditions.
         */
        if (width > (int) maxTexImageSize) {
            ErrorInvalidValue("%s: The maximum width for level %d is %u.",
                              InfoFrom(func, dims), level, maxTexImageSize);
            return false;
        }

        if (height > (int) maxTexImageSize) {
            ErrorInvalidValue("%s: The maximum height for level %d is %u.",
                              InfoFrom(func, dims), level, maxTexImageSize);
            return false;
        }

        /* GL ES Version 2.0.25 - 3.7.1 Texture Image Specification
         *   "If level is greater than zero, and either width or
         *   height is not a power-of-two, the error INVALID_VALUE is
         *   generated."
         *
         * This restriction does not apply to GL ES Version 3.0+.
         */
        if (!IsWebGL2() && level > 0) {
            if (!IsPOTAssumingNonnegative(width)) {
                ErrorInvalidValue("%s: For level > 0, width of %d must be a"
                                  " power of two.", InfoFrom(func, dims),
                                  width);
                return false;
            }

            if (!IsPOTAssumingNonnegative(height)) {
                ErrorInvalidValue("%s: For level > 0, height of %d must be a"
                                  " power of two.", InfoFrom(func, dims),
                                  height);
                return false;
            }
        }
    }

    // TODO: WebGL 2
    if (texImageTarget == LOCAL_GL_TEXTURE_3D) {
        if (depth < 0) {
            ErrorInvalidValue("%s: Depth must be >= 0.", InfoFrom(func, dims));
            return false;
        }

        if (!IsWebGL2() && !IsPOTAssumingNonnegative(depth)) {
            ErrorInvalidValue("%s: Depth of %d must be a power of two.",
                              InfoFrom(func, dims), depth);
            return false;
        }
    }

    return true;
}

/**
 * Validate texture image sizing for Tex(Sub)?Image variants.
 */
// TODO: WebGL 2. Update this to handle 3D textures.
bool
WebGLContext::ValidateTexSubImageSize(GLint xoffset, GLint yoffset,
                                      GLint /*zoffset*/, GLsizei width,
                                      GLsizei height, GLsizei /*depth*/,
                                      GLsizei baseWidth, GLsizei baseHeight,
                                      GLsizei /*baseDepth*/,
                                      WebGLTexImageFunc func,
                                      WebGLTexDimensions dims)
{
    /* GL ES Version 2.0.25 - 3.7.1 Texture Image Specification
     *   "Taking wt and ht to be the specified width and height of the
     *   texture array, and taking x, y, w, and h to be the xoffset,
     *   yoffset, width, and height argument values, any of the
     *   following relationships generates the error INVALID_VALUE:
     *       x < 0
     *       x + w > wt
     *       y < 0
     *       y + h > ht"
     */

    if (xoffset < 0) {
        ErrorInvalidValue("%s: xoffset must be >= 0.", InfoFrom(func, dims));
        return false;
    }

    if (yoffset < 0) {
        ErrorInvalidValue("%s: yoffset must be >= 0.", InfoFrom(func, dims));
        return false;
    }

    if (!CanvasUtils::CheckSaneSubrectSize(xoffset, yoffset, width, height,
                                           baseWidth, baseHeight))
    {
        ErrorInvalidValue("%s: Subtexture rectangle out-of-bounds.",
                          InfoFrom(func, dims));
        return false;
    }

    return true;
}

/**
 * Perform validation of format/type combinations for TexImage variants.
 * Returns true if the format/type is a valid combination, false otherwise.
 */
bool
WebGLContext::ValidateTexImageFormatAndType(GLenum format, GLenum type,
                                            WebGLTexImageFunc func,
                                            WebGLTexDimensions dims)
{
    if (IsCompressedFunc(func) || IsCopyFunc(func)) {
        MOZ_ASSERT(type == LOCAL_GL_NONE && format == LOCAL_GL_NONE);
        return true;
    }

    if (!ValidateTexImageFormat(format, func, dims) ||
        !ValidateTexImageType(type, func, dims))
    {
        return false;
    }

    // Here we're reinterpreting format as an unsized internalformat;
    // these are the same in practice and there's no point in having the
    // same code implemented twice.
    TexInternalFormat effective =
        EffectiveInternalFormatFromInternalFormatAndType(format, type);

    if (effective != LOCAL_GL_NONE)
        return true;

    ErrorInvalidOperation("%s: Invalid combination of format %s and type %s.",
                          InfoFrom(func, dims), WebGLContext::EnumName(format),
                          WebGLContext::EnumName(type));
    return false;
}

bool
WebGLContext::ValidateCompTexImageInternalFormat(GLenum format,
                                                 WebGLTexImageFunc func,
                                                 WebGLTexDimensions dims)
{
    if (!IsCompressedTextureFormat(format)) {
        ErrorInvalidEnum("%s: Invalid compressed texture format: %s",
                         InfoFrom(func, dims), WebGLContext::EnumName(format));
        return false;
    }

    /* WEBGL_compressed_texture_atc added formats */
    if (format == LOCAL_GL_ATC_RGB ||
        format == LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA ||
        format == LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA)
    {
        if (IsExtensionEnabled(WebGLExtensionID::WEBGL_compressed_texture_atc))
            return true;

        ErrorInvalidEnum("%s: Invalid format %s: Requires that"
                         " WEBGL_compressed_texture_atc is enabled.",
                         InfoFrom(func, dims), WebGLContext::EnumName(format));
        return false;
    }

    // WEBGL_compressed_texture_etc1
    if (format == LOCAL_GL_ETC1_RGB8_OES) {
        if (IsExtensionEnabled(WebGLExtensionID::WEBGL_compressed_texture_etc1))
            return true;

        ErrorInvalidEnum("%s: Invalid format %s: Requires that"
                         " WEBGL_compressed_texture_etc1 is enabled.",
                         InfoFrom(func, dims), WebGLContext::EnumName(format));
        return false;
    }


    if (format == LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1 ||
        format == LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1 ||
        format == LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1 ||
        format == LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1)
    {
        if (IsExtensionEnabled(WebGLExtensionID::WEBGL_compressed_texture_pvrtc))
            return true;

        ErrorInvalidEnum("%s: Invalid format %s: Requires that"
                         " WEBGL_compressed_texture_pvrtc is enabled.",
                         InfoFrom(func, dims), WebGLContext::EnumName(format));
        return false;
    }


    if (format == LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT ||
        format == LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ||
        format == LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT ||
        format == LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
    {
        if (IsExtensionEnabled(WebGLExtensionID::WEBGL_compressed_texture_s3tc))
            return true;

        ErrorInvalidEnum("%s: Invalid format %s: Requires that"
                         " WEBGL_compressed_texture_s3tc is enabled.",
                         InfoFrom(func, dims), WebGLContext::EnumName(format));
        return false;
    }

    MOZ_ASSERT(false);
    return false;
}

bool
WebGLContext::ValidateCopyTexImageInternalFormat(GLenum format,
                                                 WebGLTexImageFunc func,
                                                 WebGLTexDimensions dims)
{
    switch (format) {
    case LOCAL_GL_RGBA:
    case LOCAL_GL_RGB:
    case LOCAL_GL_LUMINANCE_ALPHA:
    case LOCAL_GL_LUMINANCE:
    case LOCAL_GL_ALPHA:
        return true;
    }
    // In CopyTexImage, internalFormat is a function parameter,
    // so a bad value is an INVALID_ENUM error.
    // In CopyTexSubImage, internalFormat is part of existing state,
    // so this is an INVALID_OPERATION error.
    GenerateWarning("%s: Invalid texture internal format: %s",
                    InfoFrom(func, dims), WebGLContext::EnumName(format));

    GLenum error;
    if (func == WebGLTexImageFunc::CopyTexImage)
        error = LOCAL_GL_INVALID_ENUM;
    else
        error = LOCAL_GL_INVALID_OPERATION;

    SynthesizeGLError(error);
    return false;
}
/**
 * Return true if format, type and jsArrayType are a valid combination.
 * Also returns the size for texel of format and type (in bytes) via
 * \a texelSize.
 *
 * It is assumed that type has previously been validated.
 */
bool
WebGLContext::ValidateTexInputData(GLenum type, js::Scalar::Type jsArrayType,
                                   WebGLTexImageFunc func,
                                   WebGLTexDimensions dims)
{
    // We're using js::Scalar::MaxTypedArrayViewType as dummy value for when
    // the tex source wasn't a typed array.
    if (jsArrayType == js::Scalar::MaxTypedArrayViewType)
        return true;

    const char invalidTypedArray[] = "%s: Invalid typed array type for given"
                                     " texture data type.";

    bool validInput = false;
    switch (type) {
    case LOCAL_GL_UNSIGNED_BYTE:
        validInput = jsArrayType == js::Scalar::Uint8;
        break;

    case LOCAL_GL_BYTE:
        validInput = jsArrayType == js::Scalar::Int8;
        break;

    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_UNSIGNED_SHORT:
    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
        validInput = jsArrayType == js::Scalar::Uint16;
        break;

    case LOCAL_GL_SHORT:
        validInput = jsArrayType == js::Scalar::Int16;
        break;

    case LOCAL_GL_UNSIGNED_INT:
    case LOCAL_GL_UNSIGNED_INT_24_8:
    case LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV:
    case LOCAL_GL_UNSIGNED_INT_10F_11F_11F_REV:
    case LOCAL_GL_UNSIGNED_INT_5_9_9_9_REV:
        validInput = jsArrayType == js::Scalar::Uint32;
        break;

    case LOCAL_GL_INT:
        validInput = jsArrayType == js::Scalar::Int32;
        break;

    case LOCAL_GL_FLOAT:
        validInput = jsArrayType == js::Scalar::Float32;
        break;

    default:
        break;
    }

    if (!validInput)
        ErrorInvalidOperation(invalidTypedArray, InfoFrom(func, dims));

    return validInput;
}

/**
 * Checks specific for the CopyTex[Sub]Image2D functions.
 * Verifies:
 * - Framebuffer is complete and has valid read planes
 * - Copy format is a subset of framebuffer format (i.e. all required components
 *   are available)
 */
bool
WebGLContext::ValidateCopyTexImage(GLenum format, WebGLTexImageFunc func,
                                   WebGLTexDimensions dims)
{
    MOZ_ASSERT(IsCopyFunc(func));

    // Default framebuffer format
    GLenum fboFormat = mOptions.alpha ? LOCAL_GL_RGBA : LOCAL_GL_RGB;

    if (mBoundReadFramebuffer) {
        TexInternalFormat srcFormat;
        if (!mBoundReadFramebuffer->ValidateForRead(InfoFrom(func, dims), &srcFormat))
            return false;

        fboFormat = srcFormat.get();
    }

    // Make sure the format of the framebuffer is a superset of the format
    // requested by the CopyTex[Sub]Image2D functions.
    const GLComponents formatComps = GLComponents(format);
    const GLComponents fboComps = GLComponents(fboFormat);
    if (!formatComps.IsSubsetOf(fboComps)) {
        ErrorInvalidOperation("%s: Format %s is not a subset of the current"
                              " framebuffer format, which is %s.",
                              InfoFrom(func, dims), EnumName(format),
                              EnumName(fboFormat));
        return false;
    }

    return true;
}

/**
 * Test the gl(Copy|Compressed)?Tex[Sub]?Image[23]() parameters for errors.
 * Verifies each of the parameters against the WebGL standard and enabled
 * extensions.
 */
// TODO: Texture dims is here for future expansion in WebGL 2.0
bool
WebGLContext::ValidateTexImage(TexImageTarget texImageTarget, GLint level,
                               GLenum internalFormat, GLint xoffset,
                               GLint yoffset, GLint zoffset, GLint width,
                               GLint height, GLint depth, GLint border,
                               GLenum format, GLenum type,
                               WebGLTexImageFunc func,
                               WebGLTexDimensions dims)
{
    const char* info = InfoFrom(func, dims);

    // Check level
    if (level < 0) {
        ErrorInvalidValue("%s: `level` must be >= 0.", info);
        return false;
    }

    // Check border
    if (border != 0) {
        ErrorInvalidValue("%s: `border` must be 0.", info);
        return false;
    }

    // Check incoming image format and type
    if (!ValidateTexImageFormatAndType(format, type, func, dims))
        return false;

    if (!TexInternalFormat::IsValueLegal(internalFormat)) {
        ErrorInvalidEnum("%s: Invalid `internalformat` enum %s.", info,
                         EnumName(internalFormat));
        return false;
    }
    TexInternalFormat unsizedInternalFormat =
        UnsizedInternalFormatFromInternalFormat(internalFormat);

    if (IsCompressedFunc(func)) {
        if (!ValidateCompTexImageInternalFormat(internalFormat, func, dims))
            return false;

    } else if (IsCopyFunc(func)) {
        if (!ValidateCopyTexImageInternalFormat(unsizedInternalFormat.get(),
                                                func, dims))
        {
            return false;
        }
    } else if (format != unsizedInternalFormat) {
        if (IsWebGL2()) {
            // In WebGL2, it's OK to have `internalFormat != format` if
            // internalFormat is the sized internal format corresponding to the
            // (format, type) pair according to Table 3.2 in the OpenGL ES 3.0.3
            // spec.
            auto effectiveFormat = EffectiveInternalFormatFromInternalFormatAndType(format,
                                                                                    type);
            if (internalFormat != effectiveFormat) {
                bool exceptionallyAllowed = false;
                if (internalFormat == LOCAL_GL_SRGB8_ALPHA8 &&
                    format == LOCAL_GL_RGBA &&
                    type == LOCAL_GL_UNSIGNED_BYTE)
                {
                    exceptionallyAllowed = true;
                }
                else if (internalFormat == LOCAL_GL_SRGB8 &&
                         format == LOCAL_GL_RGB &&
                         type == LOCAL_GL_UNSIGNED_BYTE)
                {
                    exceptionallyAllowed = true;
                }
                if (!exceptionallyAllowed) {
                    ErrorInvalidOperation("%s: `internalformat` does not match"
                                          " `format` and `type`.", info);
                    return false;
                }
            }
        } else {
            // In WebGL 1, format must be equal to internalformat.
            ErrorInvalidOperation("%s: `internalformat` does not match"
                                  " `format`.", info);
            return false;
        }
    }

    // Check texture image size
    if (!ValidateTexImageSize(texImageTarget, level, width, height, 0, func,
                              dims))
    {
        return false;
    }

    /* 5.14.8 Texture objects - WebGL Spec.
     *   "If an attempt is made to call these functions with no
     *    WebGLTexture bound (see above), an INVALID_OPERATION error
     *    is generated."
     */
    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);
    if (!tex) {
        ErrorInvalidOperation("%s: No texture is bound to target %s.", info,
                              WebGLContext::EnumName(texImageTarget.get()));
        return false;
    }

    if (IsSubFunc(func)) {
        if (!tex->HasImageInfoAt(texImageTarget, level)) {
            ErrorInvalidOperation("%s: No texture image previously defined for"
                                  " target %s at level %d.", info,
                                  WebGLContext::EnumName(texImageTarget.get()),
                                                         level);
            return false;
        }

        const auto& imageInfo = tex->ImageInfoAt(texImageTarget, level);
        if (!ValidateTexSubImageSize(xoffset, yoffset, zoffset, width, height,
                                     depth, imageInfo.Width(),
                                     imageInfo.Height(), 0, func, dims))
        {
            return false;
        }
    }

    // Additional checks for depth textures
    if (texImageTarget != LOCAL_GL_TEXTURE_2D &&
        (format == LOCAL_GL_DEPTH_COMPONENT ||
         format == LOCAL_GL_DEPTH_STENCIL))
    {
        ErrorInvalidOperation("%s: With format of %s, target must be"
                              " TEXTURE_2D.", info,
                              WebGLContext::EnumName(format));
        return false;
    }

    // Additional checks for compressed textures
    if (!IsAllowedFromSource(internalFormat, func)) {
        ErrorInvalidOperation("%s: Invalid format %s for this operation.",
                              info, WebGLContext::EnumName(format));
        return false;
    }

    // Parameters are OK
    return true;
}

bool
WebGLContext::ValidateUniformLocation(WebGLUniformLocation* loc, const char* funcName)
{
    /* GLES 2.0.25, p38:
     *   If the value of location is -1, the Uniform* commands will silently
     *   ignore the data passed in, and the current uniform values will not be
     *   changed.
     */
    if (!loc)
        return false;

    if (!ValidateObject(funcName, loc))
        return false;

    if (!mCurrentProgram) {
        ErrorInvalidOperation("%s: No program is currently bound.", funcName);
        return false;
    }

    return loc->ValidateForProgram(mCurrentProgram, this, funcName);
}

bool
WebGLContext::ValidateAttribArraySetter(const char* name, uint32_t setterElemSize,
                                        uint32_t arrayLength)
{
    if (IsContextLost())
        return false;

    if (arrayLength < setterElemSize) {
        ErrorInvalidOperation("%s: Array must have >= %d elements.", name,
                              setterElemSize);
        return false;
    }

    return true;
}

bool
WebGLContext::ValidateUniformSetter(WebGLUniformLocation* loc,
                                    uint8_t setterElemSize, GLenum setterType,
                                    const char* funcName, GLuint* out_rawLoc)
{
    if (IsContextLost())
        return false;

    if (!ValidateUniformLocation(loc, funcName))
        return false;

    if (!loc->ValidateSizeAndType(setterElemSize, setterType, this, funcName))
        return false;

    *out_rawLoc = loc->mLoc;
    return true;
}

bool
WebGLContext::ValidateUniformArraySetter(WebGLUniformLocation* loc,
                                         uint8_t setterElemSize,
                                         GLenum setterType,
                                         size_t setterArraySize,
                                         const char* funcName,
                                         GLuint* const out_rawLoc,
                                         GLsizei* const out_numElementsToUpload)
{
    if (IsContextLost())
        return false;

    if (!ValidateUniformLocation(loc, funcName))
        return false;

    if (!loc->ValidateSizeAndType(setterElemSize, setterType, this, funcName))
        return false;

    if (!loc->ValidateArrayLength(setterElemSize, setterArraySize, this, funcName))
        return false;

    *out_rawLoc = loc->mLoc;
    *out_numElementsToUpload = std::min((size_t)loc->mActiveInfo->mElemCount,
                                        setterArraySize / setterElemSize);
    return true;
}

bool
WebGLContext::ValidateUniformMatrixArraySetter(WebGLUniformLocation* loc,
                                               uint8_t setterDims,
                                               GLenum setterType,
                                               size_t setterArraySize,
                                               bool setterTranspose,
                                               const char* funcName,
                                               GLuint* const out_rawLoc,
                                               GLsizei* const out_numElementsToUpload)
{
    uint8_t setterElemSize = setterDims * setterDims;

    if (IsContextLost())
        return false;

    if (!ValidateUniformLocation(loc, funcName))
        return false;

    if (!loc->ValidateSizeAndType(setterElemSize, setterType, this, funcName))
        return false;

    if (!loc->ValidateArrayLength(setterElemSize, setterArraySize, this, funcName))
        return false;

    if (setterTranspose) {
        ErrorInvalidValue("%s: `transpose` must be false.", funcName);
        return false;
    }

    *out_rawLoc = loc->mLoc;
    *out_numElementsToUpload = std::min((size_t)loc->mActiveInfo->mElemCount,
                                        setterArraySize / setterElemSize);
    return true;
}

bool
WebGLContext::ValidateAttribIndex(GLuint index, const char* info)
{
    bool valid = (index < MaxVertexAttribs());

    if (!valid) {
        if (index == GLuint(-1)) {
            ErrorInvalidValue("%s: -1 is not a valid `index`. This value"
                              " probably comes from a getAttribLocation()"
                              " call, where this return value -1 means"
                              " that the passed name didn't correspond to"
                              " an active attribute in the specified"
                              " program.", info);
        } else {
            ErrorInvalidValue("%s: `index` must be less than"
                              " MAX_VERTEX_ATTRIBS.", info);
        }
    }

    return valid;
}

bool
WebGLContext::ValidateAttribPointer(bool integerMode, GLuint index, GLint size, GLenum type,
                                    WebGLboolean normalized, GLsizei stride,
                                    WebGLintptr byteOffset, const char* info)
{
    WebGLBuffer* buffer = mBoundArrayBuffer;
    if (!buffer) {
        ErrorInvalidOperation("%s: must have valid GL_ARRAY_BUFFER binding", info);
        return false;
    }

    GLsizei requiredAlignment = 0;
    if (!ValidateAttribPointerType(integerMode, type, &requiredAlignment, info))
        return false;

    // requiredAlignment should always be a power of two
    MOZ_ASSERT(IsPOTAssumingNonnegative(requiredAlignment));
    GLsizei requiredAlignmentMask = requiredAlignment - 1;

    if (size < 1 || size > 4) {
        ErrorInvalidValue("%s: invalid element size", info);
        return false;
    }

    // see WebGL spec section 6.6 "Vertex Attribute Data Stride"
    if (stride < 0 || stride > 255) {
        ErrorInvalidValue("%s: negative or too large stride", info);
        return false;
    }

    if (byteOffset < 0) {
        ErrorInvalidValue("%s: negative offset", info);
        return false;
    }

    if (stride & requiredAlignmentMask) {
        ErrorInvalidOperation("%s: stride doesn't satisfy the alignment "
                              "requirement of given type", info);
        return false;
    }

    if (byteOffset & requiredAlignmentMask) {
        ErrorInvalidOperation("%s: byteOffset doesn't satisfy the alignment "
                              "requirement of given type", info);
        return false;
    }

    return true;
}

bool
WebGLContext::ValidateStencilParamsForDrawCall()
{
    const char msg[] = "%s set different front and back stencil %s. Drawing in"
                       " this configuration is not allowed.";

    if (mStencilRefFront != mStencilRefBack) {
        ErrorInvalidOperation(msg, "stencilFuncSeparate", "reference values");
        return false;
    }

    if (mStencilValueMaskFront != mStencilValueMaskBack) {
        ErrorInvalidOperation(msg, "stencilFuncSeparate", "value masks");
        return false;
    }

    if (mStencilWriteMaskFront != mStencilWriteMaskBack) {
        ErrorInvalidOperation(msg, "stencilMaskSeparate", "write masks");
        return false;
    }

    return true;
}

static inline int32_t
FloorPOT(int32_t x)
{
    MOZ_ASSERT(x > 0);
    int32_t pot = 1;
    while (pot < 0x40000000) {
        if (x < pot*2)
            break;
        pot *= 2;
    }
    return pot;
}

bool
WebGLContext::InitAndValidateGL()
{
    if (!gl)
        return false;

    GLenum error = gl->fGetError();
    if (error != LOCAL_GL_NO_ERROR) {
        GenerateWarning("GL error 0x%x occurred during OpenGL context"
                        " initialization, before WebGL initialization!", error);
        return false;
    }

    mMinCapability = Preferences::GetBool("webgl.min_capability_mode", false);
    mDisableExtensions = Preferences::GetBool("webgl.disable-extensions", false);
    mLoseContextOnMemoryPressure = Preferences::GetBool("webgl.lose-context-on-memory-pressure", false);
    mCanLoseContextInForeground = Preferences::GetBool("webgl.can-lose-context-in-foreground", true);
    mRestoreWhenVisible = Preferences::GetBool("webgl.restore-context-when-visible", true);

    if (MinCapabilityMode())
        mDisableFragHighP = true;

    // These are the default values, see 6.2 State tables in the
    // OpenGL ES 2.0.25 spec.
    mColorWriteMask[0] = 1;
    mColorWriteMask[1] = 1;
    mColorWriteMask[2] = 1;
    mColorWriteMask[3] = 1;
    mDepthWriteMask = 1;
    mColorClearValue[0] = 0.f;
    mColorClearValue[1] = 0.f;
    mColorClearValue[2] = 0.f;
    mColorClearValue[3] = 0.f;
    mDepthClearValue = 1.f;
    mStencilClearValue = 0;
    mStencilRefFront = 0;
    mStencilRefBack = 0;

    /*
    // Technically, we should be setting mStencil[...] values to
    // `allOnes`, but either ANGLE breaks or the SGX540s on Try break.
    GLuint stencilBits = 0;
    gl->GetUIntegerv(LOCAL_GL_STENCIL_BITS, &stencilBits);
    GLuint allOnes = ~(UINT32_MAX << stencilBits);
    mStencilValueMaskFront = allOnes;
    mStencilValueMaskBack  = allOnes;
    mStencilWriteMaskFront = allOnes;
    mStencilWriteMaskBack  = allOnes;
    */

    gl->GetUIntegerv(LOCAL_GL_STENCIL_VALUE_MASK,      &mStencilValueMaskFront);
    gl->GetUIntegerv(LOCAL_GL_STENCIL_BACK_VALUE_MASK, &mStencilValueMaskBack);
    gl->GetUIntegerv(LOCAL_GL_STENCIL_WRITEMASK,       &mStencilWriteMaskFront);
    gl->GetUIntegerv(LOCAL_GL_STENCIL_BACK_WRITEMASK,  &mStencilWriteMaskBack);

    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_VALUE_MASK,      mStencilValueMaskFront);
    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_BACK_VALUE_MASK, mStencilValueMaskBack);
    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_WRITEMASK,       mStencilWriteMaskFront);
    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_BACK_WRITEMASK,  mStencilWriteMaskBack);

    mDitherEnabled = true;
    mRasterizerDiscardEnabled = false;
    mScissorTestEnabled = false;

    // Bindings, etc.
    mActiveTexture = 0;
    mEmitContextLostErrorOnce = true;
    mWebGLError = LOCAL_GL_NO_ERROR;
    mUnderlyingGLError = LOCAL_GL_NO_ERROR;

    mBound2DTextures.Clear();
    mBoundCubeMapTextures.Clear();
    mBound3DTextures.Clear();

    mBoundArrayBuffer = nullptr;
    mBoundTransformFeedbackBuffer = nullptr;
    mCurrentProgram = nullptr;

    mBoundDrawFramebuffer = nullptr;
    mBoundReadFramebuffer = nullptr;
    mBoundRenderbuffer = nullptr;

    MakeContextCurrent();

    // For OpenGL compat. profiles, we always keep vertex attrib 0 array enabled.
    if (gl->IsCompatibilityProfile())
        gl->fEnableVertexAttribArray(0);

    if (MinCapabilityMode())
        mGLMaxVertexAttribs = MINVALUE_GL_MAX_VERTEX_ATTRIBS;
    else
        gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_ATTRIBS, &mGLMaxVertexAttribs);

    if (mGLMaxVertexAttribs < 8) {
        GenerateWarning("GL_MAX_VERTEX_ATTRIBS: %d is < 8!",
                        mGLMaxVertexAttribs);
        return false;
    }

    // Note: GL_MAX_TEXTURE_UNITS is fixed at 4 for most desktop hardware,
    // even though the hardware supports much more.  The
    // GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS value is the accurate value.
    if (MinCapabilityMode())
        mGLMaxTextureUnits = MINVALUE_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;
    else
        gl->fGetIntegerv(LOCAL_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &mGLMaxTextureUnits);

    if (mGLMaxTextureUnits < 8) {
        GenerateWarning("GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: %d is < 8!",
                        mGLMaxTextureUnits);
        return false;
    }

    mBound2DTextures.SetLength(mGLMaxTextureUnits);
    mBoundCubeMapTextures.SetLength(mGLMaxTextureUnits);
    mBound3DTextures.SetLength(mGLMaxTextureUnits);

    if (MinCapabilityMode()) {
        mGLMaxTextureSize = MINVALUE_GL_MAX_TEXTURE_SIZE;
        mGLMaxCubeMapTextureSize = MINVALUE_GL_MAX_CUBE_MAP_TEXTURE_SIZE;
        mGLMaxRenderbufferSize = MINVALUE_GL_MAX_RENDERBUFFER_SIZE;
        mGLMaxTextureImageUnits = MINVALUE_GL_MAX_TEXTURE_IMAGE_UNITS;
        mGLMaxVertexTextureImageUnits = MINVALUE_GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS;
        mGLMaxSamples = 1;
    } else {
        gl->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &mGLMaxTextureSize);
        gl->fGetIntegerv(LOCAL_GL_MAX_CUBE_MAP_TEXTURE_SIZE, &mGLMaxCubeMapTextureSize);
        gl->fGetIntegerv(LOCAL_GL_MAX_RENDERBUFFER_SIZE, &mGLMaxRenderbufferSize);
        gl->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_IMAGE_UNITS, &mGLMaxTextureImageUnits);
        gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &mGLMaxVertexTextureImageUnits);

        if (!gl->GetPotentialInteger(LOCAL_GL_MAX_SAMPLES, (GLint*)&mGLMaxSamples))
            mGLMaxSamples = 1;
    }

    // Calculate log2 of mGLMaxTextureSize and mGLMaxCubeMapTextureSize
    mGLMaxTextureSizeLog2 = 0;
    int32_t tempSize = mGLMaxTextureSize;
    while (tempSize >>= 1) {
        ++mGLMaxTextureSizeLog2;
    }

    mGLMaxCubeMapTextureSizeLog2 = 0;
    tempSize = mGLMaxCubeMapTextureSize;
    while (tempSize >>= 1) {
        ++mGLMaxCubeMapTextureSizeLog2;
    }

    mGLMaxTextureSize = FloorPOT(mGLMaxTextureSize);
    mGLMaxRenderbufferSize = FloorPOT(mGLMaxRenderbufferSize);

    if (MinCapabilityMode()) {
        mGLMaxFragmentUniformVectors = MINVALUE_GL_MAX_FRAGMENT_UNIFORM_VECTORS;
        mGLMaxVertexUniformVectors = MINVALUE_GL_MAX_VERTEX_UNIFORM_VECTORS;
        mGLMaxVaryingVectors = MINVALUE_GL_MAX_VARYING_VECTORS;
    } else {
        if (gl->IsSupported(gl::GLFeature::ES2_compatibility)) {
            gl->fGetIntegerv(LOCAL_GL_MAX_FRAGMENT_UNIFORM_VECTORS, &mGLMaxFragmentUniformVectors);
            gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_UNIFORM_VECTORS, &mGLMaxVertexUniformVectors);
            gl->fGetIntegerv(LOCAL_GL_MAX_VARYING_VECTORS, &mGLMaxVaryingVectors);
        } else {
            gl->fGetIntegerv(LOCAL_GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &mGLMaxFragmentUniformVectors);
            mGLMaxFragmentUniformVectors /= 4;
            gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_UNIFORM_COMPONENTS, &mGLMaxVertexUniformVectors);
            mGLMaxVertexUniformVectors /= 4;

            /* We are now going to try to read GL_MAX_VERTEX_OUTPUT_COMPONENTS
             * and GL_MAX_FRAGMENT_INPUT_COMPONENTS, however these constants
             * only entered the OpenGL standard at OpenGL 3.2. So we will try
             * reading, and check OpenGL error for INVALID_ENUM.
             *
             * On the public_webgl list, "problematic GetParameter pnames"
             * thread, the following formula was given:
             *   maxVaryingVectors = min(GL_MAX_VERTEX_OUTPUT_COMPONENTS,
             *                           GL_MAX_FRAGMENT_INPUT_COMPONENTS) / 4
             */
            GLint maxVertexOutputComponents = 0;
            GLint maxFragmentInputComponents = 0;

            const bool ok = (gl->GetPotentialInteger(LOCAL_GL_MAX_VERTEX_OUTPUT_COMPONENTS,
                                                     &maxVertexOutputComponents) &&
                             gl->GetPotentialInteger(LOCAL_GL_MAX_FRAGMENT_INPUT_COMPONENTS,
                                                     &maxFragmentInputComponents));

            if (ok) {
                mGLMaxVaryingVectors = std::min(maxVertexOutputComponents,
                                                maxFragmentInputComponents) / 4;
            } else {
                mGLMaxVaryingVectors = 16;
                // 16 = 64/4, and 64 is the min value for
                // maxVertexOutputComponents in the OpenGL 3.2 spec.
            }
        }
    }

    // Always 1 for GLES2
    mMaxFramebufferColorAttachments = 1;

    if (gl->IsCompatibilityProfile()) {
        // gl_PointSize is always available in ES2 GLSL, but has to be
        // specifically enabled on desktop GLSL.
        gl->fEnable(LOCAL_GL_VERTEX_PROGRAM_POINT_SIZE);

        /* gl_PointCoord is always available in ES2 GLSL and in newer desktop
         * GLSL versions, but apparently not in OpenGL 2 and apparently not (due
         * to a driver bug) on certain NVIDIA setups. See:
         *   http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=261472
         *
         * Note that this used to cause crashes on old ATI drivers... Hopefully
         * not a significant anymore. See bug 602183.
         */
        gl->fEnable(LOCAL_GL_POINT_SPRITE);
    }

#ifdef XP_MACOSX
    if (gl->WorkAroundDriverBugs() &&
        gl->Vendor() == gl::GLVendor::ATI &&
        !nsCocoaFeatures::IsAtLeastVersion(10,9))
    {
        // The Mac ATI driver, in all known OSX version up to and including
        // 10.8, renders points sprites upside-down. (Apple bug 11778921)
        gl->fPointParameterf(LOCAL_GL_POINT_SPRITE_COORD_ORIGIN,
                             LOCAL_GL_LOWER_LEFT);
    }
#endif

    // Check the shader validator pref
    NS_ENSURE_TRUE(Preferences::GetRootBranch(), false);

    mBypassShaderValidation = Preferences::GetBool("webgl.bypass-shader-validation",
                                                   mBypassShaderValidation);

    // initialize shader translator
    if (!ShInitialize()) {
        GenerateWarning("GLSL translator initialization failed!");
        return false;
    }

    // Mesa can only be detected with the GL_VERSION string, of the form
    // "2.1 Mesa 7.11.0"
    const char* versionStr = (const char*)(gl->fGetString(LOCAL_GL_VERSION));
    mIsMesa = strstr(versionStr, "Mesa");

    // Notice that the point of calling fGetError here is not only to check for
    // errors, but also to reset the error flags so that a subsequent WebGL
    // getError call will give the correct result.
    error = gl->fGetError();
    if (error != LOCAL_GL_NO_ERROR) {
        GenerateWarning("GL error 0x%x occurred during WebGL context"
                        " initialization!", error);
        return false;
    }

    if (IsWebGL2() &&
        !InitWebGL2())
    {
        // Todo: Bug 898404: Only allow WebGL2 on GL>=3.0 on desktop GL.
        return false;
    }

    // Default value for all disabled vertex attributes is [0, 0, 0, 1]
    for (int32_t index = 0; index < mGLMaxVertexAttribs; ++index) {
        VertexAttrib4f(index, 0, 0, 0, 1);
    }

    mDefaultVertexArray = WebGLVertexArray::Create(this);
    mDefaultVertexArray->mAttribs.SetLength(mGLMaxVertexAttribs);
    mBoundVertexArray = mDefaultVertexArray;

    if (mLoseContextOnMemoryPressure)
        mContextObserver->RegisterMemoryPressureEvent();

    return true;
}

bool
WebGLContext::ValidateFramebufferTarget(GLenum target,
                                        const char* const info)
{
    bool isValid = true;
    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
        break;

    case LOCAL_GL_DRAW_FRAMEBUFFER:
    case LOCAL_GL_READ_FRAMEBUFFER:
        isValid = IsWebGL2();
        break;

    default:
        isValid = false;
        break;
    }

    if (MOZ_LIKELY(isValid)) {
        return true;
    }

    ErrorInvalidEnum("%s: Invalid target: %s (0x%04x).", info, EnumName(target),
                     target);
    return false;
}

} // namespace mozilla
