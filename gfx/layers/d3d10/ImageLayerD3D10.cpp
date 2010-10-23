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
#include "yuv_convert.h"

namespace mozilla {
namespace layers {

using mozilla::MutexAutoLock;

ImageContainerD3D10::ImageContainerD3D10(LayerManagerD3D10 *aManager)
  : ImageContainer(aManager)
  , mActiveImageLock("mozilla.layers.ImageContainerD3D10.mActiveImageLock")
{
}

already_AddRefed<Image>
ImageContainerD3D10::CreateImage(const Image::Format *aFormats,
                               PRUint32 aNumFormats)
{
  if (!aNumFormats) {
    return nsnull;
  }
  nsRefPtr<Image> img;
  if (aFormats[0] == Image::PLANAR_YCBCR) {
    img = new PlanarYCbCrImageD3D10(static_cast<LayerManagerD3D10*>(mManager));
  } else if (aFormats[0] == Image::CAIRO_SURFACE) {
    img = new CairoImageD3D10(static_cast<LayerManagerD3D10*>(mManager));
  }
  return img.forget();
}

void
ImageContainerD3D10::SetCurrentImage(Image *aImage)
{
  MutexAutoLock lock(mActiveImageLock);

  mActiveImage = aImage;
}

already_AddRefed<Image>
ImageContainerD3D10::GetCurrentImage()
{
  MutexAutoLock lock(mActiveImageLock);

  nsRefPtr<Image> retval = mActiveImage;
  return retval.forget();
}

already_AddRefed<gfxASurface>
ImageContainerD3D10::GetCurrentAsSurface(gfxIntSize *aSize)
{
  MutexAutoLock lock(mActiveImageLock);
  if (!mActiveImage) {
    return nsnull;
  }

  if (mActiveImage->GetFormat() == Image::PLANAR_YCBCR) {
    PlanarYCbCrImageD3D10 *yuvImage =
      static_cast<PlanarYCbCrImageD3D10*>(mActiveImage.get());
    if (yuvImage->HasData()) {
      *aSize = yuvImage->mSize;
    }
  } else if (mActiveImage->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImageD3D10 *cairoImage =
      static_cast<CairoImageD3D10*>(mActiveImage.get());
    *aSize = cairoImage->mSize;
  }

  return static_cast<ImageD3D10*>(mActiveImage->GetImplData())->GetAsSurface();
}

gfxIntSize
ImageContainerD3D10::GetCurrentSize()
{
  MutexAutoLock lock(mActiveImageLock);
  if (!mActiveImage) {
    return gfxIntSize(0,0);
  }
  if (mActiveImage->GetFormat() == Image::PLANAR_YCBCR) {
    PlanarYCbCrImageD3D10 *yuvImage =
      static_cast<PlanarYCbCrImageD3D10*>(mActiveImage.get());
    if (!yuvImage->HasData()) {
      return gfxIntSize(0,0);
    }
    return yuvImage->mSize;

  } else if (mActiveImage->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImageD3D10 *cairoImage =
      static_cast<CairoImageD3D10*>(mActiveImage.get());
    return cairoImage->mSize;
  }

  return gfxIntSize(0,0);
}

PRBool
ImageContainerD3D10::SetLayerManager(LayerManager *aManager)
{
  // we can't do anything here for now
  return PR_FALSE;
}

Layer*
ImageLayerD3D10::GetLayer()
{
  return this;
}

void
ImageLayerD3D10::RenderLayer(float aOpacity, const gfx3DMatrix &aTransform)
{
  if (!GetContainer()) {
    return;
  }

  nsRefPtr<Image> image = GetContainer()->GetCurrentImage();


  gfx3DMatrix transform = mTransform * aTransform;
  effect()->GetVariableByName("mLayerTransform")->SetRawValue(&transform._11, 0, 64);
  effect()->GetVariableByName("fLayerOpacity")->AsScalar()->SetFloat(GetOpacity() * aOpacity);

  ID3D10EffectTechnique *technique;

  if (image->GetFormat() == Image::PLANAR_YCBCR) {
    PlanarYCbCrImageD3D10 *yuvImage =
      static_cast<PlanarYCbCrImageD3D10*>(image.get());

    if (!yuvImage->HasData()) {
      return;
    }

    // TODO: At some point we should try to deal with mFilter here, you don't
    // really want to use point filtering in the case of NEAREST, since that
    // would also use point filtering for Chroma upsampling. Where most likely
    // the user would only want point filtering for final RGB image upsampling.

    technique = effect()->GetTechniqueByName("RenderYCbCrLayer");

    effect()->GetVariableByName("tY")->AsShaderResource()->SetResource(yuvImage->mYView);
    effect()->GetVariableByName("tCb")->AsShaderResource()->SetResource(yuvImage->mCbView);
    effect()->GetVariableByName("tCr")->AsShaderResource()->SetResource(yuvImage->mCrView);

    effect()->GetVariableByName("vLayerQuad")->AsVector()->SetFloatVector(
      ShaderConstantRectD3D10(
        (float)0,
        (float)0,
        (float)yuvImage->mSize.width,
        (float)yuvImage->mSize.height)
      );
  } else if (image->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImageD3D10 *cairoImage =
      static_cast<CairoImageD3D10*>(image.get());

    if (mFilter == gfxPattern::FILTER_NEAREST) {
      technique = effect()->GetTechniqueByName("RenderRGBALayerPremulPoint");
    } else {
      technique = effect()->GetTechniqueByName("RenderRGBALayerPremul");
    }

    if (cairoImage->mSRView) {
      effect()->GetVariableByName("tRGB")->AsShaderResource()->SetResource(cairoImage->mSRView);
    }

    effect()->GetVariableByName("vLayerQuad")->AsVector()->SetFloatVector(
      ShaderConstantRectD3D10(
        (float)0,
        (float)0,
        (float)cairoImage->mSize.width,
        (float)cairoImage->mSize.height)
      );
  }

  technique->GetPassByIndex(0)->Apply(0);
  device()->Draw(4, 0);
}

PlanarYCbCrImageD3D10::PlanarYCbCrImageD3D10(mozilla::layers::LayerManagerD3D10* aManager)
  : PlanarYCbCrImage(static_cast<ImageD3D10*>(this))
  , mHasData(PR_FALSE)
{
  mDevice = aManager->device();
}

void
PlanarYCbCrImageD3D10::SetData(const PlanarYCbCrImage::Data &aData)
{
  // XXX - For D3D10Ex we really should just copy to systemmem surfaces here.
  // For now, we copy the data
  int width_shift = 0;
  int height_shift = 0;
  if (aData.mYSize.width == aData.mCbCrSize.width &&
      aData.mYSize.height == aData.mCbCrSize.height) {
     // YV24 format
     width_shift = 0;
     height_shift = 0;
     mType = gfx::YV24;
  } else if (aData.mYSize.width / 2 == aData.mCbCrSize.width &&
             aData.mYSize.height == aData.mCbCrSize.height) {
    // YV16 format
    width_shift = 1;
    height_shift = 0;
    mType = gfx::YV16;
  } else if (aData.mYSize.width / 2 == aData.mCbCrSize.width &&
             aData.mYSize.height / 2 == aData.mCbCrSize.height ) {
      // YV12 format
    width_shift = 1;
    height_shift = 1;
    mType = gfx::YV12;
  } else {
    NS_ERROR("YCbCr format not supported");
  }

  mData = aData;
  mData.mCbCrStride = mData.mCbCrSize.width = aData.mPicSize.width >> width_shift;
  // Round up the values for width and height to make sure we sample enough data
  // for the last pixel - See bug 590735
  if (width_shift && (aData.mPicSize.width & 1)) {
    mData.mCbCrStride++;
    mData.mCbCrSize.width++;
  }
  mData.mCbCrSize.height = aData.mPicSize.height >> height_shift;
  if (height_shift && (aData.mPicSize.height & 1)) {
      mData.mCbCrSize.height++;
  }
  mData.mYSize = aData.mPicSize;
  mData.mYStride = mData.mYSize.width;

  mBuffer = new PRUint8[mData.mCbCrStride * mData.mCbCrSize.height * 2 +
                        mData.mYStride * mData.mYSize.height];
  mData.mYChannel = mBuffer;
  mData.mCbChannel = mData.mYChannel + mData.mYStride * mData.mYSize.height;
  mData.mCrChannel = mData.mCbChannel + mData.mCbCrStride * mData.mCbCrSize.height;

  int cbcr_x = aData.mPicX >> width_shift;
  int cbcr_y = aData.mPicY >> height_shift;

  for (int i = 0; i < mData.mYSize.height; i++) {
    memcpy(mData.mYChannel + i * mData.mYStride,
           aData.mYChannel + ((aData.mPicY + i) * aData.mYStride) + aData.mPicX,
           mData.mYStride);
  }
  for (int i = 0; i < mData.mCbCrSize.height; i++) {
    memcpy(mData.mCbChannel + i * mData.mCbCrStride,
           aData.mCbChannel + ((cbcr_y + i) * aData.mCbCrStride) + cbcr_x,
           mData.mCbCrStride);
  }
  for (int i = 0; i < mData.mCbCrSize.height; i++) {
    memcpy(mData.mCrChannel + i * mData.mCbCrStride,
           aData.mCrChannel + ((cbcr_y + i) * aData.mCbCrStride) + cbcr_x,
           mData.mCbCrStride);
  }

  // Fix picture rect to be correct
  mData.mPicX = mData.mPicY = 0;
  mSize = aData.mPicSize;

  AllocateTextures();

  mHasData = PR_TRUE;
}

void
PlanarYCbCrImageD3D10::AllocateTextures()
{
  D3D10_SUBRESOURCE_DATA dataY;
  D3D10_SUBRESOURCE_DATA dataCb;
  D3D10_SUBRESOURCE_DATA dataCr;
  CD3D10_TEXTURE2D_DESC descY(DXGI_FORMAT_R8_UNORM,
                              mData.mYSize.width,
                              mData.mYSize.height, 1, 1);
  CD3D10_TEXTURE2D_DESC descCbCr(DXGI_FORMAT_R8_UNORM,
                                 mData.mCbCrSize.width,
                                 mData.mCbCrSize.height, 1, 1);

  descY.Usage = descCbCr.Usage = D3D10_USAGE_IMMUTABLE;

  dataY.pSysMem = mData.mYChannel;
  dataY.SysMemPitch = mData.mYStride;
  dataCb.pSysMem = mData.mCbChannel;
  dataCb.SysMemPitch = mData.mCbCrStride;
  dataCr.pSysMem = mData.mCrChannel;
  dataCr.SysMemPitch = mData.mCbCrStride;

  mDevice->CreateTexture2D(&descY, &dataY, getter_AddRefs(mYTexture));
  mDevice->CreateTexture2D(&descCbCr, &dataCb, getter_AddRefs(mCbTexture));
  mDevice->CreateTexture2D(&descCbCr, &dataCr, getter_AddRefs(mCrTexture));
  mDevice->CreateShaderResourceView(mYTexture, NULL, getter_AddRefs(mYView));
  mDevice->CreateShaderResourceView(mCbTexture, NULL, getter_AddRefs(mCbView));
  mDevice->CreateShaderResourceView(mCrTexture, NULL, getter_AddRefs(mCrView));
}

already_AddRefed<gfxASurface>
PlanarYCbCrImageD3D10::GetAsSurface()
{
  nsRefPtr<gfxImageSurface> imageSurface =
    new gfxImageSurface(mSize, gfxASurface::ImageFormatRGB24);

  // Convert from YCbCr to RGB now
  gfx::ConvertYCbCrToRGB32(mData.mYChannel,
                           mData.mCbChannel,
                           mData.mCrChannel,
                           imageSurface->Data(),
                           0,
                           0,
                           mSize.width,
                           mSize.height,
                           mData.mYStride,
                           mData.mCbCrStride,
                           imageSurface->Stride(),
                           mType);

  return imageSurface.forget().get();
}

CairoImageD3D10::~CairoImageD3D10()
{
}

void
CairoImageD3D10::SetData(const CairoImage::Data &aData)
{
  mSize = aData.mSize;

  nsRefPtr<gfxImageSurface> imageSurface;

  if (aData.mSurface->GetType() == gfxASurface::SurfaceTypeImage) {
    imageSurface = static_cast<gfxImageSurface*>(aData.mSurface);
  } else {
    imageSurface = new gfxImageSurface(aData.mSize,
                                       gfxASurface::ImageFormatARGB32);
    
    nsRefPtr<gfxContext> context = new gfxContext(imageSurface);
    context->SetSource(aData.mSurface);
    context->SetOperator(gfxContext::OPERATOR_SOURCE);
    context->Paint();
  }

  D3D10_SUBRESOURCE_DATA data;
  
  CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, mSize.width, mSize.height, 1, 1);
  desc.Usage = D3D10_USAGE_IMMUTABLE;
  
  data.pSysMem = imageSurface->Data();
  data.SysMemPitch = imageSurface->Stride();

  mManager->device()->CreateTexture2D(&desc, &data, getter_AddRefs(mTexture));
  mManager->device()->CreateShaderResourceView(mTexture, NULL, getter_AddRefs(mSRView));
}

already_AddRefed<gfxASurface>
CairoImageD3D10::GetAsSurface()
{
  return nsnull;
}

} /* layers */
} /* mozilla */
