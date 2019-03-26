/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "YUVBufferGenerator.h"

using namespace mozilla::layers;
using namespace mozilla;

void YUVBufferGenerator::Init(const mozilla::gfx::IntSize& aSize) {
  mImageSize = aSize;

  int yPlaneLen = aSize.width * aSize.height;
  int cbcrPlaneLen = (yPlaneLen + 1) / 2;
  int frameLen = yPlaneLen + cbcrPlaneLen;

  // Generate source buffer.
  mSourceBuffer.SetLength(frameLen);

  // Fill Y plane.
  memset(mSourceBuffer.Elements(), 0x10, yPlaneLen);

  // Fill Cb/Cr planes.
  memset(mSourceBuffer.Elements() + yPlaneLen, 0x80, cbcrPlaneLen);
}

mozilla::gfx::IntSize YUVBufferGenerator::GetSize() const { return mImageSize; }

already_AddRefed<Image> YUVBufferGenerator::GenerateI420Image() {
  return do_AddRef(CreateI420Image());
}

already_AddRefed<Image> YUVBufferGenerator::GenerateNV12Image() {
  return do_AddRef(CreateNV12Image());
}

already_AddRefed<Image> YUVBufferGenerator::GenerateNV21Image() {
  return do_AddRef(CreateNV21Image());
}

Image* YUVBufferGenerator::CreateI420Image() {
  PlanarYCbCrImage* image =
      new RecyclingPlanarYCbCrImage(new BufferRecycleBin());
  PlanarYCbCrData data;
  data.mPicSize = mImageSize;

  const uint32_t yPlaneSize = mImageSize.width * mImageSize.height;
  const uint32_t halfWidth = (mImageSize.width + 1) / 2;
  const uint32_t halfHeight = (mImageSize.height + 1) / 2;
  const uint32_t uvPlaneSize = halfWidth * halfHeight;

  // Y plane.
  uint8_t* y = mSourceBuffer.Elements();
  data.mYChannel = y;
  data.mYSize.width = mImageSize.width;
  data.mYSize.height = mImageSize.height;
  data.mYStride = mImageSize.width;
  data.mYSkip = 0;

  // Cr plane.
  uint8_t* cr = y + yPlaneSize + uvPlaneSize;
  data.mCrChannel = cr;
  data.mCrSkip = 0;

  // Cb plane
  uint8_t* cb = y + yPlaneSize;
  data.mCbChannel = cb;
  data.mCbSkip = 0;

  // CrCb plane vectors.
  data.mCbCrStride = halfWidth;
  data.mCbCrSize.width = halfWidth;
  data.mCbCrSize.height = halfHeight;

  image->CopyData(data);
  return image;
}

Image* YUVBufferGenerator::CreateNV12Image() {
  NVImage* image = new NVImage();
  PlanarYCbCrData data;
  data.mPicSize = mImageSize;

  const uint32_t yPlaneSize = mImageSize.width * mImageSize.height;
  const uint32_t halfWidth = (mImageSize.width + 1) / 2;
  const uint32_t halfHeight = (mImageSize.height + 1) / 2;

  // Y plane.
  uint8_t* y = mSourceBuffer.Elements();
  data.mYChannel = y;
  data.mYSize.width = mImageSize.width;
  data.mYSize.height = mImageSize.height;
  data.mYStride = mImageSize.width;
  data.mYSkip = 0;

  // Cr plane.
  uint8_t* cr = y + yPlaneSize;
  data.mCrChannel = cr;
  data.mCrSkip = 1;

  // Cb plane
  uint8_t* cb = y + yPlaneSize + 1;
  data.mCbChannel = cb;
  data.mCbSkip = 1;

  // 4:2:0.
  data.mCbCrStride = mImageSize.width;
  data.mCbCrSize.width = halfWidth;
  data.mCbCrSize.height = halfHeight;

  image->SetData(data);
  return image;
}

Image* YUVBufferGenerator::CreateNV21Image() {
  NVImage* image = new NVImage();
  PlanarYCbCrData data;
  data.mPicSize = mImageSize;

  const uint32_t yPlaneSize = mImageSize.width * mImageSize.height;
  const uint32_t halfWidth = (mImageSize.width + 1) / 2;
  const uint32_t halfHeight = (mImageSize.height + 1) / 2;

  // Y plane.
  uint8_t* y = mSourceBuffer.Elements();
  data.mYChannel = y;
  data.mYSize.width = mImageSize.width;
  data.mYSize.height = mImageSize.height;
  data.mYStride = mImageSize.width;
  data.mYSkip = 0;

  // Cr plane.
  uint8_t* cr = y + yPlaneSize + 1;
  data.mCrChannel = cr;
  data.mCrSkip = 1;

  // Cb plane
  uint8_t* cb = y + yPlaneSize;
  data.mCbChannel = cb;
  data.mCbSkip = 1;

  // 4:2:0.
  data.mCbCrStride = mImageSize.width;
  data.mCbCrSize.width = halfWidth;
  data.mCbCrSize.height = halfHeight;

  image->SetData(data);
  return image;
}
