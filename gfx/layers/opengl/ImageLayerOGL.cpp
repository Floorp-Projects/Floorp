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

#include "gfxSharedImageSurface.h"

#include "ImageLayerOGL.h"
#include "gfxImageSurface.h"
#include "yuv_convert.h"
#include "GLContextProvider.h"
#include "MacIOSurfaceImageOGL.h"

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

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
    NS_ASSERTION(aTexture, "TextureDeleter instantiated with nothing to do");
  }

  NS_IMETHOD Run() {
    mContext->MakeCurrent();
    mContext->fDeleteTextures(1, &mTexture);

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
  NS_ASSERTION(aContext->IsGlobalSharedContext() ||
               NS_IsMainThread(), "Can only allocate texture on main thread or with cx sharing");

  Release();

  mContext = aContext;

  mContext->MakeCurrent();
  mContext->fGenTextures(1, &mTexture);
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

  if (mTexture) {
    if (NS_IsMainThread() || mContext->IsGlobalSharedContext()) {
      mContext->MakeCurrent();
      mContext->fDeleteTextures(1, &mTexture);
    } else {
      nsCOMPtr<nsIRunnable> runnable =
        new TextureDeleter(mContext.forget(), mTexture);
      NS_DispatchToMainThread(runnable);
    }

    mTexture = 0;
  }

  mContext = nsnull;
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
RecycleBin::GetBuffer(PRUint32 aSize)
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
  MutexAutoLock lock(mLock);

  if (!aTexture->IsAllocated())
    return;

  if (!mRecycledTextures[aType].IsEmpty() && aSize != mRecycledTextureSizes[aType]) {
    mRecycledTextures[aType].Clear();
  }
  mRecycledTextureSizes[aType] = aSize;
  mRecycledTextures[aType].AppendElement()->TakeFrom(aTexture);
}

void
RecycleBin::GetTexture(TextureType aType, const gfxIntSize& aSize,
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
{
}

ImageContainerOGL::~ImageContainerOGL()
{
  if (mManager) {
    NS_ASSERTION(mManager->GetBackendType() == LayerManager::LAYERS_OPENGL, "Wrong layer manager got assigned to ImageContainerOGL!");

    static_cast<LayerManagerOGL*>(mManager)->ForgetImageContainer(this);
  }
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
    img = new PlanarYCbCrImageOGL(static_cast<LayerManagerOGL*>(mManager),
                                  mRecycleBin);
  } else if (aFormats[0] == Image::CAIRO_SURFACE) {
    img = new CairoImageOGL(static_cast<LayerManagerOGL*>(mManager));
  }
#ifdef XP_MACOSX
  else if (aFormats[0] == Image::MAC_IO_SURFACE) {
    img = new MacIOSurfaceImageOGL(static_cast<LayerManagerOGL*>(mManager));
  }
#endif

  return img.forget();
}

void
ImageContainerOGL::SetCurrentImage(Image *aImage)
{
  nsRefPtr<Image> oldImage;

  {
    MonitorAutoEnter mon(mMonitor);

    oldImage = mActiveImage.forget();
    mActiveImage = aImage;
    CurrentImageChanged();
  }

  // Make sure oldImage is released outside the lock, so it can take our
  // lock in RecycleBuffer
}

already_AddRefed<Image>
ImageContainerOGL::GetCurrentImage()
{
  MonitorAutoEnter mon(mMonitor);

  nsRefPtr<Image> retval = mActiveImage;
  return retval.forget();
}

already_AddRefed<gfxASurface>
ImageContainerOGL::GetCurrentAsSurface(gfxIntSize *aSize)
{
  MonitorAutoEnter mon(mMonitor);

  if (!mActiveImage) {
    *aSize = gfxIntSize(0,0);
    return nsnull;
  }

  GLContext *gl = nsnull;
  // tex1 will be RGBA or Y, tex2 will Cb, tex3 will be Cr
  GLuint tex1 = 0;
  gfxIntSize size;

  if (mActiveImage->GetFormat() == Image::PLANAR_YCBCR) {
    PlanarYCbCrImageOGL *yuvImage =
      static_cast<PlanarYCbCrImageOGL*>(mActiveImage.get());
    if (!yuvImage->HasData()) {
      *aSize = gfxIntSize(0, 0);
      return nsnull;
    }

    size = yuvImage->mSize;

    nsRefPtr<gfxImageSurface> imageSurface =
      new gfxImageSurface(size, gfxASurface::ImageFormatRGB24);

    gfx::ConvertYCbCrToRGB32(yuvImage->mData.mYChannel,
                             yuvImage->mData.mCbChannel,
                             yuvImage->mData.mCrChannel,
                             imageSurface->Data(),
                             0,
                             0,
                             size.width,
                             size.height,
                             yuvImage->mData.mYStride,
                             yuvImage->mData.mCbCrStride,
                             imageSurface->Stride(),
                             yuvImage->mType);

    *aSize = size;
    return imageSurface.forget().get();
  }

  if (mActiveImage->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImageOGL *cairoImage =
      static_cast<CairoImageOGL*>(mActiveImage.get());
    size = cairoImage->mSize;
    gl = cairoImage->mTexture.GetGLContext();
    tex1 = cairoImage->mTexture.GetTextureID();
  }

  nsRefPtr<gfxImageSurface> s = gl->ReadTextureImage(tex1, size, LOCAL_GL_RGBA);
  *aSize = size;
  return s.forget();
}

gfxIntSize
ImageContainerOGL::GetCurrentSize()
{
  MonitorAutoEnter mon(mMonitor);
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
  }

  if (mActiveImage->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImageOGL *cairoImage =
      static_cast<CairoImageOGL*>(mActiveImage.get());
    return cairoImage->mSize;
  }

#ifdef XP_MACOSX
  if (mActiveImage->GetFormat() == Image::MAC_IO_SURFACE) {
    MacIOSurfaceImageOGL *ioImage =
      static_cast<MacIOSurfaceImageOGL*>(mActiveImage.get());
      return ioImage->mSize;
  }
#endif

  return gfxIntSize(0,0);
}

PRBool
ImageContainerOGL::SetLayerManager(LayerManager *aManager)
{
  if (!aManager) {
    // the layer manager just entirely went away

    // XXX if we don't have context sharing, we should tell our images
    // that their textures are no longer valid.
    mManager = nsnull;
    return PR_TRUE;
  }

  if (aManager->GetBackendType() != LayerManager::LAYERS_OPENGL) {
    return PR_FALSE;
  }

  LayerManagerOGL* lmOld = static_cast<LayerManagerOGL*>(mManager);
  LayerManagerOGL* lmNew = static_cast<LayerManagerOGL*>(aManager);

  if (lmOld) {
    NS_ASSERTION(lmNew->glForResources() == lmOld->glForResources(),
                 "We require GL context sharing here!");
    lmOld->ForgetImageContainer(this);
  }

  mManager = aManager;

  lmNew->RememberImageContainer(this);

  return PR_TRUE;
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
  if (!image) {
    return;
  }

  if (image->GetFormat() == Image::PLANAR_YCBCR) {
    PlanarYCbCrImageOGL *yuvImage =
      static_cast<PlanarYCbCrImageOGL*>(image.get());

    if (!yuvImage->HasData()) {
      return;
    }
    
    if (!yuvImage->HasTextures()) {
      yuvImage->AllocateTextures(gl());
    }

    yuvImage->UpdateTextures(gl());

    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, yuvImage->mTextures[0].GetTextureID());
    ApplyFilter(mFilter);
    gl()->fActiveTexture(LOCAL_GL_TEXTURE1);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, yuvImage->mTextures[1].GetTextureID());
    ApplyFilter(mFilter);
    gl()->fActiveTexture(LOCAL_GL_TEXTURE2);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, yuvImage->mTextures[2].GetTextureID());
    ApplyFilter(mFilter);

    YCbCrTextureLayerProgram *program = mOGLManager->GetYCbCrLayerProgram();

    program->Activate();
    program->SetLayerQuadRect(nsIntRect(0, 0,
                                        yuvImage->mSize.width,
                                        yuvImage->mSize.height));
    program->SetLayerTransform(GetEffectiveTransform());
    program->SetLayerOpacity(GetEffectiveOpacity());
    program->SetRenderOffset(aOffset);
    program->SetYCbCrTextureUnits(0, 1, 2);

    mOGLManager->BindAndDrawQuad(program);

    // We shouldn't need to do this, but do it anyway just in case
    // someone else forgets.
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  } else if (image->GetFormat() == Image::CAIRO_SURFACE) {
    CairoImageOGL *cairoImage =
      static_cast<CairoImageOGL*>(image.get());

    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, cairoImage->mTexture.GetTextureID());

    ColorTextureLayerProgram *program = 
      mOGLManager->GetColorTextureLayerProgram(cairoImage->mLayerProgram);

    ApplyFilter(mFilter);

    program->Activate();
    program->SetLayerQuadRect(nsIntRect(0, 0,
                                        cairoImage->mSize.width,
                                        cairoImage->mSize.height));
    program->SetLayerTransform(GetEffectiveTransform());
    program->SetLayerOpacity(GetEffectiveOpacity());
    program->SetRenderOffset(aOffset);
    program->SetTextureUnit(0);

    mOGLManager->BindAndDrawQuad(program);
#ifdef XP_MACOSX
  } else if (image->GetFormat() == Image::MAC_IO_SURFACE) {
     MacIOSurfaceImageOGL *ioImage =
       static_cast<MacIOSurfaceImageOGL*>(image.get());

     if (!mOGLManager->GetThebesLayerCallback()) {
       // If its an empty transaction we still need to update
       // the plugin IO Surface and make sure we grab the
       // new image
       ioImage->Update(GetContainer());
       image = GetContainer()->GetCurrentImage();
       gl()->MakeCurrent();
       ioImage = static_cast<MacIOSurfaceImageOGL*>(image.get());
     }
     
     gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
     gl()->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, ioImage->mTexture.GetTextureID());

     ColorTextureLayerProgram *program = 
       mOGLManager->GetRGBARectLayerProgram();
     
     program->Activate();
     if (program->GetTexCoordMultiplierUniformLocation() != -1) {
       // 2DRect case, get the multiplier right for a sampler2DRect
       float f[] = { float(ioImage->mSize.width), float(ioImage->mSize.height) };
       program->SetUniform(program->GetTexCoordMultiplierUniformLocation(),
                           2, f);
     } else {
       NS_ASSERTION(0, "no rects?");
     }
     
     program->SetLayerQuadRect(nsIntRect(0, 0, 
                                         ioImage->mSize.width, 
                                         ioImage->mSize.height));
     program->SetLayerTransform(GetEffectiveTransform());
     program->SetLayerOpacity(GetEffectiveOpacity());
     program->SetRenderOffset(aOffset);
     program->SetTextureUnit(0);
    
     mOGLManager->BindAndDrawQuad(program);
     gl()->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);
#endif
  }
  GetContainer()->NotifyPaintedImage(image);
}

static void
InitTexture(GLContext* aGL, GLuint aTexture, GLenum aFormat, const gfxIntSize& aSize)
{
  aGL->fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  aGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  aGL->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                   0,
                   aFormat,
                   aSize.width,
                   aSize.height,
                   0,
                   aFormat,
                   LOCAL_GL_UNSIGNED_BYTE,
                   NULL);
}

PlanarYCbCrImageOGL::PlanarYCbCrImageOGL(LayerManagerOGL *aManager,
                                         RecycleBin *aRecycleBin)
  : PlanarYCbCrImage(nsnull), mRecycleBin(aRecycleBin), mHasData(PR_FALSE)
{
#if 0
  // We really want to allocate this on the decode thread -- but to do that,
  // we need to create a per-thread shared GL context, and it will only work
  // if we have context sharing.  For now, create the textures on the main
  // thread the first time we render.
  if (aManager) {
    AllocateTextures(aManager->glForResources());
  }
#endif
}

PlanarYCbCrImageOGL::~PlanarYCbCrImageOGL()
{
  if (mBuffer) {
    mRecycleBin->RecycleBuffer(mBuffer.forget(), mBufferSize);
  }

  if (HasTextures()) {
    mRecycleBin->RecycleTexture(&mTextures[0], RecycleBin::TEXTURE_Y, mData.mYSize);
    mRecycleBin->RecycleTexture(&mTextures[1], RecycleBin::TEXTURE_C, mData.mCbCrSize);
    mRecycleBin->RecycleTexture(&mTextures[2], RecycleBin::TEXTURE_C, mData.mCbCrSize);
  }
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

  // Recycle the previous image main-memory buffer now that we're about to get a new buffer
  if (mBuffer)
    mRecycleBin->RecycleBuffer(mBuffer.forget(), mBufferSize);

  // update buffer size
  mBufferSize = mData.mCbCrStride * mData.mCbCrSize.height * 2 +
                mData.mYStride * mData.mYSize.height;

  // get new buffer
  mBuffer = mRecycleBin->GetBuffer(mBufferSize);
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

void
PlanarYCbCrImageOGL::AllocateTextures(mozilla::gl::GLContext *gl)
{
  gl->MakeCurrent();

  mRecycleBin->GetTexture(RecycleBin::TEXTURE_Y, mData.mYSize, gl, &mTextures[0]);
  InitTexture(gl, mTextures[0].GetTextureID(), LOCAL_GL_LUMINANCE, mData.mYSize);

  mRecycleBin->GetTexture(RecycleBin::TEXTURE_C, mData.mCbCrSize, gl, &mTextures[1]);
  InitTexture(gl, mTextures[1].GetTextureID(), LOCAL_GL_LUMINANCE, mData.mCbCrSize);

  mRecycleBin->GetTexture(RecycleBin::TEXTURE_C, mData.mCbCrSize, gl, &mTextures[2]);
  InitTexture(gl, mTextures[2].GetTextureID(), LOCAL_GL_LUMINANCE, mData.mCbCrSize);
}

void
PlanarYCbCrImageOGL::UpdateTextures(GLContext *gl)
{
  if (!mBuffer || !mHasData)
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
  gl->fTexSubImage2D(LOCAL_GL_TEXTURE_2D, 0,
                     0, 0, mData.mYSize.width, mData.mYSize.height,
                     LOCAL_GL_LUMINANCE,
                     LOCAL_GL_UNSIGNED_BYTE,
                     mData.mYChannel);

  if (!((ptrdiff_t)mData.mCbCrStride & 0x7) && 
      !((ptrdiff_t)mData.mCbChannel & 0x7) &&
      !((ptrdiff_t)mData.mCrChannel & 0x7))
  {
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
  gl->fTexSubImage2D(LOCAL_GL_TEXTURE_2D, 0,
                     0, 0, mData.mCbCrSize.width, mData.mCbCrSize.height,
                     LOCAL_GL_LUMINANCE,
                     LOCAL_GL_UNSIGNED_BYTE,
                     mData.mCbChannel);

  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextures[2].GetTextureID());
  gl->fTexSubImage2D(LOCAL_GL_TEXTURE_2D, 0,
                     0, 0, mData.mCbCrSize.width, mData.mCbCrSize.height,
                     LOCAL_GL_LUMINANCE,
                     LOCAL_GL_UNSIGNED_BYTE,
                     mData.mCrChannel);

  // Reset alignment to default
  gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
}

CairoImageOGL::CairoImageOGL(LayerManagerOGL *aManager)
  : CairoImage(nsnull), mSize(0, 0)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread to create a cairo image");

  if (aManager) {
    // Allocate texture now to grab a reference to the GLContext
    mTexture.Allocate(aManager->glForResources());
  }
}

void
CairoImageOGL::SetData(const CairoImage::Data &aData)
{
  if (!mTexture.IsAllocated())
    return;

  mozilla::gl::GLContext *gl = mTexture.GetGLContext();
  gl->MakeCurrent();

  GLuint tex = mTexture.GetTextureID();

  gl->fActiveTexture(LOCAL_GL_TEXTURE0);
  InitTexture(gl, tex, LOCAL_GL_RGBA, aData.mSize);
  mSize = aData.mSize;

  mLayerProgram =
    gl->UploadSurfaceToTexture(aData.mSurface,
                               nsIntRect(0,0, mSize.width, mSize.height),
                               tex);
}

ShadowImageLayerOGL::ShadowImageLayerOGL(LayerManagerOGL* aManager)
  : ShadowImageLayer(aManager, nsnull)
  , LayerOGL(aManager)
{
  mImplData = static_cast<LayerOGL*>(this);
}  

ShadowImageLayerOGL::~ShadowImageLayerOGL()
{}

PRBool
ShadowImageLayerOGL::Init(gfxSharedImageSurface* aFront,
                          const nsIntSize& aSize)
{
  mDeadweight = aFront;
  gfxSize sz = mDeadweight->GetSize();
  mTexImage = gl()->CreateTextureImage(nsIntSize(sz.width, sz.height),
                                       mDeadweight->GetContentType(),
                                       LOCAL_GL_CLAMP_TO_EDGE);
  return PR_TRUE;
}

already_AddRefed<gfxSharedImageSurface>
ShadowImageLayerOGL::Swap(gfxSharedImageSurface* aNewFront)
{
  if (!mDestroyed && mTexImage) {
    // XXX this is always just ridiculously slow

    gfxSize sz = aNewFront->GetSize();
    nsIntRegion updateRegion(nsIntRect(0, 0, sz.width, sz.height));
    mTexImage->DirectUpdate(aNewFront, updateRegion);
  }

  return aNewFront;
}

void
ShadowImageLayerOGL::DestroyFrontBuffer()
{
  mTexImage = nsnull;
  if (mDeadweight) {
    mOGLManager->DestroySharedSurface(mDeadweight, mAllocator);
    mDeadweight = nsnull;
  }
}

void
ShadowImageLayerOGL::Disconnect()
{
  Destroy();
}

void
ShadowImageLayerOGL::Destroy()
{
  if (!mDestroyed) {
    mDestroyed = PR_TRUE;
    mTexImage = nsnull;
  }
}

Layer*
ShadowImageLayerOGL::GetLayer()
{
  return this;
}

void
ShadowImageLayerOGL::RenderLayer(int aPreviousFrameBuffer,
                                 const nsIntPoint& aOffset)
{
  mOGLManager->MakeCurrent();

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexImage->Texture());
  ColorTextureLayerProgram *program =
    mOGLManager->GetColorTextureLayerProgram(mTexImage->GetShaderProgramType());

  ApplyFilter(mFilter);

  program->Activate();
  program->SetLayerQuadRect(nsIntRect(nsIntPoint(0, 0), mTexImage->GetSize()));
  program->SetLayerTransform(GetEffectiveTransform());
  program->SetLayerOpacity(GetEffectiveOpacity());
  program->SetRenderOffset(aOffset);
  program->SetTextureUnit(0);

  mOGLManager->BindAndDrawQuad(program);
}


} /* layers */
} /* mozilla */
