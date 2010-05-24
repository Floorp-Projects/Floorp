/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Bas Schouten <bschouten@mozilla.org>
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

#include "ImageLayerD3D9.h"
#include "gfxImageSurface.h"
#include "yuv_convert.h"

namespace mozilla {
namespace layers {

using mozilla::MutexAutoLock;

ImageContainerD3D9::ImageContainerD3D9(LayerManagerD3D9 *aManager)
  : ImageContainer(aManager)
  , mActiveImageLock("mozilla.layers.ImageContainerD3D9.mActiveImageLock")
{
}

already_AddRefed<Image>
ImageContainerD3D9::CreateImage(const Image::Format *aFormats,
                               PRUint32 aNumFormats)
{
  if (!aNumFormats) {
    return nsnull;
  }
  nsRefPtr<Image> img;
  if (aFormats[0] == Image::PLANAR_YCBCR) {
    img = new PlanarYCbCrImageD3D9(static_cast<LayerManagerD3D9*>(mManager));
  } else if (aFormats[0] == Image::CAIRO_SURFACE) {
    img = new CairoImageD3D9(static_cast<LayerManagerD3D9*>(mManager));
  }
  return img.forget();
}

void
ImageContainerD3D9::SetCurrentImage(Image *aImage)
{
  MutexAutoLock lock(mActiveImageLock);

  mActiveImage = aImage;
}

already_AddRefed<Image>
ImageContainerD3D9::GetCurrentImage()
{
  MutexAutoLock lock(mActiveImageLock);

  nsRefPtr<Image> retval = mActiveImage;
  return retval.forget();
}

already_AddRefed<gfxASurface>
ImageContainerD3D9::GetCurrentAsSurface(gfxIntSize *aSize)
{
  MutexAutoLock lock(mActiveImageLock);
  if (!mActiveImage) {
    return nsnull;
  }

  if (mActiveImage->GetFormat() == Image::PLANAR_YCBCR) {
    PlanarYCbCrImageD3D9 *yuvImage =
      static_cast<PlanarYCbCrImageD3D9*>(mActiveImage.get());
    if (yuvImage->HasData()) {
      *aSize = yuvImage->mSize;
    }
  } else if (mActiveImage->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImageD3D9 *cairoImage =
      static_cast<CairoImageD3D9*>(mActiveImage.get());
    *aSize = cairoImage->mSize;
  }

  return static_cast<ImageD3D9*>(mActiveImage->GetImplData())->GetAsSurface();
}

gfxIntSize
ImageContainerD3D9::GetCurrentSize()
{
  MutexAutoLock lock(mActiveImageLock);
  if (!mActiveImage) {
    return gfxIntSize(0,0);
  }
  if (mActiveImage->GetFormat() == Image::PLANAR_YCBCR) {
    PlanarYCbCrImageD3D9 *yuvImage =
      static_cast<PlanarYCbCrImageD3D9*>(mActiveImage.get());
    if (!yuvImage->HasData()) {
      return gfxIntSize(0,0);
    }
    return yuvImage->mSize;

  } else if (mActiveImage->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImageD3D9 *cairoImage =
      static_cast<CairoImageD3D9*>(mActiveImage.get());
    return cairoImage->mSize;
  }

  return gfxIntSize(0,0);
}

LayerD3D9::LayerType
ImageLayerD3D9::GetType()
{
  return TYPE_IMAGE;
}

Layer*
ImageLayerD3D9::GetLayer()
{
  return this;
}

void
ImageLayerD3D9::RenderLayer()
{
  if (!GetContainer()) {
    return;
  }

  nsRefPtr<Image> image = GetContainer()->GetCurrentImage();

  if (image->GetFormat() == Image::PLANAR_YCBCR) {
    PlanarYCbCrImageD3D9 *yuvImage =
      static_cast<PlanarYCbCrImageD3D9*>(image.get());

    if (!yuvImage->HasData()) {
      return;
    }
    yuvImage->AllocateTextures();

    float quadTransform[4][4];
    /*
     * Matrix to transform the <0.0,0.0>, <1.0,1.0> quad to the correct position
     * and size. To get pixel perfect mapping we extend the quad half a pixel
     * beyond all edges.
     */
    memset(&quadTransform, 0, sizeof(quadTransform));
    quadTransform[0][0] = (float)yuvImage->mSize.width + 0.5f;
    quadTransform[1][1] = (float)yuvImage->mSize.height + 0.5f;
    quadTransform[2][2] = 1.0f;
    quadTransform[3][3] = 1.0f;


    device()->SetVertexShaderConstantF(0, &quadTransform[0][0], 4);
    device()->SetVertexShaderConstantF(4, &mTransform._11, 4);

    float opacity[4];
    /*
     * We always upload a 4 component float, but the shader will
     * only use the the first component since it's declared as a 'float'.
     */
    opacity[0] = GetOpacity();
    device()->SetPixelShaderConstantF(0, opacity, 1);

    mD3DManager->SetShaderMode(LayerManagerD3D9::YCBCRLAYER);

    device()->SetTexture(0, yuvImage->mYTexture);
    device()->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device()->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device()->SetTexture(1, yuvImage->mCbTexture);
    device()->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device()->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device()->SetTexture(2, yuvImage->mCrTexture);
    device()->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device()->SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);

    device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

  } else if (image->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImageD3D9 *cairoImage =
      static_cast<CairoImageD3D9*>(image.get());

    float quadTransform[4][4];
    /*
     * Matrix to transform the <0.0,0.0>, <1.0,1.0> quad to the correct position
     * and size. To get pixel perfect mapping we extend the quad half a pixel
     * beyond all edges.
     */
    memset(&quadTransform, 0, sizeof(quadTransform));
    quadTransform[0][0] = (float)cairoImage->mSize.width + 0.5f;
    quadTransform[1][1] = (float)cairoImage->mSize.height + 0.5f;
    quadTransform[2][2] = 1.0f;
    quadTransform[3][3] = 1.0f;


    device()->SetVertexShaderConstantF(0, &quadTransform[0][0], 4);
    device()->SetVertexShaderConstantF(4, &mTransform._11, 4);

    float opacity[4];
    /*
     * We always upload a 4 component float, but the shader will
     * only use the the first component since it's declared as a 'float'.
     */
    opacity[0] = GetOpacity();
    device()->SetPixelShaderConstantF(0, opacity, 1);

    mD3DManager->SetShaderMode(LayerManagerD3D9::RGBLAYER);

    device()->SetTexture(0, cairoImage->mTexture);
    device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
  }
}

PlanarYCbCrImageD3D9::PlanarYCbCrImageD3D9(mozilla::layers::LayerManagerD3D9* aManager)
  : PlanarYCbCrImage(static_cast<ImageD3D9*>(this))
  , mManager(aManager)
  , mHasData(PR_FALSE)
{
}

void
PlanarYCbCrImageD3D9::SetData(const PlanarYCbCrImage::Data &aData)
{
  // For now, we copy the data
  int width_shift = 0;
  int height_shift = 0;
  if (aData.mYSize.width == aData.mCbCrSize.width &&
      aData.mYSize.height == aData.mCbCrSize.height) {
     // YV24 format
     width_shift = 0;
     height_shift = 0;
  } else if (aData.mYSize.width / 2 == aData.mCbCrSize.width &&
             aData.mYSize.height == aData.mCbCrSize.height) {
    // YV16 format
    width_shift = 1;
    height_shift = 0;
  } else if (aData.mYSize.width / 2 == aData.mCbCrSize.width &&
             aData.mYSize.height / 2 == aData.mCbCrSize.height ) {
      // YV12 format
    width_shift = 1;
    height_shift = 1;
  } else {
    NS_ERROR("YCbCr format not supported");
  }

  mData = aData;
  mData.mCbCrStride = mData.mCbCrSize.width = aData.mPicSize.width >> width_shift;
  mData.mCbCrSize.height = aData.mPicSize.height >> height_shift;
  mData.mYSize = aData.mPicSize;
  mData.mYStride = mData.mYSize.width;
  
  mBuffer = new PRUint8[mData.mCbCrStride * mData.mCbCrSize.height * 2 +
                        mData.mYStride * mData.mYSize.height];
  mData.mYChannel = mBuffer;
  mData.mCbChannel = mData.mYChannel + mData.mYStride * mData.mYSize.height;
  mData.mCrChannel = mData.mCbChannel + mData.mCbCrStride * mData.mCbCrSize.height;

  mData.mCrChannel = new PRUint8[mData.mCbCrStride * mData.mCbCrSize.height];
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

  mHasData = PR_TRUE;
}

void
PlanarYCbCrImageD3D9::AllocateTextures()
{


  D3DLOCKED_RECT lockrect;
  PRUint8* src;
  PRUint8* dest;
  //XXX: ensure correct usage flags
  mManager->device()->CreateTexture(mData.mYSize.width, mData.mYSize.height,
                          1, 0, D3DFMT_L8, D3DPOOL_MANAGED,
                          getter_AddRefs(mYTexture), NULL);


  /* lock the entire texture */
  mYTexture->LockRect(0, &lockrect, NULL, 0);

  src  = mData.mYChannel;
  //FIX cast
  dest = (PRUint8*)lockrect.pBits;

  // copy over data
  for (int h=0; h<mData.mYSize.height; h++) {
    memcpy(dest, src, mData.mYSize.width);
    dest += lockrect.Pitch;
    src += mData.mYStride;
  }

  mYTexture->UnlockRect(0);

  //XXX: ensure correct usage flags
  mManager->device()->CreateTexture(mData.mCbCrSize.width, mData.mCbCrSize.height,
                          1, 0, D3DFMT_L8, D3DPOOL_MANAGED,
                          getter_AddRefs(mCbTexture), NULL);


  /* lock the entire texture */
  mCbTexture->LockRect(0, &lockrect, NULL, 0);

  src  = mData.mCbChannel;
  //FIX cast
  dest = (PRUint8*)lockrect.pBits;

  // copy over data
  for (int h=0; h<mData.mCbCrSize.height; h++) {
    memcpy(dest, src, mData.mCbCrSize.width);
    dest += lockrect.Pitch;
    src += mData.mCbCrStride;
  }

  mCbTexture->UnlockRect(0);


  //XXX: ensure correct usage flags
  mManager->device()->CreateTexture(mData.mCbCrSize.width, mData.mCbCrSize.height,
                          1, 0, D3DFMT_L8, D3DPOOL_MANAGED,
                          getter_AddRefs(mCrTexture), NULL);


  /* lock the entire texture */
  mCrTexture->LockRect(0, &lockrect, NULL, 0);

  src  = mData.mCrChannel;
  //FIX cast
  dest = (PRUint8*)lockrect.pBits;

  // copy over data
  for (int h=0; h<mData.mCbCrSize.height; h++) {
    memcpy(dest, src, mData.mCbCrSize.width);
    dest += lockrect.Pitch;
    src += mData.mCbCrStride;
  }

  mCrTexture->UnlockRect(0);

}

void
PlanarYCbCrImageD3D9::FreeTextures()
{
}

already_AddRefed<gfxASurface>
PlanarYCbCrImageD3D9::GetAsSurface()
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
                           gfx::YV12); 

  return imageSurface.forget().get();
}

CairoImageD3D9::~CairoImageD3D9()
{
}

void
CairoImageD3D9::SetData(const CairoImage::Data &aData)
{
  mSize = aData.mSize;

  nsRefPtr<gfxImageSurface> imageSurface =
    new gfxImageSurface(aData.mSize, gfxASurface::ImageFormatARGB32);

  nsRefPtr<gfxContext> context = new gfxContext(imageSurface);

  context->SetSource(aData.mSurface);
  context->Paint();

  //XXX: make sure we're using the correct usage flags
  mManager->device()->CreateTexture(aData.mSize.width, aData.mSize.height,
                  1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                  getter_AddRefs(mTexture), NULL);

  D3DLOCKED_RECT lockrect;
  /* lock the entire texture */
  mTexture->LockRect(0, &lockrect, NULL, 0);

  PRUint8* src  = imageSurface->Data();
  //FIX cast
  PRUint8* dest = (PRUint8*)lockrect.pBits;

  // copy over data. If we don't need to do any swaping we can
  // use memcpy
  for (int i=0; i<aData.mSize.width*aData.mSize.height; i++) {
    dest[0] = src[0];
    dest[1] = src[1];
    dest[2] = src[2];
    dest[3] = src[3];
    dest += 4;
    src += 4;
  }

  mTexture->UnlockRect(0);
}

already_AddRefed<gfxASurface>
CairoImageD3D9::GetAsSurface()
{
  return nsnull;
}

} /* layers */
} /* mozilla */
