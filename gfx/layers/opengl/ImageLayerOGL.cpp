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

#include "ImageLayerOGL.h"
#include "gfxImageSurface.h"

namespace mozilla {
namespace layers {

using mozilla::MutexAutoLock;

ImageContainerOGL::ImageContainerOGL(LayerManagerOGL *aManager)
  : ImageContainer(aManager)
  , mActiveImageLock("mozilla.layers.ImageContainerOGL.mActiveImageLock")
{
}

already_AddRefed<Image>
ImageContainerOGL::CreateImage(const Image::Format *aFormats,
                               PRUint32 aNumFormats)
{
  if (!aNumFormats) {
    return nsnull;
  }
  nsRefPtr<Image> img;
  if (aFormats[0] == Image::PLANAR_YCBCR) {
    img = new PlanarYCbCrImageOGL(static_cast<LayerManagerOGL*>(mManager));
  } else if (aFormats[0] == Image::CAIRO_SURFACE) {
    img = new CairoImageOGL(static_cast<LayerManagerOGL*>(mManager));
  }
  return img.forget();    
}

void
ImageContainerOGL::SetCurrentImage(Image *aImage)
{
  MutexAutoLock lock(mActiveImageLock);

  mActiveImage = aImage;
}

already_AddRefed<Image>
ImageContainerOGL::GetCurrentImage()
{
  MutexAutoLock lock(mActiveImageLock);

  nsRefPtr<Image> retval = mActiveImage;
  return retval.forget();
}

already_AddRefed<gfxASurface>
ImageContainerOGL::GetCurrentAsSurface(gfxIntSize *aSize)
{
  return nsnull;
}

gfxIntSize
ImageContainerOGL::GetCurrentSize()
{
  MutexAutoLock lock(mActiveImageLock);
  if (!mActiveImage) {
    return gfxIntSize(0,0);
  }
  if (mActiveImage->GetFormat() == Image::PLANAR_YCBCR) {
    PlanarYCbCrImageOGL *yuvImage =
      static_cast<PlanarYCbCrImageOGL*>(mActiveImage.get());
    if (!yuvImage->HasData()) {
      return gfxIntSize(0,0);
    }
    return yuvImage->mSize;

  } else if (mActiveImage->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImageOGL *cairoImage =
      static_cast<CairoImageOGL*>(mActiveImage.get());
    return cairoImage->mSize;
  }

  return gfxIntSize(0,0);
}

LayerOGL::LayerType
ImageLayerOGL::GetType()
{
  return TYPE_IMAGE;
}

Layer*
ImageLayerOGL::GetLayer()
{
  return this;
}

void
ImageLayerOGL::RenderLayer(int)
{
  if (!GetContainer()) {
    return;
  }

  static_cast<LayerManagerOGL*>(mManager)->MakeCurrent();

  nsRefPtr<Image> image = GetContainer()->GetCurrentImage();

  if (image->GetFormat() == Image::PLANAR_YCBCR) {
    PlanarYCbCrImageOGL *yuvImage =
      static_cast<PlanarYCbCrImageOGL*>(image.get());

    if (!yuvImage->HasData()) {
      return;
    }

    yuvImage->AllocateTextures();

    float quadTransform[4][4];
    // Transform the quad to the size of the video.
    memset(&quadTransform, 0, sizeof(quadTransform));
    quadTransform[0][0] = (float)yuvImage->mSize.width;
    quadTransform[1][1] = (float)yuvImage->mSize.height;
    quadTransform[2][2] = 1.0f;
    quadTransform[3][3] = 1.0f;

    YCbCrLayerProgram *program = 
      static_cast<LayerManagerOGL*>(mManager)->GetYCbCrLayerProgram();

    program->Activate();

    program->SetLayerQuadTransform(&quadTransform[0][0]);
  
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, yuvImage->mTextures[0]);
    gl()->fActiveTexture(LOCAL_GL_TEXTURE1);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, yuvImage->mTextures[1]);
    gl()->fActiveTexture(LOCAL_GL_TEXTURE2);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, yuvImage->mTextures[2]);

    program->SetLayerOpacity(GetOpacity());
    program->SetLayerTransform(&mTransform._11);
    program->Apply();

    gl()->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);

    yuvImage->FreeTextures();
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

  } else if (image->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImageOGL *cairoImage =
      static_cast<CairoImageOGL*>(image.get());

    float quadTransform[4][4];
    // Transform the quad to the size of the video.
    memset(&quadTransform, 0, sizeof(quadTransform));
    quadTransform[0][0] = (float)cairoImage->mSize.width;
    quadTransform[1][1] = (float)cairoImage->mSize.height;
    quadTransform[2][2] = 1.0f;
    quadTransform[3][3] = 1.0f;
  
    RGBLayerProgram *program = 
      static_cast<LayerManagerOGL*>(mManager)->GetRGBLayerProgram();

    program->Activate();

    program->SetLayerQuadTransform(&quadTransform[0][0]);

    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, cairoImage->mTexture);
    program->SetLayerOpacity(GetOpacity());
    program->SetLayerTransform(&mTransform._11);
    program->Apply();

    gl()->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);
  }
}

PlanarYCbCrImageOGL::PlanarYCbCrImageOGL(mozilla::layers::LayerManagerOGL* aManager)
  : PlanarYCbCrImage(NULL)
  , mLoaded(PR_FALSE)
  , mHasData(PR_FALSE)
  , mManager(aManager)
{ 
  memset(mTextures, 0, sizeof(GLuint) * 3);
}

PlanarYCbCrImageOGL::~PlanarYCbCrImageOGL()
{
  if (mHasData) {
    delete [] mData.mYChannel;
    delete [] mData.mCbChannel;
    delete [] mData.mCrChannel;
  }
}

void
PlanarYCbCrImageOGL::SetData(const PlanarYCbCrImage::Data &aData)
{
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
  mData.mCbChannel = new PRUint8[mData.mCbCrStride * mData.mCbCrSize.height];
  mData.mCrChannel = new PRUint8[mData.mCbCrStride * mData.mCbCrSize.height];
  mData.mYChannel = new PRUint8[mData.mYStride * mData.mYSize.height];
  int cbcr_x = aData.mPicX >> width_shift;
  int cbcr_y = aData.mPicY >> height_shift;

  for (int i = 0; i < mData.mCbCrSize.height; i++) {
    memcpy(mData.mCbChannel + i * mData.mCbCrStride,
           aData.mCbChannel + ((cbcr_y + i) * aData.mCbCrStride) + cbcr_x, 
           mData.mCbCrStride);
    memcpy(mData.mCrChannel + i * mData.mCbCrStride,
           aData.mCrChannel + ((cbcr_y + i) * aData.mCbCrStride) + cbcr_x,
           mData.mCbCrStride);
  }
  for (int i = 0; i < mData.mYSize.height; i++) {
    memcpy(mData.mYChannel + i * mData.mYStride, 
           aData.mYChannel + ((aData.mPicY + i) * aData.mYStride) + aData.mPicX, 
           mData.mYStride);
  }
 
  mSize = aData.mPicSize;

  mHasData = PR_TRUE;
}

void
PlanarYCbCrImageOGL::AllocateTextures()
{
  mManager->MakeCurrent();

  mozilla::gl::GLContext *gl = mManager->gl();
  gl->fGenTextures(3, mTextures);

  GLint alignment;

  if (!((ptrdiff_t)mData.mYStride & 0x7) && !((ptrdiff_t)mData.mYChannel & 0x7)) {
    alignment = 8;
  } else if (!((ptrdiff_t)mData.mYStride & 0x3)) {
    alignment = 4;
  } else if (!((ptrdiff_t)mData.mYStride & 0x1)) {
    alignment = 2;
  } else {
    alignment = 1;
  }

  // Set texture alignment for Y plane.
  gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, alignment);

  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextures[0]);

  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  gl->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                  0,
                  LOCAL_GL_LUMINANCE,
                  mSize.width,
                  mSize.height,
                  0,
                  LOCAL_GL_LUMINANCE,
                  LOCAL_GL_UNSIGNED_BYTE,
                  mData.mYChannel);

  if (!((ptrdiff_t)mData.mCbCrStride & 0x7) && 
      !((ptrdiff_t)mData.mCbChannel & 0x7) &&
      !((ptrdiff_t)mData.mCrChannel & 0x7)) {
    alignment = 8;
  } else if (!((ptrdiff_t)mData.mCbCrStride & 0x3)) {
    alignment = 4;
  } else if (!((ptrdiff_t)mData.mCbCrStride & 0x1)) {
    alignment = 2;
  } else {
    alignment = 1;
  }
  
  // Set texture alignment for Cb/Cr plane
  gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, alignment);

  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextures[1]);

  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  gl->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                  0,
                  LOCAL_GL_LUMINANCE,
                  mData.mCbCrSize.width,
                  mData.mCbCrSize.height,
                  0,
                  LOCAL_GL_LUMINANCE,
                  LOCAL_GL_UNSIGNED_BYTE,
                  mData.mCbChannel);

  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextures[2]);

  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  gl->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                  0,
                  LOCAL_GL_LUMINANCE,
                  mData.mCbCrSize.width,
                  mData.mCbCrSize.height,
                  0,
                  LOCAL_GL_LUMINANCE,
                  LOCAL_GL_UNSIGNED_BYTE,
                  mData.mCrChannel);

  // Reset alignment to default
  gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
}

void
PlanarYCbCrImageOGL::FreeTextures()
{
  static_cast<LayerManagerOGL*>(mManager)->MakeCurrent();
  if (mTextures[0]) {
    mManager->gl()->fDeleteTextures(3, mTextures);
  }
}

CairoImageOGL::~CairoImageOGL()
{
  static_cast<LayerManagerOGL*>(mManager)->MakeCurrent();
  if (mTexture) {
    mManager->gl()->fDeleteTextures(1, &mTexture);
  }
}

void
CairoImageOGL::SetData(const CairoImage::Data &aData)
{
  mSize = aData.mSize;
  mManager->MakeCurrent();
  mozilla::gl::GLContext *gl = mManager->gl();

  nsRefPtr<gfxImageSurface> imageSurface =
    new gfxImageSurface(aData.mSize, gfxASurface::ImageFormatARGB32);

  nsRefPtr<gfxContext> context = new gfxContext(imageSurface);

  context->SetSource(aData.mSurface);
  context->Paint();

  gl->fGenTextures(1, &mTexture);

  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  gl->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                  0,
                  LOCAL_GL_RGBA,
                  mSize.width,
                  mSize.height,
                  0,
                  LOCAL_GL_BGRA,
                  LOCAL_GL_UNSIGNED_BYTE,
                  imageSurface->Data());
}

} /* layers */
} /* mozilla */
