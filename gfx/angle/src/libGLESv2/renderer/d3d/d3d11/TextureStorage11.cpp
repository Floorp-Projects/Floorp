//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureStorage11.cpp: Implements the abstract rx::TextureStorage11 class and its concrete derived
// classes TextureStorage11_2D and TextureStorage11_Cube, which act as the interface to the D3D11 texture.

#include "libGLESv2/renderer/d3d/d3d11/TextureStorage11.h"
#include "libGLESv2/renderer/d3d/d3d11/Renderer11.h"
#include "libGLESv2/renderer/d3d/d3d11/RenderTarget11.h"
#include "libGLESv2/renderer/d3d/d3d11/SwapChain11.h"
#include "libGLESv2/renderer/d3d/d3d11/renderer11_utils.h"
#include "libGLESv2/renderer/d3d/d3d11/Blit11.h"
#include "libGLESv2/renderer/d3d/d3d11/formatutils11.h"
#include "libGLESv2/renderer/d3d/d3d11/Image11.h"
#include "libGLESv2/renderer/d3d/TextureD3D.h"
#include "libGLESv2/main.h"
#include "libGLESv2/ImageIndex.h"

#include "common/utilities.h"

namespace rx
{

TextureStorage11::SwizzleCacheValue::SwizzleCacheValue()
    : swizzleRed(GL_NONE), swizzleGreen(GL_NONE), swizzleBlue(GL_NONE), swizzleAlpha(GL_NONE)
{
}

TextureStorage11::SwizzleCacheValue::SwizzleCacheValue(GLenum red, GLenum green, GLenum blue, GLenum alpha)
    : swizzleRed(red), swizzleGreen(green), swizzleBlue(blue), swizzleAlpha(alpha)
{
}

bool TextureStorage11::SwizzleCacheValue::operator==(const SwizzleCacheValue &other) const
{
    return swizzleRed == other.swizzleRed &&
           swizzleGreen == other.swizzleGreen &&
           swizzleBlue == other.swizzleBlue &&
           swizzleAlpha == other.swizzleAlpha;
}

bool TextureStorage11::SwizzleCacheValue::operator!=(const SwizzleCacheValue &other) const
{
    return !(*this == other);
}

TextureStorage11::SRVKey::SRVKey(int baseLevel, int mipLevels, bool swizzle)
    : baseLevel(baseLevel), mipLevels(mipLevels), swizzle(swizzle)
{
}

bool TextureStorage11::SRVKey::operator==(const SRVKey &rhs) const
{
    return baseLevel == rhs.baseLevel &&
           mipLevels == rhs.mipLevels &&
           swizzle == rhs.swizzle;
}

TextureStorage11::SRVCache::~SRVCache()
{
    for (size_t i = 0; i < cache.size(); i++)
    {
        SafeRelease(cache[i].srv);
    }
}

ID3D11ShaderResourceView *TextureStorage11::SRVCache::find(const SRVKey &key) const
{
    for (size_t i = 0; i < cache.size(); i++)
    {
        if (cache[i].key == key)
        {
            return cache[i].srv;
        }
    }

    return NULL;
}

ID3D11ShaderResourceView *TextureStorage11::SRVCache::add(const SRVKey &key, ID3D11ShaderResourceView *srv)
{
    SRVPair pair = {key, srv};
    cache.push_back(pair);

    return srv;
}

TextureStorage11::TextureStorage11(Renderer *renderer, UINT bindFlags)
    : mBindFlags(bindFlags),
      mTopLevel(0),
      mMipLevels(0),
      mTextureFormat(DXGI_FORMAT_UNKNOWN),
      mShaderResourceFormat(DXGI_FORMAT_UNKNOWN),
      mRenderTargetFormat(DXGI_FORMAT_UNKNOWN),
      mDepthStencilFormat(DXGI_FORMAT_UNKNOWN),
      mTextureWidth(0),
      mTextureHeight(0),
      mTextureDepth(0)
{
    mRenderer = Renderer11::makeRenderer11(renderer);

    for (unsigned int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        mLevelSRVs[i] = NULL;
    }
}

TextureStorage11::~TextureStorage11()
{
    for (unsigned int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        SafeRelease(mLevelSRVs[level]);
    }
}

TextureStorage11 *TextureStorage11::makeTextureStorage11(TextureStorage *storage)
{
    ASSERT(HAS_DYNAMIC_TYPE(TextureStorage11*, storage));
    return static_cast<TextureStorage11*>(storage);
}

DWORD TextureStorage11::GetTextureBindFlags(GLenum internalFormat, bool renderTarget)
{
    UINT bindFlags = 0;

    const d3d11::TextureFormat &formatInfo = d3d11::GetTextureFormatInfo(internalFormat);
    if (formatInfo.srvFormat != DXGI_FORMAT_UNKNOWN)
    {
        bindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
    if (formatInfo.dsvFormat != DXGI_FORMAT_UNKNOWN)
    {
        bindFlags |= D3D11_BIND_DEPTH_STENCIL;
    }
    if (formatInfo.rtvFormat != DXGI_FORMAT_UNKNOWN && renderTarget)
    {
        bindFlags |= D3D11_BIND_RENDER_TARGET;
    }

    return bindFlags;
}

UINT TextureStorage11::getBindFlags() const
{
    return mBindFlags;
}

int TextureStorage11::getTopLevel() const
{
    return mTopLevel;
}

bool TextureStorage11::isRenderTarget() const
{
    return (mBindFlags & (D3D11_BIND_RENDER_TARGET | D3D11_BIND_DEPTH_STENCIL)) != 0;
}

bool TextureStorage11::isManaged() const
{
    return false;
}

int TextureStorage11::getLevelCount() const
{
    return mMipLevels - mTopLevel;
}

int TextureStorage11::getLevelWidth(int mipLevel) const
{
    return std::max(static_cast<int>(mTextureWidth) >> mipLevel, 1);
}

int TextureStorage11::getLevelHeight(int mipLevel) const
{
    return std::max(static_cast<int>(mTextureHeight) >> mipLevel, 1);
}

int TextureStorage11::getLevelDepth(int mipLevel) const
{
    return std::max(static_cast<int>(mTextureDepth) >> mipLevel, 1);
}

UINT TextureStorage11::getSubresourceIndex(int mipLevel, int layerTarget) const
{
    UINT index = 0;
    if (getResource())
    {
        index = D3D11CalcSubresource(mipLevel, layerTarget, mMipLevels);
    }
    return index;
}

ID3D11ShaderResourceView *TextureStorage11::getSRV(const gl::SamplerState &samplerState)
{
    bool swizzleRequired = samplerState.swizzleRequired();
    bool mipmapping = gl::IsMipmapFiltered(samplerState);
    unsigned int mipLevels = mipmapping ? (samplerState.maxLevel - samplerState.baseLevel) : 1;

    // Make sure there's 'mipLevels' mipmap levels below the base level (offset by the top level,  which corresponds to GL level 0)
    mipLevels = std::min(mipLevels, mMipLevels - mTopLevel - samplerState.baseLevel);

    if (swizzleRequired)
    {
        verifySwizzleExists(samplerState.swizzleRed, samplerState.swizzleGreen, samplerState.swizzleBlue, samplerState.swizzleAlpha);
    }
    
    SRVKey key(samplerState.baseLevel, mipLevels, swizzleRequired);
    ID3D11ShaderResourceView *srv = srvCache.find(key);

    if(srv)
    {
        return srv;
    }

    DXGI_FORMAT format = (swizzleRequired ? mSwizzleShaderResourceFormat : mShaderResourceFormat);
    ID3D11Resource *texture = swizzleRequired ? getSwizzleTexture() : getResource();

    srv = createSRV(samplerState.baseLevel, mipLevels, format, texture);
    
    return srvCache.add(key, srv);
}

ID3D11ShaderResourceView *TextureStorage11::getSRVLevel(int mipLevel)
{
    if (mipLevel >= 0 && mipLevel < getLevelCount())
    {
        if (!mLevelSRVs[mipLevel])
        {
            mLevelSRVs[mipLevel] = createSRV(mipLevel, 1, mShaderResourceFormat, getResource());
        }

        return mLevelSRVs[mipLevel];
    }
    else
    {
        return NULL;
    }
}

gl::Error TextureStorage11::generateSwizzles(GLenum swizzleRed, GLenum swizzleGreen, GLenum swizzleBlue, GLenum swizzleAlpha)
{
    SwizzleCacheValue swizzleTarget(swizzleRed, swizzleGreen, swizzleBlue, swizzleAlpha);
    for (int level = 0; level < getLevelCount(); level++)
    {
        // Check if the swizzle for this level is out of date
        if (mSwizzleCache[level] != swizzleTarget)
        {
            // Need to re-render the swizzle for this level
            ID3D11ShaderResourceView *sourceSRV = getSRVLevel(level);
            ID3D11RenderTargetView *destRTV = getSwizzleRenderTarget(level);

            gl::Extents size(getLevelWidth(level), getLevelHeight(level), getLevelDepth(level));

            Blit11 *blitter = mRenderer->getBlitter();

            gl::Error error = blitter->swizzleTexture(sourceSRV, destRTV, size, swizzleRed, swizzleGreen, swizzleBlue, swizzleAlpha);
            if (error.isError())
            {
                return error;
            }

            mSwizzleCache[level] = swizzleTarget;
        }
    }

    return gl::Error(GL_NO_ERROR);
}

void TextureStorage11::invalidateSwizzleCacheLevel(int mipLevel)
{
    if (mipLevel >= 0 && static_cast<unsigned int>(mipLevel) < ArraySize(mSwizzleCache))
    {
        // The default constructor of SwizzleCacheValue has GL_NONE for all channels which is not a
        // valid swizzle combination
        mSwizzleCache[mipLevel] = SwizzleCacheValue();
    }
}

void TextureStorage11::invalidateSwizzleCache()
{
    for (unsigned int mipLevel = 0; mipLevel < ArraySize(mSwizzleCache); mipLevel++)
    {
        invalidateSwizzleCacheLevel(mipLevel);
    }
}

gl::Error TextureStorage11::updateSubresourceLevel(ID3D11Resource *srcTexture, unsigned int sourceSubresource,
                                                   int level, int layerTarget, GLint xoffset, GLint yoffset, GLint zoffset,
                                                   GLsizei width, GLsizei height, GLsizei depth)
{
    ASSERT(srcTexture);

    invalidateSwizzleCacheLevel(level);

    gl::Extents texSize(getLevelWidth(level), getLevelHeight(level), getLevelDepth(level));
    gl::Box copyArea(xoffset, yoffset, zoffset, width, height, depth);

    bool fullCopy = copyArea.x == 0 &&
                    copyArea.y == 0 &&
                    copyArea.z == 0 &&
                    copyArea.width  == texSize.width &&
                    copyArea.height == texSize.height &&
                    copyArea.depth  == texSize.depth;

    ID3D11Resource *dstTexture = getResource();
    unsigned int dstSubresource = getSubresourceIndex(level + mTopLevel, layerTarget);

    ASSERT(dstTexture);

    const d3d11::DXGIFormat &dxgiFormatInfo = d3d11::GetDXGIFormatInfo(mTextureFormat);
    if (!fullCopy && (dxgiFormatInfo.depthBits > 0 || dxgiFormatInfo.stencilBits > 0))
    {
        // CopySubresourceRegion cannot copy partial depth stencils, use the blitter instead
        Blit11 *blitter = mRenderer->getBlitter();

        return blitter->copyDepthStencil(srcTexture, sourceSubresource, copyArea, texSize,
                                         dstTexture, dstSubresource, copyArea, texSize,
                                         NULL);
    }
    else
    {
        const d3d11::DXGIFormat &dxgiFormatInfo = d3d11::GetDXGIFormatInfo(mTextureFormat);

        D3D11_BOX srcBox;
        srcBox.left = copyArea.x;
        srcBox.top = copyArea.y;
        srcBox.right = copyArea.x + roundUp((unsigned int)width, dxgiFormatInfo.blockWidth);
        srcBox.bottom = copyArea.y + roundUp((unsigned int)height, dxgiFormatInfo.blockHeight);
        srcBox.front = copyArea.z;
        srcBox.back = copyArea.z + copyArea.depth;

        ID3D11DeviceContext *context = mRenderer->getDeviceContext();

        context->CopySubresourceRegion(dstTexture, dstSubresource, copyArea.x, copyArea.y, copyArea.z,
                                       srcTexture, sourceSubresource, fullCopy ? NULL : &srcBox);
        return gl::Error(GL_NO_ERROR);
    }
}

bool TextureStorage11::copySubresourceLevel(ID3D11Resource* dstTexture, unsigned int dstSubresource,
                                            int level, int layerTarget, GLint xoffset, GLint yoffset, GLint zoffset,
                                            GLsizei width, GLsizei height, GLsizei depth)
{
    if (dstTexture)
    {
        ID3D11Resource *srcTexture = getResource();
        unsigned int srcSubresource = getSubresourceIndex(level + mTopLevel, layerTarget);

        ASSERT(srcTexture);

        ID3D11DeviceContext *context = mRenderer->getDeviceContext();

        context->CopySubresourceRegion(dstTexture, dstSubresource, xoffset, yoffset, zoffset,
                                       srcTexture, srcSubresource, NULL);
        return true;
    }

    return false;
}

void TextureStorage11::generateMipmap(const gl::ImageIndex &sourceIndex, const gl::ImageIndex &destIndex)
{
    ASSERT(sourceIndex.layerIndex == destIndex.layerIndex);

    invalidateSwizzleCacheLevel(destIndex.mipIndex);

    RenderTarget11 *source = RenderTarget11::makeRenderTarget11(getRenderTarget(sourceIndex));
    RenderTarget11 *dest = RenderTarget11::makeRenderTarget11(getRenderTarget(destIndex));

    if (source && dest)
    {
        ID3D11ShaderResourceView *sourceSRV = source->getShaderResourceView();
        ID3D11RenderTargetView *destRTV = dest->getRenderTargetView();

        if (sourceSRV && destRTV)
        {
            gl::Box sourceArea(0, 0, 0, source->getWidth(), source->getHeight(), source->getDepth());
            gl::Extents sourceSize(source->getWidth(), source->getHeight(), source->getDepth());

            gl::Box destArea(0, 0, 0, dest->getWidth(), dest->getHeight(), dest->getDepth());
            gl::Extents destSize(dest->getWidth(), dest->getHeight(), dest->getDepth());

            Blit11 *blitter = mRenderer->getBlitter();

            blitter->copyTexture(sourceSRV, sourceArea, sourceSize, destRTV, destArea, destSize, NULL,
                                 gl::GetInternalFormatInfo(source->getInternalFormat()).format, GL_LINEAR);
        }
    }
}

void TextureStorage11::verifySwizzleExists(GLenum swizzleRed, GLenum swizzleGreen, GLenum swizzleBlue, GLenum swizzleAlpha)
{
    SwizzleCacheValue swizzleTarget(swizzleRed, swizzleGreen, swizzleBlue, swizzleAlpha);
    for (unsigned int level = 0; level < mMipLevels; level++)
    {
        ASSERT(mSwizzleCache[level] == swizzleTarget);
    }
}

gl::Error TextureStorage11::copyToStorage(TextureStorage *destStorage)
{
    ASSERT(destStorage);

    TextureStorage11 *dest11 = TextureStorage11::makeTextureStorage11(destStorage);

    ID3D11DeviceContext *immediateContext = mRenderer->getDeviceContext();
    immediateContext->CopyResource(dest11->getResource(), getResource());

    dest11->invalidateSwizzleCache();

    return gl::Error(GL_NO_ERROR);
}

TextureStorage11_2D::TextureStorage11_2D(Renderer *renderer, SwapChain11 *swapchain)
    : TextureStorage11(renderer, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE),
      mTexture(swapchain->getOffscreenTexture()),
      mSwizzleTexture(NULL)
{
    mTexture->AddRef();

    for (unsigned int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        mAssociatedImages[i] = NULL;
        mRenderTarget[i] = NULL;
        mSwizzleRenderTargets[i] = NULL;
    }

    D3D11_TEXTURE2D_DESC texDesc;
    mTexture->GetDesc(&texDesc);
    mMipLevels = texDesc.MipLevels;
    mTextureFormat = texDesc.Format;
    mTextureWidth = texDesc.Width;
    mTextureHeight = texDesc.Height;
    mTextureDepth = 1;

    ID3D11ShaderResourceView *srv = swapchain->getRenderTargetShaderResource();
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srv->GetDesc(&srvDesc);
    mShaderResourceFormat = srvDesc.Format;

    ID3D11RenderTargetView* offscreenRTV = swapchain->getRenderTarget();
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
    offscreenRTV->GetDesc(&rtvDesc);
    mRenderTargetFormat = rtvDesc.Format;

    const d3d11::DXGIFormat &dxgiFormatInfo = d3d11::GetDXGIFormatInfo(mTextureFormat);
    const d3d11::TextureFormat &formatInfo = d3d11::GetTextureFormatInfo(dxgiFormatInfo.internalFormat);
    mSwizzleTextureFormat = formatInfo.swizzleTexFormat;
    mSwizzleShaderResourceFormat = formatInfo.swizzleSRVFormat;
    mSwizzleRenderTargetFormat = formatInfo.swizzleRTVFormat;

    mDepthStencilFormat = DXGI_FORMAT_UNKNOWN;

    initializeSerials(1, 1);
}

TextureStorage11_2D::TextureStorage11_2D(Renderer *renderer, GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, int levels)
    : TextureStorage11(renderer, GetTextureBindFlags(internalformat, renderTarget)),
      mTexture(NULL),
      mSwizzleTexture(NULL)
{
    for (unsigned int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        mAssociatedImages[i] = NULL;
        mRenderTarget[i] = NULL;
        mSwizzleRenderTargets[i] = NULL;
    }

    const d3d11::TextureFormat &formatInfo = d3d11::GetTextureFormatInfo(internalformat);
    mTextureFormat = formatInfo.texFormat;
    mShaderResourceFormat = formatInfo.srvFormat;
    mDepthStencilFormat = formatInfo.dsvFormat;
    mRenderTargetFormat = formatInfo.rtvFormat;
    mSwizzleTextureFormat = formatInfo.swizzleTexFormat;
    mSwizzleShaderResourceFormat = formatInfo.swizzleSRVFormat;
    mSwizzleRenderTargetFormat = formatInfo.swizzleRTVFormat;

    // if the width or height is not positive this should be treated as an incomplete texture
    // we handle that here by skipping the d3d texture creation
    if (width > 0 && height > 0)
    {
        // adjust size if needed for compressed textures
        d3d11::MakeValidSize(false, mTextureFormat, &width, &height, &mTopLevel);

        ID3D11Device *device = mRenderer->getDevice();

        D3D11_TEXTURE2D_DESC desc;
        desc.Width = width;      // Compressed texture size constraints?
        desc.Height = height;
        desc.MipLevels = ((levels > 0) ? (mTopLevel + levels) : 0);
        desc.ArraySize = 1;
        desc.Format = mTextureFormat;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = getBindFlags();
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        HRESULT result = device->CreateTexture2D(&desc, NULL, &mTexture);

        // this can happen from windows TDR
        if (d3d11::isDeviceLostError(result))
        {
            mRenderer->notifyDeviceLost();
            gl::error(GL_OUT_OF_MEMORY);
        }
        else if (FAILED(result))
        {
            ASSERT(result == E_OUTOFMEMORY);
            ERR("Creating image failed.");
            gl::error(GL_OUT_OF_MEMORY);
        }
        else
        {
            mTexture->GetDesc(&desc);
            mMipLevels = desc.MipLevels;
            mTextureWidth = desc.Width;
            mTextureHeight = desc.Height;
            mTextureDepth = 1;
        }
    }

    initializeSerials(getLevelCount(), 1);
}

TextureStorage11_2D::~TextureStorage11_2D()
{
    for (unsigned i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        if (mAssociatedImages[i] != NULL)
        {
            bool imageAssociationCorrect = mAssociatedImages[i]->isAssociatedStorageValid(this);
            ASSERT(imageAssociationCorrect);

            if (imageAssociationCorrect)
            {
                // We must let the Images recover their data before we delete it from the TextureStorage.
                mAssociatedImages[i]->recoverFromAssociatedStorage();
            }
        }
    }

    SafeRelease(mTexture);
    SafeRelease(mSwizzleTexture);

    for (unsigned int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        SafeDelete(mRenderTarget[i]);
        SafeRelease(mSwizzleRenderTargets[i]);
    }
}

TextureStorage11_2D *TextureStorage11_2D::makeTextureStorage11_2D(TextureStorage *storage)
{
    ASSERT(HAS_DYNAMIC_TYPE(TextureStorage11_2D*, storage));
    return static_cast<TextureStorage11_2D*>(storage);
}

void TextureStorage11_2D::associateImage(Image11* image, int level, int layerTarget)
{
    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);

    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        mAssociatedImages[level] = image;
    }
}

bool TextureStorage11_2D::isAssociatedImageValid(int level, int layerTarget, Image11* expectedImage)
{
    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        // This validation check should never return false. It means the Image/TextureStorage association is broken.
        bool retValue = (mAssociatedImages[level] == expectedImage);
        ASSERT(retValue);
        return retValue;
    }

    return false;
}

// disassociateImage allows an Image to end its association with a Storage.
void TextureStorage11_2D::disassociateImage(int level, int layerTarget, Image11* expectedImage)
{
    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);

    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        ASSERT(mAssociatedImages[level] == expectedImage);

        if (mAssociatedImages[level] == expectedImage)
        {
            mAssociatedImages[level] = NULL;
        }
    }
}

// releaseAssociatedImage prepares the Storage for a new Image association. It lets the old Image recover its data before ending the association.
void TextureStorage11_2D::releaseAssociatedImage(int level, int layerTarget, Image11* incomingImage)
{
    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);

    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        // No need to let the old Image recover its data, if it is also the incoming Image.
        if (mAssociatedImages[level] != NULL && mAssociatedImages[level] != incomingImage)
        {
            // Ensure that the Image is still associated with this TextureStorage. This should be true.
            bool imageAssociationCorrect = mAssociatedImages[level]->isAssociatedStorageValid(this);
            ASSERT(imageAssociationCorrect);

            if (imageAssociationCorrect)
            {
                // Force the image to recover from storage before its data is overwritten.
                // This will reset mAssociatedImages[level] to NULL too.
                mAssociatedImages[level]->recoverFromAssociatedStorage();
            }
        }
    }
}

ID3D11Resource *TextureStorage11_2D::getResource() const
{
    return mTexture;
}

RenderTarget *TextureStorage11_2D::getRenderTarget(const gl::ImageIndex &index)
{
    ASSERT(!index.hasLayer());

    int level = index.mipIndex;

    if (level >= 0 && level < getLevelCount())
    {
        if (!mRenderTarget[level])
        {
            ID3D11ShaderResourceView *srv = getSRVLevel(level);
            if (!srv)
            {
                return NULL;
            }

            if (mRenderTargetFormat != DXGI_FORMAT_UNKNOWN)
            {
                ID3D11Device *device = mRenderer->getDevice();

                D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
                rtvDesc.Format = mRenderTargetFormat;
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Texture2D.MipSlice = mTopLevel + level;

                ID3D11RenderTargetView *rtv;
                HRESULT result = device->CreateRenderTargetView(mTexture, &rtvDesc, &rtv);

                if (result == E_OUTOFMEMORY)
                {
                    return gl::error(GL_OUT_OF_MEMORY, static_cast<RenderTarget*>(NULL));
                }
                ASSERT(SUCCEEDED(result));

                mRenderTarget[level] = new RenderTarget11(mRenderer, rtv, mTexture, srv, getLevelWidth(level), getLevelHeight(level), 1);

                // RenderTarget will take ownership of these resources
                SafeRelease(rtv);
            }
            else if (mDepthStencilFormat != DXGI_FORMAT_UNKNOWN)
            {
                ID3D11Device *device = mRenderer->getDevice();

                D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
                dsvDesc.Format = mDepthStencilFormat;
                dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                dsvDesc.Texture2D.MipSlice = mTopLevel + level;
                dsvDesc.Flags = 0;

                ID3D11DepthStencilView *dsv;
                HRESULT result = device->CreateDepthStencilView(mTexture, &dsvDesc, &dsv);

                if (result == E_OUTOFMEMORY)
                {
                    SafeRelease(srv);
                    return gl::error(GL_OUT_OF_MEMORY, static_cast<RenderTarget*>(NULL));
                }
                ASSERT(SUCCEEDED(result));

                mRenderTarget[level] = new RenderTarget11(mRenderer, dsv, mTexture, srv, getLevelWidth(level), getLevelHeight(level), 1);

                // RenderTarget will take ownership of these resources
                SafeRelease(dsv);
            }
            else
            {
                UNREACHABLE();
            }
        }

        return mRenderTarget[level];
    }
    else
    {
        return NULL;
    }
}

ID3D11ShaderResourceView *TextureStorage11_2D::createSRV(int baseLevel, int mipLevels, DXGI_FORMAT format, ID3D11Resource *texture)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = mTopLevel + baseLevel;
    srvDesc.Texture2D.MipLevels = mipLevels;

    ID3D11ShaderResourceView *SRV = NULL;

    ID3D11Device *device = mRenderer->getDevice();
    HRESULT result = device->CreateShaderResourceView(texture, &srvDesc, &SRV);

    if (result == E_OUTOFMEMORY)
    {
        gl::error(GL_OUT_OF_MEMORY);
    }
    ASSERT(SUCCEEDED(result));

    return SRV;
}

ID3D11Resource *TextureStorage11_2D::getSwizzleTexture()
{
    if (!mSwizzleTexture)
    {
        ID3D11Device *device = mRenderer->getDevice();

        D3D11_TEXTURE2D_DESC desc;
        desc.Width = mTextureWidth;
        desc.Height = mTextureHeight;
        desc.MipLevels = mMipLevels;
        desc.ArraySize = 1;
        desc.Format = mSwizzleTextureFormat;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        HRESULT result = device->CreateTexture2D(&desc, NULL, &mSwizzleTexture);

        if (result == E_OUTOFMEMORY)
        {
            return gl::error(GL_OUT_OF_MEMORY, static_cast<ID3D11Texture2D*>(NULL));
        }
        ASSERT(SUCCEEDED(result));
    }

    return mSwizzleTexture;
}

ID3D11RenderTargetView *TextureStorage11_2D::getSwizzleRenderTarget(int mipLevel)
{
    if (mipLevel >= 0 && mipLevel < getLevelCount())
    {
        if (!mSwizzleRenderTargets[mipLevel])
        {
            ID3D11Resource *swizzleTexture = getSwizzleTexture();
            if (!swizzleTexture)
            {
                return NULL;
            }

            ID3D11Device *device = mRenderer->getDevice();

            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            rtvDesc.Format = mSwizzleRenderTargetFormat;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = mTopLevel + mipLevel;

            HRESULT result = device->CreateRenderTargetView(mSwizzleTexture, &rtvDesc, &mSwizzleRenderTargets[mipLevel]);
            if (result == E_OUTOFMEMORY)
            {
                return gl::error(GL_OUT_OF_MEMORY, static_cast<ID3D11RenderTargetView*>(NULL));
            }
            ASSERT(SUCCEEDED(result));
        }

        return mSwizzleRenderTargets[mipLevel];
    }
    else
    {
        return NULL;
    }
}

TextureStorage11_Cube::TextureStorage11_Cube(Renderer *renderer, GLenum internalformat, bool renderTarget, int size, int levels)
    : TextureStorage11(renderer, GetTextureBindFlags(internalformat, renderTarget))
{
    mTexture = NULL;
    mSwizzleTexture = NULL;

    for (unsigned int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        mSwizzleRenderTargets[level] = NULL;
        for (unsigned int face = 0; face < 6; face++)
        {
            mAssociatedImages[face][level] = NULL;
            mRenderTarget[face][level] = NULL;
        }
    }

    const d3d11::TextureFormat &formatInfo = d3d11::GetTextureFormatInfo(internalformat);
    mTextureFormat = formatInfo.texFormat;
    mShaderResourceFormat = formatInfo.srvFormat;
    mDepthStencilFormat = formatInfo.dsvFormat;
    mRenderTargetFormat = formatInfo.rtvFormat;
    mSwizzleTextureFormat = formatInfo.swizzleTexFormat;
    mSwizzleShaderResourceFormat = formatInfo.swizzleSRVFormat;
    mSwizzleRenderTargetFormat = formatInfo.swizzleRTVFormat;

    // if the size is not positive this should be treated as an incomplete texture
    // we handle that here by skipping the d3d texture creation
    if (size > 0)
    {
        // adjust size if needed for compressed textures
        int height = size;
        d3d11::MakeValidSize(false, mTextureFormat, &size, &height, &mTopLevel);

        ID3D11Device *device = mRenderer->getDevice();

        D3D11_TEXTURE2D_DESC desc;
        desc.Width = size;
        desc.Height = size;
        desc.MipLevels = ((levels > 0) ? (mTopLevel + levels) : 0);
        desc.ArraySize = 6;
        desc.Format = mTextureFormat;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = getBindFlags();
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        HRESULT result = device->CreateTexture2D(&desc, NULL, &mTexture);

        if (FAILED(result))
        {
            ASSERT(result == E_OUTOFMEMORY);
            ERR("Creating image failed.");
            gl::error(GL_OUT_OF_MEMORY);
        }
        else
        {
            mTexture->GetDesc(&desc);
            mMipLevels = desc.MipLevels;
            mTextureWidth = desc.Width;
            mTextureHeight = desc.Height;
            mTextureDepth = 1;
        }
    }

    initializeSerials(getLevelCount() * 6, 6);
}


TextureStorage11_Cube::~TextureStorage11_Cube()
{
    for (unsigned int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        for (unsigned int face = 0; face < 6; face++)
        {
            if (mAssociatedImages[face][level] != NULL)
            {
                bool imageAssociationCorrect = mAssociatedImages[face][level]->isAssociatedStorageValid(this);
                ASSERT(imageAssociationCorrect);

                if (imageAssociationCorrect)
                {
                    // We must let the Images recover their data before we delete it from the TextureStorage.
                    mAssociatedImages[face][level]->recoverFromAssociatedStorage();
                }
            }
        }
    }

    SafeRelease(mTexture);
    SafeRelease(mSwizzleTexture);

    for (unsigned int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        SafeRelease(mSwizzleRenderTargets[level]);
        for (unsigned int face = 0; face < 6; face++)
        {
            SafeDelete(mRenderTarget[face][level]);
        }
    }
}

TextureStorage11_Cube *TextureStorage11_Cube::makeTextureStorage11_Cube(TextureStorage *storage)
{
    ASSERT(HAS_DYNAMIC_TYPE(TextureStorage11_Cube*, storage));
    return static_cast<TextureStorage11_Cube*>(storage);
}

void TextureStorage11_Cube::associateImage(Image11* image, int level, int layerTarget)
{
    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(0 <= layerTarget && layerTarget < 6);

    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        if (0 <= layerTarget && layerTarget < 6)
        {
            mAssociatedImages[layerTarget][level] = image;
        }
    }
}

bool TextureStorage11_Cube::isAssociatedImageValid(int level, int layerTarget, Image11* expectedImage)
{
    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        if (0 <= layerTarget && layerTarget < 6)
        {
            // This validation check should never return false. It means the Image/TextureStorage association is broken.
            bool retValue = (mAssociatedImages[layerTarget][level] == expectedImage);
            ASSERT(retValue);
            return retValue;
        }
    }

    return false;
}

// disassociateImage allows an Image to end its association with a Storage.
void TextureStorage11_Cube::disassociateImage(int level, int layerTarget, Image11* expectedImage)
{
    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(0 <= layerTarget && layerTarget < 6);

    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        if (0 <= layerTarget && layerTarget < 6)
        {
            ASSERT(mAssociatedImages[layerTarget][level] == expectedImage);

            if (mAssociatedImages[layerTarget][level] == expectedImage)
            {
                mAssociatedImages[layerTarget][level] = NULL;
            }
        }
    }
}

// releaseAssociatedImage prepares the Storage for a new Image association. It lets the old Image recover its data before ending the association.
void TextureStorage11_Cube::releaseAssociatedImage(int level, int layerTarget, Image11* incomingImage)
{
    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(0 <= layerTarget && layerTarget < 6);

    if ((0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS))
    {
        if (0 <= layerTarget && layerTarget < 6)
        {
            // No need to let the old Image recover its data, if it is also the incoming Image.
            if (mAssociatedImages[layerTarget][level] != NULL && mAssociatedImages[layerTarget][level] != incomingImage)
            {
                // Ensure that the Image is still associated with this TextureStorage. This should be true.
                bool imageAssociationCorrect = mAssociatedImages[layerTarget][level]->isAssociatedStorageValid(this);
                ASSERT(imageAssociationCorrect);

                if (imageAssociationCorrect)
                {
                    // Force the image to recover from storage before its data is overwritten.
                    // This will reset mAssociatedImages[level] to NULL too.
                    mAssociatedImages[layerTarget][level]->recoverFromAssociatedStorage();
                }
            }
        }
    }
}

ID3D11Resource *TextureStorage11_Cube::getResource() const
{
    return mTexture;
}

RenderTarget *TextureStorage11_Cube::getRenderTarget(const gl::ImageIndex &index)
{
    int faceIndex = index.layerIndex;
    int level = index.mipIndex;

    if (level >= 0 && level < getLevelCount())
    {
        if (!mRenderTarget[faceIndex][level])
        {
            ID3D11Device *device = mRenderer->getDevice();
            HRESULT result;

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            srvDesc.Format = mShaderResourceFormat;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY; // Will be used with Texture2D sampler, not TextureCube
            srvDesc.Texture2DArray.MostDetailedMip = mTopLevel + level;
            srvDesc.Texture2DArray.MipLevels = 1;
            srvDesc.Texture2DArray.FirstArraySlice = faceIndex;
            srvDesc.Texture2DArray.ArraySize = 1;

            ID3D11ShaderResourceView *srv;
            result = device->CreateShaderResourceView(mTexture, &srvDesc, &srv);

            if (result == E_OUTOFMEMORY)
            {
                return gl::error(GL_OUT_OF_MEMORY, static_cast<RenderTarget*>(NULL));
            }
            ASSERT(SUCCEEDED(result));

            if (mRenderTargetFormat != DXGI_FORMAT_UNKNOWN)
            {
                D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
                rtvDesc.Format = mRenderTargetFormat;
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice = mTopLevel + level;
                rtvDesc.Texture2DArray.FirstArraySlice = faceIndex;
                rtvDesc.Texture2DArray.ArraySize = 1;

                ID3D11RenderTargetView *rtv;
                result = device->CreateRenderTargetView(mTexture, &rtvDesc, &rtv);

                if (result == E_OUTOFMEMORY)
                {
                    SafeRelease(srv);
                    return gl::error(GL_OUT_OF_MEMORY, static_cast<RenderTarget*>(NULL));
                }
                ASSERT(SUCCEEDED(result));

                mRenderTarget[faceIndex][level] = new RenderTarget11(mRenderer, rtv, mTexture, srv, getLevelWidth(level), getLevelHeight(level), 1);

                // RenderTarget will take ownership of these resources
                SafeRelease(rtv);
                SafeRelease(srv);
            }
            else if (mDepthStencilFormat != DXGI_FORMAT_UNKNOWN)
            {
                D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
                dsvDesc.Format = mDepthStencilFormat;
                dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                dsvDesc.Flags = 0;
                dsvDesc.Texture2DArray.MipSlice = mTopLevel + level;
                dsvDesc.Texture2DArray.FirstArraySlice = faceIndex;
                dsvDesc.Texture2DArray.ArraySize = 1;

                ID3D11DepthStencilView *dsv;
                result = device->CreateDepthStencilView(mTexture, &dsvDesc, &dsv);

                if (result == E_OUTOFMEMORY)
                {
                    SafeRelease(srv);
                    return gl::error(GL_OUT_OF_MEMORY, static_cast<RenderTarget*>(NULL));
                }
                ASSERT(SUCCEEDED(result));

                mRenderTarget[faceIndex][level] = new RenderTarget11(mRenderer, dsv, mTexture, srv, getLevelWidth(level), getLevelHeight(level), 1);

                // RenderTarget will take ownership of these resources
                SafeRelease(dsv);
                SafeRelease(srv);
            }
            else
            {
                UNREACHABLE();
            }
        }

        return mRenderTarget[faceIndex][level];
    }
    else
    {
        return NULL;
    }
}

ID3D11ShaderResourceView *TextureStorage11_Cube::createSRV(int baseLevel, int mipLevels, DXGI_FORMAT format, ID3D11Resource *texture)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = format;

    // Unnormalized integer cube maps are not supported by DX11; we emulate them as an array of six 2D textures
    const d3d11::DXGIFormat &dxgiFormatInfo = d3d11::GetDXGIFormatInfo(format);
    if (dxgiFormatInfo.componentType == GL_INT || dxgiFormatInfo.componentType == GL_UNSIGNED_INT)
    {
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip = mTopLevel + baseLevel;
        srvDesc.Texture2DArray.MipLevels = 1;
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.ArraySize = 6;
    }
    else
    {
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = mipLevels;
        srvDesc.TextureCube.MostDetailedMip = mTopLevel + baseLevel;
    }

    ID3D11ShaderResourceView *SRV = NULL;

    ID3D11Device *device = mRenderer->getDevice();
    HRESULT result = device->CreateShaderResourceView(texture, &srvDesc, &SRV);

    if (result == E_OUTOFMEMORY)
    {
        gl::error(GL_OUT_OF_MEMORY);
    }
    ASSERT(SUCCEEDED(result));

    return SRV;
}

ID3D11Resource *TextureStorage11_Cube::getSwizzleTexture()
{
    if (!mSwizzleTexture)
    {
        ID3D11Device *device = mRenderer->getDevice();

        D3D11_TEXTURE2D_DESC desc;
        desc.Width = mTextureWidth;
        desc.Height = mTextureHeight;
        desc.MipLevels = mMipLevels;
        desc.ArraySize = 6;
        desc.Format = mSwizzleTextureFormat;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        HRESULT result = device->CreateTexture2D(&desc, NULL, &mSwizzleTexture);

        if (result == E_OUTOFMEMORY)
        {
            return gl::error(GL_OUT_OF_MEMORY, static_cast<ID3D11Texture2D*>(NULL));
        }
        ASSERT(SUCCEEDED(result));
    }

    return mSwizzleTexture;
}

ID3D11RenderTargetView *TextureStorage11_Cube::getSwizzleRenderTarget(int mipLevel)
{
    if (mipLevel >= 0 && mipLevel < getLevelCount())
    {
        if (!mSwizzleRenderTargets[mipLevel])
        {
            ID3D11Resource *swizzleTexture = getSwizzleTexture();
            if (!swizzleTexture)
            {
                return NULL;
            }

            ID3D11Device *device = mRenderer->getDevice();

            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            rtvDesc.Format = mSwizzleRenderTargetFormat;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice = mTopLevel + mipLevel;
            rtvDesc.Texture2DArray.FirstArraySlice = 0;
            rtvDesc.Texture2DArray.ArraySize = 6;

            HRESULT result = device->CreateRenderTargetView(mSwizzleTexture, &rtvDesc, &mSwizzleRenderTargets[mipLevel]);

            if (result == E_OUTOFMEMORY)
            {
                return gl::error(GL_OUT_OF_MEMORY, static_cast<ID3D11RenderTargetView*>(NULL));
            }
            ASSERT(SUCCEEDED(result));
        }

        return mSwizzleRenderTargets[mipLevel];
    }
    else
    {
        return NULL;
    }
}

TextureStorage11_3D::TextureStorage11_3D(Renderer *renderer, GLenum internalformat, bool renderTarget,
                                         GLsizei width, GLsizei height, GLsizei depth, int levels)
    : TextureStorage11(renderer, GetTextureBindFlags(internalformat, renderTarget))
{
    mTexture = NULL;
    mSwizzleTexture = NULL;

    for (unsigned int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        mAssociatedImages[i] = NULL;
        mLevelRenderTargets[i] = NULL;
        mSwizzleRenderTargets[i] = NULL;
    }

    const d3d11::TextureFormat &formatInfo = d3d11::GetTextureFormatInfo(internalformat);
    mTextureFormat = formatInfo.texFormat;
    mShaderResourceFormat = formatInfo.srvFormat;
    mDepthStencilFormat = formatInfo.dsvFormat;
    mRenderTargetFormat = formatInfo.rtvFormat;
    mSwizzleTextureFormat = formatInfo.swizzleTexFormat;
    mSwizzleShaderResourceFormat = formatInfo.swizzleSRVFormat;
    mSwizzleRenderTargetFormat = formatInfo.swizzleRTVFormat;

    // If the width, height or depth are not positive this should be treated as an incomplete texture
    // we handle that here by skipping the d3d texture creation
    if (width > 0 && height > 0 && depth > 0)
    {
        // adjust size if needed for compressed textures
        d3d11::MakeValidSize(false, mTextureFormat, &width, &height, &mTopLevel);

        ID3D11Device *device = mRenderer->getDevice();

        D3D11_TEXTURE3D_DESC desc;
        desc.Width = width;
        desc.Height = height;
        desc.Depth = depth;
        desc.MipLevels = ((levels > 0) ? (mTopLevel + levels) : 0);
        desc.Format = mTextureFormat;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = getBindFlags();
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        HRESULT result = device->CreateTexture3D(&desc, NULL, &mTexture);

        // this can happen from windows TDR
        if (d3d11::isDeviceLostError(result))
        {
            mRenderer->notifyDeviceLost();
            gl::error(GL_OUT_OF_MEMORY);
        }
        else if (FAILED(result))
        {
            ASSERT(result == E_OUTOFMEMORY);
            ERR("Creating image failed.");
            gl::error(GL_OUT_OF_MEMORY);
        }
        else
        {
            mTexture->GetDesc(&desc);
            mMipLevels = desc.MipLevels;
            mTextureWidth = desc.Width;
            mTextureHeight = desc.Height;
            mTextureDepth = desc.Depth;
        }
    }

    initializeSerials(getLevelCount() * depth, depth);
}

TextureStorage11_3D::~TextureStorage11_3D()
{
    for (unsigned i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        if (mAssociatedImages[i] != NULL)
        {
            bool imageAssociationCorrect = mAssociatedImages[i]->isAssociatedStorageValid(this);
            ASSERT(imageAssociationCorrect);

            if (imageAssociationCorrect)
            {
                // We must let the Images recover their data before we delete it from the TextureStorage.
                mAssociatedImages[i]->recoverFromAssociatedStorage();
            }
        }
    }

    SafeRelease(mTexture);
    SafeRelease(mSwizzleTexture);

    for (RenderTargetMap::iterator i = mLevelLayerRenderTargets.begin(); i != mLevelLayerRenderTargets.end(); i++)
    {
        SafeDelete(i->second);
    }
    mLevelLayerRenderTargets.clear();

    for (unsigned int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        SafeDelete(mLevelRenderTargets[i]);
        SafeRelease(mSwizzleRenderTargets[i]);
    }
}

TextureStorage11_3D *TextureStorage11_3D::makeTextureStorage11_3D(TextureStorage *storage)
{
    ASSERT(HAS_DYNAMIC_TYPE(TextureStorage11_3D*, storage));
    return static_cast<TextureStorage11_3D*>(storage);
}

void TextureStorage11_3D::associateImage(Image11* image, int level, int layerTarget)
{
    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);

    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        mAssociatedImages[level] = image;
    }
}

bool TextureStorage11_3D::isAssociatedImageValid(int level, int layerTarget, Image11* expectedImage)
{
    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        // This validation check should never return false. It means the Image/TextureStorage association is broken.
        bool retValue = (mAssociatedImages[level] == expectedImage);
        ASSERT(retValue);
        return retValue;
    }

    return false;
}

// disassociateImage allows an Image to end its association with a Storage.
void TextureStorage11_3D::disassociateImage(int level, int layerTarget, Image11* expectedImage)
{
    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);

    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        ASSERT(mAssociatedImages[level] == expectedImage);

        if (mAssociatedImages[level] == expectedImage)
        {
            mAssociatedImages[level] = NULL;
        }
    }
}

// releaseAssociatedImage prepares the Storage for a new Image association. It lets the old Image recover its data before ending the association.
void TextureStorage11_3D::releaseAssociatedImage(int level, int layerTarget, Image11* incomingImage)
{
    ASSERT((0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS));

    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        // No need to let the old Image recover its data, if it is also the incoming Image.
        if (mAssociatedImages[level] != NULL && mAssociatedImages[level] != incomingImage)
        {
            // Ensure that the Image is still associated with this TextureStorage. This should be true.
            bool imageAssociationCorrect = mAssociatedImages[level]->isAssociatedStorageValid(this);
            ASSERT(imageAssociationCorrect);

            if (imageAssociationCorrect)
            {
                // Force the image to recover from storage before its data is overwritten.
                // This will reset mAssociatedImages[level] to NULL too.
                mAssociatedImages[level]->recoverFromAssociatedStorage();
            }
        }
    }
}

ID3D11Resource *TextureStorage11_3D::getResource() const
{
    return mTexture;
}

ID3D11ShaderResourceView *TextureStorage11_3D::createSRV(int baseLevel, int mipLevels, DXGI_FORMAT format, ID3D11Resource *texture)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MostDetailedMip = baseLevel;
    srvDesc.Texture3D.MipLevels = mipLevels;

    ID3D11ShaderResourceView *SRV = NULL;

    ID3D11Device *device = mRenderer->getDevice();
    HRESULT result = device->CreateShaderResourceView(texture, &srvDesc, &SRV);

    if (result == E_OUTOFMEMORY)
    {
        gl::error(GL_OUT_OF_MEMORY);
    }
    ASSERT(SUCCEEDED(result));

    return SRV;
}

RenderTarget *TextureStorage11_3D::getRenderTarget(const gl::ImageIndex &index)
{
    int mipLevel = index.mipIndex;

    if (mipLevel >= 0 && mipLevel < getLevelCount())
    {
        ASSERT(mRenderTargetFormat != DXGI_FORMAT_UNKNOWN);

        if (!index.hasLayer())
        {
            if (!mLevelRenderTargets[mipLevel])
            {
                ID3D11ShaderResourceView *srv = getSRVLevel(mipLevel);
                if (!srv)
                {
                    return NULL;
                }

                ID3D11Device *device = mRenderer->getDevice();

                D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
                rtvDesc.Format = mRenderTargetFormat;
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
                rtvDesc.Texture3D.MipSlice = mTopLevel + mipLevel;
                rtvDesc.Texture3D.FirstWSlice = 0;
                rtvDesc.Texture3D.WSize = -1;

                ID3D11RenderTargetView *rtv;
                HRESULT result = device->CreateRenderTargetView(mTexture, &rtvDesc, &rtv);

                if (result == E_OUTOFMEMORY)
                {
                    SafeRelease(srv);
                    return gl::error(GL_OUT_OF_MEMORY, static_cast<RenderTarget*>(NULL));
                }
                ASSERT(SUCCEEDED(result));

                mLevelRenderTargets[mipLevel] = new RenderTarget11(mRenderer, rtv, mTexture, srv, getLevelWidth(mipLevel), getLevelHeight(mipLevel), getLevelDepth(mipLevel));

                // RenderTarget will take ownership of these resources
                SafeRelease(rtv);
            }

            return mLevelRenderTargets[mipLevel];
        }
        else
        {
            int layer = index.layerIndex;

            LevelLayerKey key(mipLevel, layer);
            if (mLevelLayerRenderTargets.find(key) == mLevelLayerRenderTargets.end())
            {
                ID3D11Device *device = mRenderer->getDevice();
                HRESULT result;

                // TODO, what kind of SRV is expected here?
                ID3D11ShaderResourceView *srv = NULL;

                D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
                rtvDesc.Format = mRenderTargetFormat;
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
                rtvDesc.Texture3D.MipSlice = mTopLevel + mipLevel;
                rtvDesc.Texture3D.FirstWSlice = layer;
                rtvDesc.Texture3D.WSize = 1;

                ID3D11RenderTargetView *rtv;
                result = device->CreateRenderTargetView(mTexture, &rtvDesc, &rtv);

                if (result == E_OUTOFMEMORY)
                {
                    SafeRelease(srv);
                    return gl::error(GL_OUT_OF_MEMORY, static_cast<RenderTarget*>(NULL));
                }
                ASSERT(SUCCEEDED(result));

                mLevelLayerRenderTargets[key] = new RenderTarget11(mRenderer, rtv, mTexture, srv, getLevelWidth(mipLevel), getLevelHeight(mipLevel), 1);

                // RenderTarget will take ownership of these resources
                SafeRelease(rtv);
                SafeRelease(srv);
            }

            return mLevelLayerRenderTargets[key];
        }
    }

    return NULL;
}

ID3D11Resource *TextureStorage11_3D::getSwizzleTexture()
{
    if (!mSwizzleTexture)
    {
        ID3D11Device *device = mRenderer->getDevice();

        D3D11_TEXTURE3D_DESC desc;
        desc.Width = mTextureWidth;
        desc.Height = mTextureHeight;
        desc.Depth = mTextureDepth;
        desc.MipLevels = mMipLevels;
        desc.Format = mSwizzleTextureFormat;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        HRESULT result = device->CreateTexture3D(&desc, NULL, &mSwizzleTexture);

        if (result == E_OUTOFMEMORY)
        {
            return gl::error(GL_OUT_OF_MEMORY, static_cast<ID3D11Texture3D*>(NULL));
        }
        ASSERT(SUCCEEDED(result));
    }

    return mSwizzleTexture;
}

ID3D11RenderTargetView *TextureStorage11_3D::getSwizzleRenderTarget(int mipLevel)
{
    if (mipLevel >= 0 && mipLevel < getLevelCount())
    {
        if (!mSwizzleRenderTargets[mipLevel])
        {
            ID3D11Resource *swizzleTexture = getSwizzleTexture();
            if (!swizzleTexture)
            {
                return NULL;
            }

            ID3D11Device *device = mRenderer->getDevice();

            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            rtvDesc.Format = mSwizzleRenderTargetFormat;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
            rtvDesc.Texture3D.MipSlice = mTopLevel + mipLevel;
            rtvDesc.Texture3D.FirstWSlice = 0;
            rtvDesc.Texture3D.WSize = -1;

            HRESULT result = device->CreateRenderTargetView(mSwizzleTexture, &rtvDesc, &mSwizzleRenderTargets[mipLevel]);

            if (result == E_OUTOFMEMORY)
            {
                return gl::error(GL_OUT_OF_MEMORY, static_cast<ID3D11RenderTargetView*>(NULL));
            }
            ASSERT(SUCCEEDED(result));
        }

        return mSwizzleRenderTargets[mipLevel];
    }
    else
    {
        return NULL;
    }
}

TextureStorage11_2DArray::TextureStorage11_2DArray(Renderer *renderer, GLenum internalformat, bool renderTarget,
                                                   GLsizei width, GLsizei height, GLsizei depth, int levels)
    : TextureStorage11(renderer, GetTextureBindFlags(internalformat, renderTarget))
{
    mTexture = NULL;
    mSwizzleTexture = NULL;

    for (unsigned int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        mSwizzleRenderTargets[level] = NULL;
    }

    const d3d11::TextureFormat &formatInfo = d3d11::GetTextureFormatInfo(internalformat);
    mTextureFormat = formatInfo.texFormat;
    mShaderResourceFormat = formatInfo.srvFormat;
    mDepthStencilFormat = formatInfo.dsvFormat;
    mRenderTargetFormat = formatInfo.rtvFormat;
    mSwizzleTextureFormat = formatInfo.swizzleTexFormat;
    mSwizzleShaderResourceFormat = formatInfo.swizzleSRVFormat;
    mSwizzleRenderTargetFormat = formatInfo.swizzleRTVFormat;

    // if the width, height or depth is not positive this should be treated as an incomplete texture
    // we handle that here by skipping the d3d texture creation
    if (width > 0 && height > 0 && depth > 0)
    {
        // adjust size if needed for compressed textures
        d3d11::MakeValidSize(false, mTextureFormat, &width, &height, &mTopLevel);

        ID3D11Device *device = mRenderer->getDevice();

        D3D11_TEXTURE2D_DESC desc;
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = ((levels > 0) ? (mTopLevel + levels) : 0);
        desc.ArraySize = depth;
        desc.Format = mTextureFormat;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = getBindFlags();
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        HRESULT result = device->CreateTexture2D(&desc, NULL, &mTexture);

        // this can happen from windows TDR
        if (d3d11::isDeviceLostError(result))
        {
            mRenderer->notifyDeviceLost();
            gl::error(GL_OUT_OF_MEMORY);
        }
        else if (FAILED(result))
        {
            ASSERT(result == E_OUTOFMEMORY);
            ERR("Creating image failed.");
            gl::error(GL_OUT_OF_MEMORY);
        }
        else
        {
            mTexture->GetDesc(&desc);
            mMipLevels = desc.MipLevels;
            mTextureWidth = desc.Width;
            mTextureHeight = desc.Height;
            mTextureDepth = desc.ArraySize;
        }
    }

    initializeSerials(getLevelCount() * depth, depth);
}

TextureStorage11_2DArray::~TextureStorage11_2DArray()
{
    for (ImageMap::iterator i = mAssociatedImages.begin(); i != mAssociatedImages.end(); i++)
    {
        bool imageAssociationCorrect = i->second->isAssociatedStorageValid(this);
        ASSERT(imageAssociationCorrect);

        if (imageAssociationCorrect)
        {
            // We must let the Images recover their data before we delete it from the TextureStorage.
            i->second->recoverFromAssociatedStorage();
        }
    }
    mAssociatedImages.clear();

    SafeRelease(mTexture);
    SafeRelease(mSwizzleTexture);

    for (unsigned int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        SafeRelease(mSwizzleRenderTargets[level]);
    }

    for (RenderTargetMap::iterator i = mRenderTargets.begin(); i != mRenderTargets.end(); i++)
    {
        SafeDelete(i->second);
    }
    mRenderTargets.clear();
}

TextureStorage11_2DArray *TextureStorage11_2DArray::makeTextureStorage11_2DArray(TextureStorage *storage)
{
    ASSERT(HAS_DYNAMIC_TYPE(TextureStorage11_2DArray*, storage));
    return static_cast<TextureStorage11_2DArray*>(storage);
}

void TextureStorage11_2DArray::associateImage(Image11* image, int level, int layerTarget)
{
    ASSERT(0 <= level && level < getLevelCount());

    if (0 <= level && level < getLevelCount())
    {
        LevelLayerKey key(level, layerTarget);
        mAssociatedImages[key] = image;
    }
}

bool TextureStorage11_2DArray::isAssociatedImageValid(int level, int layerTarget, Image11* expectedImage)
{
    LevelLayerKey key(level, layerTarget);

    // This validation check should never return false. It means the Image/TextureStorage association is broken.
    bool retValue = (mAssociatedImages.find(key) != mAssociatedImages.end() && (mAssociatedImages[key] == expectedImage));
    ASSERT(retValue);
    return retValue;
}

// disassociateImage allows an Image to end its association with a Storage.
void TextureStorage11_2DArray::disassociateImage(int level, int layerTarget, Image11* expectedImage)
{
    LevelLayerKey key(level, layerTarget);

    bool imageAssociationCorrect = (mAssociatedImages.find(key) != mAssociatedImages.end() && (mAssociatedImages[key] == expectedImage));
    ASSERT(imageAssociationCorrect);

    if (imageAssociationCorrect)
    {
        mAssociatedImages[key] = NULL;
    }
}

// releaseAssociatedImage prepares the Storage for a new Image association. It lets the old Image recover its data before ending the association.
void TextureStorage11_2DArray::releaseAssociatedImage(int level, int layerTarget, Image11* incomingImage)
{
    LevelLayerKey key(level, layerTarget);

    ASSERT(mAssociatedImages.find(key) != mAssociatedImages.end());

    if (mAssociatedImages.find(key) != mAssociatedImages.end())
    {
        if (mAssociatedImages[key] != NULL && mAssociatedImages[key] != incomingImage)
        {
            // Ensure that the Image is still associated with this TextureStorage. This should be true.
            bool imageAssociationCorrect = mAssociatedImages[key]->isAssociatedStorageValid(this);
            ASSERT(imageAssociationCorrect);

            if (imageAssociationCorrect)
            {
                // Force the image to recover from storage before its data is overwritten.
                // This will reset mAssociatedImages[level] to NULL too.
                mAssociatedImages[key]->recoverFromAssociatedStorage();
            }
        }
    }
}

ID3D11Resource *TextureStorage11_2DArray::getResource() const
{
    return mTexture;
}

ID3D11ShaderResourceView *TextureStorage11_2DArray::createSRV(int baseLevel, int mipLevels, DXGI_FORMAT format, ID3D11Resource *texture)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = mTopLevel + baseLevel;
    srvDesc.Texture2DArray.MipLevels = mipLevels;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = mTextureDepth;

    ID3D11ShaderResourceView *SRV = NULL;

    ID3D11Device *device = mRenderer->getDevice();
    HRESULT result = device->CreateShaderResourceView(texture, &srvDesc, &SRV);

    if (result == E_OUTOFMEMORY)
    {
        gl::error(GL_OUT_OF_MEMORY);
    }
    ASSERT(SUCCEEDED(result));

    return SRV;
}

RenderTarget *TextureStorage11_2DArray::getRenderTarget(const gl::ImageIndex &index)
{
    ASSERT(index.hasLayer());

    int mipLevel = index.mipIndex;
    int layer = index.layerIndex;

    if (mipLevel >= 0 && mipLevel < getLevelCount())
    {
        LevelLayerKey key(mipLevel, layer);
        if (mRenderTargets.find(key) == mRenderTargets.end())
        {
            ID3D11Device *device = mRenderer->getDevice();
            HRESULT result;

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            srvDesc.Format = mShaderResourceFormat;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MostDetailedMip = mTopLevel + mipLevel;
            srvDesc.Texture2DArray.MipLevels = 1;
            srvDesc.Texture2DArray.FirstArraySlice = layer;
            srvDesc.Texture2DArray.ArraySize = 1;

            ID3D11ShaderResourceView *srv;
            result = device->CreateShaderResourceView(mTexture, &srvDesc, &srv);

            if (result == E_OUTOFMEMORY)
            {
                return gl::error(GL_OUT_OF_MEMORY, static_cast<RenderTarget*>(NULL));
            }
            ASSERT(SUCCEEDED(result));

            if (mRenderTargetFormat != DXGI_FORMAT_UNKNOWN)
            {
                D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
                rtvDesc.Format = mRenderTargetFormat;
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice = mTopLevel + mipLevel;
                rtvDesc.Texture2DArray.FirstArraySlice = layer;
                rtvDesc.Texture2DArray.ArraySize = 1;

                ID3D11RenderTargetView *rtv;
                result = device->CreateRenderTargetView(mTexture, &rtvDesc, &rtv);

                if (result == E_OUTOFMEMORY)
                {
                    SafeRelease(srv);
                    return gl::error(GL_OUT_OF_MEMORY, static_cast<RenderTarget*>(NULL));
                }
                ASSERT(SUCCEEDED(result));

                mRenderTargets[key] = new RenderTarget11(mRenderer, rtv, mTexture, srv, getLevelWidth(mipLevel), getLevelHeight(mipLevel), 1);

                // RenderTarget will take ownership of these resources
                SafeRelease(rtv);
                SafeRelease(srv);
            }
            else
            {
                UNREACHABLE();
            }
        }

        return mRenderTargets[key];
    }
    else
    {
        return NULL;
    }
}

ID3D11Resource *TextureStorage11_2DArray::getSwizzleTexture()
{
    if (!mSwizzleTexture)
    {
        ID3D11Device *device = mRenderer->getDevice();

        D3D11_TEXTURE2D_DESC desc;
        desc.Width = mTextureWidth;
        desc.Height = mTextureHeight;
        desc.MipLevels = mMipLevels;
        desc.ArraySize = mTextureDepth;
        desc.Format = mSwizzleTextureFormat;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        HRESULT result = device->CreateTexture2D(&desc, NULL, &mSwizzleTexture);

        if (result == E_OUTOFMEMORY)
        {
            return gl::error(GL_OUT_OF_MEMORY, static_cast<ID3D11Texture2D*>(NULL));
        }
        ASSERT(SUCCEEDED(result));
    }

    return mSwizzleTexture;
}

ID3D11RenderTargetView *TextureStorage11_2DArray::getSwizzleRenderTarget(int mipLevel)
{
    if (mipLevel >= 0 && mipLevel < getLevelCount())
    {
        if (!mSwizzleRenderTargets[mipLevel])
        {
            ID3D11Resource *swizzleTexture = getSwizzleTexture();
            if (!swizzleTexture)
            {
                return NULL;
            }

            ID3D11Device *device = mRenderer->getDevice();

            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            rtvDesc.Format = mSwizzleRenderTargetFormat;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice = mTopLevel + mipLevel;
            rtvDesc.Texture2DArray.FirstArraySlice = 0;
            rtvDesc.Texture2DArray.ArraySize = mTextureDepth;

            HRESULT result = device->CreateRenderTargetView(mSwizzleTexture, &rtvDesc, &mSwizzleRenderTargets[mipLevel]);

            if (result == E_OUTOFMEMORY)
            {
                return gl::error(GL_OUT_OF_MEMORY, static_cast<ID3D11RenderTargetView*>(NULL));
            }
            ASSERT(SUCCEEDED(result));
        }

        return mSwizzleRenderTargets[mipLevel];
    }
    else
    {
        return NULL;
    }
}

}
