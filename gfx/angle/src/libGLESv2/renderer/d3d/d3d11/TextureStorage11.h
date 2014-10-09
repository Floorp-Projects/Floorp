//
// Copyright (c) 2012-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureStorage11.h: Defines the abstract rx::TextureStorage11 class and its concrete derived
// classes TextureStorage11_2D and TextureStorage11_Cube, which act as the interface to the D3D11 texture.

#ifndef LIBGLESV2_RENDERER_TEXTURESTORAGE11_H_
#define LIBGLESV2_RENDERER_TEXTURESTORAGE11_H_

#include "libGLESv2/Texture.h"
#include "libGLESv2/Error.h"
#include "libGLESv2/renderer/d3d/TextureStorage.h"

#include <map>

namespace gl
{
struct ImageIndex;
}

namespace rx
{
class RenderTarget;
class RenderTarget11;
class Renderer;
class Renderer11;
class SwapChain11;
class Image11;

class TextureStorage11 : public TextureStorage
{
  public:
    virtual ~TextureStorage11();

    static TextureStorage11 *makeTextureStorage11(TextureStorage *storage);

    static DWORD GetTextureBindFlags(GLenum internalFormat, bool renderTarget);

    UINT getBindFlags() const;

    virtual ID3D11Resource *getResource() const = 0;
    virtual ID3D11ShaderResourceView *getSRV(const gl::SamplerState &samplerState);
    virtual RenderTarget *getRenderTarget(const gl::ImageIndex &index) = 0;

    virtual void generateMipmap(const gl::ImageIndex &sourceIndex, const gl::ImageIndex &destIndex);

    virtual int getTopLevel() const;
    virtual bool isRenderTarget() const;
    virtual bool isManaged() const;
    virtual int getLevelCount() const;
    UINT getSubresourceIndex(int mipLevel, int layerTarget) const;

    gl::Error generateSwizzles(GLenum swizzleRed, GLenum swizzleGreen, GLenum swizzleBlue, GLenum swizzleAlpha);
    void invalidateSwizzleCacheLevel(int mipLevel);
    void invalidateSwizzleCache();

    gl::Error updateSubresourceLevel(ID3D11Resource *texture, unsigned int sourceSubresource, int level,
                                     int layerTarget, GLint xoffset, GLint yoffset, GLint zoffset,
                                     GLsizei width, GLsizei height, GLsizei depth);

    bool copySubresourceLevel(ID3D11Resource* dstTexture, unsigned int dstSubresource, int level,
                              int layerTarget, GLint xoffset, GLint yoffset, GLint zoffset,
                              GLsizei width, GLsizei height, GLsizei depth);

    virtual void associateImage(Image11* image, int level, int layerTarget) = 0;
    virtual void disassociateImage(int level, int layerTarget, Image11* expectedImage) = 0;
    virtual bool isAssociatedImageValid(int level, int layerTarget, Image11* expectedImage) = 0;
    virtual void releaseAssociatedImage(int level, int layerTarget, Image11* incomingImage) = 0;

    virtual gl::Error copyToStorage(TextureStorage *destStorage);

  protected:
    TextureStorage11(Renderer *renderer, UINT bindFlags);
    int getLevelWidth(int mipLevel) const;
    int getLevelHeight(int mipLevel) const;
    int getLevelDepth(int mipLevel) const;

    virtual ID3D11Resource *getSwizzleTexture() = 0;
    virtual ID3D11RenderTargetView *getSwizzleRenderTarget(int mipLevel) = 0;
    ID3D11ShaderResourceView *getSRVLevel(int mipLevel);

    virtual ID3D11ShaderResourceView *createSRV(int baseLevel, int mipLevels, DXGI_FORMAT format, ID3D11Resource *texture) = 0;

    void verifySwizzleExists(GLenum swizzleRed, GLenum swizzleGreen, GLenum swizzleBlue, GLenum swizzleAlpha);

    Renderer11 *mRenderer;
    int mTopLevel;
    unsigned int mMipLevels;

    DXGI_FORMAT mTextureFormat;
    DXGI_FORMAT mShaderResourceFormat;
    DXGI_FORMAT mRenderTargetFormat;
    DXGI_FORMAT mDepthStencilFormat;
    DXGI_FORMAT mSwizzleTextureFormat;
    DXGI_FORMAT mSwizzleShaderResourceFormat;
    DXGI_FORMAT mSwizzleRenderTargetFormat;
    unsigned int mTextureWidth;
    unsigned int mTextureHeight;
    unsigned int mTextureDepth;

    struct SwizzleCacheValue
    {
        GLenum swizzleRed;
        GLenum swizzleGreen;
        GLenum swizzleBlue;
        GLenum swizzleAlpha;

        SwizzleCacheValue();
        SwizzleCacheValue(GLenum red, GLenum green, GLenum blue, GLenum alpha);

        bool operator ==(const SwizzleCacheValue &other) const;
        bool operator !=(const SwizzleCacheValue &other) const;
    };
    SwizzleCacheValue mSwizzleCache[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    struct SRVKey
    {
        SRVKey(int baseLevel = 0, int mipLevels = 0, bool swizzle = false);

        bool operator==(const SRVKey &rhs) const;

        int baseLevel;
        int mipLevels;
        bool swizzle;
    };

    struct SRVPair
    {
        SRVKey key;
        ID3D11ShaderResourceView *srv;
    };

    struct SRVCache
    {
        ~SRVCache();

        ID3D11ShaderResourceView *find(const SRVKey &key) const;
        ID3D11ShaderResourceView *add(const SRVKey &key, ID3D11ShaderResourceView *srv);

        std::vector<SRVPair> cache;
    };

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorage11);

    const UINT mBindFlags;

    SRVCache srvCache;
    ID3D11ShaderResourceView *mLevelSRVs[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];
};

class TextureStorage11_2D : public TextureStorage11
{
  public:
    TextureStorage11_2D(Renderer *renderer, SwapChain11 *swapchain);
    TextureStorage11_2D(Renderer *renderer, GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, int levels);
    virtual ~TextureStorage11_2D();

    static TextureStorage11_2D *makeTextureStorage11_2D(TextureStorage *storage);

    virtual ID3D11Resource *getResource() const;
    virtual RenderTarget *getRenderTarget(const gl::ImageIndex &index);

    virtual void associateImage(Image11* image, int level, int layerTarget);
    virtual void disassociateImage(int level, int layerTarget, Image11* expectedImage);
    virtual bool isAssociatedImageValid(int level, int layerTarget, Image11* expectedImage);
    virtual void releaseAssociatedImage(int level, int layerTarget, Image11* incomingImage);

  protected:
    virtual ID3D11Resource *getSwizzleTexture();
    virtual ID3D11RenderTargetView *getSwizzleRenderTarget(int mipLevel);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorage11_2D);

    virtual ID3D11ShaderResourceView *createSRV(int baseLevel, int mipLevels, DXGI_FORMAT format, ID3D11Resource *texture);

    ID3D11Texture2D *mTexture;
    RenderTarget11 *mRenderTarget[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    ID3D11Texture2D *mSwizzleTexture;
    ID3D11RenderTargetView *mSwizzleRenderTargets[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    Image11 *mAssociatedImages[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];
};

class TextureStorage11_Cube : public TextureStorage11
{
  public:
    TextureStorage11_Cube(Renderer *renderer, GLenum internalformat, bool renderTarget, int size, int levels);
    virtual ~TextureStorage11_Cube();

    static TextureStorage11_Cube *makeTextureStorage11_Cube(TextureStorage *storage);

    virtual ID3D11Resource *getResource() const;
    virtual RenderTarget *getRenderTarget(const gl::ImageIndex &index);

    virtual void associateImage(Image11* image, int level, int layerTarget);
    virtual void disassociateImage(int level, int layerTarget, Image11* expectedImage);
    virtual bool isAssociatedImageValid(int level, int layerTarget, Image11* expectedImage);
    virtual void releaseAssociatedImage(int level, int layerTarget, Image11* incomingImage);

  protected:
    virtual ID3D11Resource *getSwizzleTexture();
    virtual ID3D11RenderTargetView *getSwizzleRenderTarget(int mipLevel);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorage11_Cube);

    virtual ID3D11ShaderResourceView *createSRV(int baseLevel, int mipLevels, DXGI_FORMAT format, ID3D11Resource *texture);

    ID3D11Texture2D *mTexture;
    RenderTarget11 *mRenderTarget[6][gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    ID3D11Texture2D *mSwizzleTexture;
    ID3D11RenderTargetView *mSwizzleRenderTargets[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    Image11 *mAssociatedImages[6][gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];
};

class TextureStorage11_3D : public TextureStorage11
{
  public:
    TextureStorage11_3D(Renderer *renderer, GLenum internalformat, bool renderTarget,
                        GLsizei width, GLsizei height, GLsizei depth, int levels);
    virtual ~TextureStorage11_3D();

    static TextureStorage11_3D *makeTextureStorage11_3D(TextureStorage *storage);

    virtual ID3D11Resource *getResource() const;

    // Handles both layer and non-layer RTs
    virtual RenderTarget *getRenderTarget(const gl::ImageIndex &index);

    virtual void associateImage(Image11* image, int level, int layerTarget);
    virtual void disassociateImage(int level, int layerTarget, Image11* expectedImage);
    virtual bool isAssociatedImageValid(int level, int layerTarget, Image11* expectedImage);
    virtual void releaseAssociatedImage(int level, int layerTarget, Image11* incomingImage);

  protected:
    virtual ID3D11Resource *getSwizzleTexture();
    virtual ID3D11RenderTargetView *getSwizzleRenderTarget(int mipLevel);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorage11_3D);

    virtual ID3D11ShaderResourceView *createSRV(int baseLevel, int mipLevels, DXGI_FORMAT format, ID3D11Resource *texture);

    typedef std::pair<int, int> LevelLayerKey;
    typedef std::map<LevelLayerKey, RenderTarget11*> RenderTargetMap;
    RenderTargetMap mLevelLayerRenderTargets;

    RenderTarget11 *mLevelRenderTargets[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    ID3D11Texture3D *mTexture;
    ID3D11Texture3D *mSwizzleTexture;
    ID3D11RenderTargetView *mSwizzleRenderTargets[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    Image11 *mAssociatedImages[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];
};

class TextureStorage11_2DArray : public TextureStorage11
{
  public:
    TextureStorage11_2DArray(Renderer *renderer, GLenum internalformat, bool renderTarget,
                             GLsizei width, GLsizei height, GLsizei depth, int levels);
    virtual ~TextureStorage11_2DArray();

    static TextureStorage11_2DArray *makeTextureStorage11_2DArray(TextureStorage *storage);

    virtual ID3D11Resource *getResource() const;
    virtual RenderTarget *getRenderTarget(const gl::ImageIndex &index);

    virtual void associateImage(Image11* image, int level, int layerTarget);
    virtual void disassociateImage(int level, int layerTarget, Image11* expectedImage);
    virtual bool isAssociatedImageValid(int level, int layerTarget, Image11* expectedImage);
    virtual void releaseAssociatedImage(int level, int layerTarget, Image11* incomingImage);

  protected:
    virtual ID3D11Resource *getSwizzleTexture();
    virtual ID3D11RenderTargetView *getSwizzleRenderTarget(int mipLevel);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorage11_2DArray);

    virtual ID3D11ShaderResourceView *createSRV(int baseLevel, int mipLevels, DXGI_FORMAT format, ID3D11Resource *texture);

    typedef std::pair<int, int> LevelLayerKey;
    typedef std::map<LevelLayerKey, RenderTarget11*> RenderTargetMap;
    RenderTargetMap mRenderTargets;

    ID3D11Texture2D *mTexture;

    ID3D11Texture2D *mSwizzleTexture;
    ID3D11RenderTargetView *mSwizzleRenderTargets[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    typedef std::map<LevelLayerKey, Image11*> ImageMap;
    ImageMap mAssociatedImages;
};

}

#endif // LIBGLESV2_RENDERER_TEXTURESTORAGE11_H_
