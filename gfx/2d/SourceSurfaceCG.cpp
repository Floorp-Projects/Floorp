/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceSurfaceCG.h"
#include "DrawTargetCG.h"

namespace mozilla {
namespace gfx {


SourceSurfaceCG::~SourceSurfaceCG()
{
  CGImageRelease(mImage);
}

IntSize
SourceSurfaceCG::GetSize() const
{
  IntSize size;
  size.width = CGImageGetWidth(mImage);
  size.height = CGImageGetHeight(mImage);
  return size;
}

SurfaceFormat
SourceSurfaceCG::GetFormat() const
{
  return mFormat;
}

TemporaryRef<DataSourceSurface>
SourceSurfaceCG::GetDataSurface()
{
  //XXX: we should be more disciplined about who takes a reference and where
  CGImageRetain(mImage);
  RefPtr<DataSourceSurfaceCG> dataSurf =
    new DataSourceSurfaceCG(mImage);
  return dataSurf;
}

static void releaseCallback(void *info, const void *data, size_t size) {
  free(info);
}

bool
SourceSurfaceCG::InitFromData(unsigned char *aData,
                               const IntSize &aSize,
                               int32_t aStride,
                               SurfaceFormat aFormat)
{
  //XXX: we should avoid creating this colorspace everytime
  CGColorSpaceRef colorSpace = NULL;
  CGBitmapInfo bitinfo = 0;
  CGDataProviderRef dataProvider = NULL;
  int bitsPerComponent = 0;
  int bitsPerPixel = 0;

  assert(aSize.width >= 0 && aSize.height >= 0);

  switch (aFormat) {
    case FORMAT_B8G8R8A8:
      colorSpace = CGColorSpaceCreateDeviceRGB();
      bitinfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
      bitsPerComponent = 8;
      bitsPerPixel = 32;
      break;

    case FORMAT_B8G8R8X8:
      colorSpace = CGColorSpaceCreateDeviceRGB();
      bitinfo = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host;
      bitsPerComponent = 8;
      bitsPerPixel = 32;
      break;

    case FORMAT_A8:
      // XXX: why don't we set a colorspace here?
      bitsPerComponent = 8;
      bitsPerPixel = 8;
  };

  void *data = malloc(aStride * aSize.height);
  memcpy(data, aData, aStride * aSize.height);

  mFormat = aFormat;

  dataProvider = CGDataProviderCreateWithData (data,
                                               data,
					       aSize.height * aStride,
					       releaseCallback);

  if (aFormat == FORMAT_A8) {
    CGFloat decode[] = {1.0, 0.0};
    mImage = CGImageMaskCreate (aSize.width, aSize.height,
				bitsPerComponent,
				bitsPerPixel,
				aStride,
				dataProvider,
				decode,
				true);

  } else {
    mImage = CGImageCreate (aSize.width, aSize.height,
			    bitsPerComponent,
			    bitsPerPixel,
			    aStride,
			    colorSpace,
			    bitinfo,
			    dataProvider,
			    NULL,
			    true,
			    kCGRenderingIntentDefault);
  }

  CGDataProviderRelease(dataProvider);
  CGColorSpaceRelease (colorSpace);

  return mImage != NULL;
}

DataSourceSurfaceCG::~DataSourceSurfaceCG()
{
  CGImageRelease(mImage);
  free(CGBitmapContextGetData(mCg));
  CGContextRelease(mCg);
}

IntSize
DataSourceSurfaceCG::GetSize() const
{
  IntSize size;
  size.width = CGImageGetWidth(mImage);
  size.height = CGImageGetHeight(mImage);
  return size;
}

bool
DataSourceSurfaceCG::InitFromData(unsigned char *aData,
                               const IntSize &aSize,
                               int32_t aStride,
                               SurfaceFormat aFormat)
{
  //XXX: we should avoid creating this colorspace everytime
  CGColorSpaceRef colorSpace = NULL;
  CGBitmapInfo bitinfo = 0;
  CGDataProviderRef dataProvider = NULL;
  int bitsPerComponent = 0;
  int bitsPerPixel = 0;

  switch (aFormat) {
    case FORMAT_B8G8R8A8:
      colorSpace = CGColorSpaceCreateDeviceRGB();
      bitinfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
      bitsPerComponent = 8;
      bitsPerPixel = 32;
      break;

    case FORMAT_B8G8R8X8:
      colorSpace = CGColorSpaceCreateDeviceRGB();
      bitinfo = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host;
      bitsPerComponent = 8;
      bitsPerPixel = 32;
      break;

    case FORMAT_A8:
      // XXX: why don't we set a colorspace here?
      bitsPerComponent = 8;
      bitsPerPixel = 8;
  };

  void *data = malloc(aStride * aSize.height);
  memcpy(data, aData, aStride * aSize.height);

  //mFormat = aFormat;

  dataProvider = CGDataProviderCreateWithData (data,
                                               data,
					       aSize.height * aStride,
					       releaseCallback);

  if (aFormat == FORMAT_A8) {
    CGFloat decode[] = {1.0, 0.0};
    mImage = CGImageMaskCreate (aSize.width, aSize.height,
				bitsPerComponent,
				bitsPerPixel,
				aStride,
				dataProvider,
				decode,
				true);

  } else {
    mImage = CGImageCreate (aSize.width, aSize.height,
			    bitsPerComponent,
			    bitsPerPixel,
			    aStride,
			    colorSpace,
			    bitinfo,
			    dataProvider,
			    NULL,
			    true,
			    kCGRenderingIntentDefault);
  }

  CGDataProviderRelease(dataProvider);
  CGColorSpaceRelease (colorSpace);

  return mImage;
}

CGContextRef CreateBitmapContextForImage(CGImageRef image)
{
  CGColorSpaceRef colorSpace;

  size_t width  = CGImageGetWidth(image);
  size_t height = CGImageGetHeight(image);

  int bitmapBytesPerRow = (width * 4);
  int bitmapByteCount   = (bitmapBytesPerRow * height);

  void *data = calloc(bitmapByteCount, 1);
  //XXX: which color space should we be using here?
  colorSpace = CGColorSpaceCreateDeviceRGB();
  assert(colorSpace);

  // we'd like to pass NULL as the first parameter
  // to let Quartz manage this memory for us. However,
  // on 10.5 and older CGBitmapContextGetData will return
  // NULL instead of the associated buffer so we need
  // to manage it ourselves.
  CGContextRef cg = CGBitmapContextCreate(data,
                                          width,
                                          height,
                                          8,
                                          bitmapBytesPerRow,
                                          colorSpace,
                                          kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
  assert(cg);

  CGColorSpaceRelease(colorSpace);

  return cg;
}

DataSourceSurfaceCG::DataSourceSurfaceCG(CGImageRef aImage)
{
  mImage = aImage;
  mCg = CreateBitmapContextForImage(aImage);
  if (mCg == NULL) {
    // error creating context
    return;
  }

  // Get image width, height. We'll use the entire image.
  CGFloat w = CGImageGetWidth(aImage);
  CGFloat h = CGImageGetHeight(aImage);
  CGRect rect = {{0,0},{w,h}};

  // Draw the image to the bitmap context. Once we draw, the memory
  // allocated for the context for rendering will then contain the
  // raw image data in the specified color space.
  CGContextDrawImage(mCg, rect, aImage);

  // Now we can get a pointer to the image data associated with the bitmap
  // context.
  mData = CGBitmapContextGetData(mCg);
  assert(mData);
}

unsigned char *
DataSourceSurfaceCG::GetData()
{
  // See http://developer.apple.com/library/mac/#qa/qa1509/_index.html
  // the following only works on 10.5+, the Q&A above suggests a method
  // that can be used for earlier versions
  //CFDataRef data = CGDataProviderCopyData(CGImageGetDataProvider(cgImage));
  //unsigned char *dataPtr = CFDataGetBytePtr(data);
  //CFDataRelease(data);
  // unfortunately the the method above only works for read-only access and
  // we need read-write for DataSourceSurfaces
  return (unsigned char*)mData;
}

SourceSurfaceCGBitmapContext::SourceSurfaceCGBitmapContext(DrawTargetCG *aDrawTarget)
{
  mDrawTarget = aDrawTarget;
  mCg = (CGContextRef)aDrawTarget->GetNativeSurface(NATIVE_SURFACE_CGCONTEXT);
  CGContextRetain(mCg);

  mSize.width = CGBitmapContextGetWidth(mCg);
  mSize.height = CGBitmapContextGetHeight(mCg);
  mStride = CGBitmapContextGetBytesPerRow(mCg);
  mData = CGBitmapContextGetData(mCg);

  mImage = NULL;
}

void SourceSurfaceCGBitmapContext::EnsureImage() const
{
  if (!mImage) {
    if (mCg) {
      mImage = CGBitmapContextCreateImage(mCg);
    } else {
      //XXX: we should avoid creating this colorspace everytime
      CGColorSpaceRef colorSpace = NULL;
      CGBitmapInfo bitinfo = 0;
      CGDataProviderRef dataProvider = NULL;
      int bitsPerComponent = 8;
      int bitsPerPixel = 32;

      colorSpace = CGColorSpaceCreateDeviceRGB();
      bitinfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;

      dataProvider = CGDataProviderCreateWithData (mData,
                                                   mData,
                                                   mSize.height * mStride,
                                                   releaseCallback);

      mImage = CGImageCreate (mSize.width, mSize.height,
                              bitsPerComponent,
                              bitsPerPixel,
                              mStride,
                              colorSpace,
                              bitinfo,
                              dataProvider,
                              NULL,
                              true,
                              kCGRenderingIntentDefault);

      CGDataProviderRelease(dataProvider);
      CGColorSpaceRelease (colorSpace);
    }
  }
}

IntSize
SourceSurfaceCGBitmapContext::GetSize() const
{
  return mSize;
}

void
SourceSurfaceCGBitmapContext::DrawTargetWillChange()
{
  if (mDrawTarget) {
    size_t stride = CGBitmapContextGetBytesPerRow(mCg);
    size_t height = CGBitmapContextGetHeight(mCg);
    //XXX: infalliable malloc?
    mData = malloc(stride * height);
    memcpy(mData, CGBitmapContextGetData(mCg), stride*height);
    CGContextRelease(mCg);
    mCg = NULL;
    mDrawTarget = NULL;
  }
}

SourceSurfaceCGBitmapContext::~SourceSurfaceCGBitmapContext()
{
  if (!mImage && !mCg) {
    // neither mImage or mCg owns the data
    free(mData);
  }
  if (mCg)
    CGContextRelease(mCg);
  if (mImage)
    CGImageRelease(mImage);
}

}
}
