/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/RefPtr.h"
#include "mozilla/layers/PLayerTransaction.h"
#include "gfxSharedImageSurface.h"

#include "ImageLayerD3D9.h"
#include "PaintedLayerD3D9.h"
#include "gfxPlatform.h"
#include "gfx2DGlue.h"
#include "yuv_convert.h"
#include "nsIServiceManager.h"
#include "nsIConsoleService.h"
#include "Nv3DVUtils.h"
#include "D3D9SurfaceImage.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

static inline _D3DFORMAT
D3dFormatForSurfaceFormat(SurfaceFormat aFormat)
{
  if (aFormat == SurfaceFormat::A8) {
    return D3DFMT_A8;
  }

  return D3DFMT_A8R8G8B8;
}

static already_AddRefed<IDirect3DTexture9>
DataToTexture(IDirect3DDevice9 *aDevice,
              unsigned char *aData,
              int aStride,
              const IntSize &aSize,
              _D3DFORMAT aFormat)
{
  nsRefPtr<IDirect3DTexture9> texture;
  nsRefPtr<IDirect3DDevice9Ex> deviceEx;
  aDevice->QueryInterface(IID_IDirect3DDevice9Ex,
                          (void**)getter_AddRefs(deviceEx));

  nsRefPtr<IDirect3DSurface9> surface;
  D3DLOCKED_RECT lockedRect;
  if (deviceEx) {
    // D3D9Ex doesn't support managed textures. We could use dynamic textures
    // here but since Images are immutable that probably isn't such a great
    // idea.
    if (FAILED(aDevice->
               CreateTexture(aSize.width, aSize.height,
                             1, 0, aFormat, D3DPOOL_DEFAULT,
                             getter_AddRefs(texture), nullptr)))
    {
      return nullptr;
    }

    nsRefPtr<IDirect3DTexture9> tmpTexture;
    if (FAILED(aDevice->
               CreateTexture(aSize.width, aSize.height,
                             1, 0, aFormat, D3DPOOL_SYSTEMMEM,
                             getter_AddRefs(tmpTexture), nullptr)))
    {
      return nullptr;
    }

    tmpTexture->GetSurfaceLevel(0, getter_AddRefs(surface));
    surface->LockRect(&lockedRect, nullptr, 0);
    NS_ASSERTION(lockedRect.pBits, "Could not lock surface");
  } else {
    if (FAILED(aDevice->
               CreateTexture(aSize.width, aSize.height,
                             1, 0, aFormat, D3DPOOL_MANAGED,
                             getter_AddRefs(texture), nullptr)))
    {
      return nullptr;
    }

    /* lock the entire texture */
    texture->LockRect(0, &lockedRect, nullptr, 0);
  }

  uint32_t width = aSize.width;
  if (aFormat == D3DFMT_A8R8G8B8) {
    width *= 4;
  }
  for (int y = 0; y < aSize.height; y++) {
    memcpy((char*)lockedRect.pBits + lockedRect.Pitch * y,
            aData + aStride * y,
            width);
  }

  if (deviceEx) {
    surface->UnlockRect();
    nsRefPtr<IDirect3DSurface9> dstSurface;
    texture->GetSurfaceLevel(0, getter_AddRefs(dstSurface));
    aDevice->UpdateSurface(surface, nullptr, dstSurface, nullptr);
  } else {
    texture->UnlockRect(0);
  }

  return texture.forget();
}

static already_AddRefed<IDirect3DTexture9>
OpenSharedTexture(const D3DSURFACE_DESC& aDesc,
                  HANDLE aShareHandle,
                  IDirect3DDevice9 *aDevice)
{
  MOZ_ASSERT(aDesc.Format == D3DFMT_X8R8G8B8);

  // Open the frame from DXVA's device in our device using the resource
  // sharing handle.
  nsRefPtr<IDirect3DTexture9> sharedTexture;
  HRESULT hr = aDevice->CreateTexture(aDesc.Width,
                                      aDesc.Height,
                                      1,
                                      D3DUSAGE_RENDERTARGET,
                                      D3DFMT_X8R8G8B8,
                                      D3DPOOL_DEFAULT,
                                      getter_AddRefs(sharedTexture),
                                      &aShareHandle);
  if (FAILED(hr)) {
    NS_WARNING("Failed to open shared texture on our device");
  }
  return sharedTexture.forget();
}

static already_AddRefed<IDirect3DTexture9>
SurfaceToTexture(IDirect3DDevice9 *aDevice,
                 SourceSurface *aSurface,
                 const IntSize &aSize)
{
  RefPtr<DataSourceSurface> dataSurface = aSurface->GetDataSurface();
  if (!dataSurface) {
    return nullptr;
  }
  DataSourceSurface::MappedSurface map;
  if (!dataSurface->Map(DataSourceSurface::MapType::READ, &map)) {
    return nullptr;
  }
  nsRefPtr<IDirect3DTexture9> texture =
    DataToTexture(aDevice, map.mData, map.mStride, aSize,
                  D3dFormatForSurfaceFormat(dataSurface->GetFormat()));
  dataSurface->Unmap();
  return texture.forget();
}

static void AllocateTexturesYCbCr(PlanarYCbCrImage *aImage,
                                  IDirect3DDevice9 *aDevice,
                                  LayerManagerD3D9 *aManager)
{
  nsAutoPtr<PlanarYCbCrD3D9BackendData> backendData(
    new PlanarYCbCrD3D9BackendData);

  const PlanarYCbCrData *data = aImage->GetData();

  D3DLOCKED_RECT lockrectY;
  D3DLOCKED_RECT lockrectCb;
  D3DLOCKED_RECT lockrectCr;
  uint8_t* src;
  uint8_t* dest;

  nsRefPtr<IDirect3DSurface9> tmpSurfaceY;
  nsRefPtr<IDirect3DSurface9> tmpSurfaceCb;
  nsRefPtr<IDirect3DSurface9> tmpSurfaceCr;

  nsRefPtr<IDirect3DDevice9Ex> deviceEx;
  aDevice->QueryInterface(IID_IDirect3DDevice9Ex,
                           getter_AddRefs(deviceEx));

  bool isD3D9Ex = deviceEx;

  if (isD3D9Ex) {
    nsRefPtr<IDirect3DTexture9> tmpYTexture;
    nsRefPtr<IDirect3DTexture9> tmpCbTexture;
    nsRefPtr<IDirect3DTexture9> tmpCrTexture;
    // D3D9Ex does not support the managed pool, could use dynamic textures
    // here. But since an Image is immutable static textures are probably a
    // better idea.

    HRESULT hr;
    hr = aDevice->CreateTexture(data->mYSize.width, data->mYSize.height,
                                1, 0, D3DFMT_A8, D3DPOOL_DEFAULT,
                                getter_AddRefs(backendData->mYTexture), nullptr);
    if (!FAILED(hr)) {
      hr = aDevice->CreateTexture(data->mCbCrSize.width, data->mCbCrSize.height,
                                  1, 0, D3DFMT_A8, D3DPOOL_DEFAULT,
                                  getter_AddRefs(backendData->mCbTexture), nullptr);
    }
    if (!FAILED(hr)) {
      hr = aDevice->CreateTexture(data->mCbCrSize.width, data->mCbCrSize.height,
                                  1, 0, D3DFMT_A8, D3DPOOL_DEFAULT,
                                  getter_AddRefs(backendData->mCrTexture), nullptr);
    }
    if (!FAILED(hr)) {
      hr = aDevice->CreateTexture(data->mYSize.width, data->mYSize.height,
                                  1, 0, D3DFMT_A8, D3DPOOL_SYSTEMMEM,
                                  getter_AddRefs(tmpYTexture), nullptr);
    }
    if (!FAILED(hr)) {
      hr = aDevice->CreateTexture(data->mCbCrSize.width, data->mCbCrSize.height,
                                  1, 0, D3DFMT_A8, D3DPOOL_SYSTEMMEM,
                                  getter_AddRefs(tmpCbTexture), nullptr);
    }
    if (!FAILED(hr)) {
      hr = aDevice->CreateTexture(data->mCbCrSize.width, data->mCbCrSize.height,
                                  1, 0, D3DFMT_A8, D3DPOOL_SYSTEMMEM,
                                  getter_AddRefs(tmpCrTexture), nullptr);
    }

    if (FAILED(hr)) {
      aManager->ReportFailure(NS_LITERAL_CSTRING("PlanarYCbCrImageD3D9::AllocateTextures(): Failed to create texture (isD3D9Ex)"),
                              hr);
      return;
    }

    tmpYTexture->GetSurfaceLevel(0, getter_AddRefs(tmpSurfaceY));
    tmpCbTexture->GetSurfaceLevel(0, getter_AddRefs(tmpSurfaceCb));
    tmpCrTexture->GetSurfaceLevel(0, getter_AddRefs(tmpSurfaceCr));
    tmpSurfaceY->LockRect(&lockrectY, nullptr, 0);
    tmpSurfaceCb->LockRect(&lockrectCb, nullptr, 0);
    tmpSurfaceCr->LockRect(&lockrectCr, nullptr, 0);
  } else {
    HRESULT hr;
    hr = aDevice->CreateTexture(data->mYSize.width, data->mYSize.height,
                                1, 0, D3DFMT_A8, D3DPOOL_MANAGED,
                                getter_AddRefs(backendData->mYTexture), nullptr);
    if (!FAILED(hr)) {
      aDevice->CreateTexture(data->mCbCrSize.width, data->mCbCrSize.height,
                             1, 0, D3DFMT_A8, D3DPOOL_MANAGED,
                             getter_AddRefs(backendData->mCbTexture), nullptr);
    }
    if (!FAILED(hr)) {
      aDevice->CreateTexture(data->mCbCrSize.width, data->mCbCrSize.height,
                             1, 0, D3DFMT_A8, D3DPOOL_MANAGED,
                             getter_AddRefs(backendData->mCrTexture), nullptr);
    }

    if (FAILED(hr)) {
      aManager->ReportFailure(NS_LITERAL_CSTRING("PlanarYCbCrImageD3D9::AllocateTextures(): Failed to create texture (!isD3D9Ex)"),
                              hr);
      return;
    }

    /* lock the entire texture */
    backendData->mYTexture->LockRect(0, &lockrectY, nullptr, 0);
    backendData->mCbTexture->LockRect(0, &lockrectCb, nullptr, 0);
    backendData->mCrTexture->LockRect(0, &lockrectCr, nullptr, 0);
  }

  src  = data->mYChannel;
  //FIX cast
  dest = (uint8_t*)lockrectY.pBits;

  // copy over data
  for (int h=0; h<data->mYSize.height; h++) {
    memcpy(dest, src, data->mYSize.width);
    dest += lockrectY.Pitch;
    src += data->mYStride;
  }

  src  = data->mCbChannel;
  //FIX cast
  dest = (uint8_t*)lockrectCb.pBits;

  // copy over data
  for (int h=0; h<data->mCbCrSize.height; h++) {
    memcpy(dest, src, data->mCbCrSize.width);
    dest += lockrectCb.Pitch;
    src += data->mCbCrStride;
  }

  src  = data->mCrChannel;
  //FIX cast
  dest = (uint8_t*)lockrectCr.pBits;

  // copy over data
  for (int h=0; h<data->mCbCrSize.height; h++) {
    memcpy(dest, src, data->mCbCrSize.width);
    dest += lockrectCr.Pitch;
    src += data->mCbCrStride;
  }

  if (isD3D9Ex) {
    tmpSurfaceY->UnlockRect();
    tmpSurfaceCb->UnlockRect();
    tmpSurfaceCr->UnlockRect();
    nsRefPtr<IDirect3DSurface9> dstSurface;
    backendData->mYTexture->GetSurfaceLevel(0, getter_AddRefs(dstSurface));
    aDevice->UpdateSurface(tmpSurfaceY, nullptr, dstSurface, nullptr);
    backendData->mCbTexture->GetSurfaceLevel(0, getter_AddRefs(dstSurface));
    aDevice->UpdateSurface(tmpSurfaceCb, nullptr, dstSurface, nullptr);
    backendData->mCrTexture->GetSurfaceLevel(0, getter_AddRefs(dstSurface));
    aDevice->UpdateSurface(tmpSurfaceCr, nullptr, dstSurface, nullptr);
  } else {
    backendData->mYTexture->UnlockRect(0);
    backendData->mCbTexture->UnlockRect(0);
    backendData->mCrTexture->UnlockRect(0);
  }

  aImage->SetBackendData(mozilla::layers::LayersBackend::LAYERS_D3D9, backendData.forget());
}

Layer*
ImageLayerD3D9::GetLayer()
{
  return this;
}

/*
  * Returns a texture which backs aImage
  * Will only work if aImage is a cairo image.
  * Returns nullptr if unsuccessful.
  * If successful, aHasAlpha will be set to true if the texture has an
  * alpha component, false otherwise.
  */
IDirect3DTexture9*
ImageLayerD3D9::GetTexture(Image *aImage, bool& aHasAlpha)
{
  NS_ASSERTION(aImage, "Null image.");

  if (aImage->GetFormat() == ImageFormat::CAIRO_SURFACE) {
    CairoImage *cairoImage =
      static_cast<CairoImage*>(aImage);

    RefPtr<SourceSurface> surf = cairoImage->GetAsSourceSurface();
    if (!surf) {
      return nullptr;
    }

    if (!aImage->GetBackendData(mozilla::layers::LayersBackend::LAYERS_D3D9)) {
      nsAutoPtr<TextureD3D9BackendData> dat(new TextureD3D9BackendData());
      dat->mTexture = SurfaceToTexture(device(), surf, cairoImage->GetSize());
      if (dat->mTexture) {
        aImage->SetBackendData(mozilla::layers::LayersBackend::LAYERS_D3D9, dat.forget());
      }
    }

    aHasAlpha = surf->GetFormat() == SurfaceFormat::B8G8R8A8;
  } else if (aImage->GetFormat() == ImageFormat::D3D9_RGB32_TEXTURE) {
    if (!aImage->GetBackendData(mozilla::layers::LayersBackend::LAYERS_D3D9)) {
      // The texture in which the frame is stored belongs to DXVA's D3D9 device.
      // We need to open it on our device before we can use it.
      nsAutoPtr<TextureD3D9BackendData> backendData(new TextureD3D9BackendData());
      D3D9SurfaceImage* image = static_cast<D3D9SurfaceImage*>(aImage);
      backendData->mTexture = OpenSharedTexture(image->GetDesc(), image->GetShareHandle(), device());
      if (backendData->mTexture) {
        aImage->SetBackendData(mozilla::layers::LayersBackend::LAYERS_D3D9, backendData.forget());
      }
    }
    aHasAlpha = false;
  } else {
    NS_WARNING("Inappropriate image type.");
    return nullptr;
  }

  TextureD3D9BackendData *data =
    static_cast<TextureD3D9BackendData*>(aImage->GetBackendData(mozilla::layers::LayersBackend::LAYERS_D3D9));

  if (!data) {
    return nullptr;
  }

  nsRefPtr<IDirect3DDevice9> dev;
  data->mTexture->GetDevice(getter_AddRefs(dev));
  if (dev != device()) {
    return nullptr;
  }

  return data->mTexture;
}

void
ImageLayerD3D9::RenderLayer()
{
  ImageContainer *container = GetContainer();
  if (!container || mD3DManager->CompositingDisabled()) {
    return;
  }

  AutoLockImage autoLock(container);

  Image *image = autoLock.GetImage();
  if (!image) {
    return;
  }

  SetShaderTransformAndOpacity();

  gfx::IntSize size = image->GetSize();

  if (image->GetFormat() == ImageFormat::CAIRO_SURFACE ||
      image->GetFormat() == ImageFormat::D3D9_RGB32_TEXTURE)
  {
    NS_ASSERTION(image->GetFormat() != ImageFormat::CAIRO_SURFACE ||
                 !static_cast<CairoImage*>(image)->mSourceSurface ||
                 static_cast<CairoImage*>(image)->mSourceSurface->GetFormat() != SurfaceFormat::A8,
                 "Image layer has alpha image");

    bool hasAlpha = false;
    nsRefPtr<IDirect3DTexture9> texture = GetTexture(image, hasAlpha);

    device()->SetVertexShaderConstantF(CBvLayerQuad,
                                       ShaderConstantRect(0,
                                                          0,
                                                          size.width,
                                                          size.height),
                                       1);

    if (hasAlpha) {
      mD3DManager->SetShaderMode(DeviceManagerD3D9::RGBALAYER, GetMaskLayer());
    } else {
      mD3DManager->SetShaderMode(DeviceManagerD3D9::RGBLAYER, GetMaskLayer());
    }

    if (mFilter == GraphicsFilter::FILTER_NEAREST) {
      device()->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
      device()->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    }
    device()->SetTexture(0, texture);

    image = nullptr;
    autoLock.Unlock();

    device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
    if (mFilter == GraphicsFilter::FILTER_NEAREST) {
      device()->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
      device()->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    }
  } else {
    PlanarYCbCrImage *yuvImage =
      static_cast<PlanarYCbCrImage*>(image);

    if (!yuvImage->IsValid()) {
      return;
    }

    if (!yuvImage->GetBackendData(mozilla::layers::LayersBackend::LAYERS_D3D9)) {
      AllocateTexturesYCbCr(yuvImage, device(), mD3DManager);
    }

    PlanarYCbCrD3D9BackendData *data =
      static_cast<PlanarYCbCrD3D9BackendData*>(yuvImage->GetBackendData(mozilla::layers::LayersBackend::LAYERS_D3D9));

    if (!data) {
      return;
    }

    nsRefPtr<IDirect3DDevice9> dev;
    data->mYTexture->GetDevice(getter_AddRefs(dev));
    if (dev != device()) {
      return;
    }

    device()->SetVertexShaderConstantF(CBvLayerQuad,
                                       ShaderConstantRect(0,
                                                          0,
                                                          size.width,
                                                          size.height),
                                       1);

    device()->SetVertexShaderConstantF(CBvTextureCoords,
      ShaderConstantRect(
        (float)yuvImage->GetData()->mPicX / yuvImage->GetData()->mYSize.width,
        (float)yuvImage->GetData()->mPicY / yuvImage->GetData()->mYSize.height,
        (float)yuvImage->GetData()->mPicSize.width / yuvImage->GetData()->mYSize.width,
        (float)yuvImage->GetData()->mPicSize.height / yuvImage->GetData()->mYSize.height
      ),
      1);

    mD3DManager->SetShaderMode(DeviceManagerD3D9::YCBCRLAYER, GetMaskLayer());

    /*
     * Send 3d control data and metadata
     */
    if (mD3DManager->GetNv3DVUtils()) {
      Nv_Stereo_Mode mode;
      switch (yuvImage->GetData()->mStereoMode) {
      case StereoMode::LEFT_RIGHT:
        mode = NV_STEREO_MODE_LEFT_RIGHT;
        break;
      case StereoMode::RIGHT_LEFT:
        mode = NV_STEREO_MODE_RIGHT_LEFT;
        break;
      case StereoMode::BOTTOM_TOP:
        mode = NV_STEREO_MODE_BOTTOM_TOP;
        break;
      case StereoMode::TOP_BOTTOM:
        mode = NV_STEREO_MODE_TOP_BOTTOM;
        break;
      case StereoMode::MONO:
        mode = NV_STEREO_MODE_MONO;
        break;
      }

      // Send control data even in mono case so driver knows to leave stereo mode.
      mD3DManager->GetNv3DVUtils()->SendNv3DVControl(mode, true, FIREFOX_3DV_APP_HANDLE);

      if (yuvImage->GetData()->mStereoMode != StereoMode::MONO) {
        mD3DManager->GetNv3DVUtils()->SendNv3DVControl(mode, true, FIREFOX_3DV_APP_HANDLE);

        nsRefPtr<IDirect3DSurface9> renderTarget;
        device()->GetRenderTarget(0, getter_AddRefs(renderTarget));
        mD3DManager->GetNv3DVUtils()->SendNv3DVMetaData((unsigned int)yuvImage->GetSize().width,
                                                        (unsigned int)yuvImage->GetSize().height, (HANDLE)(data->mYTexture), (HANDLE)(renderTarget));
      }
    }

    // Linear scaling is default here, adhering to mFilter is difficult since
    // presumably even with point filtering we'll still want chroma upsampling
    // to be linear. In the current approach we can't.
    device()->SetTexture(0, data->mYTexture);
    device()->SetTexture(1, data->mCbTexture);
    device()->SetTexture(2, data->mCrTexture);

    image = nullptr;
    data = nullptr;
    autoLock.Unlock();

    device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

    device()->SetVertexShaderConstantF(CBvTextureCoords,
      ShaderConstantRect(0, 0, 1.0f, 1.0f), 1);
  }

  GetContainer()->NotifyPaintedImage(image);
}

already_AddRefed<IDirect3DTexture9>
ImageLayerD3D9::GetAsTexture(gfx::IntSize* aSize)
{
  if (!GetContainer()) {
    return nullptr;
  }

  AutoLockImage autoLock(GetContainer());

  Image *image = autoLock.GetImage();

  if (!image) {
    return nullptr;
  }

  if (image->GetFormat() != ImageFormat::CAIRO_SURFACE) {
    return nullptr;
  }

  bool dontCare;
  *aSize = image->GetSize();
  nsRefPtr<IDirect3DTexture9> result = GetTexture(image, dontCare);
  return result.forget();
}

} /* layers */
} /* mozilla */
