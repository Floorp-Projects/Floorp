#include "precompiled.h"
//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// renderer11_utils.cpp: Conversion functions and other utility routines
// specific to the D3D11 renderer.

#include "libGLESv2/renderer/d3d/d3d11/renderer11_utils.h"
#include "libGLESv2/renderer/d3d/d3d11/formatutils11.h"
#include "common/debug.h"

namespace rx
{

namespace gl_d3d11
{

D3D11_BLEND ConvertBlendFunc(GLenum glBlend, bool isAlpha)
{
    D3D11_BLEND d3dBlend = D3D11_BLEND_ZERO;

    switch (glBlend)
    {
      case GL_ZERO:                     d3dBlend = D3D11_BLEND_ZERO;                break;
      case GL_ONE:                      d3dBlend = D3D11_BLEND_ONE;                 break;
      case GL_SRC_COLOR:                d3dBlend = (isAlpha ? D3D11_BLEND_SRC_ALPHA : D3D11_BLEND_SRC_COLOR);           break;
      case GL_ONE_MINUS_SRC_COLOR:      d3dBlend = (isAlpha ? D3D11_BLEND_INV_SRC_ALPHA : D3D11_BLEND_INV_SRC_COLOR);   break;
      case GL_DST_COLOR:                d3dBlend = (isAlpha ? D3D11_BLEND_DEST_ALPHA : D3D11_BLEND_DEST_COLOR);         break;
      case GL_ONE_MINUS_DST_COLOR:      d3dBlend = (isAlpha ? D3D11_BLEND_INV_DEST_ALPHA : D3D11_BLEND_INV_DEST_COLOR); break;
      case GL_SRC_ALPHA:                d3dBlend = D3D11_BLEND_SRC_ALPHA;           break;
      case GL_ONE_MINUS_SRC_ALPHA:      d3dBlend = D3D11_BLEND_INV_SRC_ALPHA;       break;
      case GL_DST_ALPHA:                d3dBlend = D3D11_BLEND_DEST_ALPHA;          break;
      case GL_ONE_MINUS_DST_ALPHA:      d3dBlend = D3D11_BLEND_INV_DEST_ALPHA;      break;
      case GL_CONSTANT_COLOR:           d3dBlend = D3D11_BLEND_BLEND_FACTOR;        break;
      case GL_ONE_MINUS_CONSTANT_COLOR: d3dBlend = D3D11_BLEND_INV_BLEND_FACTOR;    break;
      case GL_CONSTANT_ALPHA:           d3dBlend = D3D11_BLEND_BLEND_FACTOR;        break;
      case GL_ONE_MINUS_CONSTANT_ALPHA: d3dBlend = D3D11_BLEND_INV_BLEND_FACTOR;    break;
      case GL_SRC_ALPHA_SATURATE:       d3dBlend = D3D11_BLEND_SRC_ALPHA_SAT;       break;
      default: UNREACHABLE();
    }

    return d3dBlend;
}

D3D11_BLEND_OP ConvertBlendOp(GLenum glBlendOp)
{
    D3D11_BLEND_OP d3dBlendOp = D3D11_BLEND_OP_ADD;

    switch (glBlendOp)
    {
      case GL_FUNC_ADD:              d3dBlendOp = D3D11_BLEND_OP_ADD;           break;
      case GL_FUNC_SUBTRACT:         d3dBlendOp = D3D11_BLEND_OP_SUBTRACT;      break;
      case GL_FUNC_REVERSE_SUBTRACT: d3dBlendOp = D3D11_BLEND_OP_REV_SUBTRACT;  break;
      case GL_MIN:                   d3dBlendOp = D3D11_BLEND_OP_MIN;           break;
      case GL_MAX:                   d3dBlendOp = D3D11_BLEND_OP_MAX;           break;
      default: UNREACHABLE();
    }

    return d3dBlendOp;
}

UINT8 ConvertColorMask(bool red, bool green, bool blue, bool alpha)
{
    UINT8 mask = 0;
    if (red)
    {
        mask |= D3D11_COLOR_WRITE_ENABLE_RED;
    }
    if (green)
    {
        mask |= D3D11_COLOR_WRITE_ENABLE_GREEN;
    }
    if (blue)
    {
        mask |= D3D11_COLOR_WRITE_ENABLE_BLUE;
    }
    if (alpha)
    {
        mask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;
    }
    return mask;
}

D3D11_CULL_MODE ConvertCullMode(bool cullEnabled, GLenum cullMode)
{
    D3D11_CULL_MODE cull = D3D11_CULL_NONE;

    if (cullEnabled)
    {
        switch (cullMode)
        {
          case GL_FRONT:            cull = D3D11_CULL_FRONT;    break;
          case GL_BACK:             cull = D3D11_CULL_BACK;     break;
          case GL_FRONT_AND_BACK:   cull = D3D11_CULL_NONE;     break;
          default: UNREACHABLE();
        }
    }
    else
    {
        cull = D3D11_CULL_NONE;
    }

    return cull;
}

D3D11_COMPARISON_FUNC ConvertComparison(GLenum comparison)
{
    D3D11_COMPARISON_FUNC d3dComp = D3D11_COMPARISON_NEVER;
    switch (comparison)
    {
      case GL_NEVER:    d3dComp = D3D11_COMPARISON_NEVER;           break;
      case GL_ALWAYS:   d3dComp = D3D11_COMPARISON_ALWAYS;          break;
      case GL_LESS:     d3dComp = D3D11_COMPARISON_LESS;            break;
      case GL_LEQUAL:   d3dComp = D3D11_COMPARISON_LESS_EQUAL;      break;
      case GL_EQUAL:    d3dComp = D3D11_COMPARISON_EQUAL;           break;
      case GL_GREATER:  d3dComp = D3D11_COMPARISON_GREATER;         break;
      case GL_GEQUAL:   d3dComp = D3D11_COMPARISON_GREATER_EQUAL;   break;
      case GL_NOTEQUAL: d3dComp = D3D11_COMPARISON_NOT_EQUAL;       break;
      default: UNREACHABLE();
    }

    return d3dComp;
}

D3D11_DEPTH_WRITE_MASK ConvertDepthMask(bool depthWriteEnabled)
{
    return depthWriteEnabled ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
}

UINT8 ConvertStencilMask(GLuint stencilmask)
{
    return static_cast<UINT8>(stencilmask);
}

D3D11_STENCIL_OP ConvertStencilOp(GLenum stencilOp)
{
    D3D11_STENCIL_OP d3dStencilOp = D3D11_STENCIL_OP_KEEP;

    switch (stencilOp)
    {
      case GL_ZERO:      d3dStencilOp = D3D11_STENCIL_OP_ZERO;      break;
      case GL_KEEP:      d3dStencilOp = D3D11_STENCIL_OP_KEEP;      break;
      case GL_REPLACE:   d3dStencilOp = D3D11_STENCIL_OP_REPLACE;   break;
      case GL_INCR:      d3dStencilOp = D3D11_STENCIL_OP_INCR_SAT;  break;
      case GL_DECR:      d3dStencilOp = D3D11_STENCIL_OP_DECR_SAT;  break;
      case GL_INVERT:    d3dStencilOp = D3D11_STENCIL_OP_INVERT;    break;
      case GL_INCR_WRAP: d3dStencilOp = D3D11_STENCIL_OP_INCR;      break;
      case GL_DECR_WRAP: d3dStencilOp = D3D11_STENCIL_OP_DECR;      break;
      default: UNREACHABLE();
    }

    return d3dStencilOp;
}

D3D11_FILTER ConvertFilter(GLenum minFilter, GLenum magFilter, float maxAnisotropy, GLenum comparisonMode)
{
    bool comparison = comparisonMode != GL_NONE;

    if (maxAnisotropy > 1.0f)
    {
        return D3D11_ENCODE_ANISOTROPIC_FILTER(static_cast<D3D11_COMPARISON_FUNC>(comparison));
    }
    else
    {
        D3D11_FILTER_TYPE dxMin = D3D11_FILTER_TYPE_POINT;
        D3D11_FILTER_TYPE dxMip = D3D11_FILTER_TYPE_POINT;
        switch (minFilter)
        {
          case GL_NEAREST:                dxMin = D3D11_FILTER_TYPE_POINT;  dxMip = D3D11_FILTER_TYPE_POINT;  break;
          case GL_LINEAR:                 dxMin = D3D11_FILTER_TYPE_LINEAR; dxMip = D3D11_FILTER_TYPE_POINT;  break;
          case GL_NEAREST_MIPMAP_NEAREST: dxMin = D3D11_FILTER_TYPE_POINT;  dxMip = D3D11_FILTER_TYPE_POINT;  break;
          case GL_LINEAR_MIPMAP_NEAREST:  dxMin = D3D11_FILTER_TYPE_LINEAR; dxMip = D3D11_FILTER_TYPE_POINT;  break;
          case GL_NEAREST_MIPMAP_LINEAR:  dxMin = D3D11_FILTER_TYPE_POINT;  dxMip = D3D11_FILTER_TYPE_LINEAR; break;
          case GL_LINEAR_MIPMAP_LINEAR:   dxMin = D3D11_FILTER_TYPE_LINEAR; dxMip = D3D11_FILTER_TYPE_LINEAR; break;
          default:                        UNREACHABLE();
        }

        D3D11_FILTER_TYPE dxMag = D3D11_FILTER_TYPE_POINT;
        switch (magFilter)
        {
          case GL_NEAREST: dxMag = D3D11_FILTER_TYPE_POINT;  break;
          case GL_LINEAR:  dxMag = D3D11_FILTER_TYPE_LINEAR; break;
          default:         UNREACHABLE();
        }

        return D3D11_ENCODE_BASIC_FILTER(dxMin, dxMag, dxMip, static_cast<D3D11_COMPARISON_FUNC>(comparison));
    }
}

D3D11_TEXTURE_ADDRESS_MODE ConvertTextureWrap(GLenum wrap)
{
    switch (wrap)
    {
      case GL_REPEAT:          return D3D11_TEXTURE_ADDRESS_WRAP;
      case GL_CLAMP_TO_EDGE:   return D3D11_TEXTURE_ADDRESS_CLAMP;
      case GL_MIRRORED_REPEAT: return D3D11_TEXTURE_ADDRESS_MIRROR;
      default:                 UNREACHABLE();
    }

    return D3D11_TEXTURE_ADDRESS_WRAP;
}

D3D11_QUERY ConvertQueryType(GLenum queryType)
{
    switch (queryType)
    {
      case GL_ANY_SAMPLES_PASSED_EXT:
      case GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT:   return D3D11_QUERY_OCCLUSION;
      case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN: return D3D11_QUERY_SO_STATISTICS;
      default: UNREACHABLE();                        return D3D11_QUERY_EVENT;
    }
}

}


namespace d3d11_gl
{

static gl::TextureCaps GenerateTextureFormatCaps(GLenum internalFormat, ID3D11Device *device)
{
    gl::TextureCaps textureCaps;

    DXGI_FORMAT textureFormat = gl_d3d11::GetTexFormat(internalFormat);
    DXGI_FORMAT srvFormat = gl_d3d11::GetSRVFormat(internalFormat);
    DXGI_FORMAT rtvFormat = gl_d3d11::GetRTVFormat(internalFormat);
    DXGI_FORMAT dsvFormat = gl_d3d11::GetDSVFormat(internalFormat);
    DXGI_FORMAT renderFormat = gl_d3d11::GetRenderableFormat(internalFormat);

    UINT formatSupport;
    if (SUCCEEDED(device->CheckFormatSupport(textureFormat, &formatSupport)))
    {
        textureCaps.texture2D = (formatSupport & D3D11_FORMAT_SUPPORT_TEXTURE2D) != 0;
        textureCaps.textureCubeMap = (formatSupport & D3D11_FORMAT_SUPPORT_TEXTURECUBE) != 0;
        textureCaps.texture3D = (formatSupport & D3D11_FORMAT_SUPPORT_TEXTURE3D) != 0;
        textureCaps.texture2DArray = (formatSupport & D3D11_FORMAT_SUPPORT_TEXTURE2D) != 0;
    }

    if (SUCCEEDED(device->CheckFormatSupport(renderFormat, &formatSupport)) &&
        (formatSupport & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET))
    {
        for (size_t sampleCount = 1; sampleCount <= D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT; sampleCount++)
        {
            UINT qualityCount = 0;
            if (SUCCEEDED(device->CheckMultisampleQualityLevels(renderFormat, sampleCount, &qualityCount)) &&
                qualityCount > 0)
            {
                textureCaps.sampleCounts.insert(sampleCount);
            }
        }
    }

    if (SUCCEEDED(device->CheckFormatSupport(srvFormat, &formatSupport)))
    {
        textureCaps.filtering = (formatSupport & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) != 0;
    }

    if (SUCCEEDED(device->CheckFormatSupport(rtvFormat, &formatSupport)))
    {
        textureCaps.colorRendering = (formatSupport & D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0;
    }

    if (SUCCEEDED(device->CheckFormatSupport(dsvFormat, &formatSupport)))
    {
        textureCaps.depthRendering = gl::GetDepthBits(internalFormat) > 0 &&
                                     (formatSupport & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL) != 0;
        textureCaps.stencilRendering = gl::GetStencilBits(internalFormat) > 0 &&
                                       (formatSupport & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL) != 0;
    }

    return textureCaps;
}

gl::Caps GenerateCaps(ID3D11Device *device)
{
    gl::Caps caps;

    const gl::FormatSet &allFormats = gl::GetAllSizedInternalFormats();
    for (gl::FormatSet::const_iterator internalFormat = allFormats.begin(); internalFormat != allFormats.end(); ++internalFormat)
    {
        caps.textureCaps.insert(*internalFormat, GenerateTextureFormatCaps(*internalFormat, device));
    }

    D3D_FEATURE_LEVEL featureLevel = device->GetFeatureLevel();

    caps.extensions.setTextureExtensionSupport(caps.textureCaps);
    caps.extensions.elementIndexUint = true;
    caps.extensions.packedDepthStencil = true;
    caps.extensions.getProgramBinary = true;
    caps.extensions.rgb8rgba8 = true;
    caps.extensions.readFormatBGRA = true;
    caps.extensions.pixelBufferObject = true;
    caps.extensions.mapBuffer = true;
    caps.extensions.mapBufferRange = true;
    caps.extensions.textureNPOT = d3d11::GetNPOTTextureSupport(featureLevel);
    caps.extensions.drawBuffers = d3d11::GetMaximumSimultaneousRenderTargets(featureLevel) > 1;
    caps.extensions.textureStorage = true;
    caps.extensions.textureFilterAnisotropic = true;
    caps.extensions.maxTextureAnisotropy = d3d11::GetMaximumAnisotropy(featureLevel);
    caps.extensions.occlusionQueryBoolean = d3d11::GetOcclusionQuerySupport(featureLevel);
    caps.extensions.fence = d3d11::GetEventQuerySupport(featureLevel);
    caps.extensions.timerQuery = false; // Unimplemented
    caps.extensions.robustness = true;
    caps.extensions.blendMinMax = true;
    caps.extensions.framebufferBlit = true;
    caps.extensions.framebufferMultisample = true;
    caps.extensions.instancedArrays = d3d11::GetInstancingSupport(featureLevel);
    caps.extensions.packReverseRowOrder = true;
    caps.extensions.standardDerivatives = d3d11::GetDerivativeInstructionSupport(featureLevel);
    caps.extensions.shaderTextureLOD = true;
    caps.extensions.fragDepth = true;
    caps.extensions.textureUsage = true; // This could be false since it has no effect in D3D11
    caps.extensions.translatedShaderSource = true;

    return caps;
}

}

namespace d3d11
{

void GenerateInitialTextureData(GLint internalFormat, GLuint width, GLuint height, GLuint depth,
                                GLuint mipLevels, std::vector<D3D11_SUBRESOURCE_DATA> *outSubresourceData,
                                std::vector< std::vector<BYTE> > *outData)
{
    InitializeTextureDataFunction initializeFunc = gl_d3d11::GetTextureDataInitializationFunction(internalFormat);
    DXGI_FORMAT dxgiFormat = gl_d3d11::GetTexFormat(internalFormat);

    outSubresourceData->resize(mipLevels);
    outData->resize(mipLevels);

    for (unsigned int i = 0; i < mipLevels; i++)
    {
        unsigned int mipWidth = std::max(width >> i, 1U);
        unsigned int mipHeight = std::max(height >> i, 1U);
        unsigned int mipDepth = std::max(depth >> i, 1U);

        unsigned int rowWidth = d3d11::GetFormatPixelBytes(dxgiFormat) * mipWidth;
        unsigned int imageSize = rowWidth * height;

        outData->at(i).resize(rowWidth * mipHeight * mipDepth);
        initializeFunc(mipWidth, mipHeight, mipDepth, outData->at(i).data(), rowWidth, imageSize);

        outSubresourceData->at(i).pSysMem = outData->at(i).data();
        outSubresourceData->at(i).SysMemPitch = rowWidth;
        outSubresourceData->at(i).SysMemSlicePitch = imageSize;
    }
}

void SetPositionTexCoordVertex(PositionTexCoordVertex* vertex, float x, float y, float u, float v)
{
    vertex->x = x;
    vertex->y = y;
    vertex->u = u;
    vertex->v = v;
}

void SetPositionLayerTexCoord3DVertex(PositionLayerTexCoord3DVertex* vertex, float x, float y,
                                      unsigned int layer, float u, float v, float s)
{
    vertex->x = x;
    vertex->y = y;
    vertex->l = layer;
    vertex->u = u;
    vertex->v = v;
    vertex->s = s;
}

HRESULT SetDebugName(ID3D11DeviceChild *resource, const char *name)
{
#if defined(_DEBUG)
    return resource->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(name), name);
#else
    return S_OK;
#endif
}

bool GetNPOTTextureSupport(D3D_FEATURE_LEVEL featureLevel)
{
    switch (featureLevel)
    {
      case D3D_FEATURE_LEVEL_11_1:
      case D3D_FEATURE_LEVEL_11_0:
      case D3D_FEATURE_LEVEL_10_1:
      case D3D_FEATURE_LEVEL_10_0: return true;

      // From http://msdn.microsoft.com/en-us/library/windows/desktop/ff476876.aspx
      case D3D_FEATURE_LEVEL_9_3:
      case D3D_FEATURE_LEVEL_9_2:
      case D3D_FEATURE_LEVEL_9_1:  return false;

      default: UNREACHABLE();      return false;
    }
}

float GetMaximumAnisotropy(D3D_FEATURE_LEVEL featureLevel)
{
    switch (featureLevel)
    {
      case D3D_FEATURE_LEVEL_11_1:
      case D3D_FEATURE_LEVEL_11_0: return D3D11_MAX_MAXANISOTROPY;

      case D3D_FEATURE_LEVEL_10_1:
      case D3D_FEATURE_LEVEL_10_0: return D3D10_MAX_MAXANISOTROPY;

      // From http://msdn.microsoft.com/en-us/library/windows/desktop/ff476876.aspx
      case D3D_FEATURE_LEVEL_9_3:
      case D3D_FEATURE_LEVEL_9_2:  return 16;

      case D3D_FEATURE_LEVEL_9_1:  return D3D_FL9_1_DEFAULT_MAX_ANISOTROPY;

      default: UNREACHABLE();      return 0;
    }
}

bool GetOcclusionQuerySupport(D3D_FEATURE_LEVEL featureLevel)
{
    switch (featureLevel)
    {
      case D3D_FEATURE_LEVEL_11_1:
      case D3D_FEATURE_LEVEL_11_0:
      case D3D_FEATURE_LEVEL_10_1:
      case D3D_FEATURE_LEVEL_10_0: return true;

      // From http://msdn.microsoft.com/en-us/library/windows/desktop/ff476150.aspx ID3D11Device::CreateQuery
      case D3D_FEATURE_LEVEL_9_3:
      case D3D_FEATURE_LEVEL_9_2:  return true;
      case D3D_FEATURE_LEVEL_9_1:  return false;

      default: UNREACHABLE();      return false;
    }
}

bool GetEventQuerySupport(D3D_FEATURE_LEVEL featureLevel)
{
    // From http://msdn.microsoft.com/en-us/library/windows/desktop/ff476150.aspx ID3D11Device::CreateQuery

    switch (featureLevel)
    {
      case D3D_FEATURE_LEVEL_11_1:
      case D3D_FEATURE_LEVEL_11_0:
      case D3D_FEATURE_LEVEL_10_1:
      case D3D_FEATURE_LEVEL_10_0:
      case D3D_FEATURE_LEVEL_9_3:
      case D3D_FEATURE_LEVEL_9_2:
      case D3D_FEATURE_LEVEL_9_1:  return true;

      default: UNREACHABLE();      return false;
    }
}

bool GetInstancingSupport(D3D_FEATURE_LEVEL featureLevel)
{
    // From http://msdn.microsoft.com/en-us/library/windows/desktop/ff476150.aspx ID3D11Device::CreateInputLayout

    switch (featureLevel)
    {
      case D3D_FEATURE_LEVEL_11_1:
      case D3D_FEATURE_LEVEL_11_0:
      case D3D_FEATURE_LEVEL_10_1:
      case D3D_FEATURE_LEVEL_10_0:
      case D3D_FEATURE_LEVEL_9_3:  return true;

      case D3D_FEATURE_LEVEL_9_2:
      case D3D_FEATURE_LEVEL_9_1:  return false;

      default: UNREACHABLE();      return false;
    }
}

bool GetDerivativeInstructionSupport(D3D_FEATURE_LEVEL featureLevel)
{
    // http://msdn.microsoft.com/en-us/library/windows/desktop/bb509588.aspx states that shader model
    // ps_2_x is required for the ddx (and other derivative functions).

    // http://msdn.microsoft.com/en-us/library/windows/desktop/ff476876.aspx states that feature level
    // 9.3 supports shader model ps_2_x.

    switch (featureLevel)
    {
      case D3D_FEATURE_LEVEL_11_1:
      case D3D_FEATURE_LEVEL_11_0:
      case D3D_FEATURE_LEVEL_10_1:
      case D3D_FEATURE_LEVEL_10_0:
      case D3D_FEATURE_LEVEL_9_3:  return true;
      case D3D_FEATURE_LEVEL_9_2:
      case D3D_FEATURE_LEVEL_9_1:  return false;

      default: UNREACHABLE();      return false;
    }
}

size_t GetMaximumSimultaneousRenderTargets(D3D_FEATURE_LEVEL featureLevel)
{
    // From http://msdn.microsoft.com/en-us/library/windows/desktop/ff476150.aspx ID3D11Device::CreateInputLayout

    switch (featureLevel)
    {
      case D3D_FEATURE_LEVEL_11_1:
      case D3D_FEATURE_LEVEL_11_0: return D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;

      // FIXME(geofflang): Work around NVIDIA driver bug by repacking buffers
      case D3D_FEATURE_LEVEL_10_1:
      case D3D_FEATURE_LEVEL_10_0: return 1; /* D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; */

      case D3D_FEATURE_LEVEL_9_3:  return D3D_FL9_3_SIMULTANEOUS_RENDER_TARGET_COUNT;
      case D3D_FEATURE_LEVEL_9_2:
      case D3D_FEATURE_LEVEL_9_1:  return D3D_FL9_1_SIMULTANEOUS_RENDER_TARGET_COUNT;

      default: UNREACHABLE();      return 0;
    }
}

}

}
