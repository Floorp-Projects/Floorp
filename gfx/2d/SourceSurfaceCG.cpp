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
 *   Jeff Muizelaar <jmuizelaar@mozilla.com>
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

#include "SourceSurfaceCG.h"

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



}
}
