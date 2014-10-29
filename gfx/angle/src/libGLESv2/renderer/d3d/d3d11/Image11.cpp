//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Image11.h: Implements the rx::Image11 class, which acts as the interface to
// the actual underlying resources of a Texture

#include "libGLESv2/renderer/d3d/d3d11/Renderer11.h"
#include "libGLESv2/renderer/d3d/d3d11/Image11.h"
#include "libGLESv2/renderer/d3d/d3d11/RenderTarget11.h"
#include "libGLESv2/renderer/d3d/d3d11/TextureStorage11.h"
#include "libGLESv2/renderer/d3d/d3d11/formatutils11.h"
#include "libGLESv2/renderer/d3d/d3d11/renderer11_utils.h"
#include "libGLESv2/Framebuffer.h"
#include "libGLESv2/FramebufferAttachment.h"
#include "libGLESv2/main.h"

#include "common/utilities.h"

namespace rx
{

Image11::Image11()
    : mRenderer(NULL),
      mDXGIFormat(DXGI_FORMAT_UNKNOWN),
      mStagingTexture(NULL),
      mStagingSubresource(0),
      mRecoverFromStorage(false),
      mAssociatedStorage(NULL),
      mAssociatedImageIndex(gl::ImageIndex::MakeInvalid()),
      mRecoveredFromStorageCount(0)

{
}

Image11::~Image11()
{
    disassociateStorage();
    releaseStagingTexture();
}

Image11 *Image11::makeImage11(Image *img)
{
    ASSERT(HAS_DYNAMIC_TYPE(rx::Image11*, img));
    return static_cast<rx::Image11*>(img);
}

void Image11::generateMipmap(Image11 *dest, Image11 *src)
{
    ASSERT(src->getDXGIFormat() == dest->getDXGIFormat());
    ASSERT(src->getWidth() == 1 || src->getWidth() / 2 == dest->getWidth());
    ASSERT(src->getHeight() == 1 || src->getHeight() / 2 == dest->getHeight());

    const d3d11::DXGIFormat &dxgiFormatInfo = d3d11::GetDXGIFormatInfo(src->getDXGIFormat());
    ASSERT(dxgiFormatInfo.mipGenerationFunction != NULL);

    D3D11_MAPPED_SUBRESOURCE destMapped;
    HRESULT destMapResult = dest->map(D3D11_MAP_WRITE, &destMapped);
    if (FAILED(destMapResult))
    {
        ERR("Failed to map destination image for mip map generation. HRESULT:0x%X", destMapResult);
        return;
    }

    D3D11_MAPPED_SUBRESOURCE srcMapped;
    HRESULT srcMapResult = src->map(D3D11_MAP_READ, &srcMapped);
    if (FAILED(srcMapResult))
    {
        ERR("Failed to map source image for mip map generation. HRESULT:0x%X", srcMapResult);

        dest->unmap();
        return;
    }

    const uint8_t *sourceData = reinterpret_cast<const uint8_t*>(srcMapped.pData);
    uint8_t *destData = reinterpret_cast<uint8_t*>(destMapped.pData);

    dxgiFormatInfo.mipGenerationFunction(src->getWidth(), src->getHeight(), src->getDepth(),
                                         sourceData, srcMapped.RowPitch, srcMapped.DepthPitch,
                                         destData, destMapped.RowPitch, destMapped.DepthPitch);

    dest->unmap();
    src->unmap();

    dest->markDirty();
}

bool Image11::isDirty() const
{
    // If mDirty is true
    // AND mStagingTexture doesn't exist AND mStagingTexture doesn't need to be recovered from TextureStorage
    // AND the texture doesn't require init data (i.e. a blank new texture will suffice)
    // then isDirty should still return false.
    if (mDirty && !mStagingTexture && !mRecoverFromStorage && !(d3d11::GetTextureFormatInfo(mInternalFormat).dataInitializerFunction != NULL))
    {
        return false;
    }

    return mDirty;
}

gl::Error Image11::copyToStorage2D(TextureStorage *storage, const gl::ImageIndex &index, const gl::Box &region)
{
    TextureStorage11 *storage11 = TextureStorage11::makeTextureStorage11(storage);
    return copyToStorageImpl(storage11, index, region);
}

gl::Error Image11::copyToStorageCube(TextureStorage *storage, const gl::ImageIndex &index, const gl::Box &region)
{
    TextureStorage11 *storage11 = TextureStorage11::makeTextureStorage11(storage);
    return copyToStorageImpl(storage11, index, region);
}

gl::Error Image11::copyToStorage3D(TextureStorage *storage, const gl::ImageIndex &index, const gl::Box &region)
{
    TextureStorage11 *storage11 = TextureStorage11::makeTextureStorage11(storage);
    return copyToStorageImpl(storage11, index, region);
}

gl::Error Image11::copyToStorage2DArray(TextureStorage *storage, const gl::ImageIndex &index, const gl::Box &region)
{
    TextureStorage11 *storage11 = TextureStorage11::makeTextureStorage11(storage);
    return copyToStorageImpl(storage11, index, region);
}

gl::Error Image11::copyToStorageImpl(TextureStorage11 *storage11, const gl::ImageIndex &index, const gl::Box &region)
{
    // If an app's behavior results in an Image11 copying its data to/from to a TextureStorage multiple times,
    // then we should just keep the staging texture around to prevent the copying from impacting perf.
    // We allow the Image11 to copy its data to/from TextureStorage once.
    // This accounts for an app making a late call to glGenerateMipmap.
    bool attemptToReleaseStagingTexture = (mRecoveredFromStorageCount < 2);

    if (attemptToReleaseStagingTexture)
    {
        // If another image is relying on this Storage for its data, then we must let it recover its data before we overwrite it.
        storage11->releaseAssociatedImage(index, this);
    }

    gl::Error error = storage11->updateSubresourceLevel(getStagingTexture(), getStagingSubresource(),
                                                        index, region);
    if (error.isError())
    {
        return error;
    }

    // Once the image data has been copied into the Storage, we can release it locally.
    if (attemptToReleaseStagingTexture)
    {
        storage11->associateImage(this, index);
        releaseStagingTexture();
        mRecoverFromStorage = true;
        mAssociatedStorage = storage11;
        mAssociatedImageIndex = index;
    }

    return gl::Error(GL_NO_ERROR);
}

bool Image11::isAssociatedStorageValid(TextureStorage11* textureStorage) const
{
    return (mAssociatedStorage == textureStorage);
}

bool Image11::recoverFromAssociatedStorage()
{
    if (mRecoverFromStorage)
    {
        createStagingTexture();

        bool textureStorageCorrect = mAssociatedStorage->isAssociatedImageValid(mAssociatedImageIndex, this);

        // This means that the cached TextureStorage has been modified after this Image11 released its copy of its data. 
        // This should not have happened. The TextureStorage should have told this Image11 to recover its data before it was overwritten.
        ASSERT(textureStorageCorrect);

        if (textureStorageCorrect)
        {
            // CopySubResource from the Storage to the Staging texture
            gl::Box region(0, 0, 0, mWidth, mHeight, mDepth);
            mAssociatedStorage->copySubresourceLevel(mStagingTexture, mStagingSubresource, mAssociatedImageIndex, region);
            mRecoveredFromStorageCount += 1;
        }

        // Reset all the recovery parameters, even if the texture storage association is broken.
        disassociateStorage();

        return textureStorageCorrect;
    }

    return false;
}

void Image11::disassociateStorage()
{
    if (mRecoverFromStorage)
    {
        // Make the texturestorage release the Image11 too
        mAssociatedStorage->disassociateImage(mAssociatedImageIndex, this);

        mRecoverFromStorage = false;
        mAssociatedStorage = NULL;
        mAssociatedImageIndex = gl::ImageIndex::MakeInvalid();
    }
}

bool Image11::redefine(Renderer *renderer, GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, bool forceRelease)
{
    if (mWidth != width ||
        mHeight != height ||
        mInternalFormat != internalformat ||
        forceRelease)
    {
        // End the association with the TextureStorage, since that data will be out of date.
        // Also reset mRecoveredFromStorageCount since this Image is getting completely redefined.
        disassociateStorage();
        mRecoveredFromStorageCount = 0;

        mRenderer = Renderer11::makeRenderer11(renderer);

        mWidth = width;
        mHeight = height;
        mDepth = depth;
        mInternalFormat = internalformat;
        mTarget = target;

        // compute the d3d format that will be used
        const d3d11::TextureFormat &formatInfo = d3d11::GetTextureFormatInfo(internalformat);
        const d3d11::DXGIFormat &dxgiFormatInfo = d3d11::GetDXGIFormatInfo(formatInfo.texFormat);
        mDXGIFormat = formatInfo.texFormat;
        mActualFormat = dxgiFormatInfo.internalFormat;
        mRenderable = (formatInfo.rtvFormat != DXGI_FORMAT_UNKNOWN);

        SafeRelease(mStagingTexture);
        mDirty = (formatInfo.dataInitializerFunction != NULL);

        return true;
    }

    return false;
}

DXGI_FORMAT Image11::getDXGIFormat() const
{
    // this should only happen if the image hasn't been redefined first
    // which would be a bug by the caller
    ASSERT(mDXGIFormat != DXGI_FORMAT_UNKNOWN);

    return mDXGIFormat;
}

// Store the pixel rectangle designated by xoffset,yoffset,width,height with pixels stored as format/type at input
// into the target pixel rectangle.
gl::Error Image11::loadData(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                            GLint unpackAlignment, GLenum type, const void *input)
{
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(mInternalFormat);
    GLsizei inputRowPitch = formatInfo.computeRowPitch(type, width, unpackAlignment);
    GLsizei inputDepthPitch = formatInfo.computeDepthPitch(type, width, height, unpackAlignment);

    const d3d11::DXGIFormat &dxgiFormatInfo = d3d11::GetDXGIFormatInfo(mDXGIFormat);
    GLuint outputPixelSize = dxgiFormatInfo.pixelBytes;

    const d3d11::TextureFormat &d3dFormatInfo = d3d11::GetTextureFormatInfo(mInternalFormat);
    LoadImageFunction loadFunction = d3dFormatInfo.loadFunctions.at(type);

    D3D11_MAPPED_SUBRESOURCE mappedImage;
    HRESULT result = map(D3D11_MAP_WRITE, &mappedImage);
    if (FAILED(result))
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Could not map internal image for loading texture data, result: 0x%X.", result);
    }

    uint8_t* offsetMappedData = (reinterpret_cast<uint8_t*>(mappedImage.pData) + (yoffset * mappedImage.RowPitch + xoffset * outputPixelSize + zoffset * mappedImage.DepthPitch));
    loadFunction(width, height, depth,
                 reinterpret_cast<const uint8_t*>(input), inputRowPitch, inputDepthPitch,
                 offsetMappedData, mappedImage.RowPitch, mappedImage.DepthPitch);

    unmap();

    return gl::Error(GL_NO_ERROR);
}

gl::Error Image11::loadCompressedData(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                      const void *input)
{
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(mInternalFormat);
    GLsizei inputRowPitch = formatInfo.computeRowPitch(GL_UNSIGNED_BYTE, width, 1);
    GLsizei inputDepthPitch = formatInfo.computeDepthPitch(GL_UNSIGNED_BYTE, width, height, 1);

    const d3d11::DXGIFormat &dxgiFormatInfo = d3d11::GetDXGIFormatInfo(mDXGIFormat);
    GLuint outputPixelSize = dxgiFormatInfo.pixelBytes;
    GLuint outputBlockWidth = dxgiFormatInfo.blockWidth;
    GLuint outputBlockHeight = dxgiFormatInfo.blockHeight;

    ASSERT(xoffset % outputBlockWidth == 0);
    ASSERT(yoffset % outputBlockHeight == 0);

    const d3d11::TextureFormat &d3dFormatInfo = d3d11::GetTextureFormatInfo(mInternalFormat);
    LoadImageFunction loadFunction = d3dFormatInfo.loadFunctions.at(GL_UNSIGNED_BYTE);

    D3D11_MAPPED_SUBRESOURCE mappedImage;
    HRESULT result = map(D3D11_MAP_WRITE, &mappedImage);
    if (FAILED(result))
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Could not map internal image for loading texture data, result: 0x%X.", result);
    }

    uint8_t* offsetMappedData = reinterpret_cast<uint8_t*>(mappedImage.pData) + ((yoffset / outputBlockHeight) * mappedImage.RowPitch +
                                                                           (xoffset / outputBlockWidth) * outputPixelSize +
                                                                           zoffset * mappedImage.DepthPitch);

    loadFunction(width, height, depth,
                 reinterpret_cast<const uint8_t*>(input), inputRowPitch, inputDepthPitch,
                 offsetMappedData, mappedImage.RowPitch, mappedImage.DepthPitch);

    unmap();

    return gl::Error(GL_NO_ERROR);
}

void Image11::copy(GLint xoffset, GLint yoffset, GLint zoffset, const gl::Rectangle &sourceArea, RenderTarget *source)
{
    RenderTarget11 *sourceRenderTarget = RenderTarget11::makeRenderTarget11(source);
    ASSERT(sourceRenderTarget->getTexture());

    UINT subresourceIndex = sourceRenderTarget->getSubresourceIndex();
    ID3D11Texture2D *sourceTexture2D = d3d11::DynamicCastComObject<ID3D11Texture2D>(sourceRenderTarget->getTexture());

    if (!sourceTexture2D)
    {
        // Error already generated
        return;
    }

    copy(xoffset, yoffset, zoffset, sourceArea, sourceTexture2D, subresourceIndex);

    SafeRelease(sourceTexture2D);
}

void Image11::copy(GLint xoffset, GLint yoffset, GLint zoffset, const gl::Rectangle &sourceArea, const gl::ImageIndex &sourceIndex, TextureStorage *source)
{
    TextureStorage11 *sourceStorage11 = TextureStorage11::makeTextureStorage11(source);

    UINT subresourceIndex = sourceStorage11->getSubresourceIndex(sourceIndex);
    ID3D11Texture2D *sourceTexture2D = d3d11::DynamicCastComObject<ID3D11Texture2D>(sourceStorage11->getResource());

    if (!sourceTexture2D)
    {
        // Error already generated
        return;
    }

    copy(xoffset, yoffset, zoffset, sourceArea, sourceTexture2D, subresourceIndex);

    SafeRelease(sourceTexture2D);
}

void Image11::copy(GLint xoffset, GLint yoffset, GLint zoffset, const gl::Rectangle &sourceArea, ID3D11Texture2D *source, UINT sourceSubResource)
{
    D3D11_TEXTURE2D_DESC textureDesc;
    source->GetDesc(&textureDesc);

    if (textureDesc.Format == mDXGIFormat)
    {
        // No conversion needed-- use copyback fastpath
        ID3D11Device *device = mRenderer->getDevice();
        ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

        UINT subresourceAfterResolve = sourceSubResource;

        ID3D11Texture2D* srcTex = NULL;
        if (textureDesc.SampleDesc.Count > 1)
        {
            D3D11_TEXTURE2D_DESC resolveDesc;
            resolveDesc.Width = textureDesc.Width;
            resolveDesc.Height = textureDesc.Height;
            resolveDesc.MipLevels = 1;
            resolveDesc.ArraySize = 1;
            resolveDesc.Format = textureDesc.Format;
            resolveDesc.SampleDesc.Count = 1;
            resolveDesc.SampleDesc.Quality = 0;
            resolveDesc.Usage = D3D11_USAGE_DEFAULT;
            resolveDesc.BindFlags = 0;
            resolveDesc.CPUAccessFlags = 0;
            resolveDesc.MiscFlags = 0;

            HRESULT result = device->CreateTexture2D(&resolveDesc, NULL, &srcTex);
            if (FAILED(result))
            {
                ERR("Failed to create resolve texture for Image11::copy, HRESULT: 0x%X.", result);
                return;
            }

            deviceContext->ResolveSubresource(srcTex, 0, source, sourceSubResource, textureDesc.Format);
            subresourceAfterResolve = 0;
        }
        else
        {
            srcTex = source;
        }

        D3D11_BOX srcBox;
        srcBox.left = sourceArea.x;
        srcBox.right = sourceArea.x + sourceArea.width;
        srcBox.top = sourceArea.y;
        srcBox.bottom = sourceArea.y + sourceArea.height;
        srcBox.front = 0;
        srcBox.back = 1;

        deviceContext->CopySubresourceRegion(getStagingTexture(), 0, xoffset, yoffset, zoffset, srcTex, subresourceAfterResolve, &srcBox);

        if (textureDesc.SampleDesc.Count > 1)
        {
            SafeRelease(srcTex);
        }
    }
    else
    {
        // This format requires conversion, so we must copy the texture to staging and manually convert via readPixels
        D3D11_MAPPED_SUBRESOURCE mappedImage;
        HRESULT result = map(D3D11_MAP_WRITE, &mappedImage);
        if (FAILED(result))
        {
            ERR("Failed to map texture for Image11::copy, HRESULT: 0x%X.", result);
            return;
        }

        // determine the offset coordinate into the destination buffer
        GLsizei rowOffset = gl::GetInternalFormatInfo(mActualFormat).pixelBytes * xoffset;
        uint8_t *dataOffset = static_cast<uint8_t*>(mappedImage.pData) + mappedImage.RowPitch * yoffset + rowOffset + zoffset * mappedImage.DepthPitch;

        const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(mInternalFormat);

        mRenderer->readTextureData(source, sourceSubResource, sourceArea, formatInfo.format, formatInfo.type, mappedImage.RowPitch, gl::PixelPackState(), dataOffset);

        unmap();
    }

    mDirty = true;
}

ID3D11Resource *Image11::getStagingTexture()
{
    createStagingTexture();

    return mStagingTexture;
}

void Image11::releaseStagingTexture()
{
    SafeRelease(mStagingTexture);
}

unsigned int Image11::getStagingSubresource()
{
    createStagingTexture();

    return mStagingSubresource;
}

void Image11::createStagingTexture()
{
    if (mStagingTexture)
    {
        return;
    }

    const DXGI_FORMAT dxgiFormat = getDXGIFormat();

    if (mWidth > 0 && mHeight > 0 && mDepth > 0)
    {
        ID3D11Device *device = mRenderer->getDevice();
        HRESULT result;

        int lodOffset = 1;
        GLsizei width = mWidth;
        GLsizei height = mHeight;

        // adjust size if needed for compressed textures
        d3d11::MakeValidSize(false, dxgiFormat, &width, &height, &lodOffset);

        if (mTarget == GL_TEXTURE_3D)
        {
            ID3D11Texture3D *newTexture = NULL;

            D3D11_TEXTURE3D_DESC desc;
            desc.Width = width;
            desc.Height = height;
            desc.Depth = mDepth;
            desc.MipLevels = lodOffset + 1;
            desc.Format = dxgiFormat;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags = 0;

            if (d3d11::GetTextureFormatInfo(mInternalFormat).dataInitializerFunction != NULL)
            {
                std::vector<D3D11_SUBRESOURCE_DATA> initialData;
                std::vector< std::vector<BYTE> > textureData;
                d3d11::GenerateInitialTextureData(mInternalFormat, width, height, mDepth,
                                                  lodOffset + 1, &initialData, &textureData);

                result = device->CreateTexture3D(&desc, initialData.data(), &newTexture);
            }
            else
            {
                result = device->CreateTexture3D(&desc, NULL, &newTexture);
            }

            if (FAILED(result))
            {
                ASSERT(result == E_OUTOFMEMORY);
                ERR("Creating image failed.");
                return gl::error(GL_OUT_OF_MEMORY);
            }

            mStagingTexture = newTexture;
            mStagingSubresource = D3D11CalcSubresource(lodOffset, 0, lodOffset + 1);
        }
        else if (mTarget == GL_TEXTURE_2D || mTarget == GL_TEXTURE_2D_ARRAY || mTarget == GL_TEXTURE_CUBE_MAP)
        {
            ID3D11Texture2D *newTexture = NULL;

            D3D11_TEXTURE2D_DESC desc;
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = lodOffset + 1;
            desc.ArraySize = 1;
            desc.Format = dxgiFormat;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags = 0;

            if (d3d11::GetTextureFormatInfo(mInternalFormat).dataInitializerFunction != NULL)
            {
                std::vector<D3D11_SUBRESOURCE_DATA> initialData;
                std::vector< std::vector<BYTE> > textureData;
                d3d11::GenerateInitialTextureData(mInternalFormat, width, height, 1,
                                                  lodOffset + 1, &initialData, &textureData);

                result = device->CreateTexture2D(&desc, initialData.data(), &newTexture);
            }
            else
            {
                result = device->CreateTexture2D(&desc, NULL, &newTexture);
            }

            if (FAILED(result))
            {
                ASSERT(result == E_OUTOFMEMORY);
                ERR("Creating image failed.");
                return gl::error(GL_OUT_OF_MEMORY);
            }

            mStagingTexture = newTexture;
            mStagingSubresource = D3D11CalcSubresource(lodOffset, 0, lodOffset + 1);
        }
        else
        {
            UNREACHABLE();
        }
    }

    mDirty = false;
}

HRESULT Image11::map(D3D11_MAP mapType, D3D11_MAPPED_SUBRESOURCE *map)
{
    createStagingTexture();

    // We must recover from the TextureStorage if necessary, even for D3D11_MAP_WRITE.
    recoverFromAssociatedStorage();

    HRESULT result = E_FAIL;

    if (mStagingTexture)
    {
        ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();
        result = deviceContext->Map(mStagingTexture, mStagingSubresource, mapType, 0, map);

        // this can fail if the device is removed (from TDR)
        if (d3d11::isDeviceLostError(result))
        {
            mRenderer->notifyDeviceLost();
        }
        else if (SUCCEEDED(result))
        {
            mDirty = true;
        }
    }

    return result;
}

void Image11::unmap()
{
    if (mStagingTexture)
    {
        ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();
        deviceContext->Unmap(mStagingTexture, mStagingSubresource);
    }
}

}
