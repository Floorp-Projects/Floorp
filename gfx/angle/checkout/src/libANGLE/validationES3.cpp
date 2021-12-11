//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES3.cpp: Validation functions for OpenGL ES 3.0 entry point parameters

#include "libANGLE/validationES3_autogen.h"

#include "anglebase/numerics/safe_conversions.h"
#include "common/mathutil.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Texture.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/validationES.h"

using namespace angle;

namespace gl
{
using namespace err;

namespace
{
bool ValidateFramebufferTextureMultiviewBaseANGLE(const Context *context,
                                                  GLenum target,
                                                  GLenum attachment,
                                                  TextureID texture,
                                                  GLint level,
                                                  GLsizei numViews)
{
    if (!(context->getExtensions().multiview || context->getExtensions().multiview2))
    {
        context->validationError(GL_INVALID_OPERATION, kMultiviewNotAvailable);
        return false;
    }

    if (!ValidateFramebufferTextureBase(context, target, attachment, texture, level))
    {
        return false;
    }

    if (texture.value != 0 && numViews < 1)
    {
        context->validationError(GL_INVALID_VALUE, kMultiviewViewsTooSmall);
        return false;
    }

    const Extensions &extensions = context->getExtensions();
    if (static_cast<GLuint>(numViews) > extensions.maxViews)
    {
        context->validationError(GL_INVALID_VALUE, kMultiviewViewsTooLarge);
        return false;
    }

    return true;
}

bool ValidateFramebufferTextureMultiviewLevelAndFormat(const Context *context,
                                                       const Texture *texture,
                                                       GLint level)
{
    TextureType type = texture->getType();
    if (!ValidMipLevel(context, type, level))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidMipLevel);
        return false;
    }

    const auto &format = texture->getFormat(NonCubeTextureTypeToTarget(type), level);
    if (format.info->compressed)
    {
        context->validationError(GL_INVALID_OPERATION, kCompressedTexturesNotAttachable);
        return false;
    }
    return true;
}

bool ValidateUniformES3(const Context *context,
                        GLenum uniformType,
                        UniformLocation location,
                        GLint count)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateUniform(context, uniformType, location, count);
}

bool ValidateUniformMatrixES3(const Context *context,
                              GLenum valueType,
                              UniformLocation location,
                              GLsizei count,
                              GLboolean transpose)
{
    // Check for ES3 uniform entry points
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateUniformMatrix(context, valueType, location, count, transpose);
}

bool ValidateGenOrDeleteES3(const Context *context, GLint n)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateGenOrDelete(context, n);
}

bool ValidateGenOrDeleteCountES3(const Context *context, GLint count)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    if (count < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeCount);
        return false;
    }
    return true;
}

bool ValidateCopyTexture3DCommon(const Context *context,
                                 const Texture *source,
                                 GLint sourceLevel,
                                 GLint srcInternalFormat,
                                 const Texture *dest,
                                 GLint destLevel,
                                 GLint internalFormat,
                                 TextureTarget destTarget)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (!context->getExtensions().copyTexture3d)
    {
        context->validationError(GL_INVALID_OPERATION, kANGLECopyTexture3DUnavailable);
        return false;
    }

    if (!ValidTexture3DTarget(context, source->getType()))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    // Table 1.1 from the ANGLE_copy_texture_3d spec
    switch (GetUnsizedFormat(srcInternalFormat))
    {
        case GL_ALPHA:
        case GL_LUMINANCE:
        case GL_LUMINANCE_ALPHA:
        case GL_RED:
        case GL_RED_INTEGER:
        case GL_RG:
        case GL_RG_INTEGER:
        case GL_RGB:
        case GL_RGB_INTEGER:
        case GL_RGBA:
        case GL_RGBA_INTEGER:
        case GL_DEPTH_COMPONENT:
        case GL_DEPTH_STENCIL:
            break;
        default:
            context->validationError(GL_INVALID_OPERATION, kInvalidInternalFormat);
            return false;
    }

    if (!ValidTexture3DTarget(context, TextureTargetToType(destTarget)))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    // Table 1.0 from the ANGLE_copy_texture_3d spec
    switch (internalFormat)
    {
        case GL_RGB:
        case GL_RGBA:
        case GL_LUMINANCE:
        case GL_LUMINANCE_ALPHA:
        case GL_ALPHA:
        case GL_R8:
        case GL_R8_SNORM:
        case GL_R16F:
        case GL_R32F:
        case GL_R8UI:
        case GL_R8I:
        case GL_R16UI:
        case GL_R16I:
        case GL_R32UI:
        case GL_R32I:
        case GL_RG:
        case GL_RG8:
        case GL_RG8_SNORM:
        case GL_RG16F:
        case GL_RG32F:
        case GL_RG8UI:
        case GL_RG8I:
        case GL_RG16UI:
        case GL_RG16I:
        case GL_RG32UI:
        case GL_RG32I:
        case GL_RGB8:
        case GL_SRGB8:
        case GL_RGB565:
        case GL_RGB8_SNORM:
        case GL_R11F_G11F_B10F:
        case GL_RGB9_E5:
        case GL_RGB16F:
        case GL_RGB32F:
        case GL_RGB8UI:
        case GL_RGB8I:
        case GL_RGB16UI:
        case GL_RGB16I:
        case GL_RGB32UI:
        case GL_RGB32I:
        case GL_RGBA8:
        case GL_SRGB8_ALPHA8:
        case GL_RGBA8_SNORM:
        case GL_RGB5_A1:
        case GL_RGBA4:
        case GL_RGB10_A2:
        case GL_RGBA16F:
        case GL_RGBA32F:
        case GL_RGBA8UI:
        case GL_RGBA8I:
        case GL_RGB10_A2UI:
        case GL_RGBA16UI:
        case GL_RGBA16I:
        case GL_RGBA32I:
        case GL_RGBA32UI:
            break;
        default:
            context->validationError(GL_INVALID_OPERATION, kInvalidInternalFormat);
            return false;
    }

    return true;
}
}  // anonymous namespace

static bool ValidateTexImageFormatCombination(const Context *context,
                                              TextureType target,
                                              GLenum internalFormat,
                                              GLenum format,
                                              GLenum type)
{
    // Different validation if on desktop api
    if (context->getClientType() == EGL_OPENGL_API)
    {
        // The type and format are valid if any supported internal format has that type and format
        if (!ValidDesktopFormat(format))
        {
            context->validationError(GL_INVALID_ENUM, kInvalidFormat);
            return false;
        }

        if (!ValidDesktopType(type))
        {
            context->validationError(GL_INVALID_ENUM, kInvalidType);
            return false;
        }
    }
    else
    {
        // The type and format are valid if any supported internal format has that type and format
        if (!ValidES3Format(format))
        {
            context->validationError(GL_INVALID_ENUM, kInvalidFormat);
            return false;
        }

        if (!ValidES3Type(type))
        {
            context->validationError(GL_INVALID_ENUM, kInvalidType);
            return false;
        }
    }

    // For historical reasons, glTexImage2D and glTexImage3D pass in their internal format as a
    // GLint instead of a GLenum. Therefor an invalid internal format gives a GL_INVALID_VALUE
    // error instead of a GL_INVALID_ENUM error. As this validation function is only called in
    // the validation codepaths for glTexImage2D/3D, we record a GL_INVALID_VALUE error.
    if (!ValidES3InternalFormat(internalFormat))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidInternalFormat);
        return false;
    }

    // From the ES 3.0 spec section 3.8.3:
    // Textures with a base internal format of DEPTH_COMPONENT or DEPTH_STENCIL are supported by
    // texture image specification commands only if target is TEXTURE_2D, TEXTURE_2D_ARRAY, or
    // TEXTURE_CUBE_MAP.Using these formats in conjunction with any other target will result in an
    // INVALID_OPERATION error.
    if (target == TextureType::_3D && (format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL))
    {
        context->validationError(GL_INVALID_OPERATION, k3DDepthStencil);
        return false;
    }

    if (context->getClientType() == EGL_OPENGL_API)
    {
        // Check if this is a valid format combination to load texture data
        if (!ValidDesktopFormatCombination(format, type, internalFormat))
        {
            context->validationError(GL_INVALID_OPERATION, kInvalidFormatCombination);
            return false;
        }
    }
    else
    {
        // Check if this is a valid format combination to load texture data
        if (!ValidES3FormatCombination(format, type, internalFormat))
        {
            context->validationError(GL_INVALID_OPERATION, kInvalidFormatCombination);
            return false;
        }
    }

    const InternalFormat &formatInfo = GetInternalFormatInfo(internalFormat, type);
    if (!formatInfo.textureSupport(context->getClientVersion(), context->getExtensions()))
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidInternalFormat);
        return false;
    }

    return true;
}

static bool ValidateES3CompressedFormatForTexture3D(const Context *context, GLenum format)
{
    if (IsETC2EACFormat(format))
    {
        // ES 3.1, Section 8.7, page 169.
        context->validationError(GL_INVALID_OPERATION, kInternalFormatRequiresTexture2DArray);
        return false;
    }

    if (IsASTC2DFormat(format) && !(context->getExtensions().textureCompressionASTCHDRKHR ||
                                    context->getExtensions().textureCompressionSliced3dASTCKHR))
    {
        // GL_KHR_texture_compression_astc_hdr, TEXTURE_3D is not supported without HDR profile
        context->validationError(GL_INVALID_OPERATION, kInternalFormatRequiresTexture2DArrayASTC);
        return false;
    }

    if (IsS3TCFormat(format))
    {
        // GL_EXT_texture_compression_s3tc
        context->validationError(GL_INVALID_OPERATION, kInternalFormatRequiresTexture2DArrayS3TC);
        return false;
    }

    if (IsRGTCFormat(format))
    {
        // GL_EXT_texture_compression_rgtc
        context->validationError(GL_INVALID_OPERATION, kInternalFormatRequiresTexture2DArrayRGTC);
        return false;
    }
    return true;
}

bool ValidateES3TexImageParametersBase(const Context *context,
                                       TextureTarget target,
                                       GLint level,
                                       GLenum internalformat,
                                       bool isCompressed,
                                       bool isSubImage,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       GLint border,
                                       GLenum format,
                                       GLenum type,
                                       GLsizei imageSize,
                                       const void *pixels)
{
    TextureType texType = TextureTargetToType(target);

    // Validate image size
    if (!ValidImageSizeParameters(context, texType, level, width, height, depth, isSubImage))
    {
        // Error already processed.
        return false;
    }

    // Verify zero border
    if (border != 0)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidBorder);
        return false;
    }

    if (xoffset < 0 || yoffset < 0 || zoffset < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeOffset);
        return false;
    }

    if (std::numeric_limits<GLsizei>::max() - xoffset < width ||
        std::numeric_limits<GLsizei>::max() - yoffset < height ||
        std::numeric_limits<GLsizei>::max() - zoffset < depth)
    {
        context->validationError(GL_INVALID_VALUE, kOffsetOverflow);
        return false;
    }

    const Caps &caps = context->getCaps();

    switch (texType)
    {
        case TextureType::_2D:
        case TextureType::External:
        case TextureType::VideoImage:
            if (width > (caps.max2DTextureSize >> level) ||
                height > (caps.max2DTextureSize >> level))
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
            break;

        case TextureType::Rectangle:
            ASSERT(level == 0);
            if (width > caps.maxRectangleTextureSize || height > caps.maxRectangleTextureSize)
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
            if (isCompressed)
            {
                context->validationError(GL_INVALID_ENUM, kRectangleTextureCompressed);
                return false;
            }
            break;

        case TextureType::CubeMap:
            if (!isSubImage && width != height)
            {
                context->validationError(GL_INVALID_VALUE, kCubemapFacesEqualDimensions);
                return false;
            }

            if (width > (caps.maxCubeMapTextureSize >> level))
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
            break;

        case TextureType::_3D:
            if (width > (caps.max3DTextureSize >> level) ||
                height > (caps.max3DTextureSize >> level) ||
                depth > (caps.max3DTextureSize >> level))
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
            break;

        case TextureType::_2DArray:
            if (width > (caps.max2DTextureSize >> level) ||
                height > (caps.max2DTextureSize >> level) || depth > caps.maxArrayTextureLayers)
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
            break;

        case TextureType::CubeMapArray:
            if (!isSubImage && width != height)
            {
                context->validationError(GL_INVALID_VALUE, kCubemapFacesEqualDimensions);
                return false;
            }

            if (width > (caps.maxCubeMapTextureSize >> level))
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }

            if (width > (caps.max3DTextureSize >> level) ||
                height > (caps.max3DTextureSize >> level) ||
                depth > (caps.max3DTextureSize >> level))
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }

            if (!isSubImage && depth % 6 != 0)
            {
                context->validationError(GL_INVALID_VALUE, kCubemapInvalidDepth);
                return false;
            }
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    Texture *texture = context->getTextureByType(texType);
    if (!texture)
    {
        context->validationError(GL_INVALID_OPERATION, kMissingTexture);
        return false;
    }

    if (texture->getImmutableFormat() && !isSubImage)
    {
        context->validationError(GL_INVALID_OPERATION, kTextureIsImmutable);
        return false;
    }

    if (isCompressed && texType == TextureType::_3D)
    {
        GLenum compressedDataFormat = isSubImage ? format : internalformat;
        if (!ValidateES3CompressedFormatForTexture3D(context, compressedDataFormat))
        {
            // Error already generated.
            return false;
        }
    }

    // Validate texture formats
    GLenum actualInternalFormat =
        isSubImage ? texture->getFormat(target, level).info->internalFormat : internalformat;
    if (isSubImage && actualInternalFormat == GL_NONE)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidMipLevel);
        return false;
    }

    const InternalFormat &actualFormatInfo = isSubImage
                                                 ? *texture->getFormat(target, level).info
                                                 : GetInternalFormatInfo(internalformat, type);
    if (isCompressed)
    {
        if (!actualFormatInfo.compressed)
        {
            context->validationError(GL_INVALID_ENUM, kCompressedMismatch);
            return false;
        }

        if (isSubImage)
        {
            if (!ValidCompressedSubImageSize(
                    context, actualFormatInfo.internalFormat, xoffset, yoffset, zoffset, width,
                    height, depth, texture->getWidth(target, level),
                    texture->getHeight(target, level), texture->getDepth(target, level)))
            {
                context->validationError(GL_INVALID_OPERATION, kInvalidCompressedImageSize);
                return false;
            }

            if (format != actualInternalFormat)
            {
                context->validationError(GL_INVALID_OPERATION, kMismatchedFormat);
                return false;
            }

            // GL_EXT_compressed_ETC1_RGB8_sub_texture allows this format
            if (actualInternalFormat == GL_ETC1_RGB8_OES &&
                !context->getExtensions().compressedETC1RGB8SubTexture)
            {
                context->validationError(GL_INVALID_OPERATION, kInvalidInternalFormat);
                return false;
            }
        }
        else
        {
            if (!ValidCompressedImageSize(context, actualInternalFormat, level, width, height,
                                          depth))
            {
                context->validationError(GL_INVALID_OPERATION, kInvalidCompressedImageSize);
                return false;
            }
        }

        if (!actualFormatInfo.textureSupport(context->getClientVersion(), context->getExtensions()))
        {
            context->validationError(GL_INVALID_ENUM, kInvalidFormat);
            return false;
        }

        // Disallow 3D-only compressed formats from being set on 2D textures
        if (actualFormatInfo.compressedBlockDepth > 1 && texType != TextureType::_2DArray)
        {
            context->validationError(GL_INVALID_OPERATION, kInvalidTextureTarget);
            return false;
        }
    }
    else
    {
        if (!ValidateTexImageFormatCombination(context, texType, actualInternalFormat, format,
                                               type))
        {
            return false;
        }
    }

    // Validate sub image parameters
    if (isSubImage)
    {
        if (isCompressed != actualFormatInfo.compressed)
        {
            context->validationError(GL_INVALID_OPERATION, kCompressedMismatch);
            return false;
        }

        if (xoffset < 0 || yoffset < 0 || zoffset < 0)
        {
            context->validationError(GL_INVALID_VALUE, kNegativeOffset);
            return false;
        }

        if (std::numeric_limits<GLsizei>::max() - xoffset < width ||
            std::numeric_limits<GLsizei>::max() - yoffset < height ||
            std::numeric_limits<GLsizei>::max() - zoffset < depth)
        {
            context->validationError(GL_INVALID_VALUE, kOffsetOverflow);
            return false;
        }

        if (static_cast<size_t>(xoffset + width) > texture->getWidth(target, level) ||
            static_cast<size_t>(yoffset + height) > texture->getHeight(target, level) ||
            static_cast<size_t>(zoffset + depth) > texture->getDepth(target, level))
        {
            context->validationError(GL_INVALID_VALUE, kOffsetOverflow);
            return false;
        }

        if (width > 0 && height > 0 && depth > 0 && pixels == nullptr &&
            context->getState().getTargetBuffer(BufferBinding::PixelUnpack) == nullptr)
        {
            context->validationError(GL_INVALID_VALUE, kPixelDataNull);
            return false;
        }
    }

    GLenum sizeCheckFormat = isSubImage ? format : internalformat;
    if (!ValidImageDataSize(context, texType, width, height, depth, sizeCheckFormat, type, pixels,
                            imageSize))
    {
        return false;
    }

    // Check for pixel unpack buffer related API errors
    Buffer *pixelUnpackBuffer = context->getState().getTargetBuffer(BufferBinding::PixelUnpack);
    if (pixelUnpackBuffer != nullptr)
    {
        // ...data is not evenly divisible into the number of bytes needed to store in memory a
        // datum
        // indicated by type.
        if (!isCompressed)
        {
            size_t offset            = reinterpret_cast<size_t>(pixels);
            size_t dataBytesPerPixel = static_cast<size_t>(GetTypeInfo(type).bytes);

            if ((offset % dataBytesPerPixel) != 0)
            {
                context->validationError(GL_INVALID_OPERATION, kDataTypeNotAligned);
                return false;
            }
        }

        // ...the buffer object's data store is currently mapped.
        if (pixelUnpackBuffer->isMapped())
        {
            context->validationError(GL_INVALID_OPERATION, kBufferMapped);
            return false;
        }
    }

    if (context->getExtensions().webglCompatibility)
    {
        // Define:
        //   DataStoreWidth  = (GL_UNPACK_ROW_LENGTH ? GL_UNPACK_ROW_LENGTH : width)
        //   DataStoreHeight = (GL_UNPACK_IMAGE_HEIGHT ? GL_UNPACK_IMAGE_HEIGHT : height)
        //
        // WebGL 2.0 imposes the following additional constraints:
        //
        // 1) texImage2D and texSubImage2D generate INVALID_OPERATION if:
        //      GL_UNPACK_SKIP_PIXELS + width > DataStoreWidth
        //    except for texImage2D if no GL_PIXEL_UNPACK_BUFFER is
        //    bound and _pixels_ is null.
        //
        // 2) texImage3D and texSubImage3D generate INVALID_OPERATION if:
        //      GL_UNPACK_SKIP_PIXELS + width > DataStoreWidth
        //      GL_UNPACK_SKIP_ROWS + height > DataStoreHeight
        //    except for texImage3D if no GL_PIXEL_UNPACK_BUFFER is
        //    bound and _pixels_ is null.
        if (!pixelUnpackBuffer && !pixels && !isSubImage)
        {
            // Exception case for texImage2D or texImage3D, above.
        }
        else
        {
            const auto &unpack   = context->getState().getUnpackState();
            GLint dataStoreWidth = unpack.rowLength ? unpack.rowLength : width;
            if (unpack.skipPixels + width > dataStoreWidth)
            {
                context->validationError(GL_INVALID_OPERATION, kInvalidUnpackParametersForWebGL);
                return false;
            }
            if (target == TextureTarget::_3D || target == TextureTarget::_2DArray)
            {
                GLint dataStoreHeight = unpack.imageHeight ? unpack.imageHeight : height;
                if (unpack.skipRows + height > dataStoreHeight)
                {
                    context->validationError(GL_INVALID_OPERATION,
                                             kInvalidUnpackParametersForWebGL);
                    return false;
                }
            }
        }
    }

    return true;
}

bool ValidateES3TexImage2DParameters(const Context *context,
                                     TextureTarget target,
                                     GLint level,
                                     GLenum internalformat,
                                     bool isCompressed,
                                     bool isSubImage,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLint border,
                                     GLenum format,
                                     GLenum type,
                                     GLsizei imageSize,
                                     const void *pixels)
{
    if (!ValidTexture2DDestinationTarget(context, target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    return ValidateES3TexImageParametersBase(context, target, level, internalformat, isCompressed,
                                             isSubImage, xoffset, yoffset, zoffset, width, height,
                                             depth, border, format, type, imageSize, pixels);
}

bool ValidateES3TexImage3DParameters(const Context *context,
                                     TextureTarget target,
                                     GLint level,
                                     GLenum internalformat,
                                     bool isCompressed,
                                     bool isSubImage,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLint border,
                                     GLenum format,
                                     GLenum type,
                                     GLsizei bufSize,
                                     const void *pixels)
{
    if (!ValidTexture3DDestinationTarget(context, target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    return ValidateES3TexImageParametersBase(context, target, level, internalformat, isCompressed,
                                             isSubImage, xoffset, yoffset, zoffset, width, height,
                                             depth, border, format, type, bufSize, pixels);
}

struct EffectiveInternalFormatInfo
{
    GLenum effectiveFormat;
    GLenum destFormat;
    GLuint minRedBits;
    GLuint maxRedBits;
    GLuint minGreenBits;
    GLuint maxGreenBits;
    GLuint minBlueBits;
    GLuint maxBlueBits;
    GLuint minAlphaBits;
    GLuint maxAlphaBits;
};

static bool QueryEffectiveFormatList(const InternalFormat &srcFormat,
                                     GLenum targetFormat,
                                     const EffectiveInternalFormatInfo *list,
                                     size_t size,
                                     GLenum *outEffectiveFormat)
{
    for (size_t curFormat = 0; curFormat < size; ++curFormat)
    {
        const EffectiveInternalFormatInfo &formatInfo = list[curFormat];
        if ((formatInfo.destFormat == targetFormat) &&
            (formatInfo.minRedBits <= srcFormat.redBits &&
             formatInfo.maxRedBits >= srcFormat.redBits) &&
            (formatInfo.minGreenBits <= srcFormat.greenBits &&
             formatInfo.maxGreenBits >= srcFormat.greenBits) &&
            (formatInfo.minBlueBits <= srcFormat.blueBits &&
             formatInfo.maxBlueBits >= srcFormat.blueBits) &&
            (formatInfo.minAlphaBits <= srcFormat.alphaBits &&
             formatInfo.maxAlphaBits >= srcFormat.alphaBits))
        {
            *outEffectiveFormat = formatInfo.effectiveFormat;
            return true;
        }
    }

    *outEffectiveFormat = GL_NONE;
    return false;
}

bool GetSizedEffectiveInternalFormatInfo(const InternalFormat &srcFormat,
                                         GLenum *outEffectiveFormat)
{
    // OpenGL ES 3.0.3 Specification, Table 3.17, pg 141:
    // Effective internal format coresponding to destination internal format and linear source
    // buffer component sizes.
    //                                       | Source channel min/max sizes |
    //   Effective Internal Format   |  N/A  |  R   |  G   |  B   |  A      |
    // clang-format off
    constexpr EffectiveInternalFormatInfo list[] = {
        { GL_ALPHA8_EXT,              GL_NONE, 0,  0, 0,  0, 0,  0, 1, 8 },
        { GL_R8,                      GL_NONE, 1,  8, 0,  0, 0,  0, 0, 0 },
        { GL_RG8,                     GL_NONE, 1,  8, 1,  8, 0,  0, 0, 0 },
        { GL_RGB565,                  GL_NONE, 1,  5, 1,  6, 1,  5, 0, 0 },
        { GL_RGB8,                    GL_NONE, 6,  8, 7,  8, 6,  8, 0, 0 },
        { GL_RGBA4,                   GL_NONE, 1,  4, 1,  4, 1,  4, 1, 4 },
        { GL_RGB5_A1,                 GL_NONE, 5,  5, 5,  5, 5,  5, 1, 1 },
        { GL_RGBA8,                   GL_NONE, 5,  8, 5,  8, 5,  8, 2, 8 },
        { GL_RGB10_A2,                GL_NONE, 9, 10, 9, 10, 9, 10, 2, 2 },
    };
    // clang-format on

    return QueryEffectiveFormatList(srcFormat, GL_NONE, list, ArraySize(list), outEffectiveFormat);
}

bool GetUnsizedEffectiveInternalFormatInfo(const InternalFormat &srcFormat,
                                           const InternalFormat &destFormat,
                                           GLenum *outEffectiveFormat)
{
    constexpr GLuint umax = UINT_MAX;

    // OpenGL ES 3.0.3 Specification, Table 3.17, pg 141:
    // Effective internal format coresponding to destination internal format andlinear source buffer
    // component sizes.
    //                                                   |   Source channel min/max sizes   |
    //     Effective Internal Format |   Dest Format     |   R   |    G   |    B   |    A   |
    // clang-format off
    constexpr EffectiveInternalFormatInfo list[] = {
        { GL_ALPHA8_EXT,             GL_ALPHA,           0, umax, 0, umax, 0, umax, 1,    8 },
        { GL_LUMINANCE8_EXT,         GL_LUMINANCE,       1,    8, 0, umax, 0, umax, 0, umax },
        { GL_LUMINANCE8_ALPHA8_EXT,  GL_LUMINANCE_ALPHA, 1,    8, 0, umax, 0, umax, 1,    8 },
        { GL_RGB565,                 GL_RGB,             1,    5, 1,    6, 1,    5, 0, umax },
        { GL_RGB8,                   GL_RGB,             6,    8, 7,    8, 6,    8, 0, umax },
        { GL_RGBA4,                  GL_RGBA,            1,    4, 1,    4, 1,    4, 1,    4 },
        { GL_RGB5_A1,                GL_RGBA,            5,    5, 5,    5, 5,    5, 1,    1 },
        { GL_RGBA8,                  GL_RGBA,            5,    8, 5,    8, 5,    8, 5,    8 },
    };
    // clang-format on

    return QueryEffectiveFormatList(srcFormat, destFormat.format, list, ArraySize(list),
                                    outEffectiveFormat);
}

static bool GetEffectiveInternalFormat(const InternalFormat &srcFormat,
                                       const InternalFormat &destFormat,
                                       GLenum *outEffectiveFormat)
{
    if (destFormat.sized)
    {
        return GetSizedEffectiveInternalFormatInfo(srcFormat, outEffectiveFormat);
    }
    else
    {
        return GetUnsizedEffectiveInternalFormatInfo(srcFormat, destFormat, outEffectiveFormat);
    }
}

static bool EqualOrFirstZero(GLuint first, GLuint second)
{
    return first == 0 || first == second;
}

static bool IsValidES3CopyTexImageCombination(const InternalFormat &textureFormatInfo,
                                              const InternalFormat &framebufferFormatInfo,
                                              FramebufferID readBufferHandle)
{
    if (!ValidES3CopyConversion(textureFormatInfo.format, framebufferFormatInfo.format))
    {
        return false;
    }

    // Section 3.8.5 of the GLES 3.0.3 spec states that source and destination formats
    // must both be signed, unsigned, or fixed point and both source and destinations
    // must be either both SRGB or both not SRGB. EXT_color_buffer_float adds allowed
    // conversion between fixed and floating point.

    if ((textureFormatInfo.colorEncoding == GL_SRGB) !=
        (framebufferFormatInfo.colorEncoding == GL_SRGB))
    {
        return false;
    }

    if (((textureFormatInfo.componentType == GL_INT) !=
         (framebufferFormatInfo.componentType == GL_INT)) ||
        ((textureFormatInfo.componentType == GL_UNSIGNED_INT) !=
         (framebufferFormatInfo.componentType == GL_UNSIGNED_INT)))
    {
        return false;
    }

    if ((textureFormatInfo.componentType == GL_UNSIGNED_NORMALIZED ||
         textureFormatInfo.componentType == GL_SIGNED_NORMALIZED) &&
        !(framebufferFormatInfo.componentType == GL_UNSIGNED_NORMALIZED ||
          framebufferFormatInfo.componentType == GL_SIGNED_NORMALIZED))
    {
        return false;
    }

    // SNORM is not supported (e.g. is not in the tables of "effective internal format" that
    // correspond to internal formats.
    if (textureFormatInfo.componentType == GL_SIGNED_NORMALIZED)
    {
        return false;
    }

    // Section 3.8.5 of the GLES 3.0.3 (and section 8.6 of the GLES 3.2) spec has a caveat, that
    // the KHR dEQP tests enforce:
    //
    // Note that the above rules disallow matches where some components sizes are smaller and
    // others are larger (such as RGB10_A2).
    if (!textureFormatInfo.sized && (framebufferFormatInfo.internalFormat == GL_RGB10_A2))
    {
        return false;
    }

    // GLES specification 3.0.3, sec 3.8.5, pg 139-140:
    // The effective internal format of the source buffer is determined with the following rules
    // applied in order:
    //    * If the source buffer is a texture or renderbuffer that was created with a sized internal
    //      format then the effective internal format is the source buffer's sized internal format.
    //    * If the source buffer is a texture that was created with an unsized base internal format,
    //      then the effective internal format is the source image array's effective internal
    //      format, as specified by table 3.12, which is determined from the <format> and <type>
    //      that were used when the source image array was specified by TexImage*.
    //    * Otherwise the effective internal format is determined by the row in table 3.17 or 3.18
    //      where Destination Internal Format matches internalformat and where the [source channel
    //      sizes] are consistent with the values of the source buffer's [channel sizes]. Table 3.17
    //      is used if the FRAMEBUFFER_ATTACHMENT_ENCODING is LINEAR and table 3.18 is used if the
    //      FRAMEBUFFER_ATTACHMENT_ENCODING is SRGB.
    const InternalFormat *sourceEffectiveFormat = nullptr;
    if (readBufferHandle.value != 0)
    {
        // Not the default framebuffer, therefore the read buffer must be a user-created texture or
        // renderbuffer
        if (framebufferFormatInfo.sized)
        {
            sourceEffectiveFormat = &framebufferFormatInfo;
        }
        else
        {
            // Renderbuffers cannot be created with an unsized internal format, so this must be an
            // unsized-format texture. We can use the same table we use when creating textures to
            // get its effective sized format.
            sourceEffectiveFormat =
                &GetSizedInternalFormatInfo(framebufferFormatInfo.sizedInternalFormat);
        }
    }
    else
    {
        // The effective internal format must be derived from the source framebuffer's channel
        // sizes. This is done in GetEffectiveInternalFormat for linear buffers (table 3.17)
        if (framebufferFormatInfo.colorEncoding == GL_LINEAR)
        {
            GLenum effectiveFormat;
            if (GetEffectiveInternalFormat(framebufferFormatInfo, textureFormatInfo,
                                           &effectiveFormat))
            {
                sourceEffectiveFormat = &GetSizedInternalFormatInfo(effectiveFormat);
            }
            else
            {
                return false;
            }
        }
        else if (framebufferFormatInfo.colorEncoding == GL_SRGB)
        {
            // SRGB buffers can only be copied to sized format destinations according to table 3.18
            if (textureFormatInfo.sized &&
                (framebufferFormatInfo.redBits >= 1 && framebufferFormatInfo.redBits <= 8) &&
                (framebufferFormatInfo.greenBits >= 1 && framebufferFormatInfo.greenBits <= 8) &&
                (framebufferFormatInfo.blueBits >= 1 && framebufferFormatInfo.blueBits <= 8) &&
                (framebufferFormatInfo.alphaBits >= 1 && framebufferFormatInfo.alphaBits <= 8))
            {
                sourceEffectiveFormat = &GetSizedInternalFormatInfo(GL_SRGB8_ALPHA8);
            }
            else
            {
                return false;
            }
        }
        else
        {
            UNREACHABLE();
            return false;
        }
    }

    if (textureFormatInfo.sized)
    {
        // Section 3.8.5 of the GLES 3.0.3 spec, pg 139, requires that, if the destination format is
        // sized, component sizes of the source and destination formats must exactly match if the
        // destination format exists.
        if (!EqualOrFirstZero(textureFormatInfo.redBits, sourceEffectiveFormat->redBits) ||
            !EqualOrFirstZero(textureFormatInfo.greenBits, sourceEffectiveFormat->greenBits) ||
            !EqualOrFirstZero(textureFormatInfo.blueBits, sourceEffectiveFormat->blueBits) ||
            !EqualOrFirstZero(textureFormatInfo.alphaBits, sourceEffectiveFormat->alphaBits))
        {
            return false;
        }
    }

    return true;  // A conversion function exists, and no rule in the specification has precluded
                  // conversion between these formats.
}

bool ValidateES3CopyTexImageParametersBase(const Context *context,
                                           TextureTarget target,
                                           GLint level,
                                           GLenum internalformat,
                                           bool isSubImage,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint zoffset,
                                           GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLint border)
{
    Format textureFormat = Format::Invalid();
    if (!ValidateCopyTexImageParametersBase(context, target, level, internalformat, isSubImage,
                                            xoffset, yoffset, zoffset, x, y, width, height, border,
                                            &textureFormat))
    {
        return false;
    }
    ASSERT(textureFormat.valid() || !isSubImage);

    const auto &state               = context->getState();
    Framebuffer *framebuffer        = state.getReadFramebuffer();
    FramebufferID readFramebufferID = framebuffer->id();

    if (!ValidateFramebufferComplete(context, framebuffer))
    {
        return false;
    }

    // needIntrinsic = true. Treat renderToTexture textures as single sample since they will be
    // resolved before copying
    if (!framebuffer->isDefault() &&
        !ValidateFramebufferNotMultisampled(context, framebuffer, true))
    {
        return false;
    }

    const FramebufferAttachment *source = framebuffer->getReadColorAttachment();

    // According to ES 3.x spec, if the internalformat of the texture
    // is RGB9_E5 and copy to such a texture, generate INVALID_OPERATION.
    if (textureFormat.info->internalFormat == GL_RGB9_E5)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidFormat);
        return false;
    }

    if (isSubImage)
    {
        if (!IsValidES3CopyTexImageCombination(*textureFormat.info, *source->getFormat().info,
                                               readFramebufferID))
        {
            context->validationError(GL_INVALID_OPERATION, kInvalidCopyCombination);
            return false;
        }
    }
    else
    {
        // Use format/type from the source FBO. (Might not be perfect for all cases?)
        const InternalFormat &framebufferFormat = *source->getFormat().info;
        const InternalFormat &copyFormat = GetInternalFormatInfo(internalformat, GL_UNSIGNED_BYTE);
        if (!IsValidES3CopyTexImageCombination(copyFormat, framebufferFormat, readFramebufferID))
        {
            context->validationError(GL_INVALID_OPERATION, kInvalidCopyCombination);
            return false;
        }
    }

    // If width or height is zero, it is a no-op.  Return false without setting an error.
    return (width > 0 && height > 0);
}

bool ValidateES3CopyTexImage2DParameters(const Context *context,
                                         TextureTarget target,
                                         GLint level,
                                         GLenum internalformat,
                                         bool isSubImage,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLint zoffset,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border)
{
    if (!ValidTexture2DDestinationTarget(context, target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    return ValidateES3CopyTexImageParametersBase(context, target, level, internalformat, isSubImage,
                                                 xoffset, yoffset, zoffset, x, y, width, height,
                                                 border);
}

bool ValidateES3CopyTexImage3DParameters(const Context *context,
                                         TextureTarget target,
                                         GLint level,
                                         GLenum internalformat,
                                         bool isSubImage,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLint zoffset,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border)
{
    if (!ValidTexture3DDestinationTarget(context, target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    return ValidateES3CopyTexImageParametersBase(context, target, level, internalformat, isSubImage,
                                                 xoffset, yoffset, zoffset, x, y, width, height,
                                                 border);
}

bool ValidateES3TexStorageParametersBase(const Context *context,
                                         TextureType target,
                                         GLsizei levels,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth)
{
    if (width < 1 || height < 1 || depth < 1 || levels < 1)
    {
        context->validationError(GL_INVALID_VALUE, kTextureSizeTooSmall);
        return false;
    }

    GLsizei maxDim = std::max(width, height);
    if (target != TextureType::_2DArray)
    {
        maxDim = std::max(maxDim, depth);
    }

    if (levels > log2(maxDim) + 1)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidMipLevels);
        return false;
    }

    const Caps &caps = context->getCaps();

    switch (target)
    {
        case TextureType::_2D:
        {
            if (width > caps.max2DTextureSize || height > caps.max2DTextureSize)
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
        }
        break;

        case TextureType::Rectangle:
        {
            if (levels != 1)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidMipLevels);
                return false;
            }

            if (width > caps.maxRectangleTextureSize || height > caps.maxRectangleTextureSize)
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
        }
        break;

        case TextureType::CubeMap:
        {
            if (width != height)
            {
                context->validationError(GL_INVALID_VALUE, kCubemapFacesEqualDimensions);
                return false;
            }

            if (width > caps.maxCubeMapTextureSize)
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
        }
        break;

        case TextureType::_3D:
        {
            if (width > caps.max3DTextureSize || height > caps.max3DTextureSize ||
                depth > caps.max3DTextureSize)
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
        }
        break;

        case TextureType::_2DArray:
        {
            if (width > caps.max2DTextureSize || height > caps.max2DTextureSize ||
                depth > caps.maxArrayTextureLayers)
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }
        }
        break;

        case TextureType::CubeMapArray:
        {
            if (width != height)
            {
                context->validationError(GL_INVALID_VALUE, kCubemapFacesEqualDimensions);
                return false;
            }

            if (width > caps.maxCubeMapTextureSize)
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }

            if (width > caps.max3DTextureSize || height > caps.max3DTextureSize ||
                depth > caps.max3DTextureSize)
            {
                context->validationError(GL_INVALID_VALUE, kResourceMaxTextureSize);
                return false;
            }

            if (depth % 6 != 0)
            {
                context->validationError(GL_INVALID_VALUE, kCubemapInvalidDepth);
                return false;
            }
        }
        break;

        default:
            UNREACHABLE();
            return false;
    }

    Texture *texture = context->getTextureByType(target);
    if (!texture || texture->id().value == 0)
    {
        context->validationError(GL_INVALID_OPERATION, kMissingTexture);
        return false;
    }

    if (texture->getImmutableFormat())
    {
        context->validationError(GL_INVALID_OPERATION, kTextureIsImmutable);
        return false;
    }

    const InternalFormat &formatInfo = GetSizedInternalFormatInfo(internalformat);
    if (!formatInfo.textureSupport(context->getClientVersion(), context->getExtensions()))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidFormat);
        return false;
    }

    if (!formatInfo.sized)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidFormat);
        return false;
    }

    if (formatInfo.compressed && target == TextureType::Rectangle)
    {
        context->validationError(GL_INVALID_ENUM, kRectangleTextureCompressed);
        return false;
    }

    if (formatInfo.compressed && target == TextureType::_3D)
    {
        if (!ValidateES3CompressedFormatForTexture3D(context, formatInfo.internalFormat))
        {
            // Error already generated.
            return false;
        }
    }

    return true;
}

bool ValidateES3TexStorage2DParameters(const Context *context,
                                       TextureType target,
                                       GLsizei levels,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth)
{
    if (!ValidTexture2DTarget(context, target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    return ValidateES3TexStorageParametersBase(context, target, levels, internalformat, width,
                                               height, depth);
}

bool ValidateES3TexStorage3DParameters(const Context *context,
                                       TextureType target,
                                       GLsizei levels,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth)
{
    if (!ValidTexture3DTarget(context, target))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    return ValidateES3TexStorageParametersBase(context, target, levels, internalformat, width,
                                               height, depth);
}

bool ValidateBeginQuery(const Context *context, QueryType target, QueryID id)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateBeginQueryBase(context, target, id);
}

bool ValidateEndQuery(const Context *context, QueryType target)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateEndQueryBase(context, target);
}

bool ValidateGetQueryiv(const Context *context, QueryType target, GLenum pname, const GLint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateGetQueryivBase(context, target, pname, nullptr);
}

bool ValidateGetQueryObjectuiv(const Context *context,
                               QueryID id,
                               GLenum pname,
                               const GLuint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateGetQueryObjectValueBase(context, id, pname, nullptr);
}

bool ValidateFramebufferTextureLayer(const Context *context,
                                     GLenum target,
                                     GLenum attachment,
                                     TextureID texture,
                                     GLint level,
                                     GLint layer)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (!ValidateFramebufferTextureBase(context, target, attachment, texture, level))
    {
        return false;
    }

    const Caps &caps = context->getCaps();
    if (texture.value != 0)
    {
        if (layer < 0)
        {
            context->validationError(GL_INVALID_VALUE, kNegativeLayer);
            return false;
        }

        Texture *tex = context->getTexture(texture);
        ASSERT(tex);

        switch (tex->getType())
        {
            case TextureType::_2DArray:
            {
                if (level > log2(caps.max2DTextureSize))
                {
                    context->validationError(GL_INVALID_VALUE, kFramebufferTextureInvalidMipLevel);
                    return false;
                }

                if (layer >= caps.maxArrayTextureLayers)
                {
                    context->validationError(GL_INVALID_VALUE, kFramebufferTextureInvalidLayer);
                    return false;
                }
            }
            break;

            case TextureType::_3D:
            {
                if (level > log2(caps.max3DTextureSize))
                {
                    context->validationError(GL_INVALID_VALUE, kFramebufferTextureInvalidMipLevel);
                    return false;
                }

                if (layer >= caps.max3DTextureSize)
                {
                    context->validationError(GL_INVALID_VALUE, kFramebufferTextureInvalidLayer);
                    return false;
                }
            }
            break;

            case TextureType::_2DMultisampleArray:
            {
                if (level != 0)
                {
                    context->validationError(GL_INVALID_VALUE, kFramebufferTextureInvalidMipLevel);
                    return false;
                }

                if (layer >= caps.maxArrayTextureLayers)
                {
                    context->validationError(GL_INVALID_VALUE, kFramebufferTextureInvalidLayer);
                    return false;
                }
            }
            break;

            case TextureType::CubeMapArray:
            {
                if (level > log2(caps.max3DTextureSize))
                {
                    context->validationError(GL_INVALID_VALUE, kFramebufferTextureInvalidMipLevel);
                    return false;
                }

                if (layer >= caps.max3DTextureSize)
                {
                    context->validationError(GL_INVALID_VALUE, kFramebufferTextureInvalidLayer);
                    return false;
                }
            }
            break;

            default:
                context->validationError(GL_INVALID_OPERATION,
                                         kFramebufferTextureLayerIncorrectTextureType);
                return false;
        }

        const auto &format = tex->getFormat(NonCubeTextureTypeToTarget(tex->getType()), level);
        if (format.info->compressed)
        {
            context->validationError(GL_INVALID_OPERATION, kCompressedTexturesNotAttachable);
            return false;
        }
    }

    return true;
}

bool ValidateInvalidateFramebuffer(const Context *context,
                                   GLenum target,
                                   GLsizei numAttachments,
                                   const GLenum *attachments)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    bool defaultFramebuffer = false;

    switch (target)
    {
        case GL_DRAW_FRAMEBUFFER:
        case GL_FRAMEBUFFER:
            defaultFramebuffer = context->getState().getDrawFramebuffer()->isDefault();
            break;
        case GL_READ_FRAMEBUFFER:
            defaultFramebuffer = context->getState().getReadFramebuffer()->isDefault();
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidFramebufferTarget);
            return false;
    }

    return ValidateDiscardFramebufferBase(context, target, numAttachments, attachments,
                                          defaultFramebuffer);
}

bool ValidateInvalidateSubFramebuffer(const Context *context,
                                      GLenum target,
                                      GLsizei numAttachments,
                                      const GLenum *attachments,
                                      GLint x,
                                      GLint y,
                                      GLsizei width,
                                      GLsizei height)
{
    if (width < 0 || height < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeSize);
        return false;
    }

    return ValidateInvalidateFramebuffer(context, target, numAttachments, attachments);
}

bool ValidateClearBuffer(const Context *context)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (!ValidateFramebufferComplete(context, context->getState().getDrawFramebuffer()))
    {
        return false;
    }

    return true;
}

bool ValidateDrawRangeElements(const Context *context,
                               PrimitiveMode mode,
                               GLuint start,
                               GLuint end,
                               GLsizei count,
                               DrawElementsType type,
                               const void *indices)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (end < start)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidElementRange);
        return false;
    }

    if (!ValidateDrawElementsCommon(context, mode, count, type, indices, 0))
    {
        return false;
    }

    // Skip range checks for no-op calls.
    if (count <= 0)
    {
        return true;
    }

    return true;
}

bool ValidateGetUniformuiv(const Context *context,
                           ShaderProgramID program,
                           UniformLocation location,
                           const GLuint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateGetUniformBase(context, program, location);
}

bool ValidateReadBuffer(const Context *context, GLenum src)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    const Framebuffer *readFBO = context->getState().getReadFramebuffer();

    if (readFBO == nullptr)
    {
        context->validationError(GL_INVALID_OPERATION, kNoReadFramebuffer);
        return false;
    }

    if (src == GL_NONE)
    {
        return true;
    }

    if (src != GL_BACK && (src < GL_COLOR_ATTACHMENT0 || src > GL_COLOR_ATTACHMENT31))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidReadBuffer);
        return false;
    }

    if (readFBO->isDefault())
    {
        if (src != GL_BACK)
        {
            context->validationError(GL_INVALID_OPERATION, kInvalidDefaultReadBuffer);
            return false;
        }
    }
    else
    {
        GLuint drawBuffer = static_cast<GLuint>(src - GL_COLOR_ATTACHMENT0);

        if (drawBuffer >= static_cast<GLuint>(context->getCaps().maxColorAttachments))
        {
            context->validationError(GL_INVALID_OPERATION, kExceedsMaxColorAttachments);
            return false;
        }
    }

    return true;
}

bool ValidateCompressedTexImage3D(const Context *context,
                                  TextureTarget target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLint border,
                                  GLsizei imageSize,
                                  const void *data)
{
    if ((context->getClientMajorVersion() < 3) && !context->getExtensions().texture3DOES)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (!ValidTextureTarget(context, TextureTargetToType(target)))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    // Validate image size
    if (!ValidImageSizeParameters(context, TextureTargetToType(target), level, width, height, depth,
                                  false))
    {
        // Error already generated.
        return false;
    }

    const InternalFormat &formatInfo = GetSizedInternalFormatInfo(internalformat);
    if (!formatInfo.compressed)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidCompressedFormat);
        return false;
    }

    GLuint blockSize = 0;
    if (!formatInfo.computeCompressedImageSize(Extents(width, height, depth), &blockSize))
    {
        context->validationError(GL_INVALID_VALUE, kIntegerOverflow);
        return false;
    }

    if (imageSize < 0 || static_cast<GLuint>(imageSize) != blockSize)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidCompressedImageSize);
        return false;
    }

    // 3D texture target validation
    if (target != TextureTarget::_3D && target != TextureTarget::_2DArray)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidTextureTarget);
        return false;
    }

    // validateES3TexImageFormat sets the error code if there is an error
    if (!ValidateES3TexImage3DParameters(context, target, level, internalformat, true, false, 0, 0,
                                         0, width, height, depth, border, GL_NONE, GL_NONE, -1,
                                         data))
    {
        return false;
    }

    return true;
}

bool ValidateCompressedTexImage3DRobustANGLE(const Context *context,
                                             TextureTarget target,
                                             GLint level,
                                             GLenum internalformat,
                                             GLsizei width,
                                             GLsizei height,
                                             GLsizei depth,
                                             GLint border,
                                             GLsizei imageSize,
                                             GLsizei dataSize,
                                             const void *data)
{
    if (!ValidateRobustCompressedTexImageBase(context, imageSize, dataSize))
    {
        return false;
    }

    return ValidateCompressedTexImage3D(context, target, level, internalformat, width, height,
                                        depth, border, imageSize, data);
}

bool ValidateBindVertexArray(const Context *context, VertexArrayID array)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateBindVertexArrayBase(context, array);
}

bool ValidateIsVertexArray(const Context *context, VertexArrayID array)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return true;
}

static bool ValidateBindBufferCommon(const Context *context,
                                     BufferBinding target,
                                     GLuint index,
                                     BufferID buffer,
                                     GLintptr offset,
                                     GLsizeiptr size)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (buffer.value != 0 && offset < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeOffset);
        return false;
    }

    if (!context->getState().isBindGeneratesResourceEnabled() &&
        !context->isBufferGenerated(buffer))
    {
        context->validationError(GL_INVALID_OPERATION, kObjectNotGenerated);
        return false;
    }

    const Caps &caps = context->getCaps();
    switch (target)
    {
        case BufferBinding::TransformFeedback:
        {
            if (index >= static_cast<GLuint>(caps.maxTransformFeedbackSeparateAttributes))
            {
                context->validationError(GL_INVALID_VALUE,
                                         kIndexExceedsTransformFeedbackBufferBindings);
                return false;
            }
            if (buffer.value != 0 && ((offset % 4) != 0 || (size % 4) != 0))
            {
                context->validationError(GL_INVALID_VALUE, kOffsetAndSizeAlignment);
                return false;
            }

            if (context->getState().isTransformFeedbackActive())
            {
                context->validationError(GL_INVALID_OPERATION, kTransformFeedbackTargetActive);
                return false;
            }
            break;
        }
        case BufferBinding::Uniform:
        {
            if (index >= static_cast<GLuint>(caps.maxUniformBufferBindings))
            {
                context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxUniformBufferBindings);
                return false;
            }

            ASSERT(caps.uniformBufferOffsetAlignment);
            if (buffer.value != 0 && (offset % caps.uniformBufferOffsetAlignment) != 0)
            {
                context->validationError(GL_INVALID_VALUE, kUniformBufferOffsetAlignment);
                return false;
            }
            break;
        }
        case BufferBinding::AtomicCounter:
        {
            if (context->getClientVersion() < ES_3_1)
            {
                context->validationError(GL_INVALID_ENUM, kEnumRequiresGLES31);
                return false;
            }
            if (index >= static_cast<GLuint>(caps.maxAtomicCounterBufferBindings))
            {
                context->validationError(GL_INVALID_VALUE,
                                         kIndexExceedsMaxAtomicCounterBufferBindings);
                return false;
            }
            if (buffer.value != 0 && (offset % 4) != 0)
            {
                context->validationError(GL_INVALID_VALUE, kOffsetAlignment);
                return false;
            }
            break;
        }
        case BufferBinding::ShaderStorage:
        {
            if (context->getClientVersion() < ES_3_1)
            {
                context->validationError(GL_INVALID_ENUM, kEnumRequiresGLES31);
                return false;
            }
            if (index >= static_cast<GLuint>(caps.maxShaderStorageBufferBindings))
            {
                context->validationError(GL_INVALID_VALUE, kExceedsMaxShaderStorageBufferBindings);
                return false;
            }
            ASSERT(caps.shaderStorageBufferOffsetAlignment);
            if (buffer.value != 0 && (offset % caps.shaderStorageBufferOffsetAlignment) != 0)
            {
                context->validationError(GL_INVALID_VALUE, kShaderStorageBufferOffsetAlignment);
                return false;
            }
            break;
        }
        case BufferBinding::Texture:
        {
            if (!context->getExtensions().textureBufferAny())
            {
                context->validationError(GL_INVALID_ENUM, kTextureBufferExtensionNotAvailable);
                return false;
            }
            if (index != 0)
            {
                context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxUniformBufferBindings);
                return false;
            }
            if (buffer.value != 0 && (offset % caps.textureBufferOffsetAlignment) != 0)
            {
                context->validationError(GL_INVALID_VALUE, kTextureBufferOffsetAlignment);
                return false;
            }
            break;
        }
        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    return true;
}

bool ValidateBindBufferBase(const Context *context,
                            BufferBinding target,
                            GLuint index,
                            BufferID buffer)
{
    return ValidateBindBufferCommon(context, target, index, buffer, 0, 0);
}

bool ValidateBindBufferRange(const Context *context,
                             BufferBinding target,
                             GLuint index,
                             BufferID buffer,
                             GLintptr offset,
                             GLsizeiptr size)
{
    if (buffer.value != 0 && size <= 0)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidBindBufferSize);
        return false;
    }
    return ValidateBindBufferCommon(context, target, index, buffer, offset, size);
}

bool ValidateProgramBinary(const Context *context,
                           ShaderProgramID program,
                           GLenum binaryFormat,
                           const void *binary,
                           GLsizei length)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateProgramBinaryBase(context, program, binaryFormat, binary, length);
}

bool ValidateGetProgramBinary(const Context *context,
                              ShaderProgramID program,
                              GLsizei bufSize,
                              const GLsizei *length,
                              const GLenum *binaryFormat,
                              const void *binary)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateGetProgramBinaryBase(context, program, bufSize, length, binaryFormat, binary);
}

bool ValidateProgramParameteriBase(const Context *context,
                                   ShaderProgramID program,
                                   GLenum pname,
                                   GLint value)
{
    if (GetValidProgram(context, program) == nullptr)
    {
        return false;
    }

    switch (pname)
    {
        case GL_PROGRAM_BINARY_RETRIEVABLE_HINT:
            if (value != GL_FALSE && value != GL_TRUE)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidBooleanValue);
                return false;
            }
            break;

        case GL_PROGRAM_SEPARABLE:
            if (context->getClientVersion() < ES_3_1)
            {
                context->validationError(GL_INVALID_ENUM, kES31Required);
                return false;
            }

            if (value != GL_FALSE && value != GL_TRUE)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidBooleanValue);
                return false;
            }
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kInvalidPname);
            return false;
    }

    return true;
}

bool ValidateProgramParameteri(const Context *context,
                               ShaderProgramID program,
                               GLenum pname,
                               GLint value)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateProgramParameteriBase(context, program, pname, value);
}

bool ValidateBlitFramebuffer(const Context *context,
                             GLint srcX0,
                             GLint srcY0,
                             GLint srcX1,
                             GLint srcY1,
                             GLint dstX0,
                             GLint dstY0,
                             GLint dstX1,
                             GLint dstY1,
                             GLbitfield mask,
                             GLenum filter)
{
    if (context->getClientMajorVersion() < 3 && !context->getExtensions().framebufferBlitNV)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateBlitFramebufferParameters(context, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                             dstX1, dstY1, mask, filter);
}

bool ValidateClearBufferiv(const Context *context,
                           GLenum buffer,
                           GLint drawbuffer,
                           const GLint *value)
{
    switch (buffer)
    {
        case GL_COLOR:
            if (drawbuffer < 0 || drawbuffer >= context->getCaps().maxDrawBuffers)
            {
                context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxDrawBuffer);
                return false;
            }
            if (context->getExtensions().webglCompatibility)
            {
                constexpr GLenum validComponentTypes[] = {GL_INT};
                if (!ValidateWebGLFramebufferAttachmentClearType(
                        context, drawbuffer, validComponentTypes, ArraySize(validComponentTypes)))
                {
                    return false;
                }
            }
            break;

        case GL_STENCIL:
            if (drawbuffer != 0)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidDepthStencilDrawBuffer);
                return false;
            }
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    return ValidateClearBuffer(context);
}

bool ValidateClearBufferuiv(const Context *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLuint *value)
{
    switch (buffer)
    {
        case GL_COLOR:
            if (drawbuffer < 0 || drawbuffer >= context->getCaps().maxDrawBuffers)
            {
                context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxDrawBuffer);
                return false;
            }
            if (context->getExtensions().webglCompatibility)
            {
                constexpr GLenum validComponentTypes[] = {GL_UNSIGNED_INT};
                if (!ValidateWebGLFramebufferAttachmentClearType(
                        context, drawbuffer, validComponentTypes, ArraySize(validComponentTypes)))
                {
                    return false;
                }
            }
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    return ValidateClearBuffer(context);
}

bool ValidateClearBufferfv(const Context *context,
                           GLenum buffer,
                           GLint drawbuffer,
                           const GLfloat *value)
{
    switch (buffer)
    {
        case GL_COLOR:
            if (drawbuffer < 0 || drawbuffer >= context->getCaps().maxDrawBuffers)
            {
                context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxDrawBuffer);
                return false;
            }
            if (context->getExtensions().webglCompatibility)
            {
                constexpr GLenum validComponentTypes[] = {GL_FLOAT, GL_UNSIGNED_NORMALIZED,
                                                          GL_SIGNED_NORMALIZED};
                if (!ValidateWebGLFramebufferAttachmentClearType(
                        context, drawbuffer, validComponentTypes, ArraySize(validComponentTypes)))
                {
                    return false;
                }
            }
            break;

        case GL_DEPTH:
            if (drawbuffer != 0)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidDepthStencilDrawBuffer);
                return false;
            }
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    return ValidateClearBuffer(context);
}

bool ValidateClearBufferfi(const Context *context,
                           GLenum buffer,
                           GLint drawbuffer,
                           GLfloat depth,
                           GLint stencil)
{
    switch (buffer)
    {
        case GL_DEPTH_STENCIL:
            if (drawbuffer != 0)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidDepthStencilDrawBuffer);
                return false;
            }
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    return ValidateClearBuffer(context);
}

bool ValidateDrawBuffers(const Context *context, GLsizei n, const GLenum *bufs)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateDrawBuffersBase(context, n, bufs);
}

bool ValidateCopyTexSubImage3D(const Context *context,
                               TextureTarget target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLint zoffset,
                               GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height)
{
    if ((context->getClientMajorVersion() < 3) && !context->getExtensions().texture3DOES)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateES3CopyTexImage3DParameters(context, target, level, GL_NONE, true, xoffset,
                                               yoffset, zoffset, x, y, width, height, 0);
}

bool ValidateCopyTexture3DANGLE(const Context *context,
                                TextureID sourceId,
                                GLint sourceLevel,
                                TextureTarget destTarget,
                                TextureID destId,
                                GLint destLevel,
                                GLint internalFormat,
                                GLenum destType,
                                GLboolean unpackFlipY,
                                GLboolean unpackPremultiplyAlpha,
                                GLboolean unpackUnmultiplyAlpha)
{
    const Texture *source = context->getTexture(sourceId);
    if (source == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidSourceTexture);
        return false;
    }

    TextureType sourceType = source->getType();
    ASSERT(sourceType != TextureType::CubeMap);
    TextureTarget sourceTarget = NonCubeTextureTypeToTarget(sourceType);
    const Format &sourceFormat = source->getFormat(sourceTarget, sourceLevel);

    const Texture *dest = context->getTexture(destId);
    if (dest == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidDestinationTexture);
        return false;
    }

    if (!ValidateCopyTexture3DCommon(context, source, sourceLevel,
                                     sourceFormat.info->internalFormat, dest, destLevel,
                                     internalFormat, destTarget))
    {
        return false;
    }

    if (!ValidMipLevel(context, source->getType(), sourceLevel))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidSourceTextureLevel);
        return false;
    }

    GLsizei sourceWidth  = static_cast<GLsizei>(source->getWidth(sourceTarget, sourceLevel));
    GLsizei sourceHeight = static_cast<GLsizei>(source->getHeight(sourceTarget, sourceLevel));
    if (sourceWidth == 0 || sourceHeight == 0)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidSourceTextureSize);
        return false;
    }

    if (dest->getImmutableFormat())
    {
        context->validationError(GL_INVALID_OPERATION, kDestinationImmutable);
        return false;
    }

    return true;
}

bool ValidateCopySubTexture3DANGLE(const Context *context,
                                   TextureID sourceId,
                                   GLint sourceLevel,
                                   TextureTarget destTarget,
                                   TextureID destId,
                                   GLint destLevel,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLint zoffset,
                                   GLint x,
                                   GLint y,
                                   GLint z,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLboolean unpackFlipY,
                                   GLboolean unpackPremultiplyAlpha,
                                   GLboolean unpackUnmultiplyAlpha)
{
    const Texture *source = context->getTexture(sourceId);
    if (source == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidSourceTexture);
        return false;
    }

    TextureType sourceType = source->getType();
    ASSERT(sourceType != TextureType::CubeMap);
    TextureTarget sourceTarget = NonCubeTextureTypeToTarget(sourceType);
    const Format &sourceFormat = source->getFormat(sourceTarget, sourceLevel);

    const Texture *dest = context->getTexture(destId);
    if (dest == nullptr)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidDestinationTexture);
        return false;
    }

    const InternalFormat &destFormat = *dest->getFormat(destTarget, destLevel).info;

    if (!ValidateCopyTexture3DCommon(context, source, sourceLevel,
                                     sourceFormat.info->internalFormat, dest, destLevel,
                                     destFormat.internalFormat, destTarget))
    {
        return false;
    }

    if (x < 0 || y < 0 || z < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeXYZ);
        return false;
    }

    if (width < 0 || height < 0 || depth < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeHeightWidthDepth);
        return false;
    }

    if (static_cast<size_t>(x + width) > source->getWidth(sourceTarget, sourceLevel) ||
        static_cast<size_t>(y + height) > source->getHeight(sourceTarget, sourceLevel) ||
        static_cast<size_t>(z + depth) > source->getDepth(sourceTarget, sourceLevel))
    {
        context->validationError(GL_INVALID_VALUE, kSourceTextureTooSmall);
        return false;
    }

    if (TextureTargetToType(destTarget) != dest->getType())
    {
        context->validationError(GL_INVALID_VALUE, kInvalidDestinationTextureType);
        return false;
    }

    if (xoffset < 0 || yoffset < 0 || zoffset < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeOffset);
        return false;
    }

    if (static_cast<size_t>(xoffset + width) > dest->getWidth(destTarget, destLevel) ||
        static_cast<size_t>(yoffset + height) > dest->getHeight(destTarget, destLevel) ||
        static_cast<size_t>(zoffset + depth) > dest->getDepth(destTarget, destLevel))
    {
        context->validationError(GL_INVALID_VALUE, kDestinationTextureTooSmall);
        return false;
    }

    return true;
}

bool ValidateTexImage3D(const Context *context,
                        TextureTarget target,
                        GLint level,
                        GLint internalformat,
                        GLsizei width,
                        GLsizei height,
                        GLsizei depth,
                        GLint border,
                        GLenum format,
                        GLenum type,
                        const void *pixels)
{
    if ((context->getClientMajorVersion() < 3) && !context->getExtensions().texture3DOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    return ValidateES3TexImage3DParameters(context, target, level, internalformat, false, false, 0,
                                           0, 0, width, height, depth, border, format, type, -1,
                                           pixels);
}

bool ValidateTexImage3DRobustANGLE(const Context *context,
                                   TextureTarget target,
                                   GLint level,
                                   GLint internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLint border,
                                   GLenum format,
                                   GLenum type,
                                   GLsizei bufSize,
                                   const void *pixels)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    return ValidateES3TexImage3DParameters(context, target, level, internalformat, false, false, 0,
                                           0, 0, width, height, depth, border, format, type,
                                           bufSize, pixels);
}

bool ValidateTexSubImage3D(const Context *context,
                           TextureTarget target,
                           GLint level,
                           GLint xoffset,
                           GLint yoffset,
                           GLint zoffset,
                           GLsizei width,
                           GLsizei height,
                           GLsizei depth,
                           GLenum format,
                           GLenum type,
                           const void *pixels)
{
    if ((context->getClientMajorVersion() < 3) && !context->getExtensions().texture3DOES)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateES3TexImage3DParameters(context, target, level, GL_NONE, false, true, xoffset,
                                           yoffset, zoffset, width, height, depth, 0, format, type,
                                           -1, pixels);
}

bool ValidateTexSubImage3DRobustANGLE(const Context *context,
                                      TextureTarget target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLint zoffset,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth,
                                      GLenum format,
                                      GLenum type,
                                      GLsizei bufSize,
                                      const void *pixels)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    return ValidateES3TexImage3DParameters(context, target, level, GL_NONE, false, true, xoffset,
                                           yoffset, zoffset, width, height, depth, 0, format, type,
                                           bufSize, pixels);
}

bool ValidateCompressedTexSubImage3D(const Context *context,
                                     TextureTarget target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLenum format,
                                     GLsizei imageSize,
                                     const void *data)
{
    if ((context->getClientMajorVersion() < 3) && !context->getExtensions().texture3DOES)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    const InternalFormat &formatInfo = GetSizedInternalFormatInfo(format);
    if (!formatInfo.compressed)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidCompressedFormat);
        return false;
    }

    GLuint blockSize = 0;
    if (!formatInfo.computeCompressedImageSize(Extents(width, height, depth), &blockSize))
    {
        context->validationError(GL_INVALID_OPERATION, kIntegerOverflow);
        return false;
    }

    if (imageSize < 0 || static_cast<GLuint>(imageSize) != blockSize)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidCompressedImageSize);
        return false;
    }

    if (!ValidateES3TexImage3DParameters(context, target, level, GL_NONE, true, true, xoffset,
                                         yoffset, zoffset, width, height, depth, 0, format, GL_NONE,
                                         -1, data))
    {
        return false;
    }

    if (!data)
    {
        context->validationError(GL_INVALID_VALUE, kPixelDataNull);
        return false;
    }

    return true;
}

bool ValidateCompressedTexSubImage3DRobustANGLE(const Context *context,
                                                TextureTarget target,
                                                GLint level,
                                                GLint xoffset,
                                                GLint yoffset,
                                                GLint zoffset,
                                                GLsizei width,
                                                GLsizei height,
                                                GLsizei depth,
                                                GLenum format,
                                                GLsizei imageSize,
                                                GLsizei dataSize,
                                                const void *data)
{
    if (!ValidateRobustCompressedTexImageBase(context, imageSize, dataSize))
    {
        return false;
    }

    return ValidateCompressedTexSubImage3D(context, target, level, xoffset, yoffset, zoffset, width,
                                           height, depth, format, imageSize, data);
}

bool ValidateGenQueries(const Context *context, GLsizei n, const QueryID *queries)
{
    return ValidateGenOrDeleteES3(context, n);
}

bool ValidateDeleteQueries(const Context *context, GLsizei n, const QueryID *queries)
{
    return ValidateGenOrDeleteES3(context, n);
}

bool ValidateGenSamplers(const Context *context, GLsizei count, const SamplerID *samplers)
{
    return ValidateGenOrDeleteCountES3(context, count);
}

bool ValidateDeleteSamplers(const Context *context, GLsizei count, const SamplerID *samplers)
{
    return ValidateGenOrDeleteCountES3(context, count);
}

bool ValidateGenTransformFeedbacks(const Context *context,
                                   GLsizei n,
                                   const TransformFeedbackID *ids)
{
    return ValidateGenOrDeleteES3(context, n);
}

bool ValidateDeleteTransformFeedbacks(const Context *context,
                                      GLsizei n,
                                      const TransformFeedbackID *ids)
{
    if (!ValidateGenOrDeleteES3(context, n))
    {
        return false;
    }
    for (GLint i = 0; i < n; ++i)
    {
        auto *transformFeedback = context->getTransformFeedback(ids[i]);
        if (transformFeedback != nullptr && transformFeedback->isActive())
        {
            // ES 3.0.4 section 2.15.1 page 86
            context->validationError(GL_INVALID_OPERATION, kTransformFeedbackActiveDelete);
            return false;
        }
    }
    return true;
}

bool ValidateGenVertexArrays(const Context *context, GLsizei n, const VertexArrayID *arrays)
{
    return ValidateGenOrDeleteES3(context, n);
}

bool ValidateDeleteVertexArrays(const Context *context, GLsizei n, const VertexArrayID *arrays)
{
    return ValidateGenOrDeleteES3(context, n);
}

bool ValidateBeginTransformFeedback(const Context *context, PrimitiveMode primitiveMode)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    switch (primitiveMode)
    {
        case PrimitiveMode::Triangles:
        case PrimitiveMode::Lines:
        case PrimitiveMode::Points:
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kInvalidPrimitiveMode);
            return false;
    }

    TransformFeedback *transformFeedback = context->getState().getCurrentTransformFeedback();
    ASSERT(transformFeedback != nullptr);

    if (transformFeedback->isActive())
    {
        context->validationError(GL_INVALID_OPERATION, kTransfomFeedbackAlreadyActive);
        return false;
    }

    for (size_t i = 0; i < transformFeedback->getIndexedBufferCount(); i++)
    {
        const auto &buffer = transformFeedback->getIndexedBuffer(i);
        if (buffer.get())
        {
            if (buffer->isMapped())
            {
                context->validationError(GL_INVALID_OPERATION, kBufferMapped);
                return false;
            }
            if ((context->getLimitations().noDoubleBoundTransformFeedbackBuffers ||
                 context->getExtensions().webglCompatibility) &&
                buffer->isDoubleBoundForTransformFeedback())
            {
                context->validationError(GL_INVALID_OPERATION,
                                         kTransformFeedbackBufferMultipleOutputs);
                return false;
            }
        }
    }

    const ProgramExecutable *programExecutable = context->getState().getProgramExecutable();
    if (programExecutable)
    {
        if (programExecutable->getLinkedTransformFeedbackVaryings().size() == 0)
        {
            context->validationError(GL_INVALID_OPERATION, kNoTransformFeedbackOutputVariables);
            return false;
        }
    }
    else
    {
        context->validationError(GL_INVALID_OPERATION, kProgramNotBound);
        return false;
    }

    return true;
}

bool ValidateGetBufferPointerv(const Context *context,
                               BufferBinding target,
                               GLenum pname,
                               void *const *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateGetBufferPointervBase(context, target, pname, nullptr, params);
}

bool ValidateGetBufferPointervRobustANGLE(const Context *context,
                                          BufferBinding target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          const GLsizei *length,
                                          void *const *params)
{
    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    GLsizei numParams = 0;

    if (context->getClientMajorVersion() < 3 && !context->getExtensions().mapBufferOES)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }

    if (!ValidateGetBufferPointervBase(context, target, pname, &numParams, params))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, numParams))
    {
        return false;
    }

    SetRobustLengthParam(length, numParams);

    return true;
}

bool ValidateUnmapBuffer(const Context *context, BufferBinding target)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateUnmapBufferBase(context, target);
}

bool ValidateMapBufferRange(const Context *context,
                            BufferBinding target,
                            GLintptr offset,
                            GLsizeiptr length,
                            GLbitfield access)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateMapBufferRangeBase(context, target, offset, length, access);
}

bool ValidateFlushMappedBufferRange(const Context *context,
                                    BufferBinding target,
                                    GLintptr offset,
                                    GLsizeiptr length)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateFlushMappedBufferRangeBase(context, target, offset, length);
}

bool ValidateIndexedStateQuery(const Context *context, GLenum pname, GLuint index, GLsizei *length)
{
    if (length)
    {
        *length = 0;
    }

    GLenum nativeType;
    unsigned int numParams;
    if (!context->getIndexedQueryParameterInfo(pname, &nativeType, &numParams))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidPname);
        return false;
    }

    const Caps &caps = context->getCaps();
    switch (pname)
    {
        case GL_BLEND_SRC_RGB:
        case GL_BLEND_SRC_ALPHA:
        case GL_BLEND_DST_RGB:
        case GL_BLEND_DST_ALPHA:
        case GL_BLEND_EQUATION_RGB:
        case GL_BLEND_EQUATION_ALPHA:
        case GL_COLOR_WRITEMASK:
            if (!context->getExtensions().drawBuffersIndexedAny())
            {
                context->validationError(GL_INVALID_ENUM, kDrawBuffersIndexedExtensionNotAvailable);
                return false;
            }
            if (index >= static_cast<GLuint>(caps.maxDrawBuffers))
            {
                context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxDrawBuffer);
                return false;
            }
            break;
        case GL_TRANSFORM_FEEDBACK_BUFFER_START:
        case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
        case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
            if (index >= static_cast<GLuint>(caps.maxTransformFeedbackSeparateAttributes))
            {
                context->validationError(GL_INVALID_VALUE,
                                         kIndexExceedsMaxTransformFeedbackAttribs);
                return false;
            }
            break;

        case GL_UNIFORM_BUFFER_START:
        case GL_UNIFORM_BUFFER_SIZE:
        case GL_UNIFORM_BUFFER_BINDING:
            if (index >= static_cast<GLuint>(caps.maxUniformBufferBindings))
            {
                context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxUniformBufferBindings);
                return false;
            }
            break;

        case GL_MAX_COMPUTE_WORK_GROUP_SIZE:
        case GL_MAX_COMPUTE_WORK_GROUP_COUNT:
            if (index >= 3u)
            {
                context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxWorkgroupDimensions);
                return false;
            }
            break;

        case GL_ATOMIC_COUNTER_BUFFER_START:
        case GL_ATOMIC_COUNTER_BUFFER_SIZE:
        case GL_ATOMIC_COUNTER_BUFFER_BINDING:
            if (context->getClientVersion() < ES_3_1)
            {
                context->validationError(GL_INVALID_ENUM, kEnumRequiresGLES31);
                return false;
            }
            if (index >= static_cast<GLuint>(caps.maxAtomicCounterBufferBindings))
            {
                context->validationError(GL_INVALID_VALUE,
                                         kIndexExceedsMaxAtomicCounterBufferBindings);
                return false;
            }
            break;

        case GL_SHADER_STORAGE_BUFFER_START:
        case GL_SHADER_STORAGE_BUFFER_SIZE:
        case GL_SHADER_STORAGE_BUFFER_BINDING:
            if (context->getClientVersion() < ES_3_1)
            {
                context->validationError(GL_INVALID_ENUM, kEnumRequiresGLES31);
                return false;
            }
            if (index >= static_cast<GLuint>(caps.maxShaderStorageBufferBindings))
            {
                context->validationError(GL_INVALID_VALUE, kExceedsMaxShaderStorageBufferBindings);
                return false;
            }
            break;

        case GL_VERTEX_BINDING_BUFFER:
        case GL_VERTEX_BINDING_DIVISOR:
        case GL_VERTEX_BINDING_OFFSET:
        case GL_VERTEX_BINDING_STRIDE:
            if (context->getClientVersion() < ES_3_1)
            {
                context->validationError(GL_INVALID_ENUM, kEnumRequiresGLES31);
                return false;
            }
            if (index >= static_cast<GLuint>(caps.maxVertexAttribBindings))
            {
                context->validationError(GL_INVALID_VALUE, kExceedsMaxVertexAttribBindings);
                return false;
            }
            break;
        case GL_SAMPLE_MASK_VALUE:
            if (context->getClientVersion() < ES_3_1)
            {
                context->validationError(GL_INVALID_ENUM, kEnumRequiresGLES31);
                return false;
            }
            if (index >= static_cast<GLuint>(caps.maxSampleMaskWords))
            {
                context->validationError(GL_INVALID_VALUE, kInvalidSampleMaskNumber);
                return false;
            }
            break;
        case GL_IMAGE_BINDING_NAME:
        case GL_IMAGE_BINDING_LEVEL:
        case GL_IMAGE_BINDING_LAYERED:
        case GL_IMAGE_BINDING_LAYER:
        case GL_IMAGE_BINDING_ACCESS:
        case GL_IMAGE_BINDING_FORMAT:
            if (context->getClientVersion() < ES_3_1)
            {
                context->validationError(GL_INVALID_ENUM, kEnumRequiresGLES31);
                return false;
            }
            if (index >= static_cast<GLuint>(caps.maxImageUnits))
            {
                context->validationError(GL_INVALID_VALUE, kExceedsMaxImageUnits);
                return false;
            }
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    if (length)
    {
        if (pname == GL_COLOR_WRITEMASK)
        {
            *length = 4;
        }
        else
        {
            *length = 1;
        }
    }

    return true;
}

bool ValidateGetIntegeri_v(const Context *context, GLenum target, GLuint index, const GLint *data)
{
    if (context->getClientVersion() < ES_3_0)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateIndexedStateQuery(context, target, index, nullptr);
}

bool ValidateGetIntegeri_vRobustANGLE(const Context *context,
                                      GLenum target,
                                      GLuint index,
                                      GLsizei bufSize,
                                      const GLsizei *length,
                                      const GLint *data)
{
    if (context->getClientVersion() < ES_3_0)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    GLsizei numParams = 0;

    if (!ValidateIndexedStateQuery(context, target, index, &numParams))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, numParams))
    {
        return false;
    }

    SetRobustLengthParam(length, numParams);

    return true;
}

bool ValidateGetInteger64i_v(const Context *context,
                             GLenum target,
                             GLuint index,
                             const GLint64 *data)
{
    if (context->getClientVersion() < ES_3_0)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateIndexedStateQuery(context, target, index, nullptr);
}

bool ValidateGetInteger64i_vRobustANGLE(const Context *context,
                                        GLenum target,
                                        GLuint index,
                                        GLsizei bufSize,
                                        const GLsizei *length,
                                        const GLint64 *data)
{
    if (context->getClientVersion() < ES_3_0)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (!ValidateRobustEntryPoint(context, bufSize))
    {
        return false;
    }

    GLsizei numParams = 0;

    if (!ValidateIndexedStateQuery(context, target, index, &numParams))
    {
        return false;
    }

    if (!ValidateRobustBufferSize(context, bufSize, numParams))
    {
        return false;
    }

    SetRobustLengthParam(length, numParams);

    return true;
}

bool ValidateCopyBufferSubData(const Context *context,
                               BufferBinding readTarget,
                               BufferBinding writeTarget,
                               GLintptr readOffset,
                               GLintptr writeOffset,
                               GLsizeiptr size)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (!context->isValidBufferBinding(readTarget) || !context->isValidBufferBinding(writeTarget))
    {
        context->validationError(GL_INVALID_ENUM, kInvalidBufferTypes);
        return false;
    }

    Buffer *readBuffer  = context->getState().getTargetBuffer(readTarget);
    Buffer *writeBuffer = context->getState().getTargetBuffer(writeTarget);

    if (!readBuffer || !writeBuffer)
    {
        context->validationError(GL_INVALID_OPERATION, kBufferNotBound);
        return false;
    }

    // Verify that readBuffer and writeBuffer are not currently mapped
    if (readBuffer->isMapped() || writeBuffer->isMapped())
    {
        context->validationError(GL_INVALID_OPERATION, kBufferMapped);
        return false;
    }

    if (context->getExtensions().webglCompatibility &&
        (readBuffer->isBoundForTransformFeedbackAndOtherUse() ||
         writeBuffer->isBoundForTransformFeedbackAndOtherUse()))
    {
        context->validationError(GL_INVALID_OPERATION, kBufferBoundForTransformFeedback);
        return false;
    }

    CheckedNumeric<GLintptr> checkedReadOffset(readOffset);
    CheckedNumeric<GLintptr> checkedWriteOffset(writeOffset);
    CheckedNumeric<GLintptr> checkedSize(size);

    auto checkedReadSum  = checkedReadOffset + checkedSize;
    auto checkedWriteSum = checkedWriteOffset + checkedSize;

    if (!checkedReadSum.IsValid() || !checkedWriteSum.IsValid() ||
        !IsValueInRangeForNumericType<GLintptr>(readBuffer->getSize()) ||
        !IsValueInRangeForNumericType<GLintptr>(writeBuffer->getSize()))
    {
        context->validationError(GL_INVALID_VALUE, kIntegerOverflow);
        return false;
    }

    if (readOffset < 0 || writeOffset < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeOffset);
        return false;
    }

    if (size < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeSize);
        return false;
    }

    if (checkedReadSum.ValueOrDie() > readBuffer->getSize() ||
        checkedWriteSum.ValueOrDie() > writeBuffer->getSize())
    {
        context->validationError(GL_INVALID_VALUE, kBufferOffsetOverflow);
        return false;
    }

    if (readBuffer == writeBuffer)
    {
        auto checkedOffsetDiff = (checkedReadOffset - checkedWriteOffset).Abs();
        if (!checkedOffsetDiff.IsValid())
        {
            // This shold not be possible.
            UNREACHABLE();
            context->validationError(GL_INVALID_VALUE, kIntegerOverflow);
            return false;
        }

        if (checkedOffsetDiff.ValueOrDie() < size)
        {
            context->validationError(GL_INVALID_VALUE, kCopyAlias);
            return false;
        }
    }

    return true;
}

bool ValidateGetStringi(const Context *context, GLenum name, GLuint index)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    switch (name)
    {
        case GL_EXTENSIONS:
            if (index >= context->getExtensionStringCount())
            {
                context->validationError(GL_INVALID_VALUE, kExceedsNumExtensions);
                return false;
            }
            break;

        case GL_REQUESTABLE_EXTENSIONS_ANGLE:
            if (!context->getExtensions().requestExtension)
            {
                context->validationError(GL_INVALID_ENUM, kInvalidName);
                return false;
            }
            if (index >= context->getRequestableExtensionStringCount())
            {
                context->validationError(GL_INVALID_VALUE, kExceedsNumRequestableExtensions);
                return false;
            }
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kInvalidName);
            return false;
    }

    return true;
}

bool ValidateRenderbufferStorageMultisample(const Context *context,
                                            GLenum target,
                                            GLsizei samples,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (!ValidateRenderbufferStorageParametersBase(context, target, samples, internalformat, width,
                                                   height))
    {
        return false;
    }

    // The ES3 spec(section 4.4.2) states that the internal format must be sized and not an integer
    // format if samples is greater than zero. In ES3.1(section 9.2.5), it can support integer
    // multisample renderbuffer, but the samples should not be greater than MAX_INTEGER_SAMPLES.
    const InternalFormat &formatInfo = GetSizedInternalFormatInfo(internalformat);
    if (formatInfo.isInt())
    {
        if ((samples > 0 && context->getClientVersion() == ES_3_0) ||
            samples > context->getCaps().maxIntegerSamples)
        {
            context->validationError(GL_INVALID_OPERATION, kSamplesOutOfRange);
            return false;
        }
    }

    // The behavior is different than the ANGLE version, which would generate a GL_OUT_OF_MEMORY.
    const TextureCaps &formatCaps = context->getTextureCaps().get(internalformat);
    if (static_cast<GLuint>(samples) > formatCaps.getMaxSamples())
    {
        context->validationError(GL_INVALID_OPERATION, kSamplesOutOfRange);
        return false;
    }

    return true;
}

bool ValidateVertexAttribIPointer(const Context *context,
                                  GLuint index,
                                  GLint size,
                                  VertexAttribType type,
                                  GLsizei stride,
                                  const void *pointer)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (!ValidateIntegerVertexFormat(context, index, size, type))
    {
        return false;
    }

    if (stride < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeStride);
        return false;
    }

    const Caps &caps = context->getCaps();
    if (context->getClientVersion() >= ES_3_1)
    {
        if (stride > caps.maxVertexAttribStride)
        {
            context->validationError(GL_INVALID_VALUE, kExceedsMaxVertexAttribStride);
            return false;
        }

        // [OpenGL ES 3.1] Section 10.3.1 page 245:
        // glVertexAttribBinding is part of the equivalent code of VertexAttribIPointer, so its
        // validation should be inherited.
        if (index >= static_cast<GLuint>(caps.maxVertexAttribBindings))
        {
            context->validationError(GL_INVALID_VALUE, kExceedsMaxVertexAttribBindings);
            return false;
        }
    }

    // [OpenGL ES 3.0.2] Section 2.8 page 24:
    // An INVALID_OPERATION error is generated when a non-zero vertex array object
    // is bound, zero is bound to the ARRAY_BUFFER buffer object binding point,
    // and the pointer argument is not NULL.
    if (context->getState().getVertexArrayId().value != 0 &&
        context->getState().getTargetBuffer(BufferBinding::Array) == 0 && pointer != nullptr)
    {
        context->validationError(GL_INVALID_OPERATION, kClientDataInVertexArray);
        return false;
    }

    if (context->getExtensions().webglCompatibility)
    {
        if (!ValidateWebGLVertexAttribPointer(context, type, false, stride, pointer, true))
        {
            return false;
        }
    }

    return true;
}

bool ValidateGetSynciv(const Context *context,
                       GLsync sync,
                       GLenum pname,
                       GLsizei bufSize,
                       const GLsizei *length,
                       const GLint *values)
{
    if ((context->getClientMajorVersion() < 3) && !context->getExtensions().glSyncARB)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (bufSize < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeBufferSize);
        return false;
    }

    if (context->isContextLost())
    {
        context->validationError(GL_CONTEXT_LOST, kContextLost);

        if (pname == GL_SYNC_STATUS)
        {
            // Generate an error but still return true, the context still needs to return a
            // value in this case.
            return true;
        }
        else
        {
            return false;
        }
    }

    Sync *syncObject = context->getSync(sync);
    if (!syncObject)
    {
        context->validationError(GL_INVALID_VALUE, kSyncMissing);
        return false;
    }

    switch (pname)
    {
        case GL_OBJECT_TYPE:
        case GL_SYNC_CONDITION:
        case GL_SYNC_FLAGS:
        case GL_SYNC_STATUS:
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kInvalidPname);
            return false;
    }

    return true;
}

bool ValidateDrawElementsInstanced(const Context *context,
                                   PrimitiveMode mode,
                                   GLsizei count,
                                   DrawElementsType type,
                                   const void *indices,
                                   GLsizei instanceCount)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateDrawElementsInstancedBase(context, mode, count, type, indices, instanceCount);
}

bool ValidateMultiDrawArraysInstancedANGLE(const Context *context,
                                           PrimitiveMode mode,
                                           const GLint *firsts,
                                           const GLsizei *counts,
                                           const GLsizei *instanceCounts,
                                           GLsizei drawcount)
{
    if (!context->getExtensions().multiDraw)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }
    if (context->getClientMajorVersion() < 3)
    {
        if (!context->getExtensions().instancedArraysAny())
        {
            context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
            return false;
        }
        if (!ValidateDrawInstancedANGLE(context))
        {
            return false;
        }
    }
    for (GLsizei drawID = 0; drawID < drawcount; ++drawID)
    {
        if (!ValidateDrawArraysInstancedBase(context, mode, firsts[drawID], counts[drawID],
                                             instanceCounts[drawID]))
        {
            return false;
        }
    }
    return true;
}

bool ValidateMultiDrawElementsInstancedANGLE(const Context *context,
                                             PrimitiveMode mode,
                                             const GLsizei *counts,
                                             DrawElementsType type,
                                             const GLvoid *const *indices,
                                             const GLsizei *instanceCounts,
                                             GLsizei drawcount)
{
    if (!context->getExtensions().multiDraw)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }
    if (context->getClientMajorVersion() < 3)
    {
        if (!context->getExtensions().instancedArraysAny())
        {
            context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
            return false;
        }
        if (!ValidateDrawInstancedANGLE(context))
        {
            return false;
        }
    }
    for (GLsizei drawID = 0; drawID < drawcount; ++drawID)
    {
        if (!ValidateDrawElementsInstancedBase(context, mode, counts[drawID], type, indices[drawID],
                                               instanceCounts[drawID]))
        {
            return false;
        }
    }
    return true;
}

bool ValidateDrawArraysInstancedBaseInstanceANGLE(const Context *context,
                                                  PrimitiveMode mode,
                                                  GLint first,
                                                  GLsizei count,
                                                  GLsizei instanceCount,
                                                  GLuint baseInstance)
{
    if (!context->getExtensions().baseVertexBaseInstance)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }
    if (!ValidateDrawArraysInstancedBase(context, mode, first, count, instanceCount))
    {
        return false;
    }
    return true;
}

bool ValidateDrawElementsInstancedBaseVertexBaseInstanceANGLE(const Context *context,
                                                              PrimitiveMode mode,
                                                              GLsizei count,
                                                              DrawElementsType type,
                                                              const GLvoid *indices,
                                                              GLsizei instanceCounts,
                                                              GLint baseVertex,
                                                              GLuint baseInstance)
{
    if (!context->getExtensions().baseVertexBaseInstance)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }
    if (!ValidateDrawElementsInstancedBase(context, mode, count, type, indices, instanceCounts))
    {
        return false;
    }
    return true;
}

bool ValidateMultiDrawArraysInstancedBaseInstanceANGLE(const Context *context,
                                                       PrimitiveMode modePacked,
                                                       const GLint *firsts,
                                                       const GLsizei *counts,
                                                       const GLsizei *instanceCounts,
                                                       const GLuint *baseInstances,
                                                       GLsizei drawcount)
{
    if (!context->getExtensions().multiDraw)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }
    if (drawcount < 0)
    {
        return false;
    }
    for (GLsizei drawID = 0; drawID < drawcount; ++drawID)
    {
        if (!ValidateDrawArraysInstancedBase(context, modePacked, firsts[drawID], counts[drawID],
                                             instanceCounts[drawID]))
        {
            return false;
        }
    }
    return true;
}

bool ValidateMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE(const Context *context,
                                                                   PrimitiveMode modePacked,
                                                                   const GLsizei *counts,
                                                                   DrawElementsType typePacked,
                                                                   const GLvoid *const *indices,
                                                                   const GLsizei *instanceCounts,
                                                                   const GLint *baseVertices,
                                                                   const GLuint *baseInstances,
                                                                   GLsizei drawcount)
{
    if (!context->getExtensions().multiDraw)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }
    if (drawcount < 0)
    {
        return false;
    }
    for (GLsizei drawID = 0; drawID < drawcount; ++drawID)
    {
        if (!ValidateDrawElementsInstancedBase(context, modePacked, counts[drawID], typePacked,
                                               indices[drawID], instanceCounts[drawID]))
        {
            return false;
        }
    }
    return true;
}

bool ValidateFramebufferTextureMultiviewOVR(const Context *context,
                                            GLenum target,
                                            GLenum attachment,
                                            TextureID texture,
                                            GLint level,
                                            GLint baseViewIndex,
                                            GLsizei numViews)
{
    if (!ValidateFramebufferTextureMultiviewBaseANGLE(context, target, attachment, texture, level,
                                                      numViews))
    {
        return false;
    }

    if (texture.value != 0)
    {
        if (baseViewIndex < 0)
        {
            context->validationError(GL_INVALID_VALUE, kNegativeBaseViewIndex);
            return false;
        }

        Texture *tex = context->getTexture(texture);
        ASSERT(tex);

        switch (tex->getType())
        {
            case TextureType::_2DArray:
            case TextureType::_2DMultisampleArray:
            {
                if (tex->getType() == TextureType::_2DMultisampleArray)
                {
                    if (!context->getExtensions().multiviewMultisample)
                    {
                        context->validationError(GL_INVALID_OPERATION, kInvalidTextureType);
                        return false;
                    }
                }

                const Caps &caps = context->getCaps();
                if (baseViewIndex + numViews > caps.maxArrayTextureLayers)
                {
                    context->validationError(GL_INVALID_VALUE, kViewsExceedMaxArrayLayers);
                    return false;
                }

                break;
            }
            default:
                context->validationError(GL_INVALID_OPERATION, kInvalidTextureType);
                return false;
        }

        if (!ValidateFramebufferTextureMultiviewLevelAndFormat(context, tex, level))
        {
            return false;
        }
    }

    return true;
}

bool ValidateUniform1ui(const Context *context, UniformLocation location, GLuint v0)
{
    return ValidateUniformES3(context, GL_UNSIGNED_INT, location, 1);
}

bool ValidateUniform2ui(const Context *context, UniformLocation location, GLuint v0, GLuint v1)
{
    return ValidateUniformES3(context, GL_UNSIGNED_INT_VEC2, location, 1);
}

bool ValidateUniform3ui(const Context *context,
                        UniformLocation location,
                        GLuint v0,
                        GLuint v1,
                        GLuint v2)
{
    return ValidateUniformES3(context, GL_UNSIGNED_INT_VEC3, location, 1);
}

bool ValidateUniform4ui(const Context *context,
                        UniformLocation location,
                        GLuint v0,
                        GLuint v1,
                        GLuint v2,
                        GLuint v3)
{
    return ValidateUniformES3(context, GL_UNSIGNED_INT_VEC4, location, 1);
}

bool ValidateUniform1uiv(const Context *context,
                         UniformLocation location,
                         GLsizei count,
                         const GLuint *value)
{
    return ValidateUniformES3(context, GL_UNSIGNED_INT, location, count);
}

bool ValidateUniform2uiv(const Context *context,
                         UniformLocation location,
                         GLsizei count,
                         const GLuint *value)
{
    return ValidateUniformES3(context, GL_UNSIGNED_INT_VEC2, location, count);
}

bool ValidateUniform3uiv(const Context *context,
                         UniformLocation location,
                         GLsizei count,
                         const GLuint *value)
{
    return ValidateUniformES3(context, GL_UNSIGNED_INT_VEC3, location, count);
}

bool ValidateUniform4uiv(const Context *context,
                         UniformLocation location,
                         GLsizei count,
                         const GLuint *value)
{
    return ValidateUniformES3(context, GL_UNSIGNED_INT_VEC4, location, count);
}

bool ValidateIsQuery(const Context *context, QueryID id)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return true;
}

bool ValidateUniformMatrix2x3fv(const Context *context,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat *value)
{
    return ValidateUniformMatrixES3(context, GL_FLOAT_MAT2x3, location, count, transpose);
}

bool ValidateUniformMatrix3x2fv(const Context *context,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat *value)
{
    return ValidateUniformMatrixES3(context, GL_FLOAT_MAT3x2, location, count, transpose);
}

bool ValidateUniformMatrix2x4fv(const Context *context,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat *value)
{
    return ValidateUniformMatrixES3(context, GL_FLOAT_MAT2x4, location, count, transpose);
}

bool ValidateUniformMatrix4x2fv(const Context *context,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat *value)
{
    return ValidateUniformMatrixES3(context, GL_FLOAT_MAT4x2, location, count, transpose);
}

bool ValidateUniformMatrix3x4fv(const Context *context,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat *value)
{
    return ValidateUniformMatrixES3(context, GL_FLOAT_MAT3x4, location, count, transpose);
}

bool ValidateUniformMatrix4x3fv(const Context *context,
                                UniformLocation location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat *value)
{
    return ValidateUniformMatrixES3(context, GL_FLOAT_MAT4x3, location, count, transpose);
}

bool ValidateEndTransformFeedback(const Context *context)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    TransformFeedback *transformFeedback = context->getState().getCurrentTransformFeedback();
    ASSERT(transformFeedback != nullptr);

    if (!transformFeedback->isActive())
    {
        context->validationError(GL_INVALID_OPERATION, kTransformFeedbackNotActive);
        return false;
    }

    return true;
}

bool ValidateTransformFeedbackVaryings(const Context *context,
                                       ShaderProgramID program,
                                       GLsizei count,
                                       const GLchar *const *varyings,
                                       GLenum bufferMode)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (count < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeCount);
        return false;
    }

    switch (bufferMode)
    {
        case GL_INTERLEAVED_ATTRIBS:
            break;
        case GL_SEPARATE_ATTRIBS:
        {
            const Caps &caps = context->getCaps();
            if (count > caps.maxTransformFeedbackSeparateAttributes)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidTransformFeedbackAttribsCount);
                return false;
            }
            break;
        }
        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    return true;
}

bool ValidateGetTransformFeedbackVarying(const Context *context,
                                         ShaderProgramID program,
                                         GLuint index,
                                         GLsizei bufSize,
                                         const GLsizei *length,
                                         const GLsizei *size,
                                         const GLenum *type,
                                         const GLchar *name)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (bufSize < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeBufferSize);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    if (index >= static_cast<GLuint>(programObject->getTransformFeedbackVaryingCount()))
    {
        context->validationError(GL_INVALID_VALUE, kTransformFeedbackVaryingIndexOutOfRange);
        return false;
    }

    return true;
}

bool ValidateBindTransformFeedback(const Context *context, GLenum target, TransformFeedbackID id)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    switch (target)
    {
        case GL_TRANSFORM_FEEDBACK:
        {
            // Cannot bind a transform feedback object if the current one is started and not
            // paused (3.0.2 pg 85 section 2.14.1)
            if (context->getState().isTransformFeedbackActiveUnpaused())
            {
                context->validationError(GL_INVALID_OPERATION, kTransformFeedbackNotPaused);
                return false;
            }

            // Cannot bind a transform feedback object that does not exist (3.0.2 pg 85 section
            // 2.14.1)
            if (!context->isTransformFeedbackGenerated(id))
            {
                context->validationError(GL_INVALID_OPERATION, kTransformFeedbackDoesNotExist);
                return false;
            }
        }
        break;

        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    return true;
}

bool ValidateIsTransformFeedback(const Context *context, TransformFeedbackID id)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return true;
}

bool ValidatePauseTransformFeedback(const Context *context)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    TransformFeedback *transformFeedback = context->getState().getCurrentTransformFeedback();
    ASSERT(transformFeedback != nullptr);

    // Current transform feedback must be active and not paused in order to pause (3.0.2 pg 86)
    if (!transformFeedback->isActive())
    {
        context->validationError(GL_INVALID_OPERATION, kTransformFeedbackNotActive);
        return false;
    }

    if (transformFeedback->isPaused())
    {
        context->validationError(GL_INVALID_OPERATION, kTransformFeedbackPaused);
        return false;
    }

    return true;
}

bool ValidateResumeTransformFeedback(const Context *context)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    TransformFeedback *transformFeedback = context->getState().getCurrentTransformFeedback();
    ASSERT(transformFeedback != nullptr);

    // Current transform feedback must be active and paused in order to resume (3.0.2 pg 86)
    if (!transformFeedback->isActive())
    {
        context->validationError(GL_INVALID_OPERATION, kTransformFeedbackNotActive);
        return false;
    }

    if (!transformFeedback->isPaused())
    {
        context->validationError(GL_INVALID_OPERATION, kTransformFeedbackNotPaused);
        return false;
    }

    return true;
}

bool ValidateVertexAttribI4i(const Context *context,
                             GLuint index,
                             GLint x,
                             GLint y,
                             GLint z,
                             GLint w)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttribI4ui(const Context *context,
                              GLuint index,
                              GLuint x,
                              GLuint y,
                              GLuint z,
                              GLuint w)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttribI4iv(const Context *context, GLuint index, const GLint *v)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateVertexAttribIndex(context, index);
}

bool ValidateVertexAttribI4uiv(const Context *context, GLuint index, const GLuint *v)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateVertexAttribIndex(context, index);
}

bool ValidateGetFragDataLocation(const Context *context,
                                 ShaderProgramID program,
                                 const GLchar *name)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    if (!programObject->isLinked())
    {
        context->validationError(GL_INVALID_OPERATION, kProgramNotLinked);
        return false;
    }

    return true;
}

bool ValidateGetUniformIndices(const Context *context,
                               ShaderProgramID program,
                               GLsizei uniformCount,
                               const GLchar *const *uniformNames,
                               const GLuint *uniformIndices)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (uniformCount < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeCount);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    return true;
}

bool ValidateGetActiveUniformsiv(const Context *context,
                                 ShaderProgramID program,
                                 GLsizei uniformCount,
                                 const GLuint *uniformIndices,
                                 GLenum pname,
                                 const GLint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (uniformCount < 0)
    {
        context->validationError(GL_INVALID_VALUE, kNegativeCount);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    switch (pname)
    {
        case GL_UNIFORM_TYPE:
        case GL_UNIFORM_SIZE:
            break;
        case GL_UNIFORM_NAME_LENGTH:
            if (context->getExtensions().webglCompatibility)
            {
                context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
                return false;
            }
            break;
        case GL_UNIFORM_BLOCK_INDEX:
        case GL_UNIFORM_OFFSET:
        case GL_UNIFORM_ARRAY_STRIDE:
        case GL_UNIFORM_MATRIX_STRIDE:
        case GL_UNIFORM_IS_ROW_MAJOR:
            break;

        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }

    if (uniformCount > programObject->getActiveUniformCount())
    {
        context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxActiveUniform);
        return false;
    }

    for (int uniformId = 0; uniformId < uniformCount; uniformId++)
    {
        const GLuint index = uniformIndices[uniformId];

        if (index >= static_cast<GLuint>(programObject->getActiveUniformCount()))
        {
            context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxActiveUniform);
            return false;
        }
    }

    return true;
}

bool ValidateGetUniformBlockIndex(const Context *context,
                                  ShaderProgramID program,
                                  const GLchar *uniformBlockName)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    return true;
}

bool ValidateGetActiveUniformBlockiv(const Context *context,
                                     ShaderProgramID program,
                                     UniformBlockIndex uniformBlockIndex,
                                     GLenum pname,
                                     const GLint *params)
{
    return ValidateGetActiveUniformBlockivBase(context, program, uniformBlockIndex, pname, nullptr);
}

bool ValidateGetActiveUniformBlockName(const Context *context,
                                       ShaderProgramID program,
                                       UniformBlockIndex uniformBlockIndex,
                                       GLsizei bufSize,
                                       const GLsizei *length,
                                       const GLchar *uniformBlockName)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    if (uniformBlockIndex.value >= programObject->getActiveUniformBlockCount())
    {
        context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxActiveUniformBlock);
        return false;
    }

    return true;
}

bool ValidateUniformBlockBinding(const Context *context,
                                 ShaderProgramID program,
                                 UniformBlockIndex uniformBlockIndex,
                                 GLuint uniformBlockBinding)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (uniformBlockBinding >= static_cast<GLuint>(context->getCaps().maxUniformBufferBindings))
    {
        context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxUniformBufferBindings);
        return false;
    }

    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }

    // if never linked, there won't be any uniform blocks
    if (uniformBlockIndex.value >= programObject->getActiveUniformBlockCount())
    {
        context->validationError(GL_INVALID_VALUE, kIndexExceedsMaxUniformBufferBindings);
        return false;
    }

    return true;
}

bool ValidateDrawArraysInstanced(const Context *context,
                                 PrimitiveMode mode,
                                 GLint first,
                                 GLsizei count,
                                 GLsizei primcount)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateDrawArraysInstancedBase(context, mode, first, count, primcount);
}

bool ValidateFenceSync(const Context *context, GLenum condition, GLbitfield flags)
{
    if ((context->getClientMajorVersion() < 3) && !context->getExtensions().glSyncARB)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (condition != GL_SYNC_GPU_COMMANDS_COMPLETE)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidFenceCondition);
        return false;
    }

    if (flags != 0)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidFlags);
        return false;
    }

    return true;
}

bool ValidateIsSync(const Context *context, GLsync sync)
{
    if ((context->getClientMajorVersion() < 3) && !context->getExtensions().glSyncARB)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return true;
}

bool ValidateDeleteSync(const Context *context, GLsync sync)
{
    if ((context->getClientMajorVersion() < 3) && !context->getExtensions().glSyncARB)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (sync != static_cast<GLsync>(0) && !context->getSync(sync))
    {
        context->validationError(GL_INVALID_VALUE, kSyncMissing);
        return false;
    }

    return true;
}

bool ValidateClientWaitSync(const Context *context, GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    if ((context->getClientMajorVersion() < 3) && !context->getExtensions().glSyncARB)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if ((flags & ~(GL_SYNC_FLUSH_COMMANDS_BIT)) != 0)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidFlags);
        return false;
    }

    Sync *clientWaitSync = context->getSync(sync);
    if (!clientWaitSync)
    {
        context->validationError(GL_INVALID_VALUE, kSyncMissing);
        return false;
    }

    return true;
}

bool ValidateWaitSync(const Context *context, GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    if ((context->getClientMajorVersion() < 3) && !context->getExtensions().glSyncARB)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (flags != 0)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidFlags);
        return false;
    }

    if (timeout != GL_TIMEOUT_IGNORED)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidTimeout);
        return false;
    }

    Sync *waitSync = context->getSync(sync);
    if (!waitSync)
    {
        context->validationError(GL_INVALID_VALUE, kSyncMissing);
        return false;
    }

    return true;
}

bool ValidateGetInteger64v(const Context *context, GLenum pname, const GLint64 *params)
{
    if ((context->getClientMajorVersion() < 3) && !context->getExtensions().glSyncARB)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    GLenum nativeType      = GL_NONE;
    unsigned int numParams = 0;
    if (!ValidateStateQuery(context, pname, &nativeType, &numParams))
    {
        return false;
    }

    return true;
}

bool ValidateIsSampler(const Context *context, SamplerID sampler)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return true;
}

bool ValidateBindSampler(const Context *context, GLuint unit, SamplerID sampler)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (GetIDValue(sampler) != 0 && !context->isSampler(sampler))
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidSampler);
        return false;
    }

    if (unit >= static_cast<GLuint>(context->getCaps().maxCombinedTextureImageUnits))
    {
        context->validationError(GL_INVALID_VALUE, kInvalidCombinedImageUnit);
        return false;
    }

    return true;
}

bool ValidateVertexAttribDivisor(const Context *context, GLuint index, GLuint divisor)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    return ValidateVertexAttribIndex(context, index);
}

bool ValidateTexStorage2D(const Context *context,
                          TextureType target,
                          GLsizei levels,
                          GLenum internalformat,
                          GLsizei width,
                          GLsizei height)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (!ValidateES3TexStorage2DParameters(context, target, levels, internalformat, width, height,
                                           1))
    {
        return false;
    }

    return true;
}

bool ValidateTexStorage3D(const Context *context,
                          TextureType target,
                          GLsizei levels,
                          GLenum internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLsizei depth)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }

    if (!ValidateES3TexStorage3DParameters(context, target, levels, internalformat, width, height,
                                           depth))
    {
        return false;
    }

    return true;
}

bool ValidateGetBufferParameteri64v(const Context *context,
                                    BufferBinding target,
                                    GLenum pname,
                                    const GLint64 *params)
{
    return ValidateGetBufferParameterBase(context, target, pname, false, nullptr);
}

bool ValidateGetSamplerParameterfv(const Context *context,
                                   SamplerID sampler,
                                   GLenum pname,
                                   const GLfloat *params)
{
    return ValidateGetSamplerParameterBase(context, sampler, pname, nullptr);
}

bool ValidateGetSamplerParameteriv(const Context *context,
                                   SamplerID sampler,
                                   GLenum pname,
                                   const GLint *params)
{
    return ValidateGetSamplerParameterBase(context, sampler, pname, nullptr);
}

bool ValidateGetSamplerParameterIivOES(const Context *context,
                                       SamplerID sampler,
                                       GLenum pname,
                                       const GLint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateGetSamplerParameterBase(context, sampler, pname, nullptr);
}

bool ValidateGetSamplerParameterIuivOES(const Context *context,
                                        SamplerID sampler,
                                        GLenum pname,
                                        const GLuint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateGetSamplerParameterBase(context, sampler, pname, nullptr);
}

bool ValidateSamplerParameterf(const Context *context,
                               SamplerID sampler,
                               GLenum pname,
                               GLfloat param)
{
    return ValidateSamplerParameterBase(context, sampler, pname, -1, false, &param);
}

bool ValidateSamplerParameterfv(const Context *context,
                                SamplerID sampler,
                                GLenum pname,
                                const GLfloat *params)
{
    return ValidateSamplerParameterBase(context, sampler, pname, -1, true, params);
}

bool ValidateSamplerParameteri(const Context *context, SamplerID sampler, GLenum pname, GLint param)
{
    return ValidateSamplerParameterBase(context, sampler, pname, -1, false, &param);
}

bool ValidateSamplerParameteriv(const Context *context,
                                SamplerID sampler,
                                GLenum pname,
                                const GLint *params)
{
    return ValidateSamplerParameterBase(context, sampler, pname, -1, true, params);
}

bool ValidateSamplerParameterIivOES(const Context *context,
                                    SamplerID sampler,
                                    GLenum pname,
                                    const GLint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateSamplerParameterBase(context, sampler, pname, -1, true, params);
}

bool ValidateSamplerParameterIuivOES(const Context *context,
                                     SamplerID sampler,
                                     GLenum pname,
                                     const GLuint *params)
{
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    return ValidateSamplerParameterBase(context, sampler, pname, -1, true, params);
}

bool ValidateGetVertexAttribIiv(const Context *context,
                                GLuint index,
                                GLenum pname,
                                const GLint *params)
{
    return ValidateGetVertexAttribBase(context, index, pname, nullptr, false, true);
}

bool ValidateGetVertexAttribIuiv(const Context *context,
                                 GLuint index,
                                 GLenum pname,
                                 const GLuint *params)
{
    return ValidateGetVertexAttribBase(context, index, pname, nullptr, false, true);
}

bool ValidateGetInternalformativ(const Context *context,
                                 GLenum target,
                                 GLenum internalformat,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 const GLint *params)
{
    return ValidateGetInternalFormativBase(context, target, internalformat, pname, bufSize,
                                           nullptr);
}

bool ValidateBindFragDataLocationIndexedEXT(const Context *context,
                                            ShaderProgramID program,
                                            GLuint colorNumber,
                                            GLuint index,
                                            const char *name)
{
    if (!context->getExtensions().blendFuncExtended)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    if (index > 1)
    {
        // This error is not explicitly specified but the spec does say that "<index> may be zero or
        // one to specify that the color be used as either the first or second color input to the
        // blend equation, respectively"
        context->validationError(GL_INVALID_VALUE, kFragDataBindingIndexOutOfRange);
        return false;
    }
    if (index == 1)
    {
        if (colorNumber >= context->getExtensions().maxDualSourceDrawBuffers)
        {
            context->validationError(GL_INVALID_VALUE,
                                     kColorNumberGreaterThanMaxDualSourceDrawBuffers);
            return false;
        }
    }
    else
    {
        if (colorNumber >= static_cast<GLuint>(context->getCaps().maxDrawBuffers))
        {
            context->validationError(GL_INVALID_VALUE, kColorNumberGreaterThanMaxDrawBuffers);
            return false;
        }
    }
    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }
    return true;
}

bool ValidateBindFragDataLocationEXT(const Context *context,
                                     ShaderProgramID program,
                                     GLuint colorNumber,
                                     const char *name)
{
    return ValidateBindFragDataLocationIndexedEXT(context, program, colorNumber, 0u, name);
}

bool ValidateGetFragDataIndexEXT(const Context *context, ShaderProgramID program, const char *name)
{
    if (!context->getExtensions().blendFuncExtended)
    {
        context->validationError(GL_INVALID_OPERATION, kExtensionNotEnabled);
        return false;
    }
    if (context->getClientMajorVersion() < 3)
    {
        context->validationError(GL_INVALID_OPERATION, kES3Required);
        return false;
    }
    Program *programObject = GetValidProgram(context, program);
    if (!programObject)
    {
        return false;
    }
    if (!programObject->isLinked())
    {
        context->validationError(GL_INVALID_OPERATION, kProgramNotLinked);
        return false;
    }
    return true;
}

bool ValidateTexStorage2DMultisampleANGLE(const Context *context,
                                          TextureType target,
                                          GLsizei samples,
                                          GLenum internalFormat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLboolean fixedSampleLocations)
{
    if (!context->getExtensions().textureMultisample)
    {
        context->validationError(GL_INVALID_OPERATION, kMultisampleTextureExtensionOrES31Required);
        return false;
    }

    return ValidateTexStorage2DMultisampleBase(context, target, samples, internalFormat, width,
                                               height);
}

bool ValidateGetTexLevelParameterfvANGLE(const Context *context,
                                         TextureTarget target,
                                         GLint level,
                                         GLenum pname,
                                         const GLfloat *params)
{
    if (!context->getExtensions().textureMultisample &&
        !context->getExtensions().getTexLevelParameterANGLE)
    {
        context->validationError(
            GL_INVALID_OPERATION,
            kMultisampleTextureExtensionOrGetTexLevelParameterExtensionOrES31Required);
        return false;
    }

    return ValidateGetTexLevelParameterBase(context, target, level, pname, nullptr);
}

bool ValidateGetTexLevelParameterivANGLE(const Context *context,
                                         TextureTarget target,
                                         GLint level,
                                         GLenum pname,
                                         const GLint *params)
{
    if (!context->getExtensions().textureMultisample &&
        !context->getExtensions().getTexLevelParameterANGLE)
    {
        context->validationError(
            GL_INVALID_OPERATION,
            kMultisampleTextureExtensionOrGetTexLevelParameterExtensionOrES31Required);
        return false;
    }

    return ValidateGetTexLevelParameterBase(context, target, level, pname, nullptr);
}

bool ValidateGetMultisamplefvANGLE(const Context *context,
                                   GLenum pname,
                                   GLuint index,
                                   const GLfloat *val)
{
    if (!context->getExtensions().textureMultisample)
    {
        context->validationError(GL_INVALID_OPERATION, kMultisampleTextureExtensionOrES31Required);
        return false;
    }

    return ValidateGetMultisamplefvBase(context, pname, index, val);
}

bool ValidateSampleMaskiANGLE(const Context *context, GLuint maskNumber, GLbitfield mask)
{
    if (!context->getExtensions().textureMultisample)
    {
        context->validationError(GL_INVALID_OPERATION, kMultisampleTextureExtensionOrES31Required);
        return false;
    }

    return ValidateSampleMaskiBase(context, maskNumber, mask);
}
}  // namespace gl
