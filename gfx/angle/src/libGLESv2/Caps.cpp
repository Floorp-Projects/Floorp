//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libGLESv2/Caps.h"
#include "common/debug.h"
#include "common/angleutils.h"

#include "angle_gl.h"

#include <algorithm>
#include <sstream>

namespace gl
{

TextureCaps::TextureCaps()
    : texture2D(false),
      textureCubeMap(false),
      texture3D(false),
      texture2DArray(false),
      filtering(false),
      colorRendering(false),
      depthRendering(false),
      stencilRendering(false),
      sampleCounts()
{
}

void TextureCapsMap::insert(GLenum internalFormat, const TextureCaps &caps)
{
    mCapsMap.insert(std::make_pair(internalFormat, caps));
}

void TextureCapsMap::remove(GLenum internalFormat)
{
    InternalFormatToCapsMap::iterator i = mCapsMap.find(internalFormat);
    if (i != mCapsMap.end())
    {
        mCapsMap.erase(i);
    }
}

const TextureCaps &TextureCapsMap::get(GLenum internalFormat) const
{
    static TextureCaps defaultUnsupportedTexture;
    InternalFormatToCapsMap::const_iterator iter = mCapsMap.find(internalFormat);
    return (iter != mCapsMap.end()) ? iter->second : defaultUnsupportedTexture;
}

Extensions::Extensions()
    : elementIndexUint(false),
      packedDepthStencil(false),
      getProgramBinary(false),
      rgb8rgba8(false),
      textureFormatBGRA8888(false),
      readFormatBGRA(false),
      pixelBufferObject(false),
      mapBuffer(false),
      mapBufferRange(false),
      textureHalfFloat(false),
      textureHalfFloatLinear(false),
      textureFloat(false),
      textureFloatLinear(false),
      textureRG(false),
      textureCompressionDXT1(false),
      textureCompressionDXT3(false),
      textureCompressionDXT5(false),
      depthTextures(false),
      textureNPOT(false),
      drawBuffers(false),
      textureStorage(false),
      textureFilterAnisotropic(false),
      maxTextureAnisotropy(false),
      occlusionQueryBoolean(false),
      fence(false),
      timerQuery(false),
      robustness(false),
      blendMinMax(false),
      framebufferBlit(false),
      framebufferMultisample(false),
      instancedArrays(false),
      packReverseRowOrder(false),
      standardDerivatives(false),
      shaderTextureLOD(false),
      fragDepth(false),
      textureUsage(false),
      translatedShaderSource(false),
      colorBufferFloat(false)
{
}

static void InsertExtensionString(const std::string &extension, GLuint minClientVersion, GLuint maxClientVersion, bool supported,
                                  GLuint curClientVersion, std::vector<std::string> *extensionVector)
{
    if (supported && minClientVersion >= curClientVersion && (maxClientVersion == 0 || curClientVersion <= maxClientVersion))
    {
        extensionVector->push_back(extension);
    }
}

std::vector<std::string> Extensions::getStrings(GLuint clientVersion) const
{
    std::vector<std::string> extensionStrings;

    //                   | Extension name                     | Min | Max  | Supported flag          | Current      | Output vector   |
    //                   |                                    | ver | ver  |                         | version      |                 |
    InsertExtensionString("GL_OES_element_index_uint",         2,    0,     elementIndexUint,         clientVersion, &extensionStrings);
    InsertExtensionString("GL_OES_packed_depth_stencil",       2,    0,     packedDepthStencil,       clientVersion, &extensionStrings);
    InsertExtensionString("GL_OES_get_program_binary",         2,    0,     getProgramBinary,         clientVersion, &extensionStrings);
    InsertExtensionString("GL_OES_rgb8_rgba8",                 2,    0,     rgb8rgba8,                clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_texture_format_BGRA8888",    2,    0,     textureFormatBGRA8888,    clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_read_format_bgra",           2,    0,     readFormatBGRA,           clientVersion, &extensionStrings);
    InsertExtensionString("GL_NV_pixel_buffer_object",         2,    0,     pixelBufferObject,        clientVersion, &extensionStrings);
    InsertExtensionString("GL_OES_mapbuffer",                  2,    0,     mapBuffer,                clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_map_buffer_range",           2,    0,     mapBufferRange,           clientVersion, &extensionStrings);
    InsertExtensionString("GL_OES_texture_half_float",         2,    0,     textureHalfFloat,         clientVersion, &extensionStrings);
    InsertExtensionString("GL_OES_texture_half_float_linear",  2,    0,     textureHalfFloatLinear,   clientVersion, &extensionStrings);
    InsertExtensionString("GL_OES_texture_float",              2,    0,     textureFloat,             clientVersion, &extensionStrings);
    InsertExtensionString("GL_OES_texture_float_linear",       2,    0,     textureFloatLinear,       clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_texture_rg",                 2,    0,     textureRG,                clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_texture_compression_dxt1",   2,    0,     textureCompressionDXT1,   clientVersion, &extensionStrings);
    InsertExtensionString("GL_ANGLE_texture_compression_dxt3", 2,    0,     textureCompressionDXT3,   clientVersion, &extensionStrings);
    InsertExtensionString("GL_ANGLE_texture_compression_dxt5", 2,    0,     textureCompressionDXT5,   clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_sRGB",                       2,    0,     sRGB,                     clientVersion, &extensionStrings); // FIXME: Don't advertise in ES3
    InsertExtensionString("GL_ANGLE_depth_texture",            2,    0,     depthTextures,            clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_texture_storage",            2,    0,     textureStorage,           clientVersion, &extensionStrings);
    InsertExtensionString("GL_OES_texture_npot",               2,    0,     textureNPOT,              clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_draw_buffers",               2,    0,     drawBuffers,              clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_texture_filter_anisotropic", 2,    0,     textureFilterAnisotropic, clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_occlusion_query_boolean",    2,    0,     occlusionQueryBoolean,    clientVersion, &extensionStrings);
    InsertExtensionString("GL_NV_fence",                       2,    0,     fence,                    clientVersion, &extensionStrings);
    InsertExtensionString("GL_ANGLE_timer_query",              2,    0,     timerQuery,               clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_robustness",                 2,    0,     robustness,               clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_blend_minmax",               2,    0,     blendMinMax,              clientVersion, &extensionStrings);
    InsertExtensionString("GL_ANGLE_framebuffer_blit",         2,    0,     framebufferBlit,          clientVersion, &extensionStrings);
    InsertExtensionString("GL_ANGLE_framebuffer_multisample",  2,    0,     framebufferMultisample,   clientVersion, &extensionStrings);
    InsertExtensionString("GL_ANGLE_instanced_arrays",         2,    0,     instancedArrays,          clientVersion, &extensionStrings);
    InsertExtensionString("GL_ANGLE_pack_reverse_row_order",   2,    0,     packReverseRowOrder,      clientVersion, &extensionStrings);
    InsertExtensionString("GL_OES_standard_derivatives",       2,    0,     standardDerivatives,      clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_shader_texture_lod",         2,    0,     shaderTextureLOD,         clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_frag_depth",                 2,    0,     fragDepth,                clientVersion, &extensionStrings);
    InsertExtensionString("GL_ANGLE_texture_usage",            2,    0,     textureUsage,             clientVersion, &extensionStrings);
    InsertExtensionString("GL_ANGLE_translated_shader_source", 2,    0,     translatedShaderSource,   clientVersion, &extensionStrings);
    InsertExtensionString("GL_EXT_color_buffer_float",         3,    0,     colorBufferFloat,         clientVersion, &extensionStrings);

    return extensionStrings;
}

static bool GetFormatSupport(const TextureCapsMap &textureCaps, const std::vector<GLenum> &requiredFormats,
                             bool requiresFiltering, bool requiresColorBuffer, bool requiresDepthStencil)
{
    for (size_t i = 0; i < requiredFormats.size(); i++)
    {
        const TextureCaps &cap = textureCaps.get(requiredFormats[i]);

        if (requiresFiltering && !cap.filtering)
        {
            return false;
        }

        if (requiresColorBuffer && !cap.colorRendering)
        {
            return false;
        }

        if (requiresDepthStencil && !cap.depthRendering)
        {
            return false;
        }
    }

    return true;
}

// Checks for GL_OES_rgb8_rgba8 support
static bool DetermineRGB8AndRGBA8TextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_RGB8);
    requiredFormats.push_back(GL_RGBA8);

    return GetFormatSupport(textureCaps, requiredFormats, true, true, false);
}

// Checks for GL_EXT_texture_format_BGRA8888 support
static bool DetermineBGRA8TextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_BGRA8_EXT);

    return GetFormatSupport(textureCaps, requiredFormats, true, true, false);
}

// Checks for GL_OES_texture_half_float support
static bool DetermineHalfFloatTextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_RGB16F);
    requiredFormats.push_back(GL_RGBA16F);

    return GetFormatSupport(textureCaps, requiredFormats, false, true, false);
}

// Checks for GL_OES_texture_half_float_linear support
static bool DetermineHalfFloatTextureFilteringSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_RGB16F);
    requiredFormats.push_back(GL_RGBA16F);

    return GetFormatSupport(textureCaps, requiredFormats, true, false, false);
}

// Checks for GL_OES_texture_float support
static bool DetermineFloatTextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_RGB32F);
    requiredFormats.push_back(GL_RGBA32F);

    return GetFormatSupport(textureCaps, requiredFormats, false, true, false);
}

// Checks for GL_OES_texture_float_linear support
static bool DetermineFloatTextureFilteringSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_RGB32F);
    requiredFormats.push_back(GL_RGBA32F);

    return GetFormatSupport(textureCaps, requiredFormats, true, false, false);
}

// Checks for GL_EXT_texture_rg support
static bool DetermineRGTextureSupport(const TextureCapsMap &textureCaps, bool checkHalfFloatFormats, bool checkFloatFormats)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_R8);
    requiredFormats.push_back(GL_RG8);
    if (checkHalfFloatFormats)
    {
        requiredFormats.push_back(GL_R16F);
        requiredFormats.push_back(GL_RG16F);
    }
    if (checkFloatFormats)
    {
        requiredFormats.push_back(GL_R32F);
        requiredFormats.push_back(GL_RG32F);
    }

    return GetFormatSupport(textureCaps, requiredFormats, true, false, false);
}

// Check for GL_EXT_texture_compression_dxt1
static bool DetermineDXT1TextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
    requiredFormats.push_back(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);

    return GetFormatSupport(textureCaps, requiredFormats, true, false, false);
}

// Check for GL_ANGLE_texture_compression_dxt3
static bool DetermineDXT3TextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE);

    return GetFormatSupport(textureCaps, requiredFormats, true, false, false);
}

// Check for GL_ANGLE_texture_compression_dxt5
static bool DetermineDXT5TextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE);

    return GetFormatSupport(textureCaps, requiredFormats, true, false, false);
}

// Check for GL_ANGLE_texture_compression_dxt5
static bool DetermineSRGBTextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFilterFormats;
    requiredFilterFormats.push_back(GL_SRGB8);
    requiredFilterFormats.push_back(GL_SRGB8_ALPHA8);

    std::vector<GLenum> requiredRenderFormats;
    requiredRenderFormats.push_back(GL_SRGB8_ALPHA8);

    return GetFormatSupport(textureCaps, requiredFilterFormats, true, false, false) &&
           GetFormatSupport(textureCaps, requiredRenderFormats, false, true, false);
}

// Check for GL_ANGLE_depth_texture
static bool DetermineDepthTextureSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_DEPTH_COMPONENT16);
    requiredFormats.push_back(GL_DEPTH_COMPONENT32_OES);
    requiredFormats.push_back(GL_DEPTH24_STENCIL8_OES);

    return GetFormatSupport(textureCaps, requiredFormats, true, false, true);
}

// Check for GL_EXT_color_buffer_float
static bool DetermineColorBufferFloatSupport(const TextureCapsMap &textureCaps)
{
    std::vector<GLenum> requiredFormats;
    requiredFormats.push_back(GL_R16F);
    requiredFormats.push_back(GL_RG16F);
    requiredFormats.push_back(GL_RGBA16F);
    requiredFormats.push_back(GL_R32F);
    requiredFormats.push_back(GL_RG32F);
    requiredFormats.push_back(GL_RGBA32F);
    requiredFormats.push_back(GL_R11F_G11F_B10F);

    return GetFormatSupport(textureCaps, requiredFormats, false, true, false);
}

void Extensions::setTextureExtensionSupport(const TextureCapsMap &textureCaps)
{
    rgb8rgba8 = DetermineRGB8AndRGBA8TextureSupport(textureCaps);
    textureFormatBGRA8888 = DetermineBGRA8TextureSupport(textureCaps);
    textureHalfFloat = DetermineHalfFloatTextureSupport(textureCaps);
    textureHalfFloatLinear = DetermineHalfFloatTextureFilteringSupport(textureCaps);
    textureFloat = DetermineFloatTextureSupport(textureCaps);
    textureFloatLinear = DetermineFloatTextureFilteringSupport(textureCaps);
    textureRG = DetermineRGTextureSupport(textureCaps, textureHalfFloat, textureFloat);
    textureCompressionDXT1 = DetermineDXT1TextureSupport(textureCaps);
    textureCompressionDXT3 = DetermineDXT3TextureSupport(textureCaps);
    textureCompressionDXT5 = DetermineDXT5TextureSupport(textureCaps);
    sRGB = DetermineSRGBTextureSupport(textureCaps);
    depthTextures = DetermineDepthTextureSupport(textureCaps);
    colorBufferFloat = DetermineColorBufferFloatSupport(textureCaps);
}

Caps::Caps()
{
}

}
