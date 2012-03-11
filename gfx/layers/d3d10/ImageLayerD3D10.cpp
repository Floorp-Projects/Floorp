/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "ImageLayerD3D10.h"
#include "gfxImageSurface.h"
#include "gfxD2DSurface.h"
#include "gfxWindowsSurface.h"
#include "yuv_convert.h"
#include "../d3d9/Nv3DVUtils.h"

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

  if (image->GetFormat() == Image::CAIRO_SURFACE || image->GetFormat() == Image::REMOTE_IMAGE_BITMAP)
  {
    bool hasAlpha = false;

    if (image->GetFormat() == Image::REMOTE_IMAGE_BITMAP) {
      RemoteBitmapImage *remoteImage =
        static_cast<RemoteBitmapImage*>(image);
      
      if (!image->GetBackendData(LayerManager::LAYERS_D3D10)) {
        nsAutoPtr<TextureD3D10BackendData> dat = new TextureD3D10BackendData();
        dat->mTexture = DataToTexture(device(), remoteImage->mData, remoteImage->mStride, remoteImage->mSize);

        if (dat->mTexture) {
          device()->CreateShaderResourceView(dat->mTexture, NULL, getter_AddRefs(dat->mSRView));
          image->SetBackendData(LayerManager::LAYERS_D3D10, dat.forget());
        }
      }

      hasAlpha = remoteImage->mFormat == RemoteImageData::BGRA32;
    } else {
      CairoImage *cairoImage =
        static_cast<CairoImage*>(image);

      if (!cairoImage->mSurface) {
        return;
      }

      if (!image->GetBackendData(LayerManager::LAYERS_D3D10)) {
        nsAutoPtr<TextureD3D10BackendData> dat = new TextureD3D10BackendData();
        dat->mTexture = SurfaceToTexture(device(), cairoImage->mSurface, cairoImage->mSize);

        if (dat->mTexture) {
          device()->CreateShaderResourceView(dat->mTexture, NULL, getter_AddRefs(dat->mSRView));
          image->SetBackendData(LayerManager::LAYERS_D3D10, dat.forget());
        }
      }

      hasAlpha = cairoImage->mSurface->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA;
    }

    TextureD3D10BackendData *data =
      static_cast<TextureD3D10BackendData*>(image->GetBackendData(LayerManager::LAYERS_D3D10));

    if (!data) {
      return;
    }

    nsRefPtr<ID3D10Device> dev;
    data->mTexture->GetDevice(getter_AddRefs(dev));
    if (dev != device()) {
      return;
    }
    
    if (hasAlpha) {
      if (mFilter == gfxPattern::FILTER_NEAREST) {
        technique = effect()->GetTechniqueByName("RenderRGBALayerPremulPoint");
      } else {
        technique = effect()->GetTechniqueByName("RenderRGBALayerPremul");
      }
    } else {
      if (mFilter == gfxPattern::FILTER_NEAREST) {
        technique = effect()->GetTechniqueByName("RenderRGBLayerPremulPoint");
      } else {
        technique = effect()->GetTechniqueByName("RenderRGBLayerPremul");
      }
    }

    effect()->GetVariableByName("tRGB")->AsShaderResource()->SetResource(data->mSRView);

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

    technique = effect()->GetTechniqueByName("RenderYCbCrLayer");

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
        GetNv3DVUtils()->SendNv3DVMetaData((unsigned int)yuvImage->mSize.width, 
                                           (unsigned int)yuvImage->mSize.height, (HANDLE)(data->mYTexture), (HANDLE)(NULL));
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

} /* layers */
} /* mozilla */
