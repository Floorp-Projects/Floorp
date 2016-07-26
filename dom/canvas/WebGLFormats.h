/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_FORMATS_H_
#define WEBGL_FORMATS_H_

#include <map>
#include <set>

#include "mozilla/UniquePtr.h"
#include "WebGLTypes.h"

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

    ////////////////////////////////////

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
    COMPRESSED_RGB_S3TC_DXT1_EXT,
    COMPRESSED_RGBA_S3TC_DXT1_EXT,
    COMPRESSED_RGBA_S3TC_DXT3_EXT,
    COMPRESSED_RGBA_S3TC_DXT5_EXT,

    // IMG_texture_compression_pvrtc
    COMPRESSED_RGB_PVRTC_4BPPV1,
    COMPRESSED_RGBA_PVRTC_4BPPV1,
    COMPRESSED_RGB_PVRTC_2BPPV1,
    COMPRESSED_RGBA_PVRTC_2BPPV1,

    // OES_compressed_ETC1_RGB8_texture
    ETC1_RGB8_OES,

    ////////////////////////////////////

    // GLES 3.0.4, p128, table 3.12.
    Luminance8Alpha8,
    Luminance8,
    Alpha8,

    // OES_texture_float
    Luminance32FAlpha32F,
    Luminance32F,
    Alpha32F,

    // OES_texture_half_float
    Luminance16FAlpha16F,
    Luminance16F,
    Alpha16F,

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

// GLES 3.0.4 p114 Table 3.4, p240
enum class ComponentType : uint8_t {
    None,
    Int,          // RGBA32I
    UInt,         // RGBA32UI, STENCIL_INDEX8
    NormInt,      // RGBA8_SNORM
    NormUInt,     // RGBA8, DEPTH_COMPONENT16
    Float,        // RGBA32F
    Special,      // DEPTH24_STENCIL8
};

enum class CompressionFamily : uint8_t {
    ETC1,
    ES3, // ETC2 or EAC
    ATC,
    S3TC,
    PVRTC,
};

////////////////////////////////////////////////////////////////////////////////

struct CompressedFormatInfo
{
    const EffectiveFormat effectiveFormat;
    const uint8_t bytesPerBlock;
    const uint8_t blockWidth;
    const uint8_t blockHeight;
    const CompressionFamily family;
};

struct FormatInfo
{
    const EffectiveFormat effectiveFormat;
    const char* const name;
    const GLenum sizedFormat;
    const UnsizedFormat unsizedFormat;
    const ComponentType componentType;
    const bool isSRGB;

    const CompressedFormatInfo* const compression;

    const uint8_t estimatedBytesPerPixel; // 0 iff bool(compression).

    // In bits. Iff bool(compression), active channels are 1.
    const uint8_t r;
    const uint8_t g;
    const uint8_t b;
    const uint8_t a;
    const uint8_t d;
    const uint8_t s;

    //////

    std::map<UnsizedFormat, const FormatInfo*> copyDecayFormats;

    const FormatInfo* GetCopyDecayFormat(UnsizedFormat) const;

    bool IsColorFormat() const {
         // Alpha is a 'color format' since it's 'color-attachable'.
        return bool(compression) ||
               bool(r | g | b | a);
    }
};

struct PackingInfo
{
    GLenum format;
    GLenum type;

    bool operator <(const PackingInfo& x) const
    {
        if (format != x.format)
            return format < x.format;

        return type < x.type;
    }
};

struct DriverUnpackInfo
{
    GLenum internalFormat;
    GLenum unpackFormat;
    GLenum unpackType;

    PackingInfo ToPacking() const {
        return {unpackFormat, unpackType};
    }
};

//////////////////////////////////////////////////////////////////////////////////////////

const FormatInfo* GetFormat(EffectiveFormat format);
uint8_t BytesPerPixel(const PackingInfo& packing);
bool GetBytesPerPixel(const PackingInfo& packing, uint8_t* const out_bytes);
/*
GLint ComponentSize(const FormatInfo* format, GLenum component);
GLenum ComponentType(const FormatInfo* format);
*/
////////////////////////////////////////

struct FormatUsageInfo
{
    const FormatInfo* const format;
private:
    bool isRenderable;
public:
    bool isFilterable;

    std::map<PackingInfo, DriverUnpackInfo> validUnpacks;
    const DriverUnpackInfo* idealUnpack;

    const GLint* textureSwizzleRGBA;

    bool maxSamplesKnown;
    uint32_t maxSamples;

    static const GLint kLuminanceSwizzleRGBA[4];
    static const GLint kAlphaSwizzleRGBA[4];
    static const GLint kLumAlphaSwizzleRGBA[4];

    explicit FormatUsageInfo(const FormatInfo* _format)
        : format(_format)
        , isRenderable(false)
        , isFilterable(false)
        , idealUnpack(nullptr)
        , textureSwizzleRGBA(nullptr)
        , maxSamplesKnown(false)
        , maxSamples(0)
    { }

    bool IsRenderable() const { return isRenderable; }
    void SetRenderable();

    bool IsUnpackValid(const PackingInfo& key,
                       const DriverUnpackInfo** const out_value) const;

    void ResolveMaxSamples(gl::GLContext* gl);
};

class FormatUsageAuthority
{
    std::map<EffectiveFormat, FormatUsageInfo> mUsageMap;

    std::map<GLenum, const FormatUsageInfo*> mRBFormatMap;
    std::map<GLenum, const FormatUsageInfo*> mSizedTexFormatMap;
    std::map<PackingInfo, const FormatUsageInfo*> mUnsizedTexFormatMap;

    std::set<GLenum> mValidTexInternalFormats;
    std::set<GLenum> mValidTexUnpackFormats;
    std::set<GLenum> mValidTexUnpackTypes;

public:
    static UniquePtr<FormatUsageAuthority> CreateForWebGL1(gl::GLContext* gl);
    static UniquePtr<FormatUsageAuthority> CreateForWebGL2(gl::GLContext* gl);

private:
    FormatUsageAuthority() { }

public:
    FormatUsageInfo* EditUsage(EffectiveFormat format);
    const FormatUsageInfo* GetUsage(EffectiveFormat format) const;

    void AddTexUnpack(FormatUsageInfo* usage, const PackingInfo& pi,
                      const DriverUnpackInfo& dui);

    bool IsInternalFormatEnumValid(GLenum internalFormat) const;
    bool AreUnpackEnumsValid(GLenum unpackFormat, GLenum unpackType) const;

    void AllowRBFormat(GLenum sizedFormat, const FormatUsageInfo* usage);
    void AllowSizedTexFormat(GLenum sizedFormat, const FormatUsageInfo* usage);
    void AllowUnsizedTexFormat(const PackingInfo& pi, const FormatUsageInfo* usage);

    const FormatUsageInfo* GetRBUsage(GLenum sizedFormat) const;
    const FormatUsageInfo* GetSizedTexUsage(GLenum sizedFormat) const;
    const FormatUsageInfo* GetUnsizedTexUsage(const PackingInfo& pi) const;
};

} // namespace webgl
} // namespace mozilla

#endif // WEBGL_FORMATS_H_
