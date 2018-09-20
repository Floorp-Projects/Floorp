//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// formatutils.cpp: Queries for GL image formats.

#include "libANGLE/formatutils.h"

#include "common/mathutil.h"
#include "libANGLE/Context.h"
#include "libANGLE/Framebuffer.h"

using namespace angle;

namespace gl
{

// ES2 requires that format is equal to internal format at all glTex*Image2D entry points and the
// implementation can decide the true, sized, internal format. The ES2FormatMap determines the
// internal format for all valid format and type combinations.
GLenum GetSizedFormatInternal(GLenum format, GLenum type);

namespace
{
using InternalFormatInfoMap =
    std::unordered_map<GLenum, std::unordered_map<GLenum, InternalFormat>>;

bool CheckedMathResult(const CheckedNumeric<GLuint> &value, GLuint *resultOut)
{
    if (!value.IsValid())
    {
        return false;
    }
    else
    {
        *resultOut = value.ValueOrDie();
        return true;
    }
}
}  // anonymous namespace

FormatType::FormatType() : format(GL_NONE), type(GL_NONE)
{
}

FormatType::FormatType(GLenum format_, GLenum type_) : format(format_), type(type_)
{
}

bool FormatType::operator<(const FormatType &other) const
{
    if (format != other.format)
        return format < other.format;
    return type < other.type;
}

Type::Type() : bytes(0), bytesShift(0), specialInterpretation(false)
{
}

static Type GenTypeInfo(GLuint bytes, bool specialInterpretation)
{
    Type info;
    info.bytes = bytes;
    GLuint i   = 0;
    while ((1u << i) < bytes)
    {
        ++i;
    }
    info.bytesShift = i;
    ASSERT((1u << info.bytesShift) == bytes);
    info.specialInterpretation = specialInterpretation;
    return info;
}

bool operator<(const Type &a, const Type &b)
{
    return memcmp(&a, &b, sizeof(Type)) < 0;
}

// Information about internal formats
static bool AlwaysSupported(const Version &, const Extensions &)
{
    return true;
}

static bool NeverSupported(const Version &, const Extensions &)
{
    return false;
}

template <GLuint minCoreGLMajorVersion, GLuint minCoreGLMinorVersion>
static bool RequireES(const Version &clientVersion, const Extensions &)
{
    return clientVersion >= Version(minCoreGLMajorVersion, minCoreGLMinorVersion);
}

// Pointer to a boolean memeber of the Extensions struct
typedef bool(Extensions::*ExtensionBool);

// Check support for a single extension
template <ExtensionBool bool1>
static bool RequireExt(const Version &, const Extensions &extensions)
{
    return extensions.*bool1;
}

// Check for a minimum client version or a single extension
template <GLuint minCoreGLMajorVersion, GLuint minCoreGLMinorVersion, ExtensionBool bool1>
static bool RequireESOrExt(const Version &clientVersion, const Extensions &extensions)
{
    return clientVersion >= Version(minCoreGLMajorVersion, minCoreGLMinorVersion) ||
           extensions.*bool1;
}

// Check for a minimum client version or two extensions
template <GLuint minCoreGLMajorVersion,
          GLuint minCoreGLMinorVersion,
          ExtensionBool bool1,
          ExtensionBool bool2>
static bool RequireESOrExtAndExt(const Version &clientVersion, const Extensions &extensions)
{
    return clientVersion >= Version(minCoreGLMajorVersion, minCoreGLMinorVersion) ||
           (extensions.*bool1 && extensions.*bool2);
}

// Check for a minimum client version or at least one of two extensions
template <GLuint minCoreGLMajorVersion,
          GLuint minCoreGLMinorVersion,
          ExtensionBool bool1,
          ExtensionBool bool2>
static bool RequireESOrExtOrExt(const Version &clientVersion, const Extensions &extensions)
{
    return clientVersion >= Version(minCoreGLMajorVersion, minCoreGLMinorVersion) ||
           extensions.*bool1 || extensions.*bool2;
}

// Check support for two extensions
template <ExtensionBool bool1, ExtensionBool bool2>
static bool RequireExtAndExt(const Version &, const Extensions &extensions)
{
    return extensions.*bool1 && extensions.*bool2;
}

// Check support for either of two extensions
template <ExtensionBool bool1, ExtensionBool bool2>
static bool RequireExtOrExt(const Version &, const Extensions &extensions)
{
    return extensions.*bool1 || extensions.*bool2;
}

// Special function for half float formats with three or four channels.
static bool HalfFloatSupport(const Version &clientVersion, const Extensions &extensions)
{
    return clientVersion >= Version(3, 0) || extensions.textureHalfFloat;
}

static bool HalfFloatRGBRenderableSupport(const Version &clientVersion,
                                          const Extensions &extensions)
{
    return HalfFloatSupport(clientVersion, extensions) && extensions.colorBufferHalfFloat;
}

static bool HalfFloatRGBARenderableSupport(const Version &clientVersion,
                                           const Extensions &extensions)
{
    return HalfFloatSupport(clientVersion, extensions) &&
           (extensions.colorBufferHalfFloat || extensions.colorBufferFloat);
}

// Special function for half float formats with one or two channels.

// R16F, RG16F
static bool HalfFloatRGSupport(const Version &clientVersion, const Extensions &extensions)
{
    return clientVersion >= Version(3, 0) || (extensions.textureHalfFloat && extensions.textureRG);
}

// R16F, RG16F
static bool HalfFloatRGRenderableSupport(const Version &clientVersion, const Extensions &extensions)
{
    // It's unclear if EXT_color_buffer_half_float gives renderability to non-OES half float
    // textures
    return HalfFloatRGSupport(clientVersion, extensions) &&
           (extensions.colorBufferHalfFloat || extensions.colorBufferFloat);
}

// RED + HALF_FLOAT_OES, RG + HALF_FLOAT_OES
static bool UnsizedHalfFloatOESRGSupport(const Version &, const Extensions &extensions)
{
    return extensions.textureHalfFloat && extensions.textureRG;
}

// RED + HALF_FLOAT_OES, RG + HALF_FLOAT_OES
static bool UnsizedHalfFloatOESRGRenderableSupport(const Version &clientVersion,
                                                   const Extensions &extensions)
{
    return UnsizedHalfFloatOESRGSupport(clientVersion, extensions) &&
           extensions.colorBufferHalfFloat;
}

// RGB + HALF_FLOAT_OES, RGBA + HALF_FLOAT_OES
static bool UnsizedHalfFloatOESSupport(const Version &clientVersion, const Extensions &extensions)
{
    return extensions.textureHalfFloat;
}

// RGB + HALF_FLOAT_OES, RGBA + HALF_FLOAT_OES
static bool UnsizedHalfFloatOESRenderableSupport(const Version &clientVersion,
                                                 const Extensions &extensions)
{
    return UnsizedHalfFloatOESSupport(clientVersion, extensions) && extensions.colorBufferHalfFloat;
}

// Special function for float formats with three or four channels.

// RGB32F, RGBA32F
static bool FloatSupport(const Version &clientVersion, const Extensions &extensions)
{
    return clientVersion >= Version(3, 0) || extensions.textureFloat;
}

// RGB32F
static bool FloatRGBRenderableSupport(const Version &clientVersion, const Extensions &extensions)
{
    return FloatSupport(clientVersion, extensions) && extensions.colorBufferFloatRGB;
}

// RGBA32F
static bool FloatRGBARenderableSupport(const Version &clientVersion, const Extensions &extensions)
{
    return FloatSupport(clientVersion, extensions) &&
           (extensions.colorBufferFloat || extensions.colorBufferFloatRGBA);
}

// RGB + FLOAT, RGBA + FLOAT
static bool UnsizedFloatSupport(const Version &clientVersion, const Extensions &extensions)
{
    return extensions.textureFloat;
}

// RGB + FLOAT
static bool UnsizedFloatRGBRenderableSupport(const Version &clientVersion,
                                             const Extensions &extensions)
{
    return UnsizedFloatSupport(clientVersion, extensions) && extensions.colorBufferFloatRGB;
}

// RGBA + FLOAT
static bool UnsizedFloatRGBARenderableSupport(const Version &clientVersion,
                                              const Extensions &extensions)
{
    return UnsizedFloatSupport(clientVersion, extensions) &&
           (extensions.colorBufferFloatRGBA || extensions.colorBufferFloat);
}

// Special function for float formats with one or two channels.

// R32F, RG32F
static bool FloatRGSupport(const Version &clientVersion, const Extensions &extensions)
{
    return clientVersion >= Version(3, 0) || (extensions.textureFloat && extensions.textureRG);
}

// R32F, RG32F
static bool FloatRGRenderableSupport(const Version &clientVersion, const Extensions &extensions)
{
    return FloatRGSupport(clientVersion, extensions) && extensions.colorBufferFloat;
}

// RED + FLOAT, RG + FLOAT
static bool UnsizedFloatRGSupport(const Version &clientVersion, const Extensions &extensions)
{
    return extensions.textureFloat && extensions.textureRG;
}

// RED + FLOAT, RG + FLOAT
static bool UnsizedFloatRGRenderableSupport(const Version &clientVersion,
                                            const Extensions &extensions)
{
    return FloatRGSupport(clientVersion, extensions) && extensions.colorBufferFloat;
}

InternalFormat::InternalFormat()
    : internalFormat(GL_NONE),
      sized(false),
      sizedInternalFormat(GL_NONE),
      redBits(0),
      greenBits(0),
      blueBits(0),
      luminanceBits(0),
      alphaBits(0),
      sharedBits(0),
      depthBits(0),
      stencilBits(0),
      pixelBytes(0),
      componentCount(0),
      compressed(false),
      compressedBlockWidth(0),
      compressedBlockHeight(0),
      format(GL_NONE),
      type(GL_NONE),
      componentType(GL_NONE),
      colorEncoding(GL_NONE),
      textureSupport(NeverSupported),
      filterSupport(NeverSupported),
      textureAttachmentSupport(NeverSupported),
      renderbufferSupport(NeverSupported)
{
}

InternalFormat::InternalFormat(const InternalFormat &other) = default;

bool InternalFormat::isLUMA() const
{
    return ((redBits + greenBits + blueBits + depthBits + stencilBits) == 0 &&
            (luminanceBits + alphaBits) > 0);
}

GLenum InternalFormat::getReadPixelsFormat() const
{
    return format;
}

GLenum InternalFormat::getReadPixelsType(const Version &version) const
{
    switch (type)
    {
        case GL_HALF_FLOAT:
        case GL_HALF_FLOAT_OES:
            if (version < Version(3, 0))
            {
                // The internal format may have a type of GL_HALF_FLOAT but when exposing this type
                // as the IMPLEMENTATION_READ_TYPE, only HALF_FLOAT_OES is allowed by
                // OES_texture_half_float.  HALF_FLOAT becomes core in ES3 and is acceptable to use
                // as an IMPLEMENTATION_READ_TYPE.
                return GL_HALF_FLOAT_OES;
            }
            else
            {
                return GL_HALF_FLOAT;
            }

        default:
            return type;
    }
}

bool InternalFormat::isRequiredRenderbufferFormat(const Version &version) const
{
    // GLES 3.0.5 section 4.4.2.2:
    // "Implementations are required to support the same internal formats for renderbuffers as the
    // required formats for textures enumerated in section 3.8.3.1, with the exception of the color
    // formats labelled "texture-only"."
    if (!sized || compressed)
    {
        return false;
    }

    // Luma formats.
    if (isLUMA())
    {
        return false;
    }

    // Depth/stencil formats.
    if (depthBits > 0 || stencilBits > 0)
    {
        // GLES 2.0.25 table 4.5.
        // GLES 3.0.5 section 3.8.3.1.
        // GLES 3.1 table 8.14.

        // Required formats in all versions.
        switch (internalFormat)
        {
            case GL_DEPTH_COMPONENT16:
            case GL_STENCIL_INDEX8:
                // Note that STENCIL_INDEX8 is not mentioned in GLES 3.0.5 section 3.8.3.1, but it
                // is in section 4.4.2.2.
                return true;
            default:
                break;
        }
        if (version.major < 3)
        {
            return false;
        }
        // Required formats in GLES 3.0 and up.
        switch (internalFormat)
        {
            case GL_DEPTH_COMPONENT32F:
            case GL_DEPTH_COMPONENT24:
            case GL_DEPTH32F_STENCIL8:
            case GL_DEPTH24_STENCIL8:
                return true;
            default:
                return false;
        }
    }

    // RGBA formats.
    // GLES 2.0.25 table 4.5.
    // GLES 3.0.5 section 3.8.3.1.
    // GLES 3.1 table 8.13.

    // Required formats in all versions.
    switch (internalFormat)
    {
        case GL_RGBA4:
        case GL_RGB5_A1:
        case GL_RGB565:
            return true;
        default:
            break;
    }
    if (version.major < 3)
    {
        return false;
    }

    if (format == GL_BGRA_EXT)
    {
        return false;
    }

    switch (componentType)
    {
        case GL_SIGNED_NORMALIZED:
        case GL_FLOAT:
            return false;
        case GL_UNSIGNED_INT:
        case GL_INT:
            // Integer RGB formats are not required renderbuffer formats.
            if (alphaBits == 0 && blueBits != 0)
            {
                return false;
            }
            // All integer R and RG formats are required.
            // Integer RGBA formats including RGB10_A2_UI are required.
            return true;
        case GL_UNSIGNED_NORMALIZED:
            if (internalFormat == GL_SRGB8)
            {
                return false;
            }
            return true;
        default:
            UNREACHABLE();
#if !UNREACHABLE_IS_NORETURN
            return false;
#endif
    }
}

Format::Format(GLenum internalFormat) : Format(GetSizedInternalFormatInfo(internalFormat))
{
}

Format::Format(const InternalFormat &internalFormat) : info(&internalFormat)
{
}

Format::Format(GLenum internalFormat, GLenum type)
    : info(&GetInternalFormatInfo(internalFormat, type))
{
}

Format::Format(const Format &other) = default;
Format &Format::operator=(const Format &other) = default;

bool Format::valid() const
{
    return info->internalFormat != GL_NONE;
}

// static
bool Format::SameSized(const Format &a, const Format &b)
{
    return a.info->sizedInternalFormat == b.info->sizedInternalFormat;
}

static GLenum EquivalentBlitInternalFormat(GLenum internalformat)
{
    // BlitFramebuffer works if the color channels are identically
    // sized, even if there is a swizzle (for example, blitting from a
    // multisampled RGBA8 renderbuffer to a BGRA8 texture). This could
    // be expanded and/or autogenerated if that is found necessary.
    if (internalformat == GL_BGRA8_EXT)
        return GL_RGBA8;
    return internalformat;
}

// static
bool Format::EquivalentForBlit(const Format &a, const Format &b)
{
    return (EquivalentBlitInternalFormat(a.info->sizedInternalFormat) ==
            EquivalentBlitInternalFormat(b.info->sizedInternalFormat));
}

// static
Format Format::Invalid()
{
    static Format invalid(GL_NONE, GL_NONE);
    return invalid;
}

std::ostream &operator<<(std::ostream &os, const Format &fmt)
{
    // TODO(ynovikov): return string representation when available
    return FmtHex(os, fmt.info->sizedInternalFormat);
}

bool InternalFormat::operator==(const InternalFormat &other) const
{
    // We assume all internal formats are unique if they have the same internal format and type
    return internalFormat == other.internalFormat && type == other.type;
}

bool InternalFormat::operator!=(const InternalFormat &other) const
{
    return !(*this == other);
}

void InsertFormatInfo(InternalFormatInfoMap *map, const InternalFormat &formatInfo)
{
    ASSERT(!formatInfo.sized || (*map).count(formatInfo.internalFormat) == 0);
    ASSERT((*map)[formatInfo.internalFormat].count(formatInfo.type) == 0);
    (*map)[formatInfo.internalFormat][formatInfo.type] = formatInfo;
}

void AddRGBAFormat(InternalFormatInfoMap *map,
                   GLenum internalFormat,
                   bool sized,
                   GLuint red,
                   GLuint green,
                   GLuint blue,
                   GLuint alpha,
                   GLuint shared,
                   GLenum format,
                   GLenum type,
                   GLenum componentType,
                   bool srgb,
                   InternalFormat::SupportCheckFunction textureSupport,
                   InternalFormat::SupportCheckFunction filterSupport,
                   InternalFormat::SupportCheckFunction textureAttachmentSupport,
                   InternalFormat::SupportCheckFunction renderbufferSupport)
{
    InternalFormat formatInfo;
    formatInfo.internalFormat = internalFormat;
    formatInfo.sized          = sized;
    formatInfo.sizedInternalFormat =
        sized ? internalFormat : GetSizedFormatInternal(internalFormat, type);
    formatInfo.redBits    = red;
    formatInfo.greenBits  = green;
    formatInfo.blueBits   = blue;
    formatInfo.alphaBits  = alpha;
    formatInfo.sharedBits = shared;
    formatInfo.pixelBytes = (red + green + blue + alpha + shared) / 8;
    formatInfo.componentCount =
        ((red > 0) ? 1 : 0) + ((green > 0) ? 1 : 0) + ((blue > 0) ? 1 : 0) + ((alpha > 0) ? 1 : 0);
    formatInfo.format                   = format;
    formatInfo.type                     = type;
    formatInfo.componentType            = componentType;
    formatInfo.colorEncoding            = (srgb ? GL_SRGB : GL_LINEAR);
    formatInfo.textureSupport           = textureSupport;
    formatInfo.filterSupport            = filterSupport;
    formatInfo.textureAttachmentSupport = textureAttachmentSupport;
    formatInfo.renderbufferSupport      = renderbufferSupport;

    InsertFormatInfo(map, formatInfo);
}

static void AddLUMAFormat(InternalFormatInfoMap *map,
                          GLenum internalFormat,
                          bool sized,
                          GLuint luminance,
                          GLuint alpha,
                          GLenum format,
                          GLenum type,
                          GLenum componentType,
                          InternalFormat::SupportCheckFunction textureSupport,
                          InternalFormat::SupportCheckFunction filterSupport,
                          InternalFormat::SupportCheckFunction textureAttachmentSupport,
                          InternalFormat::SupportCheckFunction renderbufferSupport)
{
    InternalFormat formatInfo;
    formatInfo.internalFormat = internalFormat;
    formatInfo.sized          = sized;
    formatInfo.sizedInternalFormat =
        sized ? internalFormat : GetSizedFormatInternal(internalFormat, type);
    formatInfo.luminanceBits            = luminance;
    formatInfo.alphaBits                = alpha;
    formatInfo.pixelBytes               = (luminance + alpha) / 8;
    formatInfo.componentCount           = ((luminance > 0) ? 1 : 0) + ((alpha > 0) ? 1 : 0);
    formatInfo.format                   = format;
    formatInfo.type                     = type;
    formatInfo.componentType            = componentType;
    formatInfo.colorEncoding            = GL_LINEAR;
    formatInfo.textureSupport           = textureSupport;
    formatInfo.filterSupport            = filterSupport;
    formatInfo.textureAttachmentSupport = textureAttachmentSupport;
    formatInfo.renderbufferSupport      = renderbufferSupport;

    InsertFormatInfo(map, formatInfo);
}

void AddDepthStencilFormat(InternalFormatInfoMap *map,
                           GLenum internalFormat,
                           bool sized,
                           GLuint depthBits,
                           GLuint stencilBits,
                           GLuint unusedBits,
                           GLenum format,
                           GLenum type,
                           GLenum componentType,
                           InternalFormat::SupportCheckFunction textureSupport,
                           InternalFormat::SupportCheckFunction filterSupport,
                           InternalFormat::SupportCheckFunction textureAttachmentSupport,
                           InternalFormat::SupportCheckFunction renderbufferSupport)
{
    InternalFormat formatInfo;
    formatInfo.internalFormat = internalFormat;
    formatInfo.sized          = sized;
    formatInfo.sizedInternalFormat =
        sized ? internalFormat : GetSizedFormatInternal(internalFormat, type);
    formatInfo.depthBits                = depthBits;
    formatInfo.stencilBits              = stencilBits;
    formatInfo.pixelBytes               = (depthBits + stencilBits + unusedBits) / 8;
    formatInfo.componentCount           = ((depthBits > 0) ? 1 : 0) + ((stencilBits > 0) ? 1 : 0);
    formatInfo.format                   = format;
    formatInfo.type                     = type;
    formatInfo.componentType            = componentType;
    formatInfo.colorEncoding            = GL_LINEAR;
    formatInfo.textureSupport           = textureSupport;
    formatInfo.filterSupport            = filterSupport;
    formatInfo.textureAttachmentSupport = textureAttachmentSupport;
    formatInfo.renderbufferSupport      = renderbufferSupport;

    InsertFormatInfo(map, formatInfo);
}

void AddCompressedFormat(InternalFormatInfoMap *map,
                         GLenum internalFormat,
                         GLuint compressedBlockWidth,
                         GLuint compressedBlockHeight,
                         GLuint compressedBlockSize,
                         GLuint componentCount,
                         GLenum format,
                         GLenum type,
                         bool srgb,
                         InternalFormat::SupportCheckFunction textureSupport,
                         InternalFormat::SupportCheckFunction filterSupport,
                         InternalFormat::SupportCheckFunction textureAttachmentSupport,
                         InternalFormat::SupportCheckFunction renderbufferSupport)
{
    InternalFormat formatInfo;
    formatInfo.internalFormat           = internalFormat;
    formatInfo.sized                    = true;
    formatInfo.sizedInternalFormat      = internalFormat;
    formatInfo.compressedBlockWidth     = compressedBlockWidth;
    formatInfo.compressedBlockHeight    = compressedBlockHeight;
    formatInfo.pixelBytes               = compressedBlockSize / 8;
    formatInfo.componentCount           = componentCount;
    formatInfo.format                   = format;
    formatInfo.type                     = type;
    formatInfo.componentType            = GL_UNSIGNED_NORMALIZED;
    formatInfo.colorEncoding            = (srgb ? GL_SRGB : GL_LINEAR);
    formatInfo.compressed               = true;
    formatInfo.textureSupport           = textureSupport;
    formatInfo.filterSupport            = filterSupport;
    formatInfo.textureAttachmentSupport = textureAttachmentSupport;
    formatInfo.renderbufferSupport      = renderbufferSupport;

    InsertFormatInfo(map, formatInfo);
}

static InternalFormatInfoMap BuildInternalFormatInfoMap()
{
    InternalFormatInfoMap map;

    // From ES 3.0.1 spec, table 3.12
    map[GL_NONE][GL_NONE] = InternalFormat();

    // clang-format off

    //                 | Internal format     |sized| R | G | B | A |S | Format         | Type                             | Component type        | SRGB | Texture supported                             | Filterable     | Texture attachment                            | Renderbuffer                                 |
    AddRGBAFormat(&map, GL_R8,                true,  8,  0,  0,  0, 0, GL_RED,          GL_UNSIGNED_BYTE,                  GL_UNSIGNED_NORMALIZED, false, RequireESOrExt<3, 0, &Extensions::textureRG>,   AlwaysSupported, RequireESOrExt<3, 0, &Extensions::textureRG>,   RequireESOrExt<3, 0, &Extensions::textureRG>  );
    AddRGBAFormat(&map, GL_R8_SNORM,          true,  8,  0,  0,  0, 0, GL_RED,          GL_BYTE,                           GL_SIGNED_NORMALIZED,   false, RequireES<3, 0>,                                AlwaysSupported, NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_RG8,               true,  8,  8,  0,  0, 0, GL_RG,           GL_UNSIGNED_BYTE,                  GL_UNSIGNED_NORMALIZED, false, RequireESOrExt<3, 0, &Extensions::textureRG>,   AlwaysSupported, RequireESOrExt<3, 0, &Extensions::textureRG>,   RequireESOrExt<3, 0, &Extensions::textureRG>  );
    AddRGBAFormat(&map, GL_RG8_SNORM,         true,  8,  8,  0,  0, 0, GL_RG,           GL_BYTE,                           GL_SIGNED_NORMALIZED,   false, RequireES<3, 0>,                                AlwaysSupported, NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_RGB8,              true,  8,  8,  8,  0, 0, GL_RGB,          GL_UNSIGNED_BYTE,                  GL_UNSIGNED_NORMALIZED, false, RequireESOrExt<3, 0, &Extensions::rgb8rgba8>,   AlwaysSupported, RequireESOrExt<3, 0, &Extensions::rgb8rgba8>,   RequireESOrExt<3, 0, &Extensions::rgb8rgba8>  );
    AddRGBAFormat(&map, GL_RGB8_SNORM,        true,  8,  8,  8,  0, 0, GL_RGB,          GL_BYTE,                           GL_SIGNED_NORMALIZED,   false, RequireES<3, 0>,                                AlwaysSupported, NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_RGB565,            true,  5,  6,  5,  0, 0, GL_RGB,          GL_UNSIGNED_SHORT_5_6_5,           GL_UNSIGNED_NORMALIZED, false, RequireES<1, 0>,                                AlwaysSupported, RequireES<1, 0>,                                RequireES<1, 0>                               );
    AddRGBAFormat(&map, GL_RGBA4,             true,  4,  4,  4,  4, 0, GL_RGBA,         GL_UNSIGNED_SHORT_4_4_4_4,         GL_UNSIGNED_NORMALIZED, false, RequireES<1, 0>,                                AlwaysSupported, RequireES<1, 0>,                                RequireES<1, 0>                               );
    AddRGBAFormat(&map, GL_RGB5_A1,           true,  5,  5,  5,  1, 0, GL_RGBA,         GL_UNSIGNED_SHORT_5_5_5_1,         GL_UNSIGNED_NORMALIZED, false, RequireES<1, 0>,                                AlwaysSupported, RequireES<1, 0>,                                RequireES<1, 0>                               );
    AddRGBAFormat(&map, GL_RGBA8,             true,  8,  8,  8,  8, 0, GL_RGBA,         GL_UNSIGNED_BYTE,                  GL_UNSIGNED_NORMALIZED, false, RequireESOrExt<3, 0, &Extensions::rgb8rgba8>,   AlwaysSupported, RequireESOrExt<3, 0, &Extensions::rgb8rgba8>,   RequireESOrExt<3, 0, &Extensions::rgb8rgba8>  );
    AddRGBAFormat(&map, GL_RGBA8_SNORM,       true,  8,  8,  8,  8, 0, GL_RGBA,         GL_BYTE,                           GL_SIGNED_NORMALIZED,   false, RequireES<3, 0>,                                AlwaysSupported, NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_RGB10_A2,          true, 10, 10, 10,  2, 0, GL_RGBA,         GL_UNSIGNED_INT_2_10_10_10_REV,    GL_UNSIGNED_NORMALIZED, false, RequireES<3, 0>,                                AlwaysSupported, RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_RGB10_A2UI,        true, 10, 10, 10,  2, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV,    GL_UNSIGNED_INT,        false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_SRGB8,             true,  8,  8,  8,  0, 0, GL_RGB,          GL_UNSIGNED_BYTE,                  GL_UNSIGNED_NORMALIZED, true,  RequireESOrExt<3, 0, &Extensions::sRGB>,        AlwaysSupported, NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_SRGB8_ALPHA8,      true,  8,  8,  8,  8, 0, GL_RGBA,         GL_UNSIGNED_BYTE,                  GL_UNSIGNED_NORMALIZED, true,  RequireESOrExt<3, 0, &Extensions::sRGB>,        AlwaysSupported, RequireESOrExt<3, 0, &Extensions::sRGB>,        RequireESOrExt<3, 0, &Extensions::sRGB>       );
    AddRGBAFormat(&map, GL_RGB9_E5,           true,  9,  9,  9,  0, 5, GL_RGB,          GL_UNSIGNED_INT_5_9_9_9_REV,       GL_FLOAT,               false, RequireES<3, 0>,                                AlwaysSupported, NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_R8I,               true,  8,  0,  0,  0, 0, GL_RED_INTEGER,  GL_BYTE,                           GL_INT,                 false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_R8UI,              true,  8,  0,  0,  0, 0, GL_RED_INTEGER,  GL_UNSIGNED_BYTE,                  GL_UNSIGNED_INT,        false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_R16I,              true, 16,  0,  0,  0, 0, GL_RED_INTEGER,  GL_SHORT,                          GL_INT,                 false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_R16UI,             true, 16,  0,  0,  0, 0, GL_RED_INTEGER,  GL_UNSIGNED_SHORT,                 GL_UNSIGNED_INT,        false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_R32I,              true, 32,  0,  0,  0, 0, GL_RED_INTEGER,  GL_INT,                            GL_INT,                 false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_R32UI,             true, 32,  0,  0,  0, 0, GL_RED_INTEGER,  GL_UNSIGNED_INT,                   GL_UNSIGNED_INT,        false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_RG8I,              true,  8,  8,  0,  0, 0, GL_RG_INTEGER,   GL_BYTE,                           GL_INT,                 false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_RG8UI,             true,  8,  8,  0,  0, 0, GL_RG_INTEGER,   GL_UNSIGNED_BYTE,                  GL_UNSIGNED_INT,        false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_RG16I,             true, 16, 16,  0,  0, 0, GL_RG_INTEGER,   GL_SHORT,                          GL_INT,                 false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_RG16UI,            true, 16, 16,  0,  0, 0, GL_RG_INTEGER,   GL_UNSIGNED_SHORT,                 GL_UNSIGNED_INT,        false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_RG32I,             true, 32, 32,  0,  0, 0, GL_RG_INTEGER,   GL_INT,                            GL_INT,                 false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_R11F_G11F_B10F,    true, 11, 11, 10,  0, 0, GL_RGB,          GL_UNSIGNED_INT_10F_11F_11F_REV,   GL_FLOAT,               false, RequireES<3, 0>,                                AlwaysSupported, RequireExt<&Extensions::colorBufferFloat>,      RequireExt<&Extensions::colorBufferFloat>     );
    AddRGBAFormat(&map, GL_RG32UI,            true, 32, 32,  0,  0, 0, GL_RG_INTEGER,   GL_UNSIGNED_INT,                   GL_UNSIGNED_INT,        false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_RGB8I,             true,  8,  8,  8,  0, 0, GL_RGB_INTEGER,  GL_BYTE,                           GL_INT,                 false, RequireES<3, 0>,                                NeverSupported,  NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_RGB8UI,            true,  8,  8,  8,  0, 0, GL_RGB_INTEGER,  GL_UNSIGNED_BYTE,                  GL_UNSIGNED_INT,        false, RequireES<3, 0>,                                NeverSupported,  NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_RGB16I,            true, 16, 16, 16,  0, 0, GL_RGB_INTEGER,  GL_SHORT,                          GL_INT,                 false, RequireES<3, 0>,                                NeverSupported,  NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_RGB16UI,           true, 16, 16, 16,  0, 0, GL_RGB_INTEGER,  GL_UNSIGNED_SHORT,                 GL_UNSIGNED_INT,        false, RequireES<3, 0>,                                NeverSupported,  NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_RGB32I,            true, 32, 32, 32,  0, 0, GL_RGB_INTEGER,  GL_INT,                            GL_INT,                 false, RequireES<3, 0>,                                NeverSupported,  NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_RGB32UI,           true, 32, 32, 32,  0, 0, GL_RGB_INTEGER,  GL_UNSIGNED_INT,                   GL_UNSIGNED_INT,        false, RequireES<3, 0>,                                NeverSupported,  NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_RGBA8I,            true,  8,  8,  8,  8, 0, GL_RGBA_INTEGER, GL_BYTE,                           GL_INT,                 false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_RGBA8UI,           true,  8,  8,  8,  8, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,                  GL_UNSIGNED_INT,        false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_RGBA16I,           true, 16, 16, 16, 16, 0, GL_RGBA_INTEGER, GL_SHORT,                          GL_INT,                 false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_RGBA16UI,          true, 16, 16, 16, 16, 0, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT,                 GL_UNSIGNED_INT,        false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_RGBA32I,           true, 32, 32, 32, 32, 0, GL_RGBA_INTEGER, GL_INT,                            GL_INT,                 false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );
    AddRGBAFormat(&map, GL_RGBA32UI,          true, 32, 32, 32, 32, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT,                   GL_UNSIGNED_INT,        false, RequireES<3, 0>,                                NeverSupported,  RequireES<3, 0>,                                RequireES<3, 0>                               );

    AddRGBAFormat(&map, GL_BGRA8_EXT,         true,  8,  8,  8,  8, 0, GL_BGRA_EXT,     GL_UNSIGNED_BYTE,                  GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureFormatBGRA8888>, AlwaysSupported, RequireExt<&Extensions::textureFormatBGRA8888>, RequireExt<&Extensions::textureFormatBGRA8888>);
    AddRGBAFormat(&map, GL_BGRA4_ANGLEX,      true,  4,  4,  4,  4, 0, GL_BGRA_EXT,     GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT, GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureFormatBGRA8888>, AlwaysSupported, RequireExt<&Extensions::textureFormatBGRA8888>, RequireExt<&Extensions::textureFormatBGRA8888>);
    AddRGBAFormat(&map, GL_BGR5_A1_ANGLEX,    true,  5,  5,  5,  1, 0, GL_BGRA_EXT,     GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT, GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureFormatBGRA8888>, AlwaysSupported, RequireExt<&Extensions::textureFormatBGRA8888>, RequireExt<&Extensions::textureFormatBGRA8888>);

    // Special format that is used for D3D textures that are used within ANGLE via the
    // EGL_ANGLE_d3d_texture_client_buffer extension. We don't allow uploading texture images with
    // this format, but textures in this format can be created from D3D textures, and filtering them
    // and rendering to them is allowed.
    AddRGBAFormat(&map, GL_BGRA8_SRGB_ANGLEX, true,  8,  8,  8,  8, 0, GL_BGRA_EXT,     GL_UNSIGNED_BYTE,                  GL_UNSIGNED_NORMALIZED, true,  NeverSupported,                                 AlwaysSupported, AlwaysSupported,                                AlwaysSupported                               );

    // Special format which is not really supported, so always false for all supports.
    AddRGBAFormat(&map, GL_BGRX8_ANGLEX,      true,  8,  8,  8,  0, 0, GL_BGRA_EXT,     GL_UNSIGNED_BYTE,                  GL_UNSIGNED_NORMALIZED, false, NeverSupported,                                 NeverSupported,  NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_BGR565_ANGLEX,     true,  5,  6,  5,  1, 0, GL_BGRA_EXT,     GL_UNSIGNED_SHORT_5_6_5,           GL_UNSIGNED_NORMALIZED, false, NeverSupported,                                 NeverSupported,  NeverSupported,                                 NeverSupported                                );

    // Floating point renderability and filtering is provided by OES_texture_float and OES_texture_half_float
    //                 | Internal format |sized| R | G | B | A |S | Format | Type         | Component type | SRGB | Texture supported | Filterable                                               | Texture attachment            | Renderbuffer                 |
    AddRGBAFormat(&map, GL_R16F,          true, 16,  0,  0,  0, 0, GL_RED,  GL_HALF_FLOAT, GL_FLOAT,        false, HalfFloatRGSupport, RequireESOrExt<3, 0, &Extensions::textureHalfFloatLinear>, HalfFloatRGRenderableSupport,   HalfFloatRGRenderableSupport  );
    AddRGBAFormat(&map, GL_RG16F,         true, 16, 16,  0,  0, 0, GL_RG,   GL_HALF_FLOAT, GL_FLOAT,        false, HalfFloatRGSupport, RequireESOrExt<3, 0, &Extensions::textureHalfFloatLinear>, HalfFloatRGRenderableSupport,   HalfFloatRGRenderableSupport  );
    AddRGBAFormat(&map, GL_RGB16F,        true, 16, 16, 16,  0, 0, GL_RGB,  GL_HALF_FLOAT, GL_FLOAT,        false, HalfFloatSupport,   RequireESOrExt<3, 0, &Extensions::textureHalfFloatLinear>, HalfFloatRGBRenderableSupport,  HalfFloatRGBRenderableSupport );
    AddRGBAFormat(&map, GL_RGBA16F,       true, 16, 16, 16, 16, 0, GL_RGBA, GL_HALF_FLOAT, GL_FLOAT,        false, HalfFloatSupport,   RequireESOrExt<3, 0, &Extensions::textureHalfFloatLinear>, HalfFloatRGBARenderableSupport, HalfFloatRGBARenderableSupport);
    AddRGBAFormat(&map, GL_R32F,          true, 32,  0,  0,  0, 0, GL_RED,  GL_FLOAT,      GL_FLOAT,        false, FloatRGSupport,     RequireExt<&Extensions::textureFloatLinear>,               FloatRGRenderableSupport,       FloatRGRenderableSupport      );
    AddRGBAFormat(&map, GL_RG32F,         true, 32, 32,  0,  0, 0, GL_RG,   GL_FLOAT,      GL_FLOAT,        false, FloatRGSupport,     RequireExt<&Extensions::textureFloatLinear>,               FloatRGRenderableSupport,       FloatRGRenderableSupport      );
    AddRGBAFormat(&map, GL_RGB32F,        true, 32, 32, 32,  0, 0, GL_RGB,  GL_FLOAT,      GL_FLOAT,        false, FloatSupport,       RequireExt<&Extensions::textureFloatLinear>,               FloatRGBRenderableSupport,      FloatRGBRenderableSupport     );
    AddRGBAFormat(&map, GL_RGBA32F,       true, 32, 32, 32, 32, 0, GL_RGBA, GL_FLOAT,      GL_FLOAT,        false, FloatSupport,       RequireExt<&Extensions::textureFloatLinear>,               FloatRGBARenderableSupport,     FloatRGBARenderableSupport    );

    // Depth stencil formats
    //                         | Internal format         |sized| D |S | X | Format            | Type                             | Component type        | Texture supported                                                | Filterable                                      | Texture attachment                                                                   | Renderbuffer                                                                         |
    AddDepthStencilFormat(&map, GL_DEPTH_COMPONENT16,     true, 16, 0,  0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,                 GL_UNSIGNED_NORMALIZED, RequireES<1, 0>,                                                   RequireESOrExt<3, 0, &Extensions::depthTextures>, RequireES<1, 0>,                                                                        RequireES<1, 0>                                                                       );
    AddDepthStencilFormat(&map, GL_DEPTH_COMPONENT24,     true, 24, 0,  0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,                   GL_UNSIGNED_NORMALIZED, RequireES<3, 0>,                                                   RequireESOrExt<3, 0, &Extensions::depthTextures>, RequireES<3, 0>,                                                                        RequireES<3, 0>                                                                       );
    AddDepthStencilFormat(&map, GL_DEPTH_COMPONENT32F,    true, 32, 0,  0, GL_DEPTH_COMPONENT, GL_FLOAT,                          GL_FLOAT,               RequireES<3, 0>,                                                   RequireESOrExt<3, 0, &Extensions::depthTextures>, RequireES<3, 0>,                                                                        RequireES<3, 0>                                                                       );
    AddDepthStencilFormat(&map, GL_DEPTH_COMPONENT32_OES, true, 32, 0,  0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,                   GL_UNSIGNED_NORMALIZED, RequireExtOrExt<&Extensions::depthTextures, &Extensions::depth32>, AlwaysSupported,                                  RequireExtOrExt<&Extensions::depthTextures, &Extensions::depth32>,                      RequireExtOrExt<&Extensions::depthTextures, &Extensions::depth32>                     );
    AddDepthStencilFormat(&map, GL_DEPTH24_STENCIL8,      true, 24, 8,  0, GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8,              GL_UNSIGNED_NORMALIZED, RequireESOrExt<3, 0, &Extensions::depthTextures>,                  AlwaysSupported,                                  RequireESOrExtOrExt<3, 0, &Extensions::depthTextures, &Extensions::packedDepthStencil>, RequireESOrExtOrExt<3, 0, &Extensions::depthTextures, &Extensions::packedDepthStencil>);
    AddDepthStencilFormat(&map, GL_DEPTH32F_STENCIL8,     true, 32, 8, 24, GL_DEPTH_STENCIL,   GL_FLOAT_32_UNSIGNED_INT_24_8_REV, GL_FLOAT,               RequireES<3, 0>,                                                   AlwaysSupported,                                  RequireES<3, 0>,                                                                        RequireES<3, 0>                                                                       );
    // STENCIL_INDEX8 is special-cased, see around the bottom of the list.

    // Luminance alpha formats
    //                | Internal format           |sized| L | A | Format            | Type             | Component type        | Texture supported                                                           | Filterable                                               | Texture attachment | Renderbuffer |
    AddLUMAFormat(&map, GL_ALPHA8_EXT,             true,  0,  8, GL_ALPHA,           GL_UNSIGNED_BYTE,  GL_UNSIGNED_NORMALIZED, RequireExt<&Extensions::textureStorage>,                                      AlwaysSupported,                                           NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_LUMINANCE8_EXT,         true,  8,  0, GL_LUMINANCE,       GL_UNSIGNED_BYTE,  GL_UNSIGNED_NORMALIZED, RequireExt<&Extensions::textureStorage>,                                      AlwaysSupported,                                           NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_LUMINANCE8_ALPHA8_EXT,  true,  8,  8, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE,  GL_UNSIGNED_NORMALIZED, RequireExt<&Extensions::textureStorage>,                                      AlwaysSupported,                                           NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_ALPHA16F_EXT,           true,  0, 16, GL_ALPHA,           GL_HALF_FLOAT_OES, GL_FLOAT,               RequireExtAndExt<&Extensions::textureStorage, &Extensions::textureHalfFloat>, RequireESOrExt<3, 0, &Extensions::textureHalfFloatLinear>, NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_LUMINANCE16F_EXT,       true, 16,  0, GL_LUMINANCE,       GL_HALF_FLOAT_OES, GL_FLOAT,               RequireExtAndExt<&Extensions::textureStorage, &Extensions::textureHalfFloat>, RequireESOrExt<3, 0, &Extensions::textureHalfFloatLinear>, NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_LUMINANCE_ALPHA16F_EXT, true, 16, 16, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES, GL_FLOAT,               RequireExtAndExt<&Extensions::textureStorage, &Extensions::textureHalfFloat>, RequireESOrExt<3, 0, &Extensions::textureHalfFloatLinear>, NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_ALPHA32F_EXT,           true,  0, 32, GL_ALPHA,           GL_FLOAT,          GL_FLOAT,               RequireExtAndExt<&Extensions::textureStorage, &Extensions::textureFloat>,     RequireExt<&Extensions::textureFloatLinear>,               NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_LUMINANCE32F_EXT,       true, 32,  0, GL_LUMINANCE,       GL_FLOAT,          GL_FLOAT,               RequireExtAndExt<&Extensions::textureStorage, &Extensions::textureFloat>,     RequireExt<&Extensions::textureFloatLinear>,               NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_LUMINANCE_ALPHA32F_EXT, true, 32, 32, GL_LUMINANCE_ALPHA, GL_FLOAT,          GL_FLOAT,               RequireExtAndExt<&Extensions::textureStorage, &Extensions::textureFloat>,     RequireExt<&Extensions::textureFloatLinear>,               NeverSupported,      NeverSupported);

    // Compressed formats, From ES 3.0.1 spec, table 3.16
    //                       | Internal format                             |W |H | BS |CC| Format | Type            | SRGB | Texture supported                                                              | Filterable     | Texture attachment | Renderbuffer |
    AddCompressedFormat(&map, GL_COMPRESSED_R11_EAC,                        4, 4,  64, 1, GL_RED,  GL_UNSIGNED_BYTE, false, RequireESOrExt<3, 0, &Extensions::compressedEACR11UnsignedTexture>,              AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SIGNED_R11_EAC,                 4, 4,  64, 1, GL_RED,  GL_UNSIGNED_BYTE, false, RequireESOrExt<3, 0, &Extensions::compressedEACR11SignedTexture>,                AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RG11_EAC,                       4, 4, 128, 2, GL_RG,   GL_UNSIGNED_BYTE, false, RequireESOrExt<3, 0, &Extensions::compressedEACRG11UnsignedTexture>,             AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SIGNED_RG11_EAC,                4, 4, 128, 2, GL_RG,   GL_UNSIGNED_BYTE, false, RequireESOrExt<3, 0, &Extensions::compressedEACRG11SignedTexture>,               AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGB8_ETC2,                      4, 4,  64, 3, GL_RGB,  GL_UNSIGNED_BYTE, false, RequireESOrExt<3, 0, &Extensions::compressedETC2RGB8Texture>,                    AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ETC2,                     4, 4,  64, 3, GL_RGB,  GL_UNSIGNED_BYTE, true,  RequireESOrExt<3, 0, &Extensions::compressedETC2sRGB8Texture>,                   AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,  4, 4,  64, 3, GL_RGB,  GL_UNSIGNED_BYTE, false, RequireESOrExt<3, 0, &Extensions::compressedETC2PunchthroughARGB8Texture>,       AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, 4, 4,  64, 3, GL_RGB,  GL_UNSIGNED_BYTE, true,  RequireESOrExt<3, 0, &Extensions::compressedETC2PunchthroughAsRGB8AlphaTexture>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA8_ETC2_EAC,                 4, 4, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireESOrExt<3, 0, &Extensions::compressedETC2RGBA8Texture>,                   AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,          4, 4, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireESOrExt<3, 0, &Extensions::compressedETC2sRGB8Alpha8Texture>,             AlwaysSupported, NeverSupported,      NeverSupported);

    // From GL_EXT_texture_compression_dxt1
    //                       | Internal format                   |W |H | BS |CC| Format | Type            | SRGB | Texture supported                                 | Filterable     | Texture attachment | Renderbuffer |
    AddCompressedFormat(&map, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,    4, 4,  64, 3, GL_RGB,  GL_UNSIGNED_BYTE, false, RequireExt<&Extensions::textureCompressionDXT1>,    AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,   4, 4,  64, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExt<&Extensions::textureCompressionDXT1>,    AlwaysSupported, NeverSupported,      NeverSupported);

    // From GL_ANGLE_texture_compression_dxt3
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE, 4, 4, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExt<&Extensions::textureCompressionDXT3>,    AlwaysSupported, NeverSupported,      NeverSupported);

    // From GL_ANGLE_texture_compression_dxt5
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE, 4, 4, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExt<&Extensions::textureCompressionDXT5>,    AlwaysSupported, NeverSupported,      NeverSupported);

    // From GL_OES_compressed_ETC1_RGB8_texture
    AddCompressedFormat(&map, GL_ETC1_RGB8_OES,                   4, 4,  64, 3, GL_RGB,  GL_UNSIGNED_BYTE, false, RequireExt<&Extensions::compressedETC1RGB8Texture>, AlwaysSupported, NeverSupported,      NeverSupported);

    // From GL_EXT_texture_compression_s3tc_srgb
    //                       | Internal format                       |W |H | BS |CC| Format | Type            | SRGB | Texture supported                                 | Filterable     | Texture attachment | Renderbuffer |
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT,       4, 4,  64, 3, GL_RGB,  GL_UNSIGNED_BYTE, true, RequireExt<&Extensions::textureCompressionS3TCsRGB>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, 4, 4,  64, 4, GL_RGBA, GL_UNSIGNED_BYTE, true, RequireExt<&Extensions::textureCompressionS3TCsRGB>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, 4, 4, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true, RequireExt<&Extensions::textureCompressionS3TCsRGB>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, 4, 4, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true, RequireExt<&Extensions::textureCompressionS3TCsRGB>, AlwaysSupported, NeverSupported,      NeverSupported);

    // From KHR_texture_compression_astc_hdr
    //                       | Internal format                          | W | H | BS |CC| Format | Type            | SRGB | Texture supported                                                                              | Filterable     | Texture attachment | Renderbuffer |
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_ASTC_4x4_KHR,            4,  4, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_ASTC_5x4_KHR,            5,  4, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_ASTC_5x5_KHR,            5,  5, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_ASTC_6x5_KHR,            6,  5, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_ASTC_6x6_KHR,            6,  6, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_ASTC_8x5_KHR,            8,  5, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_ASTC_8x6_KHR,            8,  6, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_ASTC_8x8_KHR,            8,  8, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_ASTC_10x5_KHR,          10,  5, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_ASTC_10x6_KHR,          10,  6, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_ASTC_10x8_KHR,          10,  8, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_ASTC_10x10_KHR,         10, 10, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_ASTC_12x10_KHR,         12, 10, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGBA_ASTC_12x12_KHR,         12, 12, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);

    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,    4,  4, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR,    5,  4, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR,    5,  5, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR,    6,  5, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR,    6,  6, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR,    8,  5, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR,    8,  6, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR,    8,  8, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR,  10,  5, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR,  10,  6, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR,  10,  8, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR, 10, 10, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR, 12, 10, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR, 12, 12, 128, 4, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExtOrExt<&Extensions::textureCompressionASTCHDR, &Extensions::textureCompressionASTCLDR>, AlwaysSupported, NeverSupported,      NeverSupported);

    // For STENCIL_INDEX8 we chose a normalized component type for the following reasons:
    // - Multisampled buffer are disallowed for non-normalized integer component types and we want to support it for STENCIL_INDEX8
    // - All other stencil formats (all depth-stencil) are either float or normalized
    // - It affects only validation of internalformat in RenderbufferStorageMultisample.
    //                         | Internal format  |sized|D |S |X | Format    | Type            | Component type        | Texture supported | Filterable    | Texture attachment | Renderbuffer  |
    AddDepthStencilFormat(&map, GL_STENCIL_INDEX8, true, 0, 8, 0, GL_STENCIL, GL_UNSIGNED_BYTE, GL_UNSIGNED_NORMALIZED, RequireES<1, 0>,    NeverSupported, RequireES<1, 0>,     RequireES<1, 0>);

    // From GL_ANGLE_lossy_etc_decode
    //                       | Internal format                                                |W |H |BS |CC| Format | Type            | SRGB | Texture supported                      | Filterable     | Texture attachment | Renderbuffer |
    AddCompressedFormat(&map, GL_ETC1_RGB8_LOSSY_DECODE_ANGLE,                                 4, 4, 64, 3, GL_RGB,  GL_UNSIGNED_BYTE, false, RequireExt<&Extensions::lossyETCDecode>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGB8_LOSSY_DECODE_ETC2_ANGLE,                      4, 4, 64, 3, GL_RGB,  GL_UNSIGNED_BYTE, false, RequireExt<&Extensions::lossyETCDecode>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_LOSSY_DECODE_ETC2_ANGLE,                     4, 4, 64, 3, GL_RGB,  GL_UNSIGNED_BYTE, true,  RequireExt<&Extensions::lossyETCDecode>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE,  4, 4, 64, 3, GL_RGBA, GL_UNSIGNED_BYTE, false, RequireExt<&Extensions::lossyETCDecode>, AlwaysSupported, NeverSupported,      NeverSupported);
    AddCompressedFormat(&map, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_LOSSY_DECODE_ETC2_ANGLE, 4, 4, 64, 3, GL_RGBA, GL_UNSIGNED_BYTE, true,  RequireExt<&Extensions::lossyETCDecode>, AlwaysSupported, NeverSupported,      NeverSupported);

    // From GL_EXT_texture_norm16
    //                 | Internal format    |sized| R | G | B | A |S | Format | Type             | Component type        | SRGB | Texture supported                     | Filterable     | Texture attachment                    | Renderbuffer                         |
    AddRGBAFormat(&map, GL_R16_EXT,          true, 16,  0,  0,  0, 0, GL_RED,  GL_UNSIGNED_SHORT, GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureNorm16>, AlwaysSupported, RequireExt<&Extensions::textureNorm16>, RequireExt<&Extensions::textureNorm16>);
    AddRGBAFormat(&map, GL_R16_SNORM_EXT,    true, 16,  0,  0,  0, 0, GL_RED,  GL_SHORT,          GL_SIGNED_NORMALIZED,   false, RequireExt<&Extensions::textureNorm16>, AlwaysSupported, NeverSupported,                         NeverSupported                        );
    AddRGBAFormat(&map, GL_RG16_EXT,         true, 16, 16,  0,  0, 0, GL_RG,   GL_UNSIGNED_SHORT, GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureNorm16>, AlwaysSupported, RequireExt<&Extensions::textureNorm16>, RequireExt<&Extensions::textureNorm16>);
    AddRGBAFormat(&map, GL_RG16_SNORM_EXT,   true, 16, 16,  0,  0, 0, GL_RG,   GL_SHORT,          GL_SIGNED_NORMALIZED,   false, RequireExt<&Extensions::textureNorm16>, AlwaysSupported, NeverSupported,                         NeverSupported                        );
    AddRGBAFormat(&map, GL_RGB16_EXT,        true, 16, 16, 16,  0, 0, GL_RGB,  GL_UNSIGNED_SHORT, GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureNorm16>, AlwaysSupported, NeverSupported,                         NeverSupported                        );
    AddRGBAFormat(&map, GL_RGB16_SNORM_EXT,  true, 16, 16, 16,  0, 0, GL_RGB,  GL_SHORT,          GL_SIGNED_NORMALIZED,   false, RequireExt<&Extensions::textureNorm16>, AlwaysSupported, NeverSupported,                         NeverSupported                        );
    AddRGBAFormat(&map, GL_RGBA16_EXT,       true, 16, 16, 16, 16, 0, GL_RGBA, GL_UNSIGNED_SHORT, GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureNorm16>, AlwaysSupported, RequireExt<&Extensions::textureNorm16>, RequireExt<&Extensions::textureNorm16>);
    AddRGBAFormat(&map, GL_RGBA16_SNORM_EXT, true, 16, 16, 16, 16, 0, GL_RGBA, GL_SHORT,          GL_SIGNED_NORMALIZED,   false, RequireExt<&Extensions::textureNorm16>, AlwaysSupported, NeverSupported,                         NeverSupported                        );

    // Unsized formats
    //                 | Internal format  |sized | R | G | B | A |S | Format           | Type                          | Component type        | SRGB | Texture supported                             | Filterable     | Texture attachment                            | Renderbuffer                                 |
    AddRGBAFormat(&map, GL_RED,            false,  8,  0,  0,  0, 0, GL_RED,            GL_UNSIGNED_BYTE,               GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureRG>,             AlwaysSupported, AlwaysSupported,                                AlwaysSupported                               );
    AddRGBAFormat(&map, GL_RED,            false,  8,  0,  0,  0, 0, GL_RED,            GL_BYTE,                        GL_SIGNED_NORMALIZED,   false, NeverSupported,                                 NeverSupported,  NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_RG,             false,  8,  8,  0,  0, 0, GL_RG,             GL_UNSIGNED_BYTE,               GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureRG>,             AlwaysSupported, AlwaysSupported,                                AlwaysSupported                               );
    AddRGBAFormat(&map, GL_RG,             false,  8,  8,  0,  0, 0, GL_RG,             GL_BYTE,                        GL_SIGNED_NORMALIZED,   false, NeverSupported,                                 NeverSupported,  NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_RGB,            false,  8,  8,  8,  0, 0, GL_RGB,            GL_UNSIGNED_BYTE,               GL_UNSIGNED_NORMALIZED, false, RequireES<1, 0>,                                AlwaysSupported, AlwaysSupported,                                AlwaysSupported                               );
    AddRGBAFormat(&map, GL_RGB,            false,  5,  6,  5,  0, 0, GL_RGB,            GL_UNSIGNED_SHORT_5_6_5,        GL_UNSIGNED_NORMALIZED, false, RequireES<1, 0>,                                AlwaysSupported, RequireES<1, 0>,                                RequireES<1, 0>                               );
    AddRGBAFormat(&map, GL_RGB,            false,  8,  8,  8,  0, 0, GL_RGB,            GL_BYTE,                        GL_SIGNED_NORMALIZED,   false, NeverSupported,                                 NeverSupported,  NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_RGBA,           false,  4,  4,  4,  4, 0, GL_RGBA,           GL_UNSIGNED_SHORT_4_4_4_4,      GL_UNSIGNED_NORMALIZED, false, RequireES<1, 0>,                                AlwaysSupported, RequireES<1, 0>,                                RequireES<1, 0>                               );
    AddRGBAFormat(&map, GL_RGBA,           false,  5,  5,  5,  1, 0, GL_RGBA,           GL_UNSIGNED_SHORT_5_5_5_1,      GL_UNSIGNED_NORMALIZED, false, RequireES<1, 0>,                                AlwaysSupported, RequireES<1, 0>,                                RequireES<1, 0>                               );
    AddRGBAFormat(&map, GL_RGBA,           false,  8,  8,  8,  8, 0, GL_RGBA,           GL_UNSIGNED_BYTE,               GL_UNSIGNED_NORMALIZED, false, RequireES<1, 0>,                                AlwaysSupported, RequireES<1, 0>,                                RequireES<1, 0>                               );
    AddRGBAFormat(&map, GL_RGBA,           false, 10, 10, 10,  2, 0, GL_RGBA,           GL_UNSIGNED_INT_2_10_10_10_REV, GL_UNSIGNED_NORMALIZED, false, RequireES<1, 0>,                                AlwaysSupported, RequireES<1, 0>,                                RequireES<1, 0>                               );
    AddRGBAFormat(&map, GL_RGBA,           false,  8,  8,  8,  8, 0, GL_RGBA,           GL_BYTE,                        GL_SIGNED_NORMALIZED,   false, NeverSupported,                                 NeverSupported,  NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_SRGB,           false,  8,  8,  8,  0, 0, GL_SRGB,           GL_UNSIGNED_BYTE,               GL_UNSIGNED_NORMALIZED, true,  RequireExt<&Extensions::sRGB>,                  AlwaysSupported, NeverSupported,                                 NeverSupported                                );
    AddRGBAFormat(&map, GL_SRGB_ALPHA_EXT, false,  8,  8,  8,  8, 0, GL_SRGB_ALPHA_EXT, GL_UNSIGNED_BYTE,               GL_UNSIGNED_NORMALIZED, true,  RequireExt<&Extensions::sRGB>,                  AlwaysSupported, RequireExt<&Extensions::sRGB>,                  RequireExt<&Extensions::sRGB>                 );
    AddRGBAFormat(&map, GL_BGRA_EXT,       false,  8,  8,  8,  8, 0, GL_BGRA_EXT,       GL_UNSIGNED_BYTE,               GL_UNSIGNED_NORMALIZED, false, RequireExt<&Extensions::textureFormatBGRA8888>, AlwaysSupported, RequireExt<&Extensions::textureFormatBGRA8888>, RequireExt<&Extensions::textureFormatBGRA8888>);

    // Unsized integer formats
    //                 |Internal format |sized | R | G | B | A |S | Format         | Type                          | Component type | SRGB | Texture supported | Filterable    | Texture attachment | Renderbuffer |
    AddRGBAFormat(&map, GL_RED_INTEGER,  false,  8,  0,  0,  0, 0, GL_RED_INTEGER,  GL_BYTE,                        GL_INT,          false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RED_INTEGER,  false,  8,  0,  0,  0, 0, GL_RED_INTEGER,  GL_UNSIGNED_BYTE,               GL_UNSIGNED_INT, false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RED_INTEGER,  false, 16,  0,  0,  0, 0, GL_RED_INTEGER,  GL_SHORT,                       GL_INT,          false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RED_INTEGER,  false, 16,  0,  0,  0, 0, GL_RED_INTEGER,  GL_UNSIGNED_SHORT,              GL_UNSIGNED_INT, false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RED_INTEGER,  false, 32,  0,  0,  0, 0, GL_RED_INTEGER,  GL_INT,                         GL_INT,          false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RED_INTEGER,  false, 32,  0,  0,  0, 0, GL_RED_INTEGER,  GL_UNSIGNED_INT,                GL_UNSIGNED_INT, false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RG_INTEGER,   false,  8,  8,  0,  0, 0, GL_RG_INTEGER,   GL_BYTE,                        GL_INT,          false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RG_INTEGER,   false,  8,  8,  0,  0, 0, GL_RG_INTEGER,   GL_UNSIGNED_BYTE,               GL_UNSIGNED_INT, false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RG_INTEGER,   false, 16, 16,  0,  0, 0, GL_RG_INTEGER,   GL_SHORT,                       GL_INT,          false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RG_INTEGER,   false, 16, 16,  0,  0, 0, GL_RG_INTEGER,   GL_UNSIGNED_SHORT,              GL_UNSIGNED_INT, false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RG_INTEGER,   false, 32, 32,  0,  0, 0, GL_RG_INTEGER,   GL_INT,                         GL_INT,          false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RG_INTEGER,   false, 32, 32,  0,  0, 0, GL_RG_INTEGER,   GL_UNSIGNED_INT,                GL_UNSIGNED_INT, false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RGB_INTEGER,  false,  8,  8,  8,  0, 0, GL_RGB_INTEGER,  GL_BYTE,                        GL_INT,          false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RGB_INTEGER,  false,  8,  8,  8,  0, 0, GL_RGB_INTEGER,  GL_UNSIGNED_BYTE,               GL_UNSIGNED_INT, false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RGB_INTEGER,  false, 16, 16, 16,  0, 0, GL_RGB_INTEGER,  GL_SHORT,                       GL_INT,          false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RGB_INTEGER,  false, 16, 16, 16,  0, 0, GL_RGB_INTEGER,  GL_UNSIGNED_SHORT,              GL_UNSIGNED_INT, false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RGB_INTEGER,  false, 32, 32, 32,  0, 0, GL_RGB_INTEGER,  GL_INT,                         GL_INT,          false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RGB_INTEGER,  false, 32, 32, 32,  0, 0, GL_RGB_INTEGER,  GL_UNSIGNED_INT,                GL_UNSIGNED_INT, false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RGBA_INTEGER, false,  8,  8,  8,  8, 0, GL_RGBA_INTEGER, GL_BYTE,                        GL_INT,          false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RGBA_INTEGER, false,  8,  8,  8,  8, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,               GL_UNSIGNED_INT, false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RGBA_INTEGER, false, 16, 16, 16, 16, 0, GL_RGBA_INTEGER, GL_SHORT,                       GL_INT,          false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RGBA_INTEGER, false, 16, 16, 16, 16, 0, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT,              GL_UNSIGNED_INT, false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RGBA_INTEGER, false, 32, 32, 32, 32, 0, GL_RGBA_INTEGER, GL_INT,                         GL_INT,          false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RGBA_INTEGER, false, 32, 32, 32, 32, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT,                GL_UNSIGNED_INT, false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);
    AddRGBAFormat(&map, GL_RGBA_INTEGER, false, 10, 10, 10,  2, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, GL_UNSIGNED_INT, false, RequireES<3, 0>,    NeverSupported, NeverSupported,      NeverSupported);

    // Unsized floating point formats
    //                 |Internal format |sized | R | G | B | A |S | Format | Type                           | Comp    | SRGB | Texture supported           | Filterable                                               | Texture attachment                    | Renderbuffer                         |
    AddRGBAFormat(&map, GL_RED,          false, 16,  0,  0,  0, 0, GL_RED,  GL_HALF_FLOAT,                   GL_FLOAT, false, NeverSupported,               NeverSupported,                                            NeverSupported,                         NeverSupported                        );
    AddRGBAFormat(&map, GL_RG,           false, 16, 16,  0,  0, 0, GL_RG,   GL_HALF_FLOAT,                   GL_FLOAT, false, NeverSupported,               NeverSupported,                                            NeverSupported,                         NeverSupported                        );
    AddRGBAFormat(&map, GL_RGB,          false, 16, 16, 16,  0, 0, GL_RGB,  GL_HALF_FLOAT,                   GL_FLOAT, false, NeverSupported,               NeverSupported,                                            NeverSupported,                         NeverSupported                        );
    AddRGBAFormat(&map, GL_RGBA,         false, 16, 16, 16, 16, 0, GL_RGBA, GL_HALF_FLOAT,                   GL_FLOAT, false, NeverSupported,               NeverSupported,                                            NeverSupported,                         NeverSupported                        );
    AddRGBAFormat(&map, GL_RED,          false, 16,  0,  0,  0, 0, GL_RED,  GL_HALF_FLOAT_OES,               GL_FLOAT, false, UnsizedHalfFloatOESRGSupport, RequireESOrExt<3, 0, &Extensions::textureHalfFloatLinear>, UnsizedHalfFloatOESRGRenderableSupport, UnsizedHalfFloatOESRGRenderableSupport);
    AddRGBAFormat(&map, GL_RG,           false, 16, 16,  0,  0, 0, GL_RG,   GL_HALF_FLOAT_OES,               GL_FLOAT, false, UnsizedHalfFloatOESRGSupport, RequireESOrExt<3, 0, &Extensions::textureHalfFloatLinear>, UnsizedHalfFloatOESRGRenderableSupport, UnsizedHalfFloatOESRGRenderableSupport);
    AddRGBAFormat(&map, GL_RGB,          false, 16, 16, 16,  0, 0, GL_RGB,  GL_HALF_FLOAT_OES,               GL_FLOAT, false, UnsizedHalfFloatOESSupport,   RequireESOrExt<3, 0, &Extensions::textureHalfFloatLinear>, UnsizedHalfFloatOESRenderableSupport,   UnsizedHalfFloatOESRenderableSupport  );
    AddRGBAFormat(&map, GL_RGBA,         false, 16, 16, 16, 16, 0, GL_RGBA, GL_HALF_FLOAT_OES,               GL_FLOAT, false, UnsizedHalfFloatOESSupport,   RequireESOrExt<3, 0, &Extensions::textureHalfFloatLinear>, UnsizedHalfFloatOESRenderableSupport,   UnsizedHalfFloatOESRenderableSupport  );
    AddRGBAFormat(&map, GL_RED,          false, 32,  0,  0,  0, 0, GL_RED,  GL_FLOAT,                        GL_FLOAT, false, UnsizedFloatRGSupport,        RequireExt<&Extensions::textureFloatLinear>,               UnsizedFloatRGRenderableSupport,        UnsizedFloatRGRenderableSupport       );
    AddRGBAFormat(&map, GL_RG,           false, 32, 32,  0,  0, 0, GL_RG,   GL_FLOAT,                        GL_FLOAT, false, UnsizedFloatRGSupport,        RequireExt<&Extensions::textureFloatLinear>,               UnsizedFloatRGRenderableSupport,        UnsizedFloatRGRenderableSupport       );
    AddRGBAFormat(&map, GL_RGB,          false, 32, 32, 32,  0, 0, GL_RGB,  GL_FLOAT,                        GL_FLOAT, false, UnsizedFloatSupport,          RequireExt<&Extensions::textureFloatLinear>,               UnsizedFloatRGBRenderableSupport,       UnsizedFloatRGBRenderableSupport      );
    AddRGBAFormat(&map, GL_RGB,          false,  9,  9,  9,  0, 5, GL_RGB,  GL_UNSIGNED_INT_5_9_9_9_REV,     GL_FLOAT, false, NeverSupported,               NeverSupported,                                            NeverSupported,                         NeverSupported                        );
    AddRGBAFormat(&map, GL_RGB,          false, 11, 11, 10,  0, 0, GL_RGB,  GL_UNSIGNED_INT_10F_11F_11F_REV, GL_FLOAT, false, NeverSupported,               NeverSupported,                                            NeverSupported,                         NeverSupported                        );
    AddRGBAFormat(&map, GL_RGBA,         false, 32, 32, 32, 32, 0, GL_RGBA, GL_FLOAT,                        GL_FLOAT, false, UnsizedFloatSupport,          RequireExt<&Extensions::textureFloatLinear>,               UnsizedFloatRGBARenderableSupport,      UnsizedFloatRGBARenderableSupport     );

    // Unsized luminance alpha formats
    //                 | Internal format   |sized | L | A | Format            | Type             | Component type        | Texture supported                        | Filterable                                     | Texture attachment | Renderbuffer |
    AddLUMAFormat(&map, GL_ALPHA,           false,  0,  8, GL_ALPHA,           GL_UNSIGNED_BYTE,  GL_UNSIGNED_NORMALIZED, RequireES<1, 0>,                           AlwaysSupported,                                 NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_LUMINANCE,       false,  8,  0, GL_LUMINANCE,       GL_UNSIGNED_BYTE,  GL_UNSIGNED_NORMALIZED, RequireES<1, 0>,                           AlwaysSupported,                                 NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_LUMINANCE_ALPHA, false,  8,  8, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE,  GL_UNSIGNED_NORMALIZED, RequireES<1, 0>,                           AlwaysSupported,                                 NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_ALPHA,           false,  0, 16, GL_ALPHA,           GL_HALF_FLOAT_OES, GL_FLOAT,               RequireExt<&Extensions::textureHalfFloat>, RequireExt<&Extensions::textureHalfFloatLinear>, NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_LUMINANCE,       false, 16,  0, GL_LUMINANCE,       GL_HALF_FLOAT_OES, GL_FLOAT,               RequireExt<&Extensions::textureHalfFloat>, RequireExt<&Extensions::textureHalfFloatLinear>, NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_LUMINANCE_ALPHA, false, 16, 16, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES, GL_FLOAT,               RequireExt<&Extensions::textureHalfFloat>, RequireExt<&Extensions::textureHalfFloatLinear>, NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_ALPHA,           false,  0, 32, GL_ALPHA,           GL_FLOAT,          GL_FLOAT,               RequireExt<&Extensions::textureFloat>,     RequireExt<&Extensions::textureFloatLinear>,     NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_LUMINANCE,       false, 32,  0, GL_LUMINANCE,       GL_FLOAT,          GL_FLOAT,               RequireExt<&Extensions::textureFloat>,     RequireExt<&Extensions::textureFloatLinear>,     NeverSupported,      NeverSupported);
    AddLUMAFormat(&map, GL_LUMINANCE_ALPHA, false, 32, 32, GL_LUMINANCE_ALPHA, GL_FLOAT,          GL_FLOAT,               RequireExt<&Extensions::textureFloat>,     RequireExt<&Extensions::textureFloatLinear>,     NeverSupported,      NeverSupported);

    // Unsized depth stencil formats
    //                         | Internal format   |sized | D |S | X | Format            | Type                             | Component type        | Texture supported                                    | Filterable     | Texture attachment                                   | Renderbuffer                                        |
    AddDepthStencilFormat(&map, GL_DEPTH_COMPONENT, false, 16, 0,  0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,                 GL_UNSIGNED_NORMALIZED, RequireES<1, 0>,                                       AlwaysSupported, RequireES<1, 0>,                                       RequireES<1, 0>                                      );
    AddDepthStencilFormat(&map, GL_DEPTH_COMPONENT, false, 24, 0,  0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,                   GL_UNSIGNED_NORMALIZED, RequireES<1, 0>,                                       AlwaysSupported, RequireES<1, 0>,                                       RequireES<1, 0>                                      );
    AddDepthStencilFormat(&map, GL_DEPTH_COMPONENT, false, 32, 0,  0, GL_DEPTH_COMPONENT, GL_FLOAT,                          GL_FLOAT,               RequireES<1, 0>,                                       AlwaysSupported, RequireES<1, 0>,                                       RequireES<1, 0>                                      );
    AddDepthStencilFormat(&map, GL_DEPTH_STENCIL,   false, 24, 8,  0, GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8,              GL_UNSIGNED_NORMALIZED, RequireESOrExt<3, 0, &Extensions::packedDepthStencil>, AlwaysSupported, RequireESOrExt<3, 0, &Extensions::packedDepthStencil>, RequireESOrExt<3, 0, &Extensions::packedDepthStencil>);
    AddDepthStencilFormat(&map, GL_DEPTH_STENCIL,   false, 32, 8, 24, GL_DEPTH_STENCIL,   GL_FLOAT_32_UNSIGNED_INT_24_8_REV, GL_FLOAT,               RequireESOrExt<3, 0, &Extensions::packedDepthStencil>, AlwaysSupported, RequireESOrExt<3, 0, &Extensions::packedDepthStencil>, RequireESOrExt<3, 0, &Extensions::packedDepthStencil>);
    AddDepthStencilFormat(&map, GL_STENCIL,         false,  0, 8,  0, GL_STENCIL,         GL_UNSIGNED_BYTE,                  GL_UNSIGNED_NORMALIZED, RequireES<1, 0>,                                       NeverSupported , RequireES<1, 0>,                                       RequireES<1, 0>                                      );
    // clang-format on

    return map;
}

static const InternalFormatInfoMap &GetInternalFormatMap()
{
    static const InternalFormatInfoMap formatMap = BuildInternalFormatInfoMap();
    return formatMap;
}

static FormatSet BuildAllSizedInternalFormatSet()
{
    FormatSet result;

    for (const auto &internalFormat : GetInternalFormatMap())
    {
        for (const auto &type : internalFormat.second)
        {
            if (type.second.sized)
            {
                // TODO(jmadill): Fix this hack.
                if (internalFormat.first == GL_BGR565_ANGLEX)
                    continue;

                result.insert(internalFormat.first);
            }
        }
    }

    return result;
}

const Type &GetTypeInfo(GLenum type)
{
    switch (type)
    {
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:
        {
            static const Type info = GenTypeInfo(1, false);
            return info;
        }
        case GL_UNSIGNED_SHORT:
        case GL_SHORT:
        case GL_HALF_FLOAT:
        case GL_HALF_FLOAT_OES:
        {
            static const Type info = GenTypeInfo(2, false);
            return info;
        }
        case GL_UNSIGNED_INT:
        case GL_INT:
        case GL_FLOAT:
        {
            static const Type info = GenTypeInfo(4, false);
            return info;
        }
        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT:
        {
            static const Type info = GenTypeInfo(2, true);
            return info;
        }
        case GL_UNSIGNED_INT_2_10_10_10_REV:
        case GL_UNSIGNED_INT_24_8:
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
        case GL_UNSIGNED_INT_5_9_9_9_REV:
        {
            ASSERT(GL_UNSIGNED_INT_24_8_OES == GL_UNSIGNED_INT_24_8);
            static const Type info = GenTypeInfo(4, true);
            return info;
        }
        case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
        {
            static const Type info = GenTypeInfo(8, true);
            return info;
        }
        default:
        {
            static const Type defaultInfo;
            return defaultInfo;
        }
    }
}

const InternalFormat &GetSizedInternalFormatInfo(GLenum internalFormat)
{
    static const InternalFormat defaultInternalFormat;
    const InternalFormatInfoMap &formatMap = GetInternalFormatMap();
    auto iter                              = formatMap.find(internalFormat);

    // Sized internal formats only have one type per entry
    if (iter == formatMap.end() || iter->second.size() != 1)
    {
        return defaultInternalFormat;
    }

    const InternalFormat &internalFormatInfo = iter->second.begin()->second;
    if (!internalFormatInfo.sized)
    {
        return defaultInternalFormat;
    }

    return internalFormatInfo;
}

const InternalFormat &GetInternalFormatInfo(GLenum internalFormat, GLenum type)
{
    static const InternalFormat defaultInternalFormat;
    const InternalFormatInfoMap &formatMap = GetInternalFormatMap();

    auto internalFormatIter = formatMap.find(internalFormat);
    if (internalFormatIter == formatMap.end())
    {
        return defaultInternalFormat;
    }

    // If the internal format is sized, simply return it without the type check.
    if (internalFormatIter->second.size() == 1 && internalFormatIter->second.begin()->second.sized)
    {
        return internalFormatIter->second.begin()->second;
    }

    auto typeIter = internalFormatIter->second.find(type);
    if (typeIter == internalFormatIter->second.end())
    {
        return defaultInternalFormat;
    }

    return typeIter->second;
}

GLuint InternalFormat::computePixelBytes(GLenum formatType) const
{
    const auto &typeInfo = GetTypeInfo(formatType);
    GLuint components    = typeInfo.specialInterpretation ? 1u : componentCount;
    return components * typeInfo.bytes;
}

bool InternalFormat::computeRowPitch(GLenum formatType,
                                     GLsizei width,
                                     GLint alignment,
                                     GLint rowLength,
                                     GLuint *resultOut) const
{
    // Compressed images do not use pack/unpack parameters.
    if (compressed)
    {
        ASSERT(rowLength == 0);
        return computeCompressedImageSize(Extents(width, 1, 1), resultOut);
    }

    CheckedNumeric<GLuint> checkedWidth(rowLength > 0 ? rowLength : width);
    CheckedNumeric<GLuint> checkedRowBytes = checkedWidth * computePixelBytes(formatType);

    ASSERT(alignment > 0 && isPow2(alignment));
    CheckedNumeric<GLuint> checkedAlignment(alignment);
    auto aligned = rx::roundUp(checkedRowBytes, checkedAlignment);
    return CheckedMathResult(aligned, resultOut);
}

bool InternalFormat::computeDepthPitch(GLsizei height,
                                       GLint imageHeight,
                                       GLuint rowPitch,
                                       GLuint *resultOut) const
{
    GLuint rows =
        (imageHeight > 0 ? static_cast<GLuint>(imageHeight) : static_cast<GLuint>(height));
    CheckedNumeric<GLuint> checkedRowPitch(rowPitch);

    return CheckedMathResult(checkedRowPitch * rows, resultOut);
}

bool InternalFormat::computeDepthPitch(GLenum formatType,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint alignment,
                                       GLint rowLength,
                                       GLint imageHeight,
                                       GLuint *resultOut) const
{
    GLuint rowPitch = 0;
    if (!computeRowPitch(formatType, width, alignment, rowLength, &rowPitch))
    {
        return false;
    }
    return computeDepthPitch(height, imageHeight, rowPitch, resultOut);
}

bool InternalFormat::computeCompressedImageSize(const Extents &size, GLuint *resultOut) const
{
    CheckedNumeric<GLuint> checkedWidth(size.width);
    CheckedNumeric<GLuint> checkedHeight(size.height);
    CheckedNumeric<GLuint> checkedDepth(size.depth);
    CheckedNumeric<GLuint> checkedBlockWidth(compressedBlockWidth);
    CheckedNumeric<GLuint> checkedBlockHeight(compressedBlockHeight);

    ASSERT(compressed);
    auto numBlocksWide = (checkedWidth + checkedBlockWidth - 1u) / checkedBlockWidth;
    auto numBlocksHigh = (checkedHeight + checkedBlockHeight - 1u) / checkedBlockHeight;
    auto bytes         = numBlocksWide * numBlocksHigh * pixelBytes * checkedDepth;
    return CheckedMathResult(bytes, resultOut);
}

bool InternalFormat::computeSkipBytes(GLenum formatType,
                                      GLuint rowPitch,
                                      GLuint depthPitch,
                                      const PixelStoreStateBase &state,
                                      bool is3D,
                                      GLuint *resultOut) const
{
    CheckedNumeric<GLuint> checkedRowPitch(rowPitch);
    CheckedNumeric<GLuint> checkedDepthPitch(depthPitch);
    CheckedNumeric<GLuint> checkedSkipImages(static_cast<GLuint>(state.skipImages));
    CheckedNumeric<GLuint> checkedSkipRows(static_cast<GLuint>(state.skipRows));
    CheckedNumeric<GLuint> checkedSkipPixels(static_cast<GLuint>(state.skipPixels));
    CheckedNumeric<GLuint> checkedPixelBytes(computePixelBytes(formatType));
    auto checkedSkipImagesBytes = checkedSkipImages * checkedDepthPitch;
    if (!is3D)
    {
        checkedSkipImagesBytes = 0;
    }
    auto skipBytes = checkedSkipImagesBytes + checkedSkipRows * checkedRowPitch +
                     checkedSkipPixels * checkedPixelBytes;
    return CheckedMathResult(skipBytes, resultOut);
}

bool InternalFormat::computePackUnpackEndByte(GLenum formatType,
                                              const Extents &size,
                                              const PixelStoreStateBase &state,
                                              bool is3D,
                                              GLuint *resultOut) const
{
    GLuint rowPitch = 0;
    if (!computeRowPitch(formatType, size.width, state.alignment, state.rowLength, &rowPitch))
    {
        return false;
    }

    GLuint depthPitch = 0;
    if (is3D && !computeDepthPitch(size.height, state.imageHeight, rowPitch, &depthPitch))
    {
        return false;
    }

    CheckedNumeric<GLuint> checkedCopyBytes(0);
    if (compressed)
    {
        GLuint copyBytes = 0;
        if (!computeCompressedImageSize(size, &copyBytes))
        {
            return false;
        }
        checkedCopyBytes = copyBytes;
    }
    else if (size.height != 0 && (!is3D || size.depth != 0))
    {
        CheckedNumeric<GLuint> bytes = computePixelBytes(formatType);
        checkedCopyBytes += size.width * bytes;

        CheckedNumeric<GLuint> heightMinusOne = size.height - 1;
        checkedCopyBytes += heightMinusOne * rowPitch;

        if (is3D)
        {
            CheckedNumeric<GLuint> depthMinusOne = size.depth - 1;
            checkedCopyBytes += depthMinusOne * depthPitch;
        }
    }

    GLuint skipBytes = 0;
    if (!computeSkipBytes(formatType, rowPitch, depthPitch, state, is3D, &skipBytes))
    {
        return false;
    }

    CheckedNumeric<GLuint> endByte = checkedCopyBytes + CheckedNumeric<GLuint>(skipBytes);

    return CheckedMathResult(endByte, resultOut);
}

GLenum GetUnsizedFormat(GLenum internalFormat)
{
    auto sizedFormatInfo = GetSizedInternalFormatInfo(internalFormat);
    if (sizedFormatInfo.internalFormat != GL_NONE)
    {
        return sizedFormatInfo.format;
    }

    return internalFormat;
}

const FormatSet &GetAllSizedInternalFormats()
{
    static FormatSet formatSet = BuildAllSizedInternalFormatSet();
    return formatSet;
}

AttributeType GetAttributeType(GLenum enumValue)
{
    switch (enumValue)
    {
        case GL_FLOAT:
            return ATTRIBUTE_FLOAT;
        case GL_FLOAT_VEC2:
            return ATTRIBUTE_VEC2;
        case GL_FLOAT_VEC3:
            return ATTRIBUTE_VEC3;
        case GL_FLOAT_VEC4:
            return ATTRIBUTE_VEC4;
        case GL_INT:
            return ATTRIBUTE_INT;
        case GL_INT_VEC2:
            return ATTRIBUTE_IVEC2;
        case GL_INT_VEC3:
            return ATTRIBUTE_IVEC3;
        case GL_INT_VEC4:
            return ATTRIBUTE_IVEC4;
        case GL_UNSIGNED_INT:
            return ATTRIBUTE_UINT;
        case GL_UNSIGNED_INT_VEC2:
            return ATTRIBUTE_UVEC2;
        case GL_UNSIGNED_INT_VEC3:
            return ATTRIBUTE_UVEC3;
        case GL_UNSIGNED_INT_VEC4:
            return ATTRIBUTE_UVEC4;
        case GL_FLOAT_MAT2:
            return ATTRIBUTE_MAT2;
        case GL_FLOAT_MAT3:
            return ATTRIBUTE_MAT3;
        case GL_FLOAT_MAT4:
            return ATTRIBUTE_MAT4;
        case GL_FLOAT_MAT2x3:
            return ATTRIBUTE_MAT2x3;
        case GL_FLOAT_MAT2x4:
            return ATTRIBUTE_MAT2x4;
        case GL_FLOAT_MAT3x2:
            return ATTRIBUTE_MAT3x2;
        case GL_FLOAT_MAT3x4:
            return ATTRIBUTE_MAT3x4;
        case GL_FLOAT_MAT4x2:
            return ATTRIBUTE_MAT4x2;
        case GL_FLOAT_MAT4x3:
            return ATTRIBUTE_MAT4x3;
        default:
            UNREACHABLE();
#if !UNREACHABLE_IS_NORETURN
            return ATTRIBUTE_FLOAT;
#endif
    }
}

angle::FormatID GetVertexFormatID(GLenum type,
                                  GLboolean normalized,
                                  GLuint components,
                                  bool pureInteger)
{
    switch (type)
    {
        case GL_BYTE:
            switch (components)
            {
                case 1:
                    if (pureInteger)
                        return angle::FormatID::R8_SINT;
                    if (normalized)
                        return angle::FormatID::R8_SNORM;
                    return angle::FormatID::R8_SSCALED;
                case 2:
                    if (pureInteger)
                        return angle::FormatID::R8G8_SINT;
                    if (normalized)
                        return angle::FormatID::R8G8_SNORM;
                    return angle::FormatID::R8G8_SSCALED;
                case 3:
                    if (pureInteger)
                        return angle::FormatID::R8G8B8_SINT;
                    if (normalized)
                        return angle::FormatID::R8G8B8_SNORM;
                    return angle::FormatID::R8G8B8_SSCALED;
                case 4:
                    if (pureInteger)
                        return angle::FormatID::R8G8B8A8_SINT;
                    if (normalized)
                        return angle::FormatID::R8G8B8A8_SNORM;
                    return angle::FormatID::R8G8B8A8_SSCALED;
                default:
                    UNREACHABLE();
#if !UNREACHABLE_IS_NORETURN
                    return angle::FormatID::NONE;
#endif
            }
        case GL_UNSIGNED_BYTE:
            switch (components)
            {
                case 1:
                    if (pureInteger)
                        return angle::FormatID::R8_UINT;
                    if (normalized)
                        return angle::FormatID::R8_UNORM;
                    return angle::FormatID::R8_USCALED;
                case 2:
                    if (pureInteger)
                        return angle::FormatID::R8G8_UINT;
                    if (normalized)
                        return angle::FormatID::R8G8_UNORM;
                    return angle::FormatID::R8G8_USCALED;
                case 3:
                    if (pureInteger)
                        return angle::FormatID::R8G8B8_UINT;
                    if (normalized)
                        return angle::FormatID::R8G8B8_UNORM;
                    return angle::FormatID::R8G8B8_USCALED;
                case 4:
                    if (pureInteger)
                        return angle::FormatID::R8G8B8A8_UINT;
                    if (normalized)
                        return angle::FormatID::R8G8B8A8_UNORM;
                    return angle::FormatID::R8G8B8A8_USCALED;
                default:
                    UNREACHABLE();
#if !UNREACHABLE_IS_NORETURN
                    return angle::FormatID::NONE;
#endif
            }
        case GL_SHORT:
            switch (components)
            {
                case 1:
                    if (pureInteger)
                        return angle::FormatID::R16_SINT;
                    if (normalized)
                        return angle::FormatID::R16_SNORM;
                    return angle::FormatID::R16_SSCALED;
                case 2:
                    if (pureInteger)
                        return angle::FormatID::R16G16_SINT;
                    if (normalized)
                        return angle::FormatID::R16G16_SNORM;
                    return angle::FormatID::R16G16_SSCALED;
                case 3:
                    if (pureInteger)
                        return angle::FormatID::R16G16B16_SINT;
                    if (normalized)
                        return angle::FormatID::R16G16B16_SNORM;
                    return angle::FormatID::R16G16B16_SSCALED;
                case 4:
                    if (pureInteger)
                        return angle::FormatID::R16G16B16A16_SINT;
                    if (normalized)
                        return angle::FormatID::R16G16B16A16_SNORM;
                    return angle::FormatID::R16G16B16A16_SSCALED;
                default:
                    UNREACHABLE();
#if !UNREACHABLE_IS_NORETURN
                    return angle::FormatID::NONE;
#endif
            }
        case GL_UNSIGNED_SHORT:
            switch (components)
            {
                case 1:
                    if (pureInteger)
                        return angle::FormatID::R16_UINT;
                    if (normalized)
                        return angle::FormatID::R16_UNORM;
                    return angle::FormatID::R16_USCALED;
                case 2:
                    if (pureInteger)
                        return angle::FormatID::R16G16_UINT;
                    if (normalized)
                        return angle::FormatID::R16G16_UNORM;
                    return angle::FormatID::R16G16_USCALED;
                case 3:
                    if (pureInteger)
                        return angle::FormatID::R16G16B16_UINT;
                    if (normalized)
                        return angle::FormatID::R16G16B16_UNORM;
                    return angle::FormatID::R16G16B16_USCALED;
                case 4:
                    if (pureInteger)
                        return angle::FormatID::R16G16B16A16_UINT;
                    if (normalized)
                        return angle::FormatID::R16G16B16A16_UNORM;
                    return angle::FormatID::R16G16B16A16_USCALED;
                default:
                    UNREACHABLE();
#if !UNREACHABLE_IS_NORETURN
                    return angle::FormatID::NONE;
#endif
            }
        case GL_INT:
            switch (components)
            {
                case 1:
                    if (pureInteger)
                        return angle::FormatID::R32_SINT;
                    if (normalized)
                        return angle::FormatID::R32_SNORM;
                    return angle::FormatID::R32_SSCALED;
                case 2:
                    if (pureInteger)
                        return angle::FormatID::R32G32_SINT;
                    if (normalized)
                        return angle::FormatID::R32G32_SNORM;
                    return angle::FormatID::R32G32_SSCALED;
                case 3:
                    if (pureInteger)
                        return angle::FormatID::R32G32B32_SINT;
                    if (normalized)
                        return angle::FormatID::R32G32B32_SNORM;
                    return angle::FormatID::R32G32B32_SSCALED;
                case 4:
                    if (pureInteger)
                        return angle::FormatID::R32G32B32A32_SINT;
                    if (normalized)
                        return angle::FormatID::R32G32B32A32_SNORM;
                    return angle::FormatID::R32G32B32A32_SSCALED;
                default:
                    UNREACHABLE();
#if !UNREACHABLE_IS_NORETURN
                    return angle::FormatID::NONE;
#endif
            }
        case GL_UNSIGNED_INT:
            switch (components)
            {
                case 1:
                    if (pureInteger)
                        return angle::FormatID::R32_UINT;
                    if (normalized)
                        return angle::FormatID::R32_UNORM;
                    return angle::FormatID::R32_USCALED;
                case 2:
                    if (pureInteger)
                        return angle::FormatID::R32G32_UINT;
                    if (normalized)
                        return angle::FormatID::R32G32_UNORM;
                    return angle::FormatID::R32G32_USCALED;
                case 3:
                    if (pureInteger)
                        return angle::FormatID::R32G32B32_UINT;
                    if (normalized)
                        return angle::FormatID::R32G32B32_UNORM;
                    return angle::FormatID::R32G32B32_USCALED;
                case 4:
                    if (pureInteger)
                        return angle::FormatID::R32G32B32A32_UINT;
                    if (normalized)
                        return angle::FormatID::R32G32B32A32_UNORM;
                    return angle::FormatID::R32G32B32A32_USCALED;
                default:
                    UNREACHABLE();
#if !UNREACHABLE_IS_NORETURN
                    return angle::FormatID::NONE;
#endif
            }
        case GL_FLOAT:
            switch (components)
            {
                case 1:
                    return angle::FormatID::R32_FLOAT;
                case 2:
                    return angle::FormatID::R32G32_FLOAT;
                case 3:
                    return angle::FormatID::R32G32B32_FLOAT;
                case 4:
                    return angle::FormatID::R32G32B32A32_FLOAT;
                default:
                    UNREACHABLE();
#if !UNREACHABLE_IS_NORETURN
                    return angle::FormatID::NONE;
#endif
            }
        case GL_HALF_FLOAT:
            switch (components)
            {
                case 1:
                    return angle::FormatID::R16_FLOAT;
                case 2:
                    return angle::FormatID::R16G16_FLOAT;
                case 3:
                    return angle::FormatID::R16G16B16_FLOAT;
                case 4:
                    return angle::FormatID::R16G16B16A16_FLOAT;
                default:
                    UNREACHABLE();
#if !UNREACHABLE_IS_NORETURN
                    return angle::FormatID::NONE;
#endif
            }
        case GL_FIXED:
            switch (components)
            {
                case 1:
                    return angle::FormatID::R32_FIXED;
                case 2:
                    return angle::FormatID::R32G32_FIXED;
                case 3:
                    return angle::FormatID::R32G32B32_FIXED;
                case 4:
                    return angle::FormatID::R32G32B32A32_FIXED;
                default:
                    UNREACHABLE();
#if !UNREACHABLE_IS_NORETURN
                    return angle::FormatID::NONE;
#endif
            }
        case GL_INT_2_10_10_10_REV:
            if (pureInteger)
                return angle::FormatID::R10G10B10A2_SINT;
            if (normalized)
                return angle::FormatID::R10G10B10A2_SNORM;
            return angle::FormatID::R10G10B10A2_SSCALED;
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            if (pureInteger)
                return angle::FormatID::R10G10B10A2_UINT;
            if (normalized)
                return angle::FormatID::R10G10B10A2_UNORM;
            return angle::FormatID::R10G10B10A2_USCALED;
        default:
            UNREACHABLE();
#if !UNREACHABLE_IS_NORETURN
            return angle::FormatID::NONE;
#endif
    }
}

angle::FormatID GetVertexFormatID(const VertexAttribute &attrib)
{
    return GetVertexFormatID(attrib.type, attrib.normalized, attrib.size, attrib.pureInteger);
}

// TODO(fjhenigman): Do away with VertexFormatType; use angle::FormatID instead. anglebug.com/2531
VertexFormatType GetVertexFormatType(GLenum type,
                                     GLboolean normalized,
                                     GLuint components,
                                     bool pureInteger)
{
    switch (GetVertexFormatID(type, normalized, components, pureInteger))
    {
        case angle::FormatID::R8_SINT:
            return VERTEX_FORMAT_SBYTE1_INT;
        case angle::FormatID::R8_SNORM:
            return VERTEX_FORMAT_SBYTE1_NORM;
        case angle::FormatID::R8_SSCALED:
            return VERTEX_FORMAT_SBYTE1;
        case angle::FormatID::R8G8_SINT:
            return VERTEX_FORMAT_SBYTE2_INT;
        case angle::FormatID::R8G8_SNORM:
            return VERTEX_FORMAT_SBYTE2_NORM;
        case angle::FormatID::R8G8_SSCALED:
            return VERTEX_FORMAT_SBYTE2;
        case angle::FormatID::R8G8B8_SINT:
            return VERTEX_FORMAT_SBYTE3_INT;
        case angle::FormatID::R8G8B8_SNORM:
            return VERTEX_FORMAT_SBYTE3_NORM;
        case angle::FormatID::R8G8B8_SSCALED:
            return VERTEX_FORMAT_SBYTE3;
        case angle::FormatID::R8G8B8A8_SINT:
            return VERTEX_FORMAT_SBYTE4_INT;
        case angle::FormatID::R8G8B8A8_SNORM:
            return VERTEX_FORMAT_SBYTE4_NORM;
        case angle::FormatID::R8G8B8A8_SSCALED:
            return VERTEX_FORMAT_SBYTE4;
        case angle::FormatID::R8_UINT:
            return VERTEX_FORMAT_UBYTE1_INT;
        case angle::FormatID::R8_UNORM:
            return VERTEX_FORMAT_UBYTE1_NORM;
        case angle::FormatID::R8_USCALED:
            return VERTEX_FORMAT_UBYTE1;
        case angle::FormatID::R8G8_UINT:
            return VERTEX_FORMAT_UBYTE2_INT;
        case angle::FormatID::R8G8_UNORM:
            return VERTEX_FORMAT_UBYTE2_NORM;
        case angle::FormatID::R8G8_USCALED:
            return VERTEX_FORMAT_UBYTE2;
        case angle::FormatID::R8G8B8_UINT:
            return VERTEX_FORMAT_UBYTE3_INT;
        case angle::FormatID::R8G8B8_UNORM:
            return VERTEX_FORMAT_UBYTE3_NORM;
        case angle::FormatID::R8G8B8_USCALED:
            return VERTEX_FORMAT_UBYTE3;
        case angle::FormatID::R8G8B8A8_UINT:
            return VERTEX_FORMAT_UBYTE4_INT;
        case angle::FormatID::R8G8B8A8_UNORM:
            return VERTEX_FORMAT_UBYTE4_NORM;
        case angle::FormatID::R8G8B8A8_USCALED:
            return VERTEX_FORMAT_UBYTE4;
        case angle::FormatID::R16_SINT:
            return VERTEX_FORMAT_SSHORT1_INT;
        case angle::FormatID::R16_SNORM:
            return VERTEX_FORMAT_SSHORT1_NORM;
        case angle::FormatID::R16_SSCALED:
            return VERTEX_FORMAT_SSHORT1;
        case angle::FormatID::R16G16_SINT:
            return VERTEX_FORMAT_SSHORT2_INT;
        case angle::FormatID::R16G16_SNORM:
            return VERTEX_FORMAT_SSHORT2_NORM;
        case angle::FormatID::R16G16_SSCALED:
            return VERTEX_FORMAT_SSHORT2;
        case angle::FormatID::R16G16B16_SINT:
            return VERTEX_FORMAT_SSHORT3_INT;
        case angle::FormatID::R16G16B16_SNORM:
            return VERTEX_FORMAT_SSHORT3_NORM;
        case angle::FormatID::R16G16B16_SSCALED:
            return VERTEX_FORMAT_SSHORT3;
        case angle::FormatID::R16G16B16A16_SINT:
            return VERTEX_FORMAT_SSHORT4_INT;
        case angle::FormatID::R16G16B16A16_SNORM:
            return VERTEX_FORMAT_SSHORT4_NORM;
        case angle::FormatID::R16G16B16A16_SSCALED:
            return VERTEX_FORMAT_SSHORT4;
        case angle::FormatID::R16_UINT:
            return VERTEX_FORMAT_USHORT1_INT;
        case angle::FormatID::R16_UNORM:
            return VERTEX_FORMAT_USHORT1_NORM;
        case angle::FormatID::R16_USCALED:
            return VERTEX_FORMAT_USHORT1;
        case angle::FormatID::R16G16_UINT:
            return VERTEX_FORMAT_USHORT2_INT;
        case angle::FormatID::R16G16_UNORM:
            return VERTEX_FORMAT_USHORT2_NORM;
        case angle::FormatID::R16G16_USCALED:
            return VERTEX_FORMAT_USHORT2;
        case angle::FormatID::R16G16B16_UINT:
            return VERTEX_FORMAT_USHORT3_INT;
        case angle::FormatID::R16G16B16_UNORM:
            return VERTEX_FORMAT_USHORT3_NORM;
        case angle::FormatID::R16G16B16_USCALED:
            return VERTEX_FORMAT_USHORT3;
        case angle::FormatID::R16G16B16A16_UINT:
            return VERTEX_FORMAT_USHORT4_INT;
        case angle::FormatID::R16G16B16A16_UNORM:
            return VERTEX_FORMAT_USHORT4_NORM;
        case angle::FormatID::R16G16B16A16_USCALED:
            return VERTEX_FORMAT_USHORT4;
        case angle::FormatID::R32_SINT:
            return VERTEX_FORMAT_SINT1_INT;
        case angle::FormatID::R32_SNORM:
            return VERTEX_FORMAT_SINT1_NORM;
        case angle::FormatID::R32_SSCALED:
            return VERTEX_FORMAT_SINT1;
        case angle::FormatID::R32G32_SINT:
            return VERTEX_FORMAT_SINT2_INT;
        case angle::FormatID::R32G32_SNORM:
            return VERTEX_FORMAT_SINT2_NORM;
        case angle::FormatID::R32G32_SSCALED:
            return VERTEX_FORMAT_SINT2;
        case angle::FormatID::R32G32B32_SINT:
            return VERTEX_FORMAT_SINT3_INT;
        case angle::FormatID::R32G32B32_SNORM:
            return VERTEX_FORMAT_SINT3_NORM;
        case angle::FormatID::R32G32B32_SSCALED:
            return VERTEX_FORMAT_SINT3;
        case angle::FormatID::R32G32B32A32_SINT:
            return VERTEX_FORMAT_SINT4_INT;
        case angle::FormatID::R32G32B32A32_SNORM:
            return VERTEX_FORMAT_SINT4_NORM;
        case angle::FormatID::R32G32B32A32_SSCALED:
            return VERTEX_FORMAT_SINT4;
        case angle::FormatID::R32_UINT:
            return VERTEX_FORMAT_UINT1_INT;
        case angle::FormatID::R32_UNORM:
            return VERTEX_FORMAT_UINT1_NORM;
        case angle::FormatID::R32_USCALED:
            return VERTEX_FORMAT_UINT1;
        case angle::FormatID::R32G32_UINT:
            return VERTEX_FORMAT_UINT2_INT;
        case angle::FormatID::R32G32_UNORM:
            return VERTEX_FORMAT_UINT2_NORM;
        case angle::FormatID::R32G32_USCALED:
            return VERTEX_FORMAT_UINT2;
        case angle::FormatID::R32G32B32_UINT:
            return VERTEX_FORMAT_UINT3_INT;
        case angle::FormatID::R32G32B32_UNORM:
            return VERTEX_FORMAT_UINT3_NORM;
        case angle::FormatID::R32G32B32_USCALED:
            return VERTEX_FORMAT_UINT3;
        case angle::FormatID::R32G32B32A32_UINT:
            return VERTEX_FORMAT_UINT4_INT;
        case angle::FormatID::R32G32B32A32_UNORM:
            return VERTEX_FORMAT_UINT4_NORM;
        case angle::FormatID::R32G32B32A32_USCALED:
            return VERTEX_FORMAT_UINT4;
        case angle::FormatID::R32_FLOAT:
            return VERTEX_FORMAT_FLOAT1;
        case angle::FormatID::R32G32_FLOAT:
            return VERTEX_FORMAT_FLOAT2;
        case angle::FormatID::R32G32B32_FLOAT:
            return VERTEX_FORMAT_FLOAT3;
        case angle::FormatID::R32G32B32A32_FLOAT:
            return VERTEX_FORMAT_FLOAT4;
        case angle::FormatID::R16_FLOAT:
            return VERTEX_FORMAT_HALF1;
        case angle::FormatID::R16G16_FLOAT:
            return VERTEX_FORMAT_HALF2;
        case angle::FormatID::R16G16B16_FLOAT:
            return VERTEX_FORMAT_HALF3;
        case angle::FormatID::R16G16B16A16_FLOAT:
            return VERTEX_FORMAT_HALF4;
        case angle::FormatID::R32_FIXED:
            return VERTEX_FORMAT_FIXED1;
        case angle::FormatID::R32G32_FIXED:
            return VERTEX_FORMAT_FIXED2;
        case angle::FormatID::R32G32B32_FIXED:
            return VERTEX_FORMAT_FIXED3;
        case angle::FormatID::R32G32B32A32_FIXED:
            return VERTEX_FORMAT_FIXED4;
        case angle::FormatID::R10G10B10A2_SINT:
            return VERTEX_FORMAT_SINT210_INT;
        case angle::FormatID::R10G10B10A2_SNORM:
            return VERTEX_FORMAT_SINT210_NORM;
        case angle::FormatID::R10G10B10A2_SSCALED:
            return VERTEX_FORMAT_SINT210;
        case angle::FormatID::R10G10B10A2_UINT:
            return VERTEX_FORMAT_UINT210_INT;
        case angle::FormatID::R10G10B10A2_UNORM:
            return VERTEX_FORMAT_UINT210_NORM;
        case angle::FormatID::R10G10B10A2_USCALED:
            return VERTEX_FORMAT_UINT210;
        default:
            return VERTEX_FORMAT_INVALID;
    }
}

VertexFormatType GetVertexFormatType(const VertexAttribute &attrib)
{
    return GetVertexFormatType(attrib.type, attrib.normalized, attrib.size, attrib.pureInteger);
}

VertexFormatType GetVertexFormatType(const VertexAttribute &attrib, GLenum currentValueType)
{
    if (!attrib.enabled)
    {
        return GetVertexFormatType(currentValueType, GL_FALSE, 4, (currentValueType != GL_FLOAT));
    }
    return GetVertexFormatType(attrib);
}

const VertexFormat &GetVertexFormatFromType(VertexFormatType vertexFormatType)
{
    switch (vertexFormatType)
    {
        case VERTEX_FORMAT_SBYTE1:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE1_NORM:
        {
            static const VertexFormat format(GL_BYTE, GL_TRUE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE2:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE2_NORM:
        {
            static const VertexFormat format(GL_BYTE, GL_TRUE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE3:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE3_NORM:
        {
            static const VertexFormat format(GL_BYTE, GL_TRUE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE4:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE4_NORM:
        {
            static const VertexFormat format(GL_BYTE, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE1:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE1_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_TRUE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE2:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE2_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_TRUE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE3:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE3_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_TRUE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE4:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_UBYTE4_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT1:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT1_NORM:
        {
            static const VertexFormat format(GL_SHORT, GL_TRUE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT2:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT2_NORM:
        {
            static const VertexFormat format(GL_SHORT, GL_TRUE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT3:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT3_NORM:
        {
            static const VertexFormat format(GL_SHORT, GL_TRUE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT4:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SSHORT4_NORM:
        {
            static const VertexFormat format(GL_SHORT, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT1:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT1_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_TRUE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT2:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT2_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_TRUE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT3:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT3_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_TRUE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT4:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_USHORT4_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SINT1:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_SINT1_NORM:
        {
            static const VertexFormat format(GL_INT, GL_TRUE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_SINT2:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_SINT2_NORM:
        {
            static const VertexFormat format(GL_INT, GL_TRUE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_SINT3:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_SINT3_NORM:
        {
            static const VertexFormat format(GL_INT, GL_TRUE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_SINT4:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SINT4_NORM:
        {
            static const VertexFormat format(GL_INT, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_UINT1:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_UINT1_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_TRUE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_UINT2:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_UINT2_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_TRUE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_UINT3:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_UINT3_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_TRUE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_UINT4:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_UINT4_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SBYTE1_INT:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 1, true);
            return format;
        }
        case VERTEX_FORMAT_SBYTE2_INT:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 2, true);
            return format;
        }
        case VERTEX_FORMAT_SBYTE3_INT:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 3, true);
            return format;
        }
        case VERTEX_FORMAT_SBYTE4_INT:
        {
            static const VertexFormat format(GL_BYTE, GL_FALSE, 4, true);
            return format;
        }
        case VERTEX_FORMAT_UBYTE1_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 1, true);
            return format;
        }
        case VERTEX_FORMAT_UBYTE2_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 2, true);
            return format;
        }
        case VERTEX_FORMAT_UBYTE3_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 3, true);
            return format;
        }
        case VERTEX_FORMAT_UBYTE4_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_BYTE, GL_FALSE, 4, true);
            return format;
        }
        case VERTEX_FORMAT_SSHORT1_INT:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 1, true);
            return format;
        }
        case VERTEX_FORMAT_SSHORT2_INT:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 2, true);
            return format;
        }
        case VERTEX_FORMAT_SSHORT3_INT:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 3, true);
            return format;
        }
        case VERTEX_FORMAT_SSHORT4_INT:
        {
            static const VertexFormat format(GL_SHORT, GL_FALSE, 4, true);
            return format;
        }
        case VERTEX_FORMAT_USHORT1_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 1, true);
            return format;
        }
        case VERTEX_FORMAT_USHORT2_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 2, true);
            return format;
        }
        case VERTEX_FORMAT_USHORT3_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 3, true);
            return format;
        }
        case VERTEX_FORMAT_USHORT4_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_SHORT, GL_FALSE, 4, true);
            return format;
        }
        case VERTEX_FORMAT_SINT1_INT:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 1, true);
            return format;
        }
        case VERTEX_FORMAT_SINT2_INT:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 2, true);
            return format;
        }
        case VERTEX_FORMAT_SINT3_INT:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 3, true);
            return format;
        }
        case VERTEX_FORMAT_SINT4_INT:
        {
            static const VertexFormat format(GL_INT, GL_FALSE, 4, true);
            return format;
        }
        case VERTEX_FORMAT_UINT1_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 1, true);
            return format;
        }
        case VERTEX_FORMAT_UINT2_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 2, true);
            return format;
        }
        case VERTEX_FORMAT_UINT3_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 3, true);
            return format;
        }
        case VERTEX_FORMAT_UINT4_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_INT, GL_FALSE, 4, true);
            return format;
        }
        case VERTEX_FORMAT_FIXED1:
        {
            static const VertexFormat format(GL_FIXED, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_FIXED2:
        {
            static const VertexFormat format(GL_FIXED, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_FIXED3:
        {
            static const VertexFormat format(GL_FIXED, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_FIXED4:
        {
            static const VertexFormat format(GL_FIXED, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_HALF1:
        {
            static const VertexFormat format(GL_HALF_FLOAT, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_HALF2:
        {
            static const VertexFormat format(GL_HALF_FLOAT, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_HALF3:
        {
            static const VertexFormat format(GL_HALF_FLOAT, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_HALF4:
        {
            static const VertexFormat format(GL_HALF_FLOAT, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_FLOAT1:
        {
            static const VertexFormat format(GL_FLOAT, GL_FALSE, 1, false);
            return format;
        }
        case VERTEX_FORMAT_FLOAT2:
        {
            static const VertexFormat format(GL_FLOAT, GL_FALSE, 2, false);
            return format;
        }
        case VERTEX_FORMAT_FLOAT3:
        {
            static const VertexFormat format(GL_FLOAT, GL_FALSE, 3, false);
            return format;
        }
        case VERTEX_FORMAT_FLOAT4:
        {
            static const VertexFormat format(GL_FLOAT, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SINT210:
        {
            static const VertexFormat format(GL_INT_2_10_10_10_REV, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_UINT210:
        {
            static const VertexFormat format(GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SINT210_NORM:
        {
            static const VertexFormat format(GL_INT_2_10_10_10_REV, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_UINT210_NORM:
        {
            static const VertexFormat format(GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, 4, false);
            return format;
        }
        case VERTEX_FORMAT_SINT210_INT:
        {
            static const VertexFormat format(GL_INT_2_10_10_10_REV, GL_FALSE, 4, true);
            return format;
        }
        case VERTEX_FORMAT_UINT210_INT:
        {
            static const VertexFormat format(GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, 4, true);
            return format;
        }
        default:
        {
            static const VertexFormat format(GL_NONE, GL_FALSE, 0, false);
            return format;
        }
    }
}

size_t GetVertexFormatTypeSize(VertexFormatType vertexFormatType)
{
    switch (vertexFormatType)
    {
        case VERTEX_FORMAT_SBYTE1:
        case VERTEX_FORMAT_SBYTE1_NORM:
        case VERTEX_FORMAT_UBYTE1:
        case VERTEX_FORMAT_UBYTE1_NORM:
        case VERTEX_FORMAT_SBYTE1_INT:
        case VERTEX_FORMAT_UBYTE1_INT:
            return 1;

        case VERTEX_FORMAT_SBYTE2:
        case VERTEX_FORMAT_SBYTE2_NORM:
        case VERTEX_FORMAT_UBYTE2:
        case VERTEX_FORMAT_UBYTE2_NORM:
        case VERTEX_FORMAT_SBYTE2_INT:
        case VERTEX_FORMAT_UBYTE2_INT:
        case VERTEX_FORMAT_SSHORT1:
        case VERTEX_FORMAT_SSHORT1_NORM:
        case VERTEX_FORMAT_USHORT1:
        case VERTEX_FORMAT_USHORT1_NORM:
        case VERTEX_FORMAT_SSHORT1_INT:
        case VERTEX_FORMAT_USHORT1_INT:
        case VERTEX_FORMAT_HALF1:
            return 2;

        case VERTEX_FORMAT_SBYTE3:
        case VERTEX_FORMAT_SBYTE3_NORM:
        case VERTEX_FORMAT_UBYTE3:
        case VERTEX_FORMAT_UBYTE3_NORM:
        case VERTEX_FORMAT_SBYTE3_INT:
        case VERTEX_FORMAT_UBYTE3_INT:
            return 3;

        case VERTEX_FORMAT_SBYTE4:
        case VERTEX_FORMAT_SBYTE4_NORM:
        case VERTEX_FORMAT_UBYTE4:
        case VERTEX_FORMAT_UBYTE4_NORM:
        case VERTEX_FORMAT_SBYTE4_INT:
        case VERTEX_FORMAT_UBYTE4_INT:
        case VERTEX_FORMAT_SSHORT2:
        case VERTEX_FORMAT_SSHORT2_NORM:
        case VERTEX_FORMAT_USHORT2:
        case VERTEX_FORMAT_USHORT2_NORM:
        case VERTEX_FORMAT_SSHORT2_INT:
        case VERTEX_FORMAT_USHORT2_INT:
        case VERTEX_FORMAT_SINT1:
        case VERTEX_FORMAT_SINT1_NORM:
        case VERTEX_FORMAT_UINT1:
        case VERTEX_FORMAT_UINT1_NORM:
        case VERTEX_FORMAT_SINT1_INT:
        case VERTEX_FORMAT_UINT1_INT:
        case VERTEX_FORMAT_HALF2:
        case VERTEX_FORMAT_FIXED1:
        case VERTEX_FORMAT_FLOAT1:
        case VERTEX_FORMAT_SINT210:
        case VERTEX_FORMAT_UINT210:
        case VERTEX_FORMAT_SINT210_NORM:
        case VERTEX_FORMAT_UINT210_NORM:
        case VERTEX_FORMAT_SINT210_INT:
        case VERTEX_FORMAT_UINT210_INT:
            return 4;

        case VERTEX_FORMAT_SSHORT3:
        case VERTEX_FORMAT_SSHORT3_NORM:
        case VERTEX_FORMAT_USHORT3:
        case VERTEX_FORMAT_USHORT3_NORM:
        case VERTEX_FORMAT_SSHORT3_INT:
        case VERTEX_FORMAT_USHORT3_INT:
        case VERTEX_FORMAT_HALF3:
            return 6;

        case VERTEX_FORMAT_SSHORT4:
        case VERTEX_FORMAT_SSHORT4_NORM:
        case VERTEX_FORMAT_USHORT4:
        case VERTEX_FORMAT_USHORT4_NORM:
        case VERTEX_FORMAT_SSHORT4_INT:
        case VERTEX_FORMAT_USHORT4_INT:
        case VERTEX_FORMAT_SINT2:
        case VERTEX_FORMAT_SINT2_NORM:
        case VERTEX_FORMAT_UINT2:
        case VERTEX_FORMAT_UINT2_NORM:
        case VERTEX_FORMAT_SINT2_INT:
        case VERTEX_FORMAT_UINT2_INT:
        case VERTEX_FORMAT_HALF4:
        case VERTEX_FORMAT_FIXED2:
        case VERTEX_FORMAT_FLOAT2:
            return 8;

        case VERTEX_FORMAT_SINT3:
        case VERTEX_FORMAT_SINT3_NORM:
        case VERTEX_FORMAT_UINT3:
        case VERTEX_FORMAT_UINT3_NORM:
        case VERTEX_FORMAT_SINT3_INT:
        case VERTEX_FORMAT_UINT3_INT:
        case VERTEX_FORMAT_FIXED3:
        case VERTEX_FORMAT_FLOAT3:
            return 12;

        case VERTEX_FORMAT_SINT4:
        case VERTEX_FORMAT_SINT4_NORM:
        case VERTEX_FORMAT_UINT4:
        case VERTEX_FORMAT_UINT4_NORM:
        case VERTEX_FORMAT_SINT4_INT:
        case VERTEX_FORMAT_UINT4_INT:
        case VERTEX_FORMAT_FIXED4:
        case VERTEX_FORMAT_FLOAT4:
            return 16;

        case VERTEX_FORMAT_INVALID:
        default:
            UNREACHABLE();
#if !UNREACHABLE_IS_NORETURN
            return 0;
#endif
    }
}

bool ValidES3InternalFormat(GLenum internalFormat)
{
    const InternalFormatInfoMap &formatMap = GetInternalFormatMap();
    return internalFormat != GL_NONE && formatMap.find(internalFormat) != formatMap.end();
}

VertexFormat::VertexFormat(GLenum typeIn,
                           GLboolean normalizedIn,
                           GLuint componentsIn,
                           bool pureIntegerIn)
    : type(typeIn), normalized(normalizedIn), components(componentsIn), pureInteger(pureIntegerIn)
{
    // float -> !normalized
    ASSERT(!(type == GL_FLOAT || type == GL_HALF_FLOAT || type == GL_FIXED) ||
           normalized == GL_FALSE);
}
}
