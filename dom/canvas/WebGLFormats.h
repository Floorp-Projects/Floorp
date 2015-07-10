/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_FORMATS_H_
#define WEBGL_FORMATS_H_

#include <map>
#include <set>

#include "GLTypes.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace webgl {

typedef uint8_t EffectiveFormatValueT;

enum class EffectiveFormat : EffectiveFormatValueT {
    // GLES 3.0.4, p128-129, "Required Texture Formats"
    // "Texture and renderbuffer color formats"
    RGBA32I,
    RGBA32UI,
    RGBA16I,
    RGBA16UI,
    RGBA8,
    RGBA8I,
    RGBA8UI,
    SRGB8_ALPHA8,
    RGB10_A2,
    RGB10_A2UI,
    RGBA4,
    RGB5_A1,

    RGB8,
    RGB565,

    RG32I,
    RG32UI,
    RG16I,
    RG16UI,
    RG8,
    RG8I,
    RG8UI,

    R32I,
    R32UI,
    R16I,
    R16UI,
    R8,
    R8I,
    R8UI,

    // "Texture-only color formats"
    RGBA32F,
    RGBA16F,
    RGBA8_SNORM,

    RGB32F,
    RGB32I,
    RGB32UI,

    RGB16F,
    RGB16I,
    RGB16UI,

    RGB8_SNORM,
    RGB8I,
    RGB8UI,
    SRGB8,

    R11F_G11F_B10F,
    RGB9_E5,

    RG32F,
    RG16F,
    RG8_SNORM,

    R32F,
    R16F,
    R8_SNORM,

    // "Depth formats"
    DEPTH_COMPONENT32F,
    DEPTH_COMPONENT24,
    DEPTH_COMPONENT16,

    // "Combined depth+stencil formats"
    DEPTH32F_STENCIL8,
    DEPTH24_STENCIL8,

    // GLES 3.0.4, p205-206, "Required Renderbuffer Formats"
    STENCIL_INDEX8,

    // GLES 3.0.4, p128, table 3.12.
    Luminance8Alpha8,
    Luminance8,
    Alpha8,

    // GLES 3.0.4, p147, table 3.19
    // GLES 3.0.4, p286+, $C.1 "ETC Compressed Texture Image Formats"
    COMPRESSED_R11_EAC,
    COMPRESSED_SIGNED_R11_EAC,
    COMPRESSED_RG11_EAC,
    COMPRESSED_SIGNED_RG11_EAC,
    COMPRESSED_RGB8_ETC2,
    COMPRESSED_SRGB8_ETC2,
    COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
    COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,
    COMPRESSED_RGBA8_ETC2_EAC,
    COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,

    // AMD_compressed_ATC_texture
    ATC_RGB_AMD,
    ATC_RGBA_EXPLICIT_ALPHA_AMD,
    ATC_RGBA_INTERPOLATED_ALPHA_AMD,

    // EXT_texture_compression_s3tc
    COMPRESSED_RGB_S3TC_DXT1,
    COMPRESSED_RGBA_S3TC_DXT1,
    COMPRESSED_RGBA_S3TC_DXT3,
    COMPRESSED_RGBA_S3TC_DXT5,

    // IMG_texture_compression_pvrtc
    COMPRESSED_RGB_PVRTC_4BPPV1,
    COMPRESSED_RGBA_PVRTC_4BPPV1,
    COMPRESSED_RGB_PVRTC_2BPPV1,
    COMPRESSED_RGBA_PVRTC_2BPPV1,

    // OES_compressed_ETC1_RGB8_texture
    ETC1_RGB8,

    MAX,
};

enum class UnsizedFormat : uint8_t {
    R,
    RG,
    RGB,
    RGBA,
    LA,
    L,
    A,
    D,
    S,
    DS,
};

// GLES 3.0.4 p114 Table 3.4
enum class ComponentType : uint8_t {
    None,         // DEPTH_COMPONENT32F
    Int,          // RGBA32I
    UInt,         // RGBA32UI
    NormInt,      // RGBA8_SNORM
    NormUInt,     // RGBA8
    NormUIntSRGB, // SRGB8_ALPHA8
    Float,        // RGBA32F
    SharedExp,    // RGB9_E5
};

enum class SubImageUpdateBehavior : uint8_t {
    Forbidden,
    FullOnly,
    BlockAligned,
};

////////////////////////////////////////////////////////////////////////////////

struct CompressedFormatInfo {
    const EffectiveFormat effectiveFormat;
    const uint8_t bytesPerBlock;
    const uint8_t blockWidth;
    const uint8_t blockHeight;
    const bool requirePOT;
    const SubImageUpdateBehavior subImageUpdateBehavior;
};

struct FormatInfo {
    const EffectiveFormat effectiveFormat;
    const char* const name;
    const UnsizedFormat unsizedFormat;
    const ComponentType colorComponentType;
    const uint8_t bytesPerPixel; // 0 iff `!!compression`.
    const bool hasColor;
    const bool hasAlpha;
    const bool hasDepth;
    const bool hasStencil;

    const CompressedFormatInfo* const compression;
};

//////////////////////////////////////////////////////////////////////////////////////////

const FormatInfo* GetFormatInfo(EffectiveFormat format);
const FormatInfo* GetInfoByUnpackTuple(GLenum unpackFormat, GLenum unpackType);
const FormatInfo* GetInfoBySizedFormat(GLenum sizedFormat);

////////////////////////////////////////

struct UnpackTuple {
    const GLenum format;
    const GLenum type;

    bool operator <(const UnpackTuple& x) const
    {
        if (format == x.format) {
            return type < x.type;
        }

        return format < x.format;
    }
};

struct FormatUsageInfo {
    const FormatInfo* const formatInfo;
    bool asRenderbuffer;
    bool isRenderable;
    bool asTexture;
    bool isFilterable;
    std::set<UnpackTuple> validUnpacks;

    bool CanUnpackWith(GLenum unpackFormat, GLenum unpackType) const;
};

class FormatUsageAuthority
{
    std::map<EffectiveFormat, FormatUsageInfo> mInfoMap;

public:
    static UniquePtr<FormatUsageAuthority> CreateForWebGL1();
    static UniquePtr<FormatUsageAuthority> CreateForWebGL2();

private:
    FormatUsageAuthority() { }

public:
    void AddFormat(EffectiveFormat format, bool asRenderbuffer, bool isRenderable,
                   bool asTexture, bool isFilterable);

    void AddUnpackOption(GLenum unpackFormat, GLenum unpackType,
                         EffectiveFormat effectiveFormat);

    FormatUsageInfo* GetInfo(EffectiveFormat format);

    FormatUsageInfo* GetInfo(const FormatInfo* format)
    {
        return GetInfo(format->effectiveFormat);
    }
};

////////////////////////////////////////

} // namespace webgl
} // namespace mozilla

#endif // WEBGL_FORMATS_H_
