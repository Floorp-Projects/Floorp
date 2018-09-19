//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Blit11.cpp: Texture copy utility class.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_BLIT11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_BLIT11_H_

#include "common/angleutils.h"
#include "libANGLE/Error.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/d3d/d3d11/ResourceManager11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

#include <map>

namespace rx
{
class Renderer11;

class Blit11 : angle::NonCopyable
{
  public:
    explicit Blit11(Renderer11 *renderer);
    ~Blit11();

    angle::Result swizzleTexture(const gl::Context *context,
                                 const d3d11::SharedSRV &source,
                                 const d3d11::RenderTargetView &dest,
                                 const gl::Extents &size,
                                 const gl::SwizzleState &swizzleTarget);

    // Set destTypeForDownsampling to GL_NONE to skip downsampling
    angle::Result copyTexture(const gl::Context *context,
                              const d3d11::SharedSRV &source,
                              const gl::Box &sourceArea,
                              const gl::Extents &sourceSize,
                              GLenum sourceFormat,
                              const d3d11::RenderTargetView &dest,
                              const gl::Box &destArea,
                              const gl::Extents &destSize,
                              const gl::Rectangle *scissor,
                              GLenum destFormat,
                              GLenum destTypeForDownsampling,
                              GLenum filter,
                              bool maskOffAlpha,
                              bool unpackPremultiplyAlpha,
                              bool unpackUnmultiplyAlpha);

    angle::Result copyStencil(const gl::Context *context,
                              const TextureHelper11 &source,
                              unsigned int sourceSubresource,
                              const gl::Box &sourceArea,
                              const gl::Extents &sourceSize,
                              const TextureHelper11 &dest,
                              unsigned int destSubresource,
                              const gl::Box &destArea,
                              const gl::Extents &destSize,
                              const gl::Rectangle *scissor);

    angle::Result copyDepth(const gl::Context *context,
                            const d3d11::SharedSRV &source,
                            const gl::Box &sourceArea,
                            const gl::Extents &sourceSize,
                            const d3d11::DepthStencilView &dest,
                            const gl::Box &destArea,
                            const gl::Extents &destSize,
                            const gl::Rectangle *scissor);

    angle::Result copyDepthStencil(const gl::Context *context,
                                   const TextureHelper11 &source,
                                   unsigned int sourceSubresource,
                                   const gl::Box &sourceArea,
                                   const gl::Extents &sourceSize,
                                   const TextureHelper11 &dest,
                                   unsigned int destSubresource,
                                   const gl::Box &destArea,
                                   const gl::Extents &destSize,
                                   const gl::Rectangle *scissor);

    angle::Result resolveDepth(const gl::Context *context,
                               RenderTarget11 *depth,
                               TextureHelper11 *textureOut);

    angle::Result resolveStencil(const gl::Context *context,
                                 RenderTarget11 *depthStencil,
                                 bool alsoDepth,
                                 TextureHelper11 *textureOut);

    using BlitConvertFunction = void(const gl::Box &sourceArea,
                                     const gl::Box &destArea,
                                     const gl::Rectangle &clipRect,
                                     const gl::Extents &sourceSize,
                                     unsigned int sourceRowPitch,
                                     unsigned int destRowPitch,
                                     ptrdiff_t readOffset,
                                     ptrdiff_t writeOffset,
                                     size_t copySize,
                                     size_t srcPixelStride,
                                     size_t destPixelStride,
                                     const uint8_t *sourceData,
                                     uint8_t *destData);

  private:
    enum BlitShaderType
    {
        BLITSHADER_INVALID,

        // Passthrough shaders
        BLITSHADER_2D_RGBAF,
        BLITSHADER_2D_BGRAF,
        BLITSHADER_2D_RGBF,
        BLITSHADER_2D_RGF,
        BLITSHADER_2D_RF,
        BLITSHADER_2D_ALPHA,
        BLITSHADER_2D_LUMA,
        BLITSHADER_2D_LUMAALPHA,
        BLITSHADER_2D_RGBAUI,
        BLITSHADER_2D_RGBAI,
        BLITSHADER_2D_RGBUI,
        BLITSHADER_2D_RGBI,
        BLITSHADER_2D_RGUI,
        BLITSHADER_2D_RGI,
        BLITSHADER_2D_RUI,
        BLITSHADER_2D_RI,
        BLITSHADER_3D_RGBAF,
        BLITSHADER_3D_RGBAUI,
        BLITSHADER_3D_RGBAI,
        BLITSHADER_3D_BGRAF,
        BLITSHADER_3D_RGBF,
        BLITSHADER_3D_RGBUI,
        BLITSHADER_3D_RGBI,
        BLITSHADER_3D_RGF,
        BLITSHADER_3D_RGUI,
        BLITSHADER_3D_RGI,
        BLITSHADER_3D_RF,
        BLITSHADER_3D_RUI,
        BLITSHADER_3D_RI,
        BLITSHADER_3D_ALPHA,
        BLITSHADER_3D_LUMA,
        BLITSHADER_3D_LUMAALPHA,

        // Multiply alpha shaders
        BLITSHADER_2D_RGBAF_PREMULTIPLY,
        BLITSHADER_2D_RGBAF_UNMULTIPLY,

        BLITSHADER_2D_RGBF_PREMULTIPLY,
        BLITSHADER_2D_RGBF_UNMULTIPLY,

        BLITSHADER_2D_RGBAF_TOUI,
        BLITSHADER_2D_RGBAF_TOUI_PREMULTIPLY,
        BLITSHADER_2D_RGBAF_TOUI_UNMULTIPLY,

        BLITSHADER_2D_RGBF_TOUI,
        BLITSHADER_2D_RGBF_TOUI_PREMULTIPLY,
        BLITSHADER_2D_RGBF_TOUI_UNMULTIPLY,

        BLITSHADER_2D_LUMAF_PREMULTIPLY,
        BLITSHADER_2D_LUMAF_UNMULTIPLY,

        BLITSHADER_2D_LUMAALPHAF_PREMULTIPLY,
        BLITSHADER_2D_LUMAALPHAF_UNMULTIPLY,

        // Downsample 16-bit shaders
        BLITSHADER_2D_RGBAF_4444,
        BLITSHADER_2D_RGBAF_4444_PREMULTIPLY,
        BLITSHADER_2D_RGBAF_4444_UNMULTIPLY,

        BLITSHADER_2D_RGBF_565,
        BLITSHADER_2D_RGBF_565_PREMULTIPLY,
        BLITSHADER_2D_RGBF_565_UNMULTIPLY,

        BLITSHADER_2D_RGBAF_5551,
        BLITSHADER_2D_RGBAF_5551_PREMULTIPLY,
        BLITSHADER_2D_RGBAF_5551_UNMULTIPLY,
    };

    enum SwizzleShaderType
    {
        SWIZZLESHADER_INVALID,
        SWIZZLESHADER_2D_FLOAT,
        SWIZZLESHADER_2D_UINT,
        SWIZZLESHADER_2D_INT,
        SWIZZLESHADER_CUBE_FLOAT,
        SWIZZLESHADER_CUBE_UINT,
        SWIZZLESHADER_CUBE_INT,
        SWIZZLESHADER_3D_FLOAT,
        SWIZZLESHADER_3D_UINT,
        SWIZZLESHADER_3D_INT,
        SWIZZLESHADER_ARRAY_FLOAT,
        SWIZZLESHADER_ARRAY_UINT,
        SWIZZLESHADER_ARRAY_INT,
    };

    typedef void (*WriteVertexFunction)(const gl::Box &sourceArea,
                                        const gl::Extents &sourceSize,
                                        const gl::Box &destArea,
                                        const gl::Extents &destSize,
                                        void *outVertices,
                                        unsigned int *outStride,
                                        unsigned int *outVertexCount,
                                        D3D11_PRIMITIVE_TOPOLOGY *outTopology);

    enum ShaderDimension
    {
        SHADER_2D,
        SHADER_3D,
    };

    struct Shader
    {
        Shader();
        Shader(Shader &&other);
        ~Shader();
        Shader &operator=(Shader &&other);

        ShaderDimension dimension;
        d3d11::PixelShader pixelShader;
    };

    struct ShaderSupport
    {
        const d3d11::InputLayout *inputLayout;
        const d3d11::VertexShader *vertexShader;
        const d3d11::GeometryShader *geometryShader;
        WriteVertexFunction vertexWriteFunction;
    };

    angle::Result initResources(const gl::Context *context);

    angle::Result getShaderSupport(const gl::Context *context,
                                   const Shader &shader,
                                   ShaderSupport *supportOut);

    static BlitShaderType GetBlitShaderType(GLenum destinationFormat,
                                            GLenum sourceFormat,
                                            bool isSigned,
                                            bool unpackPremultiplyAlpha,
                                            bool unpackUnmultiplyAlpha,
                                            GLenum destTypeForDownsampling,
                                            ShaderDimension dimension);
    static SwizzleShaderType GetSwizzleShaderType(GLenum type, D3D11_SRV_DIMENSION dimensionality);

    angle::Result copyDepthStencilImpl(const gl::Context *context,
                                       const TextureHelper11 &source,
                                       unsigned int sourceSubresource,
                                       const gl::Box &sourceArea,
                                       const gl::Extents &sourceSize,
                                       const TextureHelper11 &dest,
                                       unsigned int destSubresource,
                                       const gl::Box &destArea,
                                       const gl::Extents &destSize,
                                       const gl::Rectangle *scissor,
                                       bool stencilOnly);

    angle::Result copyAndConvertImpl(const gl::Context *context,
                                     const TextureHelper11 &source,
                                     unsigned int sourceSubresource,
                                     const gl::Box &sourceArea,
                                     const gl::Extents &sourceSize,
                                     const TextureHelper11 &destStaging,
                                     const gl::Box &destArea,
                                     const gl::Extents &destSize,
                                     const gl::Rectangle *scissor,
                                     size_t readOffset,
                                     size_t writeOffset,
                                     size_t copySize,
                                     size_t srcPixelStride,
                                     size_t destPixelStride,
                                     BlitConvertFunction *convertFunction);

    angle::Result copyAndConvert(const gl::Context *context,
                                 const TextureHelper11 &source,
                                 unsigned int sourceSubresource,
                                 const gl::Box &sourceArea,
                                 const gl::Extents &sourceSize,
                                 const TextureHelper11 &dest,
                                 unsigned int destSubresource,
                                 const gl::Box &destArea,
                                 const gl::Extents &destSize,
                                 const gl::Rectangle *scissor,
                                 size_t readOffset,
                                 size_t writeOffset,
                                 size_t copySize,
                                 size_t srcPixelStride,
                                 size_t destPixelStride,
                                 BlitConvertFunction *convertFunction);

    angle::Result addBlitShaderToMap(const gl::Context *context,
                                     BlitShaderType blitShaderType,
                                     ShaderDimension dimension,
                                     const ShaderData &shaderData,
                                     const char *name);

    angle::Result getBlitShader(const gl::Context *context,
                                GLenum destFormat,
                                GLenum sourceFormat,
                                bool isSigned,
                                bool unpackPremultiplyAlpha,
                                bool unpackUnmultiplyAlpha,
                                GLenum destTypeForDownsampling,
                                ShaderDimension dimension,
                                const Shader **shaderOut);
    angle::Result getSwizzleShader(const gl::Context *context,
                                   GLenum type,
                                   D3D11_SRV_DIMENSION viewDimension,
                                   const Shader **shaderOut);

    angle::Result addSwizzleShaderToMap(const gl::Context *context,
                                        SwizzleShaderType swizzleShaderType,
                                        ShaderDimension dimension,
                                        const ShaderData &shaderData,
                                        const char *name);

    void clearShaderMap();
    void releaseResolveDepthStencilResources();
    angle::Result initResolveDepthOnly(const gl::Context *context,
                                       const d3d11::Format &format,
                                       const gl::Extents &extents);
    angle::Result initResolveDepthStencil(const gl::Context *context, const gl::Extents &extents);

    Renderer11 *mRenderer;

    std::map<BlitShaderType, Shader> mBlitShaderMap;
    std::map<SwizzleShaderType, Shader> mSwizzleShaderMap;

    bool mResourcesInitialized;
    d3d11::Buffer mVertexBuffer;
    d3d11::SamplerState mPointSampler;
    d3d11::SamplerState mLinearSampler;
    d3d11::RasterizerState mScissorEnabledRasterizerState;
    d3d11::RasterizerState mScissorDisabledRasterizerState;
    d3d11::DepthStencilState mDepthStencilState;

    d3d11::LazyInputLayout mQuad2DIL;
    d3d11::LazyShader<ID3D11VertexShader> mQuad2DVS;
    d3d11::LazyShader<ID3D11PixelShader> mDepthPS;

    d3d11::LazyInputLayout mQuad3DIL;
    d3d11::LazyShader<ID3D11VertexShader> mQuad3DVS;
    d3d11::LazyShader<ID3D11GeometryShader> mQuad3DGS;

    d3d11::LazyBlendState mAlphaMaskBlendState;

    d3d11::Buffer mSwizzleCB;

    d3d11::LazyShader<ID3D11VertexShader> mResolveDepthStencilVS;
    d3d11::LazyShader<ID3D11PixelShader> mResolveDepthPS;
    d3d11::LazyShader<ID3D11PixelShader> mResolveDepthStencilPS;
    d3d11::LazyShader<ID3D11PixelShader> mResolveStencilPS;
    d3d11::ShaderResourceView mStencilSRV;
    TextureHelper11 mResolvedDepthStencil;
    d3d11::RenderTargetView mResolvedDepthStencilRTView;
    TextureHelper11 mResolvedDepth;
    d3d11::DepthStencilView mResolvedDepthDSView;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_D3D11_BLIT11_H_
