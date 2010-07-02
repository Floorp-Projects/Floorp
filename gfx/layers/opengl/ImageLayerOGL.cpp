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
 *   Bas Schouten <bschouten@mozilla.org>
 *   Vladimir Vukicevic <vladimir@pobox.com>
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
#include "GLContextProvider.h"

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

using mozilla::MutexAutoLock;

/**
 * This is an event used to unref a GLContext on the main thread and
 * optionally delete a texture associated with that context.
 */
class TextureDeleter : public nsRunnable {
public:
  TextureDeleter(already_AddRefed<GLContext> aContext,
                 GLuint aTexture)
      : mContext(aContext), mTexture(aTexture)
  {
  }
  NS_IMETHOD Run() {
    if (mTexture) {
      mContext->DestroyTexture(mTexture);
    }
    // Ensure context is released on the main thread
    mContext = nsnull;
    return NS_OK;
  }

  nsRefPtr<GLContext> mContext;
  GLuint mTexture;
};

void
GLTexture::Allocate(GLContext *aContext)
{
  NS_ASSERTION(NS_IsMainThread(), "Can only allocate texture on main thread");

  Release();

  mContext = aContext;
  mTexture = mContext->CreateTexture();
}

void
GLTexture::TakeFrom(GLTexture *aOther)
{
  Release();

  mContext = aOther->mContext.forget();
  mTexture = aOther->mTexture;
  aOther->mTexture = 0;
}

void
GLTexture::Release()
{
  if (!mContext) {
    NS_ASSERTION(!mTexture, "Can't delete texture without a context");
    return;
  }

  if (NS_IsMainThread()) {
    if (mTexture) {
      mContext->DestroyTexture(mTexture);
      mTexture = 0;
    }
    mContext = nsnull;
    return;
  }

  nsCOMPtr<nsIRunnable> runnable =
    new TextureDeleter(mContext.forget(), mTexture);
  NS_DispatchToMainThread(runnable);
  mTexture = 0;
}

RecycleBin::RecycleBin()
  : mLock("mozilla.layers.RecycleBin.mLock")
{
}

void
RecycleBin::RecycleBuffer(PRUint8* aBuffer, PRUint32 aSize)
{
  MutexAutoLock lock(mLock);

  if (!mRecycledBuffers.IsEmpty() && aSize != mRecycledBufferSize) {
    mRecycledBuffers.Clear();
  }
  mRecycledBufferSize = aSize;
  mRecycledBuffers.AppendElement(aBuffer);
}

PRUint8*
RecycleBin::TakeBuffer(PRUint32 aSize)
{
  MutexAutoLock lock(mLock);

  if (mRecycledBuffers.IsEmpty() || mRecycledBufferSize != aSize)
    return new PRUint8[aSize];

  PRUint32 last = mRecycledBuffers.Length() - 1;
  PRUint8* result = mRecycledBuffers[last].forget();
  mRecycledBuffers.RemoveElementAt(last);
  return result;
}

void
RecycleBin::RecycleTexture(GLTexture *aTexture, TextureType aType,
                           const gfxIntSize& aSize)
{
  if (!aTexture->IsAllocated())
    return;

  MutexAutoLock lock(mLock);

  if (!mRecycledTextures[aType].IsEmpty() && aSize != mRecycledTextureSizes[aType]) {
    mRecycledTextures[aType].Clear();
  }
  mRecycledTextureSizes[aType] = aSize;
  mRecycledTextures[aType].AppendElement()->TakeFrom(aTexture);
}

void
RecycleBin::TakeTexture(TextureType aType, const gfxIntSize& aSize,
                        GLContext *aContext, GLTexture *aOutTexture)
{
  MutexAutoLock lock(mLock);

  if (mRecycledTextures[aType].IsEmpty() || mRecycledTextureSizes[aType] != aSize) {
    aOutTexture->Allocate(aContext);
    return;
  }
  PRUint32 last = mRecycledTextures[aType].Length() - 1;
  aOutTexture->TakeFrom(&mRecycledTextures[aType].ElementAt(last));
  mRecycledTextures[aType].RemoveElementAt(last);
}

ImageContainerOGL::ImageContainerOGL(LayerManagerOGL *aManager)
  : ImageContainer(aManager)
  , mRecycleBin(new RecycleBin())
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
    img = new PlanarYCbCrImageOGL(mRecycleBin);
  } else if (aFormats[0] == Image::CAIRO_SURFACE) {
    img = new CairoImageOGL(static_cast<LayerManagerOGL*>(mManager));
  }
  return img.forget();
}

void
ImageContainerOGL::SetCurrentImage(Image *aImage)
{
  nsRefPtr<Image> oldImage;

  {
    MutexAutoLock lock(mActiveImageLock);

    oldImage = mActiveImage.forget();
    mActiveImage = aImage;
  }

  // Make sure oldImage is released outside the lock, so it can take our
  // lock in RecycleBuffer
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

Layer*
ImageLayerOGL::GetLayer()
{
  return this;
}

void
ImageLayerOGL::RenderLayer(int,
                           const nsIntPoint& aOffset)
{
  if (!GetContainer())
    return;

  mOGLManager->MakeCurrent();

  nsRefPtr<Image> image = GetContainer()->GetCurrentImage();

  if (image->GetFormat() == Image::PLANAR_YCBCR) {
    PlanarYCbCrImageOGL *yuvImage =
      static_cast<PlanarYCbCrImageOGL*>(image.get());

    if (!yuvImage->HasData())
      return;

    if (!yuvImage->HasTextures())
      yuvImage->AllocateTextures(mOGLManager);

    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, yuvImage->mTextures[0].GetTextureID());
    gl()->fActiveTexture(LOCAL_GL_TEXTURE1);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, yuvImage->mTextures[1].GetTextureID());
    gl()->fActiveTexture(LOCAL_GL_TEXTURE2);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, yuvImage->mTextures[2].GetTextureID());

    YCbCrTextureLayerProgram *program = mOGLManager->GetYCbCrLayerProgram();

    program->Activate();
    program->SetLayerQuadRect(nsIntRect(0, 0,
                                        yuvImage->mSize.width,
                                        yuvImage->mSize.height));
    program->SetLayerTransform(mTransform);
    program->SetLayerOpacity(GetOpacity());
    program->SetRenderOffset(aOffset);
    program->SetYCbCrTextureUnits(0, 1, 2);

    DEBUG_GL_ERROR_CHECK(gl());

    mOGLManager->BindAndDrawQuad(program);

    // We shouldn't need to do this, but do it anyway just in case
    // someone else forgets.
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  } else if (image->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImageOGL *cairoImage =
      static_cast<CairoImageOGL*>(image.get());

    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, cairoImage->mTexture.GetTextureID());
  
    ColorTextureLayerProgram *program;
    if (cairoImage->mASurfaceAsGLContext)
      program = mOGLManager->GetRGBALayerProgram();
    else
      program = mOGLManager->GetBGRALayerProgram();

    program->Activate();
    program->SetLayerQuadRect(nsIntRect(0, 0,
                                        cairoImage->mSize.width,
                                        cairoImage->mSize.height));
    program->SetLayerTransform(mTransform);
    program->SetLayerOpacity(GetOpacity());
    program->SetRenderOffset(aOffset);
    program->SetTextureUnit(0);

    mOGLManager->BindAndDrawQuad(program);
  }

  DEBUG_GL_ERROR_CHECK(gl());
}

PlanarYCbCrImageOGL::PlanarYCbCrImageOGL(RecycleBin *aRecycleBin)
  : PlanarYCbCrImage(nsnull), mRecycleBin(aRecycleBin), mHasData(PR_FALSE)
{
}

PlanarYCbCrImageOGL::~PlanarYCbCrImageOGL()
{
  if (mBuffer) {
    mRecycleBin->RecycleBuffer(mBuffer.forget(), mBufferSize);
  }

  mRecycleBin->RecycleTexture(&mTextures[0], RecycleBin::TEXTURE_Y, mData.mYSize);
  mRecycleBin->RecycleTexture(&mTextures[1], RecycleBin::TEXTURE_C, mData.mCbCrSize);
  mRecycleBin->RecycleTexture(&mTextures[2], RecycleBin::TEXTURE_C, mData.mCbCrSize);
}

void
PlanarYCbCrImageOGL::SetData(const PlanarYCbCrImage::Data &aData)
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

  mBufferSize = mData.mCbCrStride * mData.mCbCrSize.height * 2 +
                mData.mYStride * mData.mYSize.height;
  mBuffer = mRecycleBin->TakeBuffer(mBufferSize);
  if (!mBuffer)
    return;

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

  mHasData = PR_TRUE;
}

static void
SetupPlaneTexture(GLContext* aGL, const gfxIntSize& aSize, PRUint8* aData, PRBool aIsNew)
{
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  if (aIsNew) {
    aGL->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                     0,
                     LOCAL_GL_LUMINANCE,
                     aSize.width,
                     aSize.height,
                     0,
                     LOCAL_GL_LUMINANCE,
                     LOCAL_GL_UNSIGNED_BYTE,
                     aData);
  } else {
    aGL->fTexSubImage2D(LOCAL_GL_TEXTURE_2D,
                        0,
                        0, 0,
                        aSize.width,
                        aSize.height,
                        LOCAL_GL_LUMINANCE,
                        LOCAL_GL_UNSIGNED_BYTE,
                        aData);
  }
}

void
PlanarYCbCrImageOGL::AllocateTextures(LayerManagerOGL *aManager)
{
  aManager->MakeCurrent();

  mozilla::gl::GLContext *gl = aManager->gl();

  PRPackedBool isNewTexture[3];
  for (PRUint32 i = 0; i < 3; ++i) {
    isNewTexture[i] = !mTextures[i].IsAllocated();
  }

  mRecycleBin->TakeTexture(RecycleBin::TEXTURE_Y, mData.mYSize, gl, &mTextures[0]);
  mRecycleBin->TakeTexture(RecycleBin::TEXTURE_C, mData.mCbCrSize, gl, &mTextures[1]);
  mRecycleBin->TakeTexture(RecycleBin::TEXTURE_C, mData.mCbCrSize, gl, &mTextures[2]);
  if (!HasTextures())
    return;

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

  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextures[0].GetTextureID());

  SetupPlaneTexture(gl, mData.mYSize, mData.mYChannel, isNewTexture[0]);

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

  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextures[1].GetTextureID());

  SetupPlaneTexture(gl, mData.mCbCrSize, mData.mCbChannel, isNewTexture[1]);

  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextures[2].GetTextureID());

  SetupPlaneTexture(gl, mData.mCbCrSize, mData.mCrChannel, isNewTexture[2]);

  // Reset alignment to default
  gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);

  // Recycle main-memory buffer now that we've got the data in our textures
  if (mBuffer) {
    mRecycleBin->RecycleBuffer(mBuffer.forget(), mBufferSize);
  }
}

CairoImageOGL::CairoImageOGL(LayerManagerOGL *aManager) : CairoImage(nsnull)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread to create a cairo image");

  // Allocate texture now to grab a reference to the GLContext
  mTexture.Allocate(aManager->gl());
}

void
CairoImageOGL::SetData(const CairoImage::Data &aData)
{
  if (!mTexture.IsAllocated())
    return;

  mozilla::gl::GLContext *gl = mTexture.GetGLContext();
  gl->MakeCurrent();

  mSize = aData.mSize;

  gl->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture.GetTextureID());
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  if (!mASurfaceAsGLContext) {
    mASurfaceAsGLContext = sGLContextProvider.CreateForNativePixmapSurface(aData.mSurface);
    if (mASurfaceAsGLContext)
      mASurfaceAsGLContext->BindTexImage();
  }
  if (mASurfaceAsGLContext)
    return;

  // XXX This could be a lot more efficient if we already have an image-compatible
  // surface
  // XXX if we ever create an ImageFormatRGB24 surface, make sure that we use
  // a BGRX program in that case (instead of BGRA)
  nsRefPtr<gfxImageSurface> imageSurface =
    new gfxImageSurface(aData.mSize, gfxASurface::ImageFormatARGB32);
  nsRefPtr<gfxContext> context = new gfxContext(imageSurface);

  context->SetSource(aData.mSurface);
  context->Paint();

  gl->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                  0,
                  LOCAL_GL_RGBA,
                  mSize.width,
                  mSize.height,
                  0,
                  LOCAL_GL_RGBA,
                  LOCAL_GL_UNSIGNED_BYTE,
                  imageSurface->Data());
}

} /* layers */
} /* mozilla */
