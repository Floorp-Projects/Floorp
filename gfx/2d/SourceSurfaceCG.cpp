/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceSurfaceCG.h"
#include "DrawTargetCG.h"
#include "DataSourceSurfaceWrapper.h"
#include "DataSurfaceHelpers.h"
#include "mozilla/Types.h" // for decltype

#include "MacIOSurface.h"
#include "Tools.h"

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
  RefPtr<DataSourceSurface> dataSurf = new DataSourceSurfaceCG(mImage);

  // We also need to make sure that the returned surface has
  // surface->GetType() == SurfaceType::DATA.
  return new DataSourceSurfaceWrapper(dataSurf);
}

static void releaseCallback(void *info, const void *data, size_t size) {
  free(info);
}

CGImageRef
CreateCGImage(void *aInfo,
              const void *aData,
              const IntSize &aSize,
              int32_t aStride,
              SurfaceFormat aFormat)
{
  //XXX: we should avoid creating this colorspace everytime
  CGColorSpaceRef colorSpace = nullptr;
  CGBitmapInfo bitinfo = 0;
  int bitsPerComponent = 0;
  int bitsPerPixel = 0;

  switch (aFormat) {
    case SurfaceFormat::B8G8R8A8:
      colorSpace = CGColorSpaceCreateDeviceRGB();
      bitinfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
      bitsPerComponent = 8;
      bitsPerPixel = 32;
      break;

    case SurfaceFormat::B8G8R8X8:
      colorSpace = CGColorSpaceCreateDeviceRGB();
      bitinfo = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host;
      bitsPerComponent = 8;
      bitsPerPixel = 32;
      break;

    case SurfaceFormat::A8:
      // XXX: why don't we set a colorspace here?
      bitsPerComponent = 8;
      bitsPerPixel = 8;
      break;

    default:
      MOZ_CRASH();
  }

  size_t bufLen = BufferSizeFromStrideAndHeight(aStride, aSize.height);
  if (bufLen == 0) {
    return nullptr;
  }
  CGDataProviderRef dataProvider = CGDataProviderCreateWithData(aInfo,
                                                                aData,
                                                                bufLen,
                                                                releaseCallback);

  CGImageRef image;
  if (aFormat == SurfaceFormat::A8) {
    CGFloat decode[] = {1.0, 0.0};
    image = CGImageMaskCreate (aSize.width, aSize.height,
                               bitsPerComponent,
                               bitsPerPixel,
                               aStride,
                               dataProvider,
                               decode,
                               true);
  } else {
    image = CGImageCreate (aSize.width, aSize.height,
                           bitsPerComponent,
                           bitsPerPixel,
                           aStride,
                           colorSpace,
                           bitinfo,
                           dataProvider,
                           nullptr,
                           true,
                           kCGRenderingIntentDefault);
  }

  CGDataProviderRelease(dataProvider);
  CGColorSpaceRelease(colorSpace);

  return image;
}

bool
SourceSurfaceCG::InitFromData(unsigned char *aData,
                               const IntSize &aSize,
                               int32_t aStride,
                               SurfaceFormat aFormat)
{
  assert(aSize.width >= 0 && aSize.height >= 0);

  size_t bufLen = BufferSizeFromStrideAndHeight(aStride, aSize.height);
  if (bufLen == 0) {
    mImage = nullptr;
    return false;
  }

  void *data = malloc(bufLen);
  // Copy all the data except the stride padding on the very last
  // row since we can't guarantee that is readable.
  memcpy(data, aData, bufLen - aStride + (aSize.width * BytesPerPixel(aFormat)));

  mFormat = aFormat;
  mImage = CreateCGImage(data, data, aSize, aStride, aFormat);

  return mImage != nullptr;
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
  if (aSize.width <= 0 || aSize.height <= 0) {
    return false;
  }

  size_t bufLen = BufferSizeFromStrideAndHeight(aStride, aSize.height);
  if (bufLen == 0) {
    mImage = nullptr;
    return false;
  }

  void *data = malloc(bufLen);
  memcpy(data, aData, bufLen - aStride + (aSize.width * BytesPerPixel(aFormat)));

  mFormat = aFormat;
  mImage = CreateCGImage(data, data, aSize, aStride, aFormat);

  if (!mImage) {
    free(data);
    return false;
  }

  return true;
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

  // we'd like to pass nullptr as the first parameter
  // to let Quartz manage this memory for us. However,
  // on 10.5 and older CGBitmapContextGetData will return
  // nullptr instead of the associated buffer so we need
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
  mFormat = SurfaceFormat::B8G8R8A8;
  mImage = aImage;
  mCg = CreateBitmapContextForImage(aImage);
  if (mCg == nullptr) {
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
  mFormat = aDrawTarget->GetFormat();
  mCg = (CGContextRef)aDrawTarget->GetNativeSurface(NativeSurfaceType::CGCONTEXT);
  if (!mCg)
    abort();

  mSize.width = CGBitmapContextGetWidth(mCg);
  mSize.height = CGBitmapContextGetHeight(mCg);
  mStride = CGBitmapContextGetBytesPerRow(mCg);
  mData = CGBitmapContextGetData(mCg);

  mImage = nullptr;
}

void SourceSurfaceCGBitmapContext::EnsureImage() const
{
  // Instead of using CGBitmapContextCreateImage we create
  // a CGImage around the data associated with the CGBitmapContext
  // we do this to avoid the vm_copy that CGBitmapContextCreateImage.
  // vm_copy tends to cause all sorts of unexpected performance problems
  // because of the mm tricks that vm_copy does. Using a regular
  // memcpy when the bitmap context is modified gives us more predictable
  // performance characteristics.
  if (!mImage) {
    if (!mData) abort();
    mImage = CreateCGImage(nullptr, mData, mSize, mStride, mFormat);
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
    // This will break the weak reference we hold to mCg
    size_t stride = CGBitmapContextGetBytesPerRow(mCg);
    size_t height = CGBitmapContextGetHeight(mCg);

    size_t bufLen = BufferSizeFromStrideAndHeight(stride, height);
    if (bufLen == 0) {
      mDataHolder.Dealloc();
      mData = nullptr;
    } else {
      static_assert(sizeof(decltype(mDataHolder[0])) == 1,
                    "mDataHolder.Realloc() takes an object count, so its objects must be 1-byte sized if we use bufLen");
      mDataHolder.Realloc(/* actually an object count */ bufLen);
      mData = mDataHolder;

      // copy out the data from the CGBitmapContext
      // we'll maintain ownership of mData until
      // we transfer it to mImage
      memcpy(mData, CGBitmapContextGetData(mCg), bufLen);
    }

    // drop the current image for the data associated with the CGBitmapContext
    if (mImage)
      CGImageRelease(mImage);
    mImage = nullptr;

    mCg = nullptr;
    mDrawTarget = nullptr;
  }
}

SourceSurfaceCGBitmapContext::~SourceSurfaceCGBitmapContext()
{
  if (mImage)
    CGImageRelease(mImage);
}

SourceSurfaceCGIOSurfaceContext::SourceSurfaceCGIOSurfaceContext(DrawTargetCG *aDrawTarget)
{
  CGContextRef cg = (CGContextRef)aDrawTarget->GetNativeSurface(NativeSurfaceType::CGCONTEXT_ACCELERATED);

  RefPtr<MacIOSurface> surf = MacIOSurface::IOSurfaceContextGetSurface(cg);

  mFormat = aDrawTarget->GetFormat();
  mSize.width = surf->GetWidth();
  mSize.height = surf->GetHeight();

  // TODO use CreateImageFromIOSurfaceContext instead of reading back the surface
  //mImage = MacIOSurface::CreateImageFromIOSurfaceContext(cg);
  mImage = nullptr;

  aDrawTarget->Flush();
  surf->Lock();
  size_t bytesPerRow = surf->GetBytesPerRow();
  size_t ioHeight = surf->GetHeight();
  void* ioData = surf->GetBaseAddress();
  // XXX If the width is much less then the stride maybe
  //     we should repack the image?
  mData = malloc(ioHeight*bytesPerRow);
  memcpy(mData, ioData, ioHeight*(bytesPerRow));
  mStride = bytesPerRow;
  surf->Unlock();
}

void SourceSurfaceCGIOSurfaceContext::EnsureImage() const
{
  // TODO Use CreateImageFromIOSurfaceContext and remove this

  // Instead of using CGBitmapContextCreateImage we create
  // a CGImage around the data associated with the CGBitmapContext
  // we do this to avoid the vm_copy that CGBitmapContextCreateImage.
  // vm_copy tends to cause all sorts of unexpected performance problems
  // because of the mm tricks that vm_copy does. Using a regular
  // memcpy when the bitmap context is modified gives us more predictable
  // performance characteristics.
  if (!mImage) {
    mImage = CreateCGImage(mData, mData, mSize, mStride, SurfaceFormat::B8G8R8A8);
  }

}

IntSize
SourceSurfaceCGIOSurfaceContext::GetSize() const
{
  return mSize;
}

void
SourceSurfaceCGIOSurfaceContext::DrawTargetWillChange()
{
}

SourceSurfaceCGIOSurfaceContext::~SourceSurfaceCGIOSurfaceContext()
{
  if (mImage)
    CGImageRelease(mImage);
  else
    free(mData);
}

unsigned char*
SourceSurfaceCGIOSurfaceContext::GetData()
{
  return (unsigned char*)mData;
}

}
}
