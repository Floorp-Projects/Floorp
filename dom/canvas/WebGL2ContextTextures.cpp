/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "WebGLContextUtils.h"
#include "GLContext.h"

using namespace mozilla;
using namespace mozilla::dom;

bool
WebGL2Context::ValidateSizedInternalFormat(GLenum internalformat, const char* info)
{
    switch (internalformat) {
        // Sized Internal Formats
        // https://www.khronos.org/opengles/sdk/docs/man3/html/glTexStorage2D.xhtml
    case LOCAL_GL_R8:
    case LOCAL_GL_R8_SNORM:
    case LOCAL_GL_R16F:
    case LOCAL_GL_R32F:
    case LOCAL_GL_R8UI:
    case LOCAL_GL_R8I:
    case LOCAL_GL_R16UI:
    case LOCAL_GL_R16I:
    case LOCAL_GL_R32UI:
    case LOCAL_GL_R32I:
    case LOCAL_GL_RG8:
    case LOCAL_GL_RG8_SNORM:
    case LOCAL_GL_RG16F:
    case LOCAL_GL_RG32F:
    case LOCAL_GL_RG8UI:
    case LOCAL_GL_RG8I:
    case LOCAL_GL_RG16UI:
    case LOCAL_GL_RG16I:
    case LOCAL_GL_RG32UI:
    case LOCAL_GL_RG32I:
    case LOCAL_GL_RGB8:
    case LOCAL_GL_SRGB8:
    case LOCAL_GL_RGB565:
    case LOCAL_GL_RGB8_SNORM:
    case LOCAL_GL_R11F_G11F_B10F:
    case LOCAL_GL_RGB9_E5:
    case LOCAL_GL_RGB16F:
    case LOCAL_GL_RGB32F:
    case LOCAL_GL_RGB8UI:
    case LOCAL_GL_RGB8I:
    case LOCAL_GL_RGB16UI:
    case LOCAL_GL_RGB16I:
    case LOCAL_GL_RGB32UI:
    case LOCAL_GL_RGB32I:
    case LOCAL_GL_RGBA8:
    case LOCAL_GL_SRGB8_ALPHA8:
    case LOCAL_GL_RGBA8_SNORM:
    case LOCAL_GL_RGB5_A1:
    case LOCAL_GL_RGBA4:
    case LOCAL_GL_RGB10_A2:
    case LOCAL_GL_RGBA16F:
    case LOCAL_GL_RGBA32F:
    case LOCAL_GL_RGBA8UI:
    case LOCAL_GL_RGBA8I:
    case LOCAL_GL_RGB10_A2UI:
    case LOCAL_GL_RGBA16UI:
    case LOCAL_GL_RGBA16I:
    case LOCAL_GL_RGBA32I:
    case LOCAL_GL_RGBA32UI:
    case LOCAL_GL_DEPTH_COMPONENT16:
    case LOCAL_GL_DEPTH_COMPONENT24:
    case LOCAL_GL_DEPTH_COMPONENT32F:
    case LOCAL_GL_DEPTH24_STENCIL8:
    case LOCAL_GL_DEPTH32F_STENCIL8:
        return true;
    }

    if (IsCompressedTextureFormat(internalformat)) {
        return true;
    }

    const char* name = EnumName(internalformat);
    if (name && name[0] != '[')
        ErrorInvalidEnum("%s: invalid internal format %s", info, name);
    else
        ErrorInvalidEnum("%s: invalid internal format 0x%04X", info, internalformat);

    return false;
}

/** Validates parameters to texStorage{2D,3D} */
bool
WebGL2Context::ValidateTexStorage(GLenum target, GLsizei levels, GLenum internalformat,
                                      GLsizei width, GLsizei height, GLsizei depth,
                                      const char* info)
{
    // GL_INVALID_OPERATION is generated if the default texture object is curently bound to target.
    WebGLTexture* tex = activeBoundTextureForTarget(target);
    if (!tex) {
        ErrorInvalidOperation("%s: no texture is bound to target %s", info, EnumName(target));
        return false;
    }

    // GL_INVALID_OPERATION is generated if the texture object currently bound to target already has
    // GL_TEXTURE_IMMUTABLE_FORMAT set to GL_TRUE.
    if (tex->IsImmutable()) {
        ErrorInvalidOperation("%s: texture bound to target %s is already immutable", info, EnumName(target));
        return false;
    }

    // GL_INVALID_ENUM is generated if internalformat is not a valid sized internal format.
    if (!ValidateSizedInternalFormat(internalformat, info))
        return false;

    // GL_INVALID_VALUE is generated if width, height or levels are less than 1.
    if (width < 1)  { ErrorInvalidValue("%s: width is < 1", info);  return false; }
    if (height < 1) { ErrorInvalidValue("%s: height is < 1", info); return false; }
    if (depth < 1)  { ErrorInvalidValue("%s: depth is < 1", info);  return false; }
    if (levels < 1) { ErrorInvalidValue("%s: levels is < 1", info); return false; }

    // GL_INVALID_OPERATION is generated if levels is greater than floor(log2(max(width, height, depth)))+1.
    if (FloorLog2(std::max(std::max(width, height), depth)) + 1 < levels) {
        ErrorInvalidOperation("%s: too many levels for given texture dimensions", info);
        return false;
    }

    return true;
}

// -------------------------------------------------------------------------
// Texture objects

void
WebGL2Context::TexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
    if (IsContextLost())
        return;

    // GL_INVALID_ENUM is generated if target is not one of the accepted target enumerants.
    if (target != LOCAL_GL_TEXTURE_2D && target != LOCAL_GL_TEXTURE_CUBE_MAP)
        return ErrorInvalidEnum("texStorage2D: target is not TEXTURE_2D or TEXTURE_CUBE_MAP");

    if (!ValidateTexStorage(target, levels, internalformat, width, height, 1, "texStorage2D"))
        return;

    WebGLTexture* tex = activeBoundTextureForTarget(target);
    tex->SetImmutable();

    const size_t facesCount = (target == LOCAL_GL_TEXTURE_2D) ? 1 : 6;
    GLsizei w = width;
    GLsizei h = height;
    for (size_t l = 0; l < size_t(levels); l++) {
        for (size_t f = 0; f < facesCount; f++) {
            tex->SetImageInfo(TexImageTargetForTargetAndFace(target, f),
                              l, w, h, 1,
                              internalformat,
                              WebGLImageDataStatus::UninitializedImageData);
        }
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
    }

    gl->fTexStorage2D(target, levels, internalformat, width, height);
}

void
WebGL2Context::TexStorage3D(GLenum target, GLsizei levels, GLenum internalformat,
                            GLsizei width, GLsizei height, GLsizei depth)
{
    if (IsContextLost())
        return;

    // GL_INVALID_ENUM is generated if target is not one of the accepted target enumerants.
    if (target != LOCAL_GL_TEXTURE_3D)
        return ErrorInvalidEnum("texStorage3D: target is not TEXTURE_3D");

    if (!ValidateTexStorage(target, levels, internalformat, width, height, depth, "texStorage3D"))
        return;

    WebGLTexture* tex = activeBoundTextureForTarget(target);
    tex->SetImmutable();

    GLsizei w = width;
    GLsizei h = height;
    GLsizei d = depth;
    for (size_t l = 0; l < size_t(levels); l++) {
        tex->SetImageInfo(TexImageTargetForTargetAndFace(target, 0),
                          l, w, h, d,
                          internalformat,
                          WebGLImageDataStatus::UninitializedImageData);
        w = std::max(1, w >> 1);
        h = std::max(1, h >> 1);
        d = std::max(1, d >> 1);
    }

    gl->fTexStorage3D(target, levels, internalformat, width, height, depth);
}

void
WebGL2Context::TexSubImage3D(GLenum rawTarget, GLint level,
                             GLint xoffset, GLint yoffset, GLint zoffset,
                             GLsizei width, GLsizei height, GLsizei depth,
                             GLenum format, GLenum type, const Nullable<dom::ArrayBufferView>& pixels,
                             ErrorResult& rv)
{
    if (IsContextLost())
        return;

    if (pixels.IsNull())
        return ErrorInvalidValue("texSubImage3D: pixels must not be null!");

    const ArrayBufferView& view = pixels.Value();
    view.ComputeLengthAndData();

    const WebGLTexImageFunc func = WebGLTexImageFunc::TexSubImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex3D;

    if (!ValidateTexImageTarget(rawTarget, func, dims))
        return;

    TexImageTarget texImageTarget(rawTarget);

    WebGLTexture* tex = activeBoundTextureForTexImageTarget(texImageTarget);
    if (!tex) {
        return ErrorInvalidOperation("texSubImage3D: no texture bound on active texture unit");
    }

    if (!tex->HasImageInfoAt(texImageTarget, level)) {
        return ErrorInvalidOperation("texSubImage3D: no previously defined texture image");
    }

    const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(texImageTarget, level);
    const TexInternalFormat existingEffectiveInternalFormat = imageInfo.EffectiveInternalFormat();
    TexInternalFormat existingUnsizedInternalFormat = LOCAL_GL_NONE;
    TexType existingType = LOCAL_GL_NONE;
    UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(existingEffectiveInternalFormat,
                                                            &existingUnsizedInternalFormat,
                                                            &existingType);

    if (!ValidateTexImage(texImageTarget, level, existingEffectiveInternalFormat.get(),
                          xoffset, yoffset, zoffset,
                          width, height, depth,
                          0, format, type, func, dims))
    {
        return;
    }

    if (type != existingType) {
        return ErrorInvalidOperation("texSubImage3D: type differs from that of the existing image");
    }

    js::Scalar::Type jsArrayType = JS_GetArrayBufferViewType(view.Obj());
    void* data = view.Data();
    size_t dataLength = view.Length();

    if (!ValidateTexInputData(type, jsArrayType, func, dims))
        return;

    const size_t bitsPerTexel = GetBitsPerTexel(existingEffectiveInternalFormat);
    MOZ_ASSERT((bitsPerTexel % 8) == 0); // should not have compressed formats here.
    size_t srcTexelSize = bitsPerTexel / 8;

    if (width == 0 || height == 0 || depth == 0)
        return; // no effect, we better return right now

    CheckedUint32 checked_neededByteLength =
        GetImageSize(height, width, depth, srcTexelSize, mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (dataLength < bytesNeeded)
        return ErrorInvalidOperation("texSubImage2D: not enough data for operation (need %d, have %d)", bytesNeeded, dataLength);

    if (imageInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xoffset == 0 &&
                                yoffset == 0 &&
                                zoffset == 0 &&
                                width == imageInfo.Width() &&
                                height == imageInfo.Height() &&
                                depth == imageInfo.Depth();
        if (coversWholeImage) {
            tex->SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            tex->DoDeferredImageInitialization(texImageTarget, level);
        }
    }

    GLenum driverType = LOCAL_GL_NONE;
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverFormat = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl,
                                             existingEffectiveInternalFormat,
                                             &driverInternalFormat,
                                             &driverFormat,
                                             &driverType);

    MakeContextCurrent();
    gl->fTexSubImage3D(texImageTarget.get(), level,
                       xoffset, yoffset, zoffset,
                       width, height, depth,
                       driverFormat, driverType, data);

}

void
WebGL2Context::TexSubImage3D(GLenum target, GLint level,
                             GLint xoffset, GLint yoffset, GLint zoffset,
                             GLenum format, GLenum type, dom::ImageData* data,
                             ErrorResult& rv)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::CopyTexSubImage3D(GLenum target, GLint level,
                                 GLint xoffset, GLint yoffset, GLint zoffset,
                                 GLint x, GLint y, GLsizei width, GLsizei height)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::CompressedTexImage3D(GLenum target, GLint level, GLenum internalformat,
                                    GLsizei width, GLsizei height, GLsizei depth,
                                    GLint border, GLsizei imageSize, const dom::ArrayBufferView& data)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::CompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                       GLsizei width, GLsizei height, GLsizei depth,
                                       GLenum format, GLsizei imageSize, const dom::ArrayBufferView& data)
{
    MOZ_CRASH("Not Implemented.");
}
