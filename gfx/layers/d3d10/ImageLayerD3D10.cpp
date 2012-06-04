/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLayerD3D10.h"
#include "gfxImageSurface.h"
#include "gfxD2DSurface.h"
#include "gfxWindowsSurface.h"
#include "yuv_convert.h"
#include "../d3d9/Nv3DVUtils.h"

#include "gfxWindowsPlatform.h"

namespace mozilla {
namespace layers {
  
static already_AddRefed<ID3D10Texture2D>
DataToTexture(ID3D10Device *aDevice,
              unsigned char *data,
              int stride,
              const gfxIntSize &aSize)
{
  D3D10_SUBRESOURCE_DATA srdata;
  
  CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM,
                             aSize.width,
                             aSize.height,
                             1, 1);
  desc.Usage = D3D10_USAGE_IMMUTABLE;
  
  srdata.pSysMem = data;
  srdata.SysMemPitch = stride;

  nsRefPtr<ID3D10Texture2D> texture;
  HRESULT hr = aDevice->CreateTexture2D(&desc, &srdata, getter_AddRefs(texture));

  if (FAILED(hr)) {
    LayerManagerD3D10::ReportFailure(NS_LITERAL_CSTRING("Failed to create texture for data"),
                                     hr);
  }

  return texture.forget();
}

static already_AddRefed<ID3D10Texture2D>
SurfaceToTexture(ID3D10Device *aDevice,
                 gfxASurface *aSurface,
                 const gfxIntSize &aSize)
{
  if (!aSurface) {
    return NULL;
  }

  if (aSurface->GetType() == gfxASurface::SurfaceTypeD2D) {
    void *data = aSurface->GetData(&gKeyD3D10Texture);
    if (data) {
      nsRefPtr<ID3D10Texture2D> texture = static_cast<ID3D10Texture2D*>(data);
      ID3D10Device *dev;
      texture->GetDevice(&dev);
      if (dev == aDevice) {
        return texture.forget();
      }
    }
  }

  nsRefPtr<gfxImageSurface> imageSurface = aSurface->GetAsImageSurface();

  if (!imageSurface) {
    imageSurface = new gfxImageSurface(aSize,
                                       gfxASurface::ImageFormatARGB32);

    nsRefPtr<gfxContext> context = new gfxContext(imageSurface);
    context->SetSource(aSurface);
    context->SetOperator(gfxContext::OPERATOR_SOURCE);
    context->Paint();
  }

  return DataToTexture(aDevice, imageSurface->Data(), imageSurface->Stride(), aSize);
}

Layer*
ImageLayerD3D10::GetLayer()
{
  return this;
}

/**
 * Returns a shader resource view for a Cairo or remote image.
 * Returns nsnull if unsuccessful.
 * If successful, aHasAlpha will be true iff the resulting texture 
 * has an alpha component.
 */
ID3D10ShaderResourceView*
ImageLayerD3D10::GetImageSRView(Image* aImage, bool& aHasAlpha, IDXGIKeyedMutex **aMutex)
{
  NS_ASSERTION(aImage, "Null image.");

  if (aImage->GetFormat() == Image::REMOTE_IMAGE_BITMAP) {
    RemoteBitmapImage *remoteImage =
      static_cast<RemoteBitmapImage*>(aImage);
      
    if (!aImage->GetBackendData(LayerManager::LAYERS_D3D10)) {
      nsAutoPtr<TextureD3D10BackendData> dat(new TextureD3D10BackendData());
      dat->mTexture = DataToTexture(device(), remoteImage->mData, remoteImage->mStride, remoteImage->mSize);

      if (dat->mTexture) {
        device()->CreateShaderResourceView(dat->mTexture, NULL, getter_AddRefs(dat->mSRView));
        aImage->SetBackendData(LayerManager::LAYERS_D3D10, dat.forget());
      }
    }

    aHasAlpha = remoteImage->mFormat == RemoteImageData::BGRA32;
  } else if (aImage->GetFormat() == Image::REMOTE_IMAGE_DXGI_TEXTURE) {
    RemoteDXGITextureImage *remoteImage =
      static_cast<RemoteDXGITextureImage*>(aImage);

    remoteImage->GetD3D10TextureBackendData(device());

    aHasAlpha = remoteImage->mFormat == RemoteImageData::BGRA32;
  } else if (aImage->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImage *cairoImage =
      static_cast<CairoImage*>(aImage);

    if (!cairoImage->mSurface) {
      return nsnull;
    }

    if (!aImage->GetBackendData(LayerManager::LAYERS_D3D10)) {
      nsAutoPtr<TextureD3D10BackendData> dat(new TextureD3D10BackendData());
      dat->mTexture = SurfaceToTexture(device(), cairoImage->mSurface, cairoImage->mSize);

      if (dat->mTexture) {
        device()->CreateShaderResourceView(dat->mTexture, NULL, getter_AddRefs(dat->mSRView));
        aImage->SetBackendData(LayerManager::LAYERS_D3D10, dat.forget());
      }
    }

    aHasAlpha = cairoImage->mSurface->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA;
  } else {
    NS_WARNING("Incorrect image type.");
    return nsnull;
  }

  TextureD3D10BackendData *data =
    static_cast<TextureD3D10BackendData*>(aImage->GetBackendData(LayerManager::LAYERS_D3D10));

  if (!data) {
    return nsnull;
  }

  if (aMutex &&
      SUCCEEDED(data->mTexture->QueryInterface(IID_IDXGIKeyedMutex, (void**)aMutex))) {
    if (FAILED((*aMutex)->AcquireSync(0, 0))) {
      NS_WARNING("Failed to acquire sync on keyed mutex, plugin forgot to release?");
      return nsnull;
    }
  }

  nsRefPtr<ID3D10Device> dev;
  data->mTexture->GetDevice(getter_AddRefs(dev));
  if (dev != device()) {
    return nsnull;
  }

  return data->mSRView;
}

void
ImageLayerD3D10::RenderLayer()
{
  ImageContainer *container = GetContainer();
  if (!container) {
    return;
  }

  AutoLockImage autoLock(container);

  Image *image = autoLock.GetImage();
  if (!image) {
    return;
  }

  gfxIntSize size = mScaleMode == SCALE_NONE ? image->GetSize() : mScaleToSize;

  SetEffectTransformAndOpacity();

  ID3D10EffectTechnique *technique;
  nsRefPtr<IDXGIKeyedMutex> keyedMutex;

  if (image->GetFormat() == Image::CAIRO_SURFACE || image->GetFormat() == Image::REMOTE_IMAGE_BITMAP ||
      image->GetFormat() == Image::REMOTE_IMAGE_DXGI_TEXTURE)
  {
    bool hasAlpha = false;

    nsRefPtr<ID3D10ShaderResourceView> srView = GetImageSRView(image, hasAlpha, getter_AddRefs(keyedMutex));
    if (!srView) {
      return;
    }
    
    PRUint8 shaderFlags = SHADER_PREMUL;
    shaderFlags |= LoadMaskTexture();
    shaderFlags |= hasAlpha
                  ? SHADER_RGBA : SHADER_RGB;
    shaderFlags |= mFilter == gfxPattern::FILTER_NEAREST
                  ? SHADER_POINT : SHADER_LINEAR;
    technique = SelectShader(shaderFlags);


    effect()->GetVariableByName("tRGB")->AsShaderResource()->SetResource(srView);

    effect()->GetVariableByName("vLayerQuad")->AsVector()->SetFloatVector(
      ShaderConstantRectD3D10(
        (float)0,
        (float)0,
        (float)size.width,
        (float)size.height)
      );
  } else if (image->GetFormat() == Image::PLANAR_YCBCR) {
    PlanarYCbCrImage *yuvImage =
      static_cast<PlanarYCbCrImage*>(image);

    if (!yuvImage->mBufferSize) {
      return;
    }

    if (!yuvImage->GetBackendData(LayerManager::LAYERS_D3D10)) {
      AllocateTexturesYCbCr(yuvImage);
    }

    PlanarYCbCrD3D10BackendData *data =
      static_cast<PlanarYCbCrD3D10BackendData*>(yuvImage->GetBackendData(LayerManager::LAYERS_D3D10));

    if (!data) {
      return;
    }

    nsRefPtr<ID3D10Device> dev;
    data->mYTexture->GetDevice(getter_AddRefs(dev));
    if (dev != device()) {
      return;
    }

    // TODO: At some point we should try to deal with mFilter here, you don't
    // really want to use point filtering in the case of NEAREST, since that
    // would also use point filtering for Chroma upsampling. Where most likely
    // the user would only want point filtering for final RGB image upsampling.

    technique = SelectShader(SHADER_YCBCR | LoadMaskTexture());

    effect()->GetVariableByName("tY")->AsShaderResource()->SetResource(data->mYView);
    effect()->GetVariableByName("tCb")->AsShaderResource()->SetResource(data->mCbView);
    effect()->GetVariableByName("tCr")->AsShaderResource()->SetResource(data->mCrView);

    /*
     * Send 3d control data and metadata to NV3DVUtils
     */
    if (GetNv3DVUtils()) {
      Nv_Stereo_Mode mode;
      switch (yuvImage->mData.mStereoMode) {
      case STEREO_MODE_LEFT_RIGHT:
        mode = NV_STEREO_MODE_LEFT_RIGHT;
        break;
      case STEREO_MODE_RIGHT_LEFT:
        mode = NV_STEREO_MODE_RIGHT_LEFT;
        break;
      case STEREO_MODE_BOTTOM_TOP:
        mode = NV_STEREO_MODE_BOTTOM_TOP;
        break;
      case STEREO_MODE_TOP_BOTTOM:
        mode = NV_STEREO_MODE_TOP_BOTTOM;
        break;
      case STEREO_MODE_MONO:
        mode = NV_STEREO_MODE_MONO;
        break;
      }
      
      // Send control data even in mono case so driver knows to leave stereo mode.
      GetNv3DVUtils()->SendNv3DVControl(mode, true, FIREFOX_3DV_APP_HANDLE);

      if (yuvImage->mData.mStereoMode != STEREO_MODE_MONO) {
        // Dst resource is optional
        GetNv3DVUtils()->SendNv3DVMetaData((unsigned int)yuvImage->mData.mYSize.width, 
                                           (unsigned int)yuvImage->mData.mYSize.height, (HANDLE)(data->mYTexture), (HANDLE)(NULL));
      }
    }

    effect()->GetVariableByName("vLayerQuad")->AsVector()->SetFloatVector(
      ShaderConstantRectD3D10(
        (float)0,
        (float)0,
        (float)size.width,
        (float)size.height)
      );

    effect()->GetVariableByName("vTextureCoords")->AsVector()->SetFloatVector(
      ShaderConstantRectD3D10(
        (float)yuvImage->mData.mPicX / yuvImage->mData.mYSize.width,
        (float)yuvImage->mData.mPicY / yuvImage->mData.mYSize.height,
        (float)yuvImage->mData.mPicSize.width / yuvImage->mData.mYSize.width,
        (float)yuvImage->mData.mPicSize.height / yuvImage->mData.mYSize.height)
       );
  }
  
  bool resetTexCoords = image->GetFormat() == Image::PLANAR_YCBCR;
  image = nsnull;
  autoLock.Unlock();

  technique->GetPassByIndex(0)->Apply(0);
  device()->Draw(4, 0);

  if (keyedMutex) {
    keyedMutex->ReleaseSync(0);
  }

  if (resetTexCoords) {
    effect()->GetVariableByName("vTextureCoords")->AsVector()->
      SetFloatVector(ShaderConstantRectD3D10(0, 0, 1.0f, 1.0f));
  }

  GetContainer()->NotifyPaintedImage(image);
}

void ImageLayerD3D10::AllocateTexturesYCbCr(PlanarYCbCrImage *aImage)
{
  nsAutoPtr<PlanarYCbCrD3D10BackendData> backendData(
    new PlanarYCbCrD3D10BackendData);

  PlanarYCbCrImage::Data &data = aImage->mData;

  D3D10_SUBRESOURCE_DATA dataY;
  D3D10_SUBRESOURCE_DATA dataCb;
  D3D10_SUBRESOURCE_DATA dataCr;
  CD3D10_TEXTURE2D_DESC descY(DXGI_FORMAT_R8_UNORM,
                              data.mYSize.width,
                              data.mYSize.height, 1, 1);
  CD3D10_TEXTURE2D_DESC descCbCr(DXGI_FORMAT_R8_UNORM,
                                 data.mCbCrSize.width,
                                 data.mCbCrSize.height, 1, 1);

  descY.Usage = descCbCr.Usage = D3D10_USAGE_IMMUTABLE;

  dataY.pSysMem = data.mYChannel;
  dataY.SysMemPitch = data.mYStride;
  dataCb.pSysMem = data.mCbChannel;
  dataCb.SysMemPitch = data.mCbCrStride;
  dataCr.pSysMem = data.mCrChannel;
  dataCr.SysMemPitch = data.mCbCrStride;

  HRESULT hr = device()->CreateTexture2D(&descY, &dataY, getter_AddRefs(backendData->mYTexture));
  if (!FAILED(hr)) {
      hr = device()->CreateTexture2D(&descCbCr, &dataCb, getter_AddRefs(backendData->mCbTexture));
  }
  if (!FAILED(hr)) {
      hr = device()->CreateTexture2D(&descCbCr, &dataCr, getter_AddRefs(backendData->mCrTexture));
  }
  if (FAILED(hr)) {
    LayerManagerD3D10::ReportFailure(NS_LITERAL_CSTRING("PlanarYCbCrImageD3D10::AllocateTextures(): Failed to create texture"),
                                     hr);
    return;
  }
  device()->CreateShaderResourceView(backendData->mYTexture, NULL, getter_AddRefs(backendData->mYView));
  device()->CreateShaderResourceView(backendData->mCbTexture, NULL, getter_AddRefs(backendData->mCbView));
  device()->CreateShaderResourceView(backendData->mCrTexture, NULL, getter_AddRefs(backendData->mCrView));

  aImage->SetBackendData(LayerManager::LAYERS_D3D10, backendData.forget());
}

already_AddRefed<ID3D10ShaderResourceView>
ImageLayerD3D10::GetAsTexture(gfxIntSize* aSize)
{
  if (!GetContainer()) {
    return nsnull;
  }

  AutoLockImage autoLock(GetContainer());

  Image *image = autoLock.GetImage();
  if (!image) {
    return nsnull;
  }

  if (image->GetFormat() != Image::CAIRO_SURFACE) {
    return nsnull;
  }
  
  *aSize = image->GetSize();
  bool dontCare;
  nsRefPtr<ID3D10ShaderResourceView> result = GetImageSRView(image, dontCare);
  return result.forget();
}

already_AddRefed<gfxASurface>
RemoteDXGITextureImage::GetAsSurface()
{
  nsRefPtr<ID3D10Device1> device =
    gfxWindowsPlatform::GetPlatform()->GetD3D10Device();
  if (!device) {
    NS_WARNING("Cannot readback from shared texture because no D3D10 device is available.");
    return nsnull;
  }

  TextureD3D10BackendData* data = GetD3D10TextureBackendData(device);

  if (!data) {
    return nsnull;
  }

  nsRefPtr<IDXGIKeyedMutex> keyedMutex;

  if (FAILED(data->mTexture->QueryInterface(IID_IDXGIKeyedMutex, getter_AddRefs(keyedMutex)))) {
    NS_WARNING("Failed to QueryInterface for IDXGIKeyedMutex, strange.");
    return nsnull;
  }

  if (FAILED(keyedMutex->AcquireSync(0, 0))) {
    NS_WARNING("Failed to acquire sync for keyedMutex, plugin failed to release?");
    return nsnull;
  }

  D3D10_TEXTURE2D_DESC desc;

  data->mTexture->GetDesc(&desc);

  desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
  desc.BindFlags = 0;
  desc.MiscFlags = 0;
  desc.Usage = D3D10_USAGE_STAGING;

  nsRefPtr<ID3D10Texture2D> softTexture;
  HRESULT hr = device->CreateTexture2D(&desc, NULL, getter_AddRefs(softTexture));

  if (FAILED(hr)) {
    NS_WARNING("Failed to create 2D staging texture.");
    return nsnull;
  }

  device->CopyResource(softTexture, data->mTexture);
  keyedMutex->ReleaseSync(0);

  nsRefPtr<gfxImageSurface> surface =
    new gfxImageSurface(mSize, mFormat == RemoteImageData::BGRX32 ?
                                          gfxASurface::ImageFormatRGB24 :
                                          gfxASurface::ImageFormatARGB32);

  if (!surface->CairoSurface() || surface->CairoStatus()) {
    NS_WARNING("Failed to created image surface for DXGI texture.");
    return nsnull;
  }

  D3D10_MAPPED_TEXTURE2D mapped;
  softTexture->Map(0, D3D10_MAP_READ, 0, &mapped);

  for (int y = 0; y < mSize.height; y++) {
    memcpy(surface->Data() + surface->Stride() * y,
           (unsigned char*)(mapped.pData) + mapped.RowPitch * y,
           mSize.width * 4);
  }

  softTexture->Unmap(0);

  return surface.forget();
}

TextureD3D10BackendData*
RemoteDXGITextureImage::GetD3D10TextureBackendData(ID3D10Device *aDevice)
{
  if (GetBackendData(LayerManager::LAYERS_D3D10)) {
    TextureD3D10BackendData *data =
      static_cast<TextureD3D10BackendData*>(GetBackendData(LayerManager::LAYERS_D3D10));
    
    nsRefPtr<ID3D10Device> device;
    data->mTexture->GetDevice(getter_AddRefs(device));

    if (device == aDevice) {
      return data;
    }
  }
  nsRefPtr<ID3D10Texture2D> texture;
  aDevice->OpenSharedResource(mHandle, __uuidof(ID3D10Texture2D), getter_AddRefs(texture));

  if (!texture) {
    NS_WARNING("Failed to get texture for shared texture handle.");
    return nsnull;
  }

  nsAutoPtr<TextureD3D10BackendData> data(new TextureD3D10BackendData());

  data->mTexture = texture;

  aDevice->CreateShaderResourceView(texture, NULL, getter_AddRefs(data->mSRView));

  SetBackendData(LayerManager::LAYERS_D3D10, data);

  return data.forget();
}

} /* layers */
} /* mozilla */
