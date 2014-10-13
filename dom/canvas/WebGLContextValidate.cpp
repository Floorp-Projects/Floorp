/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLBuffer.h"
#include "WebGLVertexAttribData.h"
#include "WebGLShader.h"
#include "WebGLProgram.h"
#include "WebGLUniformLocation.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"
#include "WebGLVertexArray.h"
#include "GLContext.h"
#include "CanvasUtils.h"
#include "WebGLContextUtils.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"

#include "jsfriendapi.h"

#include "angle/ShaderLang.h"

#include <algorithm>

#include "mozilla/Services.h"
#include "nsIObserverService.h"

using namespace mozilla;

/**
 * Return the block size for format.
 */
static void
BlockSizeFor(GLenum format, GLint* blockWidth, GLint* blockHeight)
{
    MOZ_ASSERT(blockWidth && blockHeight);

    switch (format) {
    case LOCAL_GL_ATC_RGB:
    case LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA:
    case LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA:
    case LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        if (blockWidth)
            *blockWidth = 4;
        if (blockHeight)
            *blockHeight = 4;
        break;

    case LOCAL_GL_ETC1_RGB8_OES:
        // 4x4 blocks, but no 4-multiple requirement.
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
ErrorInvalidEnumWithName(WebGLContext* ctx, const char* msg, GLenum glenum, WebGLTexImageFunc func, WebGLTexDimensions dims)
{
    const char* name = WebGLContext::EnumName(glenum);
    if (name)
        ctx->ErrorInvalidEnum("%s: %s %s", InfoFrom(func, dims), msg, name);
    else
        ctx->ErrorInvalidEnum("%s: %s 0x%04X", InfoFrom(func, dims), msg, glenum);
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

bool WebGLContext::ValidateBlendEquationEnum(GLenum mode, const char *info)
{
    switch (mode) {
        case LOCAL_GL_FUNC_ADD:
        case LOCAL_GL_FUNC_SUBTRACT:
        case LOCAL_GL_FUNC_REVERSE_SUBTRACT:
            return true;
        case LOCAL_GL_MIN:
        case LOCAL_GL_MAX:
            if (IsExtensionEnabled(WebGLExtensionID::EXT_blend_minmax)) {
                return true;
            }
            break;
        default:
            break;
    }

    ErrorInvalidEnumInfo(info, mode);
    return false;
}

bool WebGLContext::ValidateBlendFuncDstEnum(GLenum factor, const char *info)
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

bool WebGLContext::ValidateBlendFuncSrcEnum(GLenum factor, const char *info)
{
    if (factor == LOCAL_GL_SRC_ALPHA_SATURATE)
        return true;
    else
        return ValidateBlendFuncDstEnum(factor, info);
}

bool WebGLContext::ValidateBlendFuncEnumsCompatibility(GLenum sfactor, GLenum dfactor, const char *info)
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
         (dfactorIsConstantColor && sfactorIsConstantAlpha) ) {
        ErrorInvalidOperation("%s are mutually incompatible, see section 6.8 in the WebGL 1.0 spec", info);
        return false;
    } else {
        return true;
    }
}

bool WebGLContext::ValidateTextureTargetEnum(GLenum target, const char *info)
{
    switch (target) {
        case LOCAL_GL_TEXTURE_2D:
        case LOCAL_GL_TEXTURE_CUBE_MAP:
            return true;
        default:
            ErrorInvalidEnumInfo(info, target);
            return false;
    }
}

bool WebGLContext::ValidateComparisonEnum(GLenum target, const char *info)
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

bool WebGLContext::ValidateStencilOpEnum(GLenum action, const char *info)
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

bool WebGLContext::ValidateFaceEnum(GLenum face, const char *info)
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

bool WebGLContext::ValidateDrawModeEnum(GLenum mode, const char *info)
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

bool WebGLContext::ValidateGLSLVariableName(const nsAString& name, const char *info)
{
    if (name.IsEmpty())
        return false;

    const uint32_t maxSize = 256;
    if (name.Length() > maxSize) {
        ErrorInvalidValue("%s: identifier is %d characters long, exceeds the maximum allowed length of %d characters",
                          info, name.Length(), maxSize);
        return false;
    }

    if (!ValidateGLSLString(name, info)) {
        return false;
    }

    nsString prefix1 = NS_LITERAL_STRING("webgl_");
    nsString prefix2 = NS_LITERAL_STRING("_webgl_");

    if (Substring(name, 0, prefix1.Length()).Equals(prefix1) ||
        Substring(name, 0, prefix2.Length()).Equals(prefix2))
    {
        ErrorInvalidOperation("%s: string contains a reserved GLSL prefix", info);
        return false;
    }

    return true;
}

bool WebGLContext::ValidateGLSLString(const nsAString& string, const char *info)
{
    for (uint32_t i = 0; i < string.Length(); ++i) {
        if (!ValidateGLSLCharacter(string.CharAt(i))) {
             ErrorInvalidValue("%s: string contains the illegal character '%d'", info, string.CharAt(i));
             return false;
        }
    }

    return true;
}

/**
 * Return true if the framebuffer attachment is valid. Attachment must
 * be one of depth/stencil/depth_stencil/color attachment.
 */
bool
WebGLContext::ValidateFramebufferAttachment(GLenum attachment, const char* funcName)
{
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

    ErrorInvalidEnum("%s: attachment: invalid enum value 0x%x.", funcName, attachment);
    return false;
}

/**
 * Return true if format is a valid texture image format for source,
 * taking into account enabled WebGL extensions.
 */
bool
WebGLContext::ValidateTexImageFormat(GLenum format, WebGLTexImageFunc func, WebGLTexDimensions dims)
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
        bool valid = IsWebGL2();
        if (!valid) {
            ErrorInvalidEnum("%s:  invalid format %s: requires WebGL version 2.0 or newer",
                             InfoFrom(func, dims), EnumName(format));
        }
        return valid;
    }

    /* WEBGL_depth_texture added formats */
    if (format == LOCAL_GL_DEPTH_COMPONENT ||
        format == LOCAL_GL_DEPTH_STENCIL)
    {
        if (!IsExtensionEnabled(WebGLExtensionID::WEBGL_depth_texture)) {
            ErrorInvalidEnum("%s: invalid format %s: need WEBGL_depth_texture enabled",
                             InfoFrom(func, dims), EnumName(format));
            return false;
        }

        // If WEBGL_depth_texture is enabled, then it is not allowed to be used with the
        // copyTexImage, or copyTexSubImage methods, and it is not allowed with
        // texSubImage in WebGL1.
        if ((func == WebGLTexImageFunc::TexSubImage && !IsWebGL2()) ||
            func == WebGLTexImageFunc::CopyTexImage ||
            func == WebGLTexImageFunc::CopyTexSubImage)
        {
            ErrorInvalidOperation("%s: format %s is not supported", InfoFrom(func, dims), EnumName(format));
            return false;
        }

        return true;
    }

    // Needs to be below the depth_texture check because an invalid operation
    // error needs to be generated instead of invalid enum.
    /* Only core formats are valid for CopyTex(Sub)?Image */
    // TODO: Revisit this once color_buffer_(half_)?float lands
    if (IsCopyFunc(func)) {
        ErrorInvalidEnumWithName(this, "invalid format", format, func, dims);
        return false;
    }

    /* EXT_sRGB added formats */
    if (format == LOCAL_GL_SRGB ||
        format == LOCAL_GL_SRGB_ALPHA)
    {
        bool validFormat = IsExtensionEnabled(WebGLExtensionID::EXT_sRGB);
        if (!validFormat)
            ErrorInvalidEnum("%s: invalid format %s: need EXT_sRGB enabled",
                             InfoFrom(func, dims), WebGLContext::EnumName(format));
        return validFormat;
    }

    ErrorInvalidEnumWithName(this, "invalid format", format, func, dims);

    return false;
}

/**
 * Check if the given texture target is valid for TexImage.
 */
bool
WebGLContext::ValidateTexImageTarget(GLenum target,
                                     WebGLTexImageFunc func, WebGLTexDimensions dims)
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
WebGLContext::ValidateTexImageType(GLenum type,
                                   WebGLTexImageFunc func,
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
        bool validType = IsWebGL2();
        if (!validType) {
            ErrorInvalidEnum("%s: invalid type %s: requires WebGL version 2.0 or newer",
                             InfoFrom(func, dims), WebGLContext::EnumName(type));
        }
        return validType;
    }

    /* OES_texture_float added types */
    if (type == LOCAL_GL_FLOAT) {
        bool validType = IsExtensionEnabled(WebGLExtensionID::OES_texture_float);
        if (!validType)
            ErrorInvalidEnum("%s: invalid type %s: need OES_texture_float enabled",
                             InfoFrom(func, dims), WebGLContext::EnumName(type));
        return validType;
    }

    /* OES_texture_half_float add types */
    if (type == LOCAL_GL_HALF_FLOAT) {
        bool validType = IsExtensionEnabled(WebGLExtensionID::OES_texture_half_float);
        if (!validType)
            ErrorInvalidEnum("%s: invalid type %s: need OES_texture_half_float enabled",
                             InfoFrom(func, dims), WebGLContext::EnumName(type));
        return validType;
    }

    /* WEBGL_depth_texture added types */
    if (type == LOCAL_GL_UNSIGNED_SHORT ||
        type == LOCAL_GL_UNSIGNED_INT ||
        type == LOCAL_GL_UNSIGNED_INT_24_8)
    {
        bool validType = IsExtensionEnabled(WebGLExtensionID::WEBGL_depth_texture);
        if (!validType)
            ErrorInvalidEnum("%s: invalid type %s: need WEBGL_depth_texture enabled",
                             InfoFrom(func, dims), WebGLContext::EnumName(type));
        return validType;
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
WebGLContext::ValidateCompTexImageSize(GLint level,
                                       GLenum format,
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
        ErrorInvalidValue("%s: xoffset + width must be <= levelWidth", InfoFrom(func, dims));
        return false;
    }

    if (yoffset + height > (GLint) levelHeight) {
        ErrorInvalidValue("%s: yoffset + height must be <= levelHeight", InfoFrom(func, dims));
        return false;
    }

    GLint blockWidth = 1;
    GLint blockHeight = 1;
    BlockSizeFor(format, &blockWidth, &blockHeight);

    /* If blockWidth || blockHeight != 1, then the compressed format
     * had block-based constraints to be checked. (For example, PVRTC is compressed but
     * isn't a block-based format)
     */
    if (blockWidth != 1 || blockHeight != 1) {
        /* offsets must be multiple of block size */
        if (xoffset % blockWidth != 0) {
            ErrorInvalidOperation("%s: xoffset must be multiple of %d",
                                  InfoFrom(func, dims), blockWidth);
            return false;
        }

        if (yoffset % blockHeight != 0) {
            ErrorInvalidOperation("%s: yoffset must be multiple of %d",
                                  InfoFrom(func, dims), blockHeight);
            return false;
        }

        /* The size must be a multiple of blockWidth and blockHeight,
         * or must be using offset+size that exactly hits the edge.
         * Important for small mipmap levels.
         */
        /* https://www.khronos.org/registry/webgl/extensions/WEBGL_compressed_texture_s3tc/
         * "When level equals zero width and height must be a multiple of 4. When
         *  level is greater than 0 width and height must be 0, 1, 2 or a multiple of 4.
         *  If they are not an INVALID_OPERATION error is generated."
         */
        if (level == 0) {
            if (width % blockWidth != 0) {
                ErrorInvalidOperation("%s: width of level 0 must be multple of %d",
                                      InfoFrom(func, dims), blockWidth);
                return false;
            }

            if (height % blockHeight != 0) {
                ErrorInvalidOperation("%s: height of level 0 must be multipel of %d",
                                      InfoFrom(func, dims), blockHeight);
                return false;
            }
        }
        else if (level > 0) {
            if (width % blockWidth != 0 && width > 2) {
                ErrorInvalidOperation("%s: width of level %d must be multiple"
                                      " of %d or 0, 1, 2",
                                      InfoFrom(func, dims), level, blockWidth);
                return false;
            }

            if (height % blockHeight != 0 && height > 2) {
                ErrorInvalidOperation("%s: height of level %d must be multiple"
                                      " of %d or 0, 1, 2",
                                      InfoFrom(func, dims), level, blockHeight);
                return false;
            }
        }

        if (IsSubFunc(func)) {
            if ((xoffset % blockWidth) != 0) {
                ErrorInvalidOperation("%s: xoffset must be multiple of %d",
                                      InfoFrom(func, dims), blockWidth);
                return false;
            }

            if (yoffset % blockHeight != 0) {
                ErrorInvalidOperation("%s: yoffset must be multiple of %d",
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
        if (!is_pot_assuming_nonnegative(width) ||
            !is_pot_assuming_nonnegative(height))
        {
            ErrorInvalidValue("%s: width and height must be powers of two",
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
        {
            required_byteLength = ((CheckedUint32(width) + 3) / 4) * ((CheckedUint32(height) + 3) / 4) * 8;
            break;
        }
        case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA:
        case LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA:
        {
            required_byteLength = ((CheckedUint32(width) + 3) / 4) * ((CheckedUint32(height) + 3) / 4) * 16;
            break;
        }
        case LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1:
        case LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1:
        {
            required_byteLength = CheckedUint32(std::max(width, 8)) * CheckedUint32(std::max(height, 8)) / 2;
            break;
        }
        case LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1:
        case LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1:
        {
            required_byteLength = CheckedUint32(std::max(width, 16)) * CheckedUint32(std::max(height, 8)) / 4;
            break;
        }
    }

    if (!required_byteLength.isValid() || required_byteLength.value() != byteLength) {
        ErrorInvalidValue("%s: data size does not match dimensions", InfoFrom(func, dims));
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
                                   WebGLTexImageFunc func, WebGLTexDimensions dims)
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

    const GLuint maxTexImageSize = MaxTextureSizeForTarget(TexImageTargetToTexTarget(texImageTarget)) >> level;
    const bool isCubemapTarget = IsTexImageCubemapTarget(texImageTarget.get());
    const bool isSub = IsSubFunc(func);

    if (!isSub && isCubemapTarget && (width != height)) {
        /* GL ES Version 2.0.25 - 3.7.1 Texture Image Specification
         *   "When the target parameter to TexImage2D is one of the
         *   six cube map two-dimensional image targets, the error
         *   INVALID_VALUE is generated if the width and height
         *   parameters are not equal."
         */
        ErrorInvalidValue("%s: for cube map, width must equal height", InfoFrom(func, dims));
        return false;
    }

    if (texImageTarget == LOCAL_GL_TEXTURE_2D || isCubemapTarget)
    {
        /* GL ES Version 2.0.25 - 3.7.1 Texture Image Specification
         *   "If wt and ht are the specified image width and height,
         *   and if either wt or ht are less than zero, then the error
         *   INVALID_VALUE is generated."
         */
        if (width < 0) {
            ErrorInvalidValue("%s: width must be >= 0", InfoFrom(func, dims));
            return false;
        }

        if (height < 0) {
            ErrorInvalidValue("%s: height must be >= 0", InfoFrom(func, dims));
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
            ErrorInvalidValue("%s: the maximum width for level %d is %u",
                              InfoFrom(func, dims), level, maxTexImageSize);
            return false;
        }

        if (height > (int) maxTexImageSize) {
            ErrorInvalidValue("%s: tex maximum height for level %d is %u",
                              InfoFrom(func, dims), level, maxTexImageSize);
            return false;
        }

        /* GL ES Version 2.0.25 - 3.7.1 Texture Image Specification
         *   "If level is greater than zero, and either width or
         *   height is not a power-of-two, the error INVALID_VALUE is
         *   generated."
         */
        if (level > 0) {
            if (!is_pot_assuming_nonnegative(width)) {
                ErrorInvalidValue("%s: level >= 0, width of %d must be a power of two.",
                                  InfoFrom(func, dims), width);
                return false;
            }

            if (!is_pot_assuming_nonnegative(height)) {
                ErrorInvalidValue("%s: level >= 0, height of %d must be a power of two.",
                                  InfoFrom(func, dims), height);
                return false;
            }
        }
    }

    // TODO: WebGL 2
    if (texImageTarget == LOCAL_GL_TEXTURE_3D) {
        if (depth < 0) {
            ErrorInvalidValue("%s: depth must be >= 0", InfoFrom(func, dims));
            return false;
        }

        if (!is_pot_assuming_nonnegative(depth)) {
            ErrorInvalidValue("%s: level >= 0, depth of %d must be a power of two.",
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
WebGLContext::ValidateTexSubImageSize(GLint xoffset, GLint yoffset, GLint /*zoffset*/,
                                      GLsizei width, GLsizei height, GLsizei /*depth*/,
                                      GLsizei baseWidth, GLsizei baseHeight, GLsizei /*baseDepth*/,
                                      WebGLTexImageFunc func, WebGLTexDimensions dims)
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
        ErrorInvalidValue("%s: xoffset must be >= 0", InfoFrom(func, dims));
        return false;
    }

    if (yoffset < 0) {
        ErrorInvalidValue("%s: yoffset must be >= 0", InfoFrom(func, dims));
        return false;
    }

    if (!CanvasUtils::CheckSaneSubrectSize(xoffset, yoffset, width, height, baseWidth, baseHeight)) {
        ErrorInvalidValue("%s: subtexture rectangle out-of-bounds", InfoFrom(func, dims));
        return false;
    }

    return true;
}

/**
 * Perform validation of format/type combinations for TexImage variants.
 * Returns true if the format/type is a valid combination, false otherwise.
 */
bool
WebGLContext::ValidateTexImageFormatAndType(GLenum format,
                                            GLenum type,
                                            WebGLTexImageFunc func,
                                            WebGLTexDimensions dims)
{
    if (IsCompressedFunc(func) || IsCopyFunc(func))
    {
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
    bool validCombo = effective != LOCAL_GL_NONE;

    if (!validCombo)
        ErrorInvalidOperation("%s: invalid combination of format %s and type %s",
                              InfoFrom(func, dims), WebGLContext::EnumName(format), WebGLContext::EnumName(type));

    return validCombo;
}

bool
WebGLContext::ValidateCompTexImageInternalFormat(GLenum format,
                                                 WebGLTexImageFunc func,
                                                 WebGLTexDimensions dims)
{
    if (!IsCompressedTextureFormat(format)) {
        ErrorInvalidEnum("%s: invalid compressed texture format: %s",
                         InfoFrom(func, dims), WebGLContext::EnumName(format));
        return false;
    }

    /* WEBGL_compressed_texture_atc added formats */
    if (format == LOCAL_GL_ATC_RGB ||
        format == LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA ||
        format == LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA)
    {
        bool validFormat = IsExtensionEnabled(WebGLExtensionID::WEBGL_compressed_texture_atc);
        if (!validFormat)
            ErrorInvalidEnum("%s: invalid format %s: need WEBGL_compressed_texture_atc enabled",
                             InfoFrom(func, dims), WebGLContext::EnumName(format));
        return validFormat;
    }

    // WEBGL_compressed_texture_etc1
    if (format == LOCAL_GL_ETC1_RGB8_OES) {
        bool validFormat = IsExtensionEnabled(WebGLExtensionID::WEBGL_compressed_texture_etc1);
        if (!validFormat)
            ErrorInvalidEnum("%s: invalid format %s: need WEBGL_compressed_texture_etc1 enabled",
                             InfoFrom(func, dims), WebGLContext::EnumName(format));
        return validFormat;
    }


    if (format == LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1 ||
        format == LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1 ||
        format == LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1 ||
        format == LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1)
    {
        bool validFormat = IsExtensionEnabled(WebGLExtensionID::WEBGL_compressed_texture_pvrtc);
        if (!validFormat)
            ErrorInvalidEnum("%s: invalid format %s: need WEBGL_compressed_texture_pvrtc enabled",
                             InfoFrom(func, dims), WebGLContext::EnumName(format));
        return validFormat;
    }


    if (format == LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT ||
        format == LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ||
        format == LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT ||
        format == LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
    {
        bool validFormat = IsExtensionEnabled(WebGLExtensionID::WEBGL_compressed_texture_s3tc);
        if (!validFormat)
            ErrorInvalidEnum("%s: invalid format %s: need WEBGL_compressed_texture_s3tc enabled",
                             InfoFrom(func, dims), WebGLContext::EnumName(format));
        return validFormat;
    }

    return false;
}

bool
WebGLContext::ValidateCopyTexImageInternalFormat(GLenum format,
                                                 WebGLTexImageFunc func,
                                                 WebGLTexDimensions dims)
{
    bool valid = format == LOCAL_GL_RGBA ||
                 format == LOCAL_GL_RGB ||
                 format == LOCAL_GL_LUMINANCE_ALPHA ||
                 format == LOCAL_GL_LUMINANCE ||
                 format == LOCAL_GL_ALPHA;
    if (!valid)
    {
        // in CopyTexImage, the internalformat is a function parameter,
        // so a bad value is an INVALID_ENUM error.
        // in CopyTexSubImage, the internalformat is part of existing state,
        // so this is an INVALID_OPERATION error.
        GenerateWarning("%s: invalid texture internal format: %s",
                        InfoFrom(func, dims), WebGLContext::EnumName(format));
        SynthesizeGLError(func == WebGLTexImageFunc::CopyTexImage
                          ? LOCAL_GL_INVALID_ENUM
                          : LOCAL_GL_INVALID_OPERATION);
    }
    return valid;
}
/**
 * Return true if format, type and jsArrayType are a valid combination.
 * Also returns the size for texel of format and type (in bytes) via
 * \a texelSize.
 *
 * It is assumed that type has previously been validated.
 */
bool
WebGLContext::ValidateTexInputData(GLenum type,
                                   js::Scalar::Type jsArrayType,
                                   WebGLTexImageFunc func,
                                   WebGLTexDimensions dims)
{
    bool validInput = false;
    const char invalidTypedArray[] = "%s: invalid typed array type for given texture data type";

    // We're using js::Scalar::TypeMax as dummy value when the tex source wasn't a
    // typed array.
    if (jsArrayType == js::Scalar::TypeMax) {
        return true;
    }

    // First, we check for packed types
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
 * - Copy format is a subset of framebuffer format (i.e. all required components are available)
 */
bool
WebGLContext::ValidateCopyTexImage(GLenum format,
                                   WebGLTexImageFunc func,
                                   WebGLTexDimensions dims)
{
    MOZ_ASSERT(IsCopyFunc(func));

    // Default framebuffer format
    GLenum fboFormat = bool(gl->GetPixelFormat().alpha > 0) ? LOCAL_GL_RGBA : LOCAL_GL_RGB;

    if (mBoundFramebuffer) {
        if (!mBoundFramebuffer->CheckAndInitializeAttachments()) {
            ErrorInvalidFramebufferOperation("%s: incomplete framebuffer", InfoFrom(func, dims));
            return false;
        }

        GLenum readPlaneBits = LOCAL_GL_COLOR_BUFFER_BIT;
        if (!mBoundFramebuffer->HasCompletePlanes(readPlaneBits)) {
            ErrorInvalidOperation("%s: Read source attachment doesn't have the"
                                  " correct color/depth/stencil type.", InfoFrom(func, dims));
            return false;
        }

        // Get the correct format for the framebuffer, as it's not the default one
        const WebGLFramebuffer::Attachment& color0 = mBoundFramebuffer->GetAttachment(LOCAL_GL_COLOR_ATTACHMENT0);
        fboFormat = mBoundFramebuffer->GetFormatForAttachment(color0);
    }

    // Make sure the format of the framebuffer is a superset of
    // the format requested by the CopyTex[Sub]Image2D functions.
    const GLComponents formatComps = GLComponents(format);
    const GLComponents fboComps = GLComponents(fboFormat);
    if (!formatComps.IsSubsetOf(fboComps)) {
        ErrorInvalidOperation("%s: format %s is not a subset of the current framebuffer format, which is %s.",
                              InfoFrom(func, dims), EnumName(format), EnumName(fboFormat));
        return false;
    }

    return true;
}

/**
 * Test the gl(Copy|Compressed)?Tex[Sub]?Image[23]() parameters for errors.
 * Verifies each of the parameters against the WebGL standard and enabled extensions.
 */
// TODO: Texture dims is here for future expansion in WebGL 2.0
bool
WebGLContext::ValidateTexImage(TexImageTarget texImageTarget,
                               GLint level,
                               GLenum internalFormat,
                               GLint xoffset, GLint yoffset, GLint zoffset,
                               GLint width, GLint height, GLint depth,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               WebGLTexImageFunc func,
                               WebGLTexDimensions dims)
{
    const char* info = InfoFrom(func, dims);

    /* Check level */
    if (level < 0) {
        ErrorInvalidValue("%s: level must be >= 0", info);
        return false;
    }

    /* Check border */
    if (border != 0) {
        ErrorInvalidValue("%s: border must be 0", info);
        return false;
    }

    /* Check incoming image format and type */
    if (!ValidateTexImageFormatAndType(format, type, func, dims))
        return false;

    if (!TexInternalFormat::IsValueLegal(internalFormat)) {
        ErrorInvalidEnum("%s: invalid internalformat enum %s", info, EnumName(internalFormat));
        return false;
    }
    TexInternalFormat unsizedInternalFormat =
        UnsizedInternalFormatFromInternalFormat(internalFormat);

    if (IsCompressedFunc(func)) {
        if (!ValidateCompTexImageInternalFormat(internalFormat, func, dims)) {
            return false;
        }
    } else if (IsCopyFunc(func)) {
        if (!ValidateCopyTexImageInternalFormat(unsizedInternalFormat.get(), func, dims)) {
            return false;
        }
    } else if (format != unsizedInternalFormat) {
        if (IsWebGL2()) {
            // In WebGL2, it's OK to have internalformat != format if internalformat is the sized
            // internal format corresponding to the (format, type) pair according to Table 3.2
            // in the OpenGL ES 3.0.3 spec.
            if (internalFormat != EffectiveInternalFormatFromInternalFormatAndType(format, type)) {
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
                    ErrorInvalidOperation("%s: internalformat does not match format and type", info);
                    return false;
                }
            }
        } else {
            // in WebGL 1, format must be equal to internalformat
            ErrorInvalidOperation("%s: internalformat does not match format", info);
            return false;
        }
    }

    /* Check texture image size */
    if (!ValidateTexImageSize(texImageTarget, level, width, height, 0, func, dims))
        return false;

    /* 5.14.8 Texture objects - WebGL Spec.
     *   "If an attempt is made to call these functions with no
     *    WebGLTexture bound (see above), an INVALID_OPERATION error
     *    is generated."
     */
    WebGLTexture* tex = activeBoundTextureForTexImageTarget(texImageTarget);
    if (!tex) {
        ErrorInvalidOperation("%s: no texture is bound to target %s",
                              info, WebGLContext::EnumName(texImageTarget.get()));
        return false;
    }

    if (IsSubFunc(func)) {
        if (!tex->HasImageInfoAt(texImageTarget, level)) {
            ErrorInvalidOperation("%s: no texture image previously defined for target %s at level %d",
                                  info, WebGLContext::EnumName(texImageTarget.get()), level);
            return false;
        }

        const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(texImageTarget, level);

        if (!ValidateTexSubImageSize(xoffset, yoffset, zoffset,
                                     width, height, depth,
                                     imageInfo.Width(), imageInfo.Height(), 0,
                                     func, dims))
        {
            return false;
        }
    }

    /* Additional checks for depth textures */
    if (texImageTarget != LOCAL_GL_TEXTURE_2D &&
        (format == LOCAL_GL_DEPTH_COMPONENT ||
         format == LOCAL_GL_DEPTH_STENCIL))
    {
        ErrorInvalidOperation("%s: with format of %s target must be TEXTURE_2D",
                              info, WebGLContext::EnumName(format));
        return false;
    }

    /* Additional checks for compressed textures */
    if (!IsAllowedFromSource(internalFormat, func)) {
        ErrorInvalidOperation("%s: Invalid format %s for this operation",
                              info, WebGLContext::EnumName(format));
        return false;
    }

    /* Parameters are OK */
    return true;
}

bool
WebGLContext::ValidateUniformLocation(const char* info, WebGLUniformLocation *location_object)
{
    if (!ValidateObjectAllowNull(info, location_object))
        return false;
    if (!location_object)
        return false;
    /* the need to check specifically for !mCurrentProgram here is explained in bug 657556 */
    if (!mCurrentProgram) {
        ErrorInvalidOperation("%s: no program is currently bound", info);
        return false;
    }
    if (mCurrentProgram != location_object->Program()) {
        ErrorInvalidOperation("%s: this uniform location doesn't correspond to the current program", info);
        return false;
    }
    if (mCurrentProgram->Generation() != location_object->ProgramGeneration()) {
        ErrorInvalidOperation("%s: This uniform location is obsolete since the program has been relinked", info);
        return false;
    }
    return true;
}

bool
WebGLContext::ValidateSamplerUniformSetter(const char* info, WebGLUniformLocation *location, GLint value)
{
    if (location->Info().type != LOCAL_GL_SAMPLER_2D &&
        location->Info().type != LOCAL_GL_SAMPLER_CUBE)
    {
        return true;
    }

    if (value >= 0 && value < mGLMaxTextureUnits)
        return true;

    ErrorInvalidValue("%s: this uniform location is a sampler, but %d is not a valid texture unit",
                      info, value);
    return false;
}

bool
WebGLContext::ValidateAttribArraySetter(const char* name, uint32_t cnt, uint32_t arrayLength)
{
    if (IsContextLost()) {
        return false;
    }
    if (arrayLength < cnt) {
        ErrorInvalidOperation("%s: array must be >= %d elements", name, cnt);
        return false;
    }
    return true;
}

bool
WebGLContext::ValidateUniformArraySetter(const char* name, uint32_t expectedElemSize, WebGLUniformLocation *location_object,
                                         GLint& location, uint32_t& numElementsToUpload, uint32_t arrayLength)
{
    if (IsContextLost())
        return false;
    if (!ValidateUniformLocation(name, location_object))
        return false;
    location = location_object->Location();
    uint32_t uniformElemSize = location_object->ElementSize();
    if (expectedElemSize != uniformElemSize) {
        ErrorInvalidOperation("%s: this function expected a uniform of element size %d,"
                              " got a uniform of element size %d", name,
                              expectedElemSize,
                              uniformElemSize);
        return false;
    }
    if (arrayLength == 0 ||
        arrayLength % expectedElemSize)
    {
        ErrorInvalidValue("%s: expected an array of length a multiple"
                          " of %d, got an array of length %d", name,
                          expectedElemSize,
                          arrayLength);
        return false;
    }
    const WebGLUniformInfo& info = location_object->Info();
    if (!info.isArray &&
        arrayLength != expectedElemSize) {
        ErrorInvalidOperation("%s: expected an array of length exactly"
                              " %d (since this uniform is not an array"
                              " uniform), got an array of length %d", name,
                              expectedElemSize,
                              arrayLength);
        return false;
    }
    numElementsToUpload =
        std::min(info.arraySize, arrayLength / expectedElemSize);
    return true;
}

bool
WebGLContext::ValidateUniformMatrixArraySetter(const char* name, int dim, WebGLUniformLocation *location_object,
                                              GLint& location, uint32_t& numElementsToUpload, uint32_t arrayLength,
                                              WebGLboolean aTranspose)
{
    uint32_t expectedElemSize = (dim)*(dim);
    if (IsContextLost())
        return false;
    if (!ValidateUniformLocation(name, location_object))
        return false;
    location = location_object->Location();
    uint32_t uniformElemSize = location_object->ElementSize();
    if (expectedElemSize != uniformElemSize) {
        ErrorInvalidOperation("%s: this function expected a uniform of element size %d,"
                              " got a uniform of element size %d", name,
                              expectedElemSize,
                              uniformElemSize);
        return false;
    }
    if (arrayLength == 0 ||
        arrayLength % expectedElemSize)
    {
        ErrorInvalidValue("%s: expected an array of length a multiple"
                          " of %d, got an array of length %d", name,
                          expectedElemSize,
                          arrayLength);
        return false;
    }
    const WebGLUniformInfo& info = location_object->Info();
    if (!info.isArray &&
        arrayLength != expectedElemSize) {
        ErrorInvalidOperation("%s: expected an array of length exactly"
                              " %d (since this uniform is not an array"
                              " uniform), got an array of length %d", name,
                              expectedElemSize,
                              arrayLength);
        return false;
    }
    if (aTranspose) {
        ErrorInvalidValue("%s: transpose must be FALSE as per the "
                          "OpenGL ES 2.0 spec", name);
        return false;
    }
    numElementsToUpload =
        std::min(info.arraySize, arrayLength / (expectedElemSize));
    return true;
}

bool
WebGLContext::ValidateUniformSetter(const char* name, WebGLUniformLocation *location_object, GLint& location)
{
    if (IsContextLost())
        return false;
    if (!ValidateUniformLocation(name, location_object))
        return false;
    location = location_object->Location();
    return true;
}

bool WebGLContext::ValidateAttribIndex(GLuint index, const char *info)
{
    return mBoundVertexArray->EnsureAttrib(index, info);
}

bool WebGLContext::ValidateStencilParamsForDrawCall()
{
  const char *msg = "%s set different front and back stencil %s. Drawing in this configuration is not allowed.";
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

static inline int32_t floorPOT(int32_t x)
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
    if (!gl) return false;

    GLenum error = gl->fGetError();
    if (error != LOCAL_GL_NO_ERROR) {
        GenerateWarning("GL error 0x%x occurred during OpenGL context initialization, before WebGL initialization!", error);
        return false;
    }

    mMinCapability = Preferences::GetBool("webgl.min_capability_mode", false);
    mDisableExtensions = Preferences::GetBool("webgl.disable-extensions", false);
    mLoseContextOnMemoryPressure = Preferences::GetBool("webgl.lose-context-on-memory-preasure", false);
    mCanLoseContextInForeground = Preferences::GetBool("webgl.can-lose-context-in-foreground", true);
    mRestoreWhenVisible = Preferences::GetBool("webgl.restore-context-when-visible", true);

    if (MinCapabilityMode()) {
      mDisableFragHighP = true;
    }

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

    mBoundArrayBuffer = nullptr;
    mBoundTransformFeedbackBuffer = nullptr;
    mCurrentProgram = nullptr;

    mBoundFramebuffer = nullptr;
    mBoundRenderbuffer = nullptr;

    MakeContextCurrent();

    // on desktop OpenGL, we always keep vertex attrib 0 array enabled
    if (!gl->IsGLES()) {
        gl->fEnableVertexAttribArray(0);
    }

    if (MinCapabilityMode()) {
        mGLMaxVertexAttribs = MINVALUE_GL_MAX_VERTEX_ATTRIBS;
    } else {
        gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_ATTRIBS, &mGLMaxVertexAttribs);
    }
    if (mGLMaxVertexAttribs < 8) {
        GenerateWarning("GL_MAX_VERTEX_ATTRIBS: %d is < 8!", mGLMaxVertexAttribs);
        return false;
    }

    // Note: GL_MAX_TEXTURE_UNITS is fixed at 4 for most desktop hardware,
    // even though the hardware supports much more.  The
    // GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS value is the accurate value.
    if (MinCapabilityMode()) {
        mGLMaxTextureUnits = MINVALUE_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;
    } else {
        gl->fGetIntegerv(LOCAL_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &mGLMaxTextureUnits);
    }
    if (mGLMaxTextureUnits < 8) {
        GenerateWarning("GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: %d is < 8!", mGLMaxTextureUnits);
        return false;
    }

    mBound2DTextures.SetLength(mGLMaxTextureUnits);
    mBoundCubeMapTextures.SetLength(mGLMaxTextureUnits);

    if (MinCapabilityMode()) {
        mGLMaxTextureSize = MINVALUE_GL_MAX_TEXTURE_SIZE;
        mGLMaxCubeMapTextureSize = MINVALUE_GL_MAX_CUBE_MAP_TEXTURE_SIZE;
        mGLMaxRenderbufferSize = MINVALUE_GL_MAX_RENDERBUFFER_SIZE;
        mGLMaxTextureImageUnits = MINVALUE_GL_MAX_TEXTURE_IMAGE_UNITS;
        mGLMaxVertexTextureImageUnits = MINVALUE_GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS;
    } else {
        gl->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &mGLMaxTextureSize);
        gl->fGetIntegerv(LOCAL_GL_MAX_CUBE_MAP_TEXTURE_SIZE, &mGLMaxCubeMapTextureSize);
        gl->fGetIntegerv(LOCAL_GL_MAX_RENDERBUFFER_SIZE, &mGLMaxRenderbufferSize);
        gl->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_IMAGE_UNITS, &mGLMaxTextureImageUnits);
        gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &mGLMaxVertexTextureImageUnits);
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

    mGLMaxTextureSize = floorPOT(mGLMaxTextureSize);
    mGLMaxRenderbufferSize = floorPOT(mGLMaxRenderbufferSize);

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

            // we are now going to try to read GL_MAX_VERTEX_OUTPUT_COMPONENTS and GL_MAX_FRAGMENT_INPUT_COMPONENTS,
            // however these constants only entered the OpenGL standard at OpenGL 3.2. So we will try reading,
            // and check OpenGL error for INVALID_ENUM.

            // before we start, we check that no error already occurred, to prevent hiding it in our subsequent error handling
            error = gl->GetAndClearError();
            if (error != LOCAL_GL_NO_ERROR) {
                GenerateWarning("GL error 0x%x occurred during WebGL context initialization!", error);
                return false;
            }

            // On the public_webgl list, "problematic GetParameter pnames" thread, the following formula was given:
            //   mGLMaxVaryingVectors = min (GL_MAX_VERTEX_OUTPUT_COMPONENTS, GL_MAX_FRAGMENT_INPUT_COMPONENTS) / 4
            GLint maxVertexOutputComponents,
                  minFragmentInputComponents;
            gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_OUTPUT_COMPONENTS, &maxVertexOutputComponents);
            gl->fGetIntegerv(LOCAL_GL_MAX_FRAGMENT_INPUT_COMPONENTS, &minFragmentInputComponents);

            error = gl->GetAndClearError();
            switch (error) {
                case LOCAL_GL_NO_ERROR:
                    mGLMaxVaryingVectors = std::min(maxVertexOutputComponents, minFragmentInputComponents) / 4;
                    break;
                case LOCAL_GL_INVALID_ENUM:
                    mGLMaxVaryingVectors = 16; // = 64/4, 64 is the min value for maxVertexOutputComponents in OpenGL 3.2 spec
                    break;
                default:
                    GenerateWarning("GL error 0x%x occurred during WebGL context initialization!", error);
                    return false;
            }
        }
    }

    // Always 1 for GLES2
    mMaxFramebufferColorAttachments = 1;

    if (!gl->IsGLES()) {
        // gl_PointSize is always available in ES2 GLSL, but has to be
        // specifically enabled on desktop GLSL.
        gl->fEnable(LOCAL_GL_VERTEX_PROGRAM_POINT_SIZE);

        // gl_PointCoord is always available in ES2 GLSL and in newer desktop GLSL versions, but apparently
        // not in OpenGL 2 and apparently not (due to a driver bug) on certain NVIDIA setups. See:
        //   http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=261472
        // Note that this used to cause crashes on old ATI drivers... hopefully not a significant
        // problem anymore. See bug 602183.
        gl->fEnable(LOCAL_GL_POINT_SPRITE);
    }

#ifdef XP_MACOSX
    if (gl->WorkAroundDriverBugs() &&
        gl->Vendor() == gl::GLVendor::ATI) {
        // The Mac ATI driver, in all known OSX version up to and including 10.8,
        // renders points sprites upside-down. Apple bug 11778921
        gl->fPointParameterf(LOCAL_GL_POINT_SPRITE_COORD_ORIGIN, LOCAL_GL_LOWER_LEFT);
    }
#endif

    // Check the shader validator pref
    NS_ENSURE_TRUE(Preferences::GetRootBranch(), false);

    mShaderValidation =
        Preferences::GetBool("webgl.shader_validator", mShaderValidation);

    // initialize shader translator
    if (mShaderValidation) {
        if (!ShInitialize()) {
            GenerateWarning("GLSL translator initialization failed!");
            return false;
        }
    }

    // Mesa can only be detected with the GL_VERSION string, of the form "2.1 Mesa 7.11.0"
    mIsMesa = strstr((const char *)(gl->fGetString(LOCAL_GL_VERSION)), "Mesa");

    // notice that the point of calling GetAndClearError here is not only to check for error,
    // it is also to reset the error flags so that a subsequent WebGL getError call will give the correct result.
    error = gl->GetAndClearError();
    if (error != LOCAL_GL_NO_ERROR) {
        GenerateWarning("GL error 0x%x occurred during WebGL context initialization!", error);
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

    if (mLoseContextOnMemoryPressure) {
        mContextObserver->RegisterMemoryPressureEvent();
    }

    return true;
}
