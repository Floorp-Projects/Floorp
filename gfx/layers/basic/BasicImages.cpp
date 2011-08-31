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
 *   Robert O'Callahan <robert@ocallahan.org>
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

#include "mozilla/ReentrantMonitor.h"

#include "ImageLayers.h"
#include "BasicLayers.h"
#include "gfxImageSurface.h"

#ifdef XP_MACOSX
#include "gfxQuartzImageSurface.h"
#endif

#include "cairo.h"

#include "yuv_convert.h"
#include "ycbcr_to_rgb565.h"

#include "gfxPlatform.h"

using mozilla::ReentrantMonitor;

namespace mozilla {
namespace layers {

/**
 * All our images can yield up a cairo surface and their size.
 */
class BasicImageImplData {
public:
  /**
   * This must be called on the main thread.
   */
  virtual already_AddRefed<gfxASurface> GetAsSurface() = 0;

  gfxIntSize GetSize() { return mSize; }

protected:
  gfxIntSize mSize;
};

/**
 * Since BasicLayers only paint on the main thread, handling a CairoImage
 * is extremely simple. We just hang on to a reference to the surface and
 * return that surface when BasicImageLayer::Paint asks for it via
 * BasicImageContainer::GetAsSurface.
 */
class BasicCairoImage : public CairoImage, public BasicImageImplData {
public:
  BasicCairoImage() : CairoImage(static_cast<BasicImageImplData*>(this)) {}

  virtual void SetData(const Data& aData)
  {
    mSurface = aData.mSurface;
    mSize = aData.mSize;
  }

  virtual already_AddRefed<gfxASurface> GetAsSurface()
  {
    NS_ASSERTION(NS_IsMainThread(), "Must be main thread");
    nsRefPtr<gfxASurface> surface = mSurface.get();
    return surface.forget();
  }

protected:
  nsCountedRef<nsMainThreadSurfaceRef> mSurface;
};

/**
 * We handle YCbCr by converting to RGB when the image is initialized
 * (which should be done off the main thread). The RGB results are stored
 * in a memory buffer and converted to a cairo surface lazily.
 */
class BasicPlanarYCbCrImage : public PlanarYCbCrImage, public BasicImageImplData {
   typedef gfxASurface::gfxImageFormat gfxImageFormat;
public:
   /** 
    * aScaleHint is a size that the image is expected to be rendered at.
    * This is a hint for image backends to optimize scaling.
    */
  BasicPlanarYCbCrImage(const gfxIntSize& aScaleHint) :
    PlanarYCbCrImage(static_cast<BasicImageImplData*>(this)),
    mScaleHint(aScaleHint),
    mOffscreenFormat(gfxASurface::ImageFormatUnknown),
    mDelayedConversion(PR_FALSE)
    {}

  virtual void SetData(const Data& aData);
  virtual void SetDelayedConversion(PRBool aDelayed) { mDelayedConversion = aDelayed; }

  virtual already_AddRefed<gfxASurface> GetAsSurface();

  const Data* GetData() { return &mData; }

  void SetOffscreenFormat(gfxImageFormat aFormat) { mOffscreenFormat = aFormat; }
  gfxImageFormat GetOffscreenFormat() { return mOffscreenFormat; }

  PRUint32 GetDataSize() { return mBuffer ? mDelayedConversion ? mBufferSize : mSize.height * mStride : 0; }

protected:
  nsAutoArrayPtr<PRUint8>              mBuffer;
  nsCountedRef<nsMainThreadSurfaceRef> mSurface;
  gfxIntSize                           mScaleHint;
  PRInt32                              mStride;
  gfxImageFormat                       mOffscreenFormat;
  Data                                 mData;
  PRUint32                             mBufferSize;
  PRPackedBool                         mDelayedConversion;
};

void
BasicPlanarYCbCrImage::SetData(const Data& aData)
{
  // Do some sanity checks to prevent integer overflow
  if (aData.mYSize.width > 16384 || aData.mYSize.height > 16384) {
    NS_ERROR("Illegal width or height");
    return;
  }
  
  if (mDelayedConversion) {
    mBuffer = CopyData(mData, mSize, mBufferSize, aData);
    return;
  }
  
  gfx::YUVType type = 
    gfx::TypeFromSize(aData.mYSize.width,
                      aData.mYSize.height,
                      aData.mCbCrSize.width,
                      aData.mCbCrSize.height);

  gfxASurface::gfxImageFormat format = GetOffscreenFormat();

  // 'prescale' is true if the scaling is to be done as part of the
  // YCbCr to RGB conversion rather than on the RGB data when rendered.
  PRBool prescale = mScaleHint.width > 0 && mScaleHint.height > 0 &&
                    mScaleHint != aData.mPicSize;
  if (format == gfxASurface::ImageFormatRGB16_565) {
#if defined(HAVE_YCBCR_TO_RGB565)
    if (prescale &&
        !gfx::IsScaleYCbCrToRGB565Fast(aData.mPicX,
                                       aData.mPicY,
                                       aData.mPicSize.width,
                                       aData.mPicSize.height,
                                       mScaleHint.width,
                                       mScaleHint.height,
                                       type,
                                       gfx::FILTER_BILINEAR) &&
        gfx::IsConvertYCbCrToRGB565Fast(aData.mPicX,
                                        aData.mPicY,
                                        aData.mPicSize.width,
                                        aData.mPicSize.height,
                                        type)) {
      prescale = PR_FALSE;
    }
#else
    // yuv2rgb16 function not available
    format = gfxASurface::ImageFormatRGB24;
#endif
  }
  else if (format != gfxASurface::ImageFormatRGB24) {
    // No other formats are currently supported.
    format = gfxASurface::ImageFormatRGB24;
  }
  if (format == gfxASurface::ImageFormatRGB24) {
    /* ScaleYCbCrToRGB32 does not support a picture offset, nor 4:4:4 data.
       See bugs 639415 and 640073. */
    if (aData.mPicX != 0 || aData.mPicY != 0 || type == gfx::YV24)
      prescale = PR_FALSE;
  }

  gfxIntSize size(prescale ? mScaleHint.width : aData.mPicSize.width,
                  prescale ? mScaleHint.height : aData.mPicSize.height);

  mStride = gfxASurface::FormatStrideForWidth(format, size.width);
  mBuffer = AllocateBuffer(size.height * mStride);
  if (!mBuffer) {
    // out of memory
    return;
  }

  // Convert from YCbCr to RGB now, scaling the image if needed.
  if (size != aData.mPicSize) {
#if defined(HAVE_YCBCR_TO_RGB565)
    if (format == gfxASurface::ImageFormatRGB16_565) {
      gfx::ScaleYCbCrToRGB565(aData.mYChannel,
                              aData.mCbChannel,
                              aData.mCrChannel,
                              mBuffer,
                              aData.mPicX,
                              aData.mPicY,
                              aData.mPicSize.width,
                              aData.mPicSize.height,
                              size.width,
                              size.height,
                              aData.mYStride,
                              aData.mCbCrStride,
                              mStride,
                              type,
                              gfx::FILTER_BILINEAR);
    } else
#endif
      gfx::ScaleYCbCrToRGB32(aData.mYChannel,
                             aData.mCbChannel,
                             aData.mCrChannel,
                             mBuffer,
                             aData.mPicSize.width,
                             aData.mPicSize.height,
                             size.width,
                             size.height,
                             aData.mYStride,
                             aData.mCbCrStride,
                             mStride,
                             type,
                             gfx::ROTATE_0,
                             gfx::FILTER_BILINEAR);
  } else { // no prescale
#if defined(HAVE_YCBCR_TO_RGB565)
    if (format == gfxASurface::ImageFormatRGB16_565) {
      gfx::ConvertYCbCrToRGB565(aData.mYChannel,
                                aData.mCbChannel,
                                aData.mCrChannel,
                                mBuffer,
                                aData.mPicX,
                                aData.mPicY,
                                aData.mPicSize.width,
                                aData.mPicSize.height,
                                aData.mYStride,
                                aData.mCbCrStride,
                                mStride,
                                type);
    } else // format != gfxASurface::ImageFormatRGB16_565
#endif
      gfx::ConvertYCbCrToRGB32(aData.mYChannel,
                               aData.mCbChannel,
                               aData.mCrChannel,
                               mBuffer,
                               aData.mPicX,
                               aData.mPicY,
                               aData.mPicSize.width,
                               aData.mPicSize.height,
                               aData.mYStride,
                               aData.mCbCrStride,
                               mStride,
                               type);
  }
  SetOffscreenFormat(format);
  mSize = size;
}

static cairo_user_data_key_t imageSurfaceDataKey;

static void
DestroyBuffer(void* aBuffer)
{
  delete[] static_cast<PRUint8*>(aBuffer);
}

already_AddRefed<gfxASurface>
BasicPlanarYCbCrImage::GetAsSurface()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be main thread");

  if (mSurface) {
    nsRefPtr<gfxASurface> result = mSurface.get();
    return result.forget();
  }

  // XXX: If we forced delayed conversion, are we ever going to hit this?
  // We may need to implement the conversion here.
  if (!mBuffer || mDelayedConversion) {
    return nsnull;
  }

  gfxASurface::gfxImageFormat format = GetOffscreenFormat();

  nsRefPtr<gfxImageSurface> imgSurface =
      new gfxImageSurface(mBuffer, mSize, mStride, format);
  if (!imgSurface || imgSurface->CairoStatus() != 0) {
    return nsnull;
  }

  // Pass ownership of the buffer to the surface
  imgSurface->SetData(&imageSurfaceDataKey, mBuffer.forget(), DestroyBuffer);

  nsRefPtr<gfxASurface> result = imgSurface.get();
#if defined(XP_MACOSX)
  nsRefPtr<gfxQuartzImageSurface> quartzSurface =
    new gfxQuartzImageSurface(imgSurface);
  if (quartzSurface) {
    result = quartzSurface.forget();
  }
#endif
  mSurface = result.get();

  return result.forget();
}

/**
 * Our image container is very simple. It's really just a factory
 * for the image objects. We use a ReentrantMonitor to synchronize access to
 * mImage.
 */
class BasicImageContainer : public ImageContainer {
public:
  typedef gfxASurface::gfxImageFormat gfxImageFormat;

  BasicImageContainer() :
    ImageContainer(nsnull),
    mScaleHint(-1, -1),
    mOffscreenFormat(gfxASurface::ImageFormatUnknown),
    mDelayed(PR_FALSE)
  {}
  virtual already_AddRefed<Image> CreateImage(const Image::Format* aFormats,
                                              PRUint32 aNumFormats);
  virtual void SetDelayedConversion(PRBool aDelayed) { mDelayed = aDelayed; }
  virtual void SetCurrentImage(Image* aImage);
  virtual already_AddRefed<Image> GetCurrentImage();
  virtual already_AddRefed<gfxASurface> GetCurrentAsSurface(gfxIntSize* aSize);
  virtual gfxIntSize GetCurrentSize();
  virtual PRBool SetLayerManager(LayerManager *aManager);
  virtual void SetScaleHint(const gfxIntSize& aScaleHint);
  void SetOffscreenFormat(gfxImageFormat aFormat) { mOffscreenFormat = aFormat; }
  virtual LayerManager::LayersBackend GetBackendType() { return LayerManager::LAYERS_BASIC; }

protected:
  nsRefPtr<Image> mImage;
  gfxIntSize mScaleHint;
  gfxImageFormat mOffscreenFormat;
  PRPackedBool mDelayed;
};

/**
 * Returns true if aFormat is in the given format array.
 */
static PRBool
FormatInList(const Image::Format* aFormats, PRUint32 aNumFormats,
             Image::Format aFormat)
{
  for (PRUint32 i = 0; i < aNumFormats; ++i) {
    if (aFormats[i] == aFormat) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

already_AddRefed<Image>
BasicImageContainer::CreateImage(const Image::Format* aFormats,
                                 PRUint32 aNumFormats)
{
  nsRefPtr<Image> image;
  // Prefer cairo surfaces because they're native for us
  if (FormatInList(aFormats, aNumFormats, Image::CAIRO_SURFACE)) {
    image = new BasicCairoImage();
  } else if (FormatInList(aFormats, aNumFormats, Image::PLANAR_YCBCR)) {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    image = new BasicPlanarYCbCrImage(mScaleHint);
    static_cast<BasicPlanarYCbCrImage*>(image.get())->SetOffscreenFormat(mOffscreenFormat);
    static_cast<BasicPlanarYCbCrImage*>(image.get())->SetDelayedConversion(mDelayed);
  }
  return image.forget();
}

void
BasicImageContainer::SetCurrentImage(Image* aImage)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  mImage = aImage;
  CurrentImageChanged();
}

already_AddRefed<Image>
BasicImageContainer::GetCurrentImage()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  nsRefPtr<Image> image = mImage;
  return image.forget();
}

static BasicImageImplData*
ToImageData(Image* aImage)
{
  return static_cast<BasicImageImplData*>(aImage->GetImplData());
}

already_AddRefed<gfxASurface>
BasicImageContainer::GetCurrentAsSurface(gfxIntSize* aSizeResult)
{
  NS_PRECONDITION(NS_IsMainThread(), "Must be called on main thread");

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  if (!mImage) {
    return nsnull;
  }
  *aSizeResult = ToImageData(mImage)->GetSize();
  return ToImageData(mImage)->GetAsSurface();
}

gfxIntSize
BasicImageContainer::GetCurrentSize()
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  return !mImage ? gfxIntSize(0,0) : ToImageData(mImage)->GetSize();
}

void BasicImageContainer::SetScaleHint(const gfxIntSize& aScaleHint)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  mScaleHint = aScaleHint;
}

PRBool
BasicImageContainer::SetLayerManager(LayerManager *aManager)
{
  if (aManager &&
      aManager->GetBackendType() != LayerManager::LAYERS_BASIC)
  {
    return PR_FALSE;
  }

  return PR_TRUE;
}

already_AddRefed<ImageContainer>
BasicLayerManager::CreateImageContainer()
{
  nsRefPtr<ImageContainer> container = new BasicImageContainer();
  static_cast<BasicImageContainer*>(container.get())->
    SetOffscreenFormat(gfxPlatform::GetPlatform()->GetOffscreenFormat());
  return container.forget();
}

}
}
