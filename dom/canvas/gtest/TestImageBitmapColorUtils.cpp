/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "ImageBitmapColorUtils.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "libyuv.h"

using namespace mozilla::dom;

template<typename T>
class Image {
public:
  explicit Image(int aChannelCount)
  : mChannelCount(aChannelCount), mData(nullptr)
  {
  }

  virtual ~Image() {
    if (mData) delete[] mData;
  }

  int mChannelCount;
  T* mData;
};

template<typename T>
class SimpleImage : public Image<T>
{
public:
  SimpleImage(int aWidth, int aHeight, int aChannelCount)
  : Image<T>(aChannelCount)
  , mWidth(aWidth)
  , mHeight(aHeight)
  , mStride(aWidth * aChannelCount * sizeof(T))
  {
    Image<T>::mData = new T[mHeight * mStride];
  }

  virtual ~SimpleImage() {}

  T GetPixelValue(int i, int j, int c) const
  {
    // The unit of mStride is byte.
    const uint8_t* data = (const uint8_t*)Image<T>::mData;
    return *((T*)(data + i * mStride) + j * Image<T>::mChannelCount + c);
  }

  int mWidth;
  int mHeight;
  int mStride;
};

class RGBA32DataSinglePixel : public SimpleImage<uint8_t> {
public:
  RGBA32DataSinglePixel(int r, int g, int b, int a)
  : SimpleImage(1, 1, 4)
  {
    mData[0] = r;
    mData[1] = g;
    mData[2] = b;
    mData[3] = a;
  }
};

class RGB24Data : public SimpleImage<uint8_t> {
public:
  RGB24Data()
  : SimpleImage(3, 3, 3)
  {
    int i = 0;
    mData[i + 0] = 0;   mData[i + 1] = 0;   mData[i + 2] = 0;
    mData[i + 3] = 255; mData[i + 4] = 0;   mData[i + 5] = 0;
    mData[i + 6] = 0;   mData[i + 7] = 255; mData[i + 8] = 0;

    i = 1 * mStride;
    mData[i + 0] = 0;   mData[i + 1] = 0;   mData[i + 2] = 255;
    mData[i + 3] = 255; mData[i + 4] = 255; mData[i + 5] = 0;
    mData[i + 6] = 0;   mData[i + 7] = 255; mData[i + 8] = 255;

    i = 2 * mStride;
    mData[i + 0] = 255; mData[i + 1] = 0;   mData[i + 2] = 255;
    mData[i + 3] = 255; mData[i + 4] = 255; mData[i + 5] = 255;
    mData[i + 6] = 128; mData[i + 7] = 128; mData[i + 8] = 128;
  }
};

class BGR24Data : public SimpleImage<uint8_t> {
public:
  BGR24Data()
  : SimpleImage(3, 3, 3)
  {
    int i = 0;
    mData[i + 2] = 0;   mData[i + 1] = 0;   mData[i + 0] = 0;
    mData[i + 5] = 255; mData[i + 4] = 0;   mData[i + 3] = 0;
    mData[i + 8] = 0;   mData[i + 7] = 255; mData[i + 6] = 0;

    i = 1 * mStride;
    mData[i + 2] = 0;   mData[i + 1] = 0;   mData[i + 0] = 255;
    mData[i + 5] = 255; mData[i + 4] = 255; mData[i + 3] = 0;
    mData[i + 8] = 0;   mData[i + 7] = 255; mData[i + 6] = 255;

    i = 2 * mStride;
    mData[i + 2] = 255; mData[i + 1] = 0;   mData[i + 0] = 255;
    mData[i + 5] = 255; mData[i + 4] = 255; mData[i + 3] = 255;
    mData[i + 8] = 128; mData[i + 7] = 128; mData[i + 6] = 128;
  }
};

class RGBA32Data : public SimpleImage<uint8_t> {
public:
  RGBA32Data()
  : SimpleImage(3, 3, 4)
  {
    int i = 0;
    mData[i + 0] = 0;   mData[i + 1] = 0;   mData[i + 2] = 0;   mData[i + 3] = 255;
    mData[i + 4] = 255; mData[i + 5] = 0;   mData[i + 6] = 0;   mData[i + 7] = 255;
    mData[i + 8] = 0;   mData[i + 9] = 255; mData[i + 10] = 0;  mData[i + 11] = 255;

    i = 1 * mStride;
    mData[i + 0] = 0;   mData[i + 1] = 0;   mData[i + 2] = 255;  mData[i + 3] = 255;
    mData[i + 4] = 255; mData[i + 5] = 255; mData[i + 6] = 0;    mData[i + 7] = 255;
    mData[i + 8] = 0;   mData[i + 9] = 255; mData[i + 10] = 255; mData[i + 11] = 255;

    i = 2 * mStride;
    mData[i + 0] = 255; mData[i + 1] = 0;   mData[i + 2] = 255;  mData[i + 3] = 255;
    mData[i + 4] = 255; mData[i + 5] = 255; mData[i + 6] = 255;  mData[i + 7] = 255;
    mData[i + 8] = 128; mData[i + 9] = 128; mData[i + 10] = 128; mData[i + 11] = 255;
  }
};

class BGRA32Data : public SimpleImage<uint8_t> {
public:
  BGRA32Data()
  : SimpleImage(3, 3, 4)
  {
    int i = 0;
    mData[i + 2] = 0;   mData[i + 1] = 0;   mData[i + 0] = 0;   mData[i + 3] = 255;
    mData[i + 6] = 255; mData[i + 5] = 0;   mData[i + 4] = 0;   mData[i + 7] = 255;
    mData[i + 10] = 0;  mData[i + 9] = 255; mData[i + 8] = 0;   mData[i + 11] = 255;

    i = 1 * mStride;
    mData[i + 2] = 0;   mData[i + 1] = 0;   mData[i + 0] = 255;  mData[i + 3] = 255;
    mData[i + 6] = 255; mData[i + 5] = 255; mData[i + 4] = 0;    mData[i + 7] = 255;
    mData[i + 10] = 0;  mData[i + 9] = 255; mData[i + 8] = 255;  mData[i + 11] = 255;

    i = 2 * mStride;
    mData[i + 2] = 255;  mData[i + 1] = 0;   mData[i + 0] = 255; mData[i + 3] = 255;
    mData[i + 6] = 255;  mData[i + 5] = 255; mData[i + 4] = 255; mData[i + 7] = 255;
    mData[i + 10] = 128; mData[i + 9] = 128; mData[i + 8] = 128; mData[i + 11] = 255;
  }
};

class RGBA32Image : public SimpleImage<uint8_t>
{
public:
  RGBA32Image(int aWidth, int aHeight, int aChannelCount)
  : SimpleImage(aWidth, aHeight, aChannelCount)
  {}

  const uint8_t* GetPixel(int x, int y) const {
    return mData + mStride * y + x * mChannelCount;
  }
};

class RGBA32DataFromYUV444PData : public RGBA32Image {
public:
  RGBA32DataFromYUV444PData()
  : RGBA32Image(3, 3, 4)
  {
//    libyuv: (16, 128, 128) --> (0, 0, 0, 255)
//    libyuv: (82, 90, 240) --> (254, 0, 0, 255)
//    libyuv: (144, 54, 34) --> (0, 253, 1, 255)
//    libyuv: (41, 240, 110) --> (0, 0, 251, 255)
//    libyuv: (210, 16, 146) --> (253, 253, 2, 255)
//    libyuv: (169, 166, 16) --> (0, 253, 252, 255)
//    libyuv: (107, 202, 222) --> (255, 0, 252, 255)
//    libyuv: (235, 128, 128) --> (253, 253, 253, 255)
//    libyuv: (126, 128, 128) --> (127, 127, 127, 255)

    int i = 0;
    mData[i + 0] = 0;   mData[i + 1] = 0;   mData[i + 2] = 0;    mData[i + 3] = 255;
    mData[i + 4] = 254; mData[i + 5] = 0;   mData[i + 6] = 0;    mData[i + 7] = 255;
    mData[i + 8] = 0;   mData[i + 9] = 253; mData[i + 10] = 1;   mData[i + 11] = 255;

    i = 1 * mStride;
    mData[i + 0] = 0;   mData[i + 1] = 0;   mData[i + 2] = 251;  mData[i + 3] = 255;
    mData[i + 4] = 253; mData[i + 5] = 253; mData[i + 6] = 2;    mData[i + 7] = 255;
    mData[i + 8] = 0;   mData[i + 9] = 253; mData[i + 10] = 252; mData[i + 11] = 255;

    i = 2 * mStride;
    mData[i + 0] = 255; mData[i + 1] = 0;   mData[i + 2] = 252;  mData[i + 3] = 255;
    mData[i + 4] = 253; mData[i + 5] = 253; mData[i + 6] = 253;  mData[i + 7] = 255;
    mData[i + 8] = 127; mData[i + 9] = 127; mData[i + 10] = 127; mData[i + 11] = 255;
  }
};

class RGBA32DataFromYUV422PData : public RGBA32Image {
public:
  RGBA32DataFromYUV422PData()
  : RGBA32Image(3, 3, 4)
  {
//    libyuv: (16, 109, 184) --> (89, 0, 0, 255)
//    libyuv: (82, 109, 184) --> (165, 38, 38, 255)
//    libyuv: (144, 54, 34) --> (0, 253, 1, 255)
//    libyuv: (41, 128, 128) --> (28, 28, 28, 255)
//    libyuv: (210, 128, 128) --> (224, 224, 224, 255)
//    libyuv: (169, 166, 16) --> (0, 253, 252, 255)
//    libyuv: (107, 165, 175) --> (180, 52, 178, 255)
//    libyuv: (235, 165, 175) --> (255, 200, 255, 255)
//    libyuv: (126, 128, 128) --> (127, 127, 127, 255)

    int i = 0;
    mData[i + 0] = 89;  mData[i + 1] = 0;   mData[i + 2] = 0;    mData[i + 3] = 255;
    mData[i + 4] = 165; mData[i + 5] = 38;  mData[i + 6] = 38;   mData[i + 7] = 255;
    mData[i + 8] = 0;   mData[i + 9] = 253; mData[i + 10] = 1;   mData[i + 11] = 255;

    i = 1 * mStride;
    mData[i + 0] = 28;  mData[i + 1] = 28;  mData[i + 2] = 28;   mData[i + 3] = 255;
    mData[i + 4] = 224; mData[i + 5] = 224; mData[i + 6] = 224;  mData[i + 7] = 255;
    mData[i + 8] = 0;   mData[i + 9] = 253; mData[i + 10] = 252; mData[i + 11] = 255;

    i = 2 * mStride;
    mData[i + 0] = 180; mData[i + 1] = 52;  mData[i + 2] = 178;  mData[i + 3] = 255;
    mData[i + 4] = 255; mData[i + 5] = 200; mData[i + 6] = 255;  mData[i + 7] = 255;
    mData[i + 8] = 127; mData[i + 9] = 127; mData[i + 10] = 127; mData[i + 11] = 255;
  }
};

class RGBA32DataFromYUV420PData : public RGBA32Image {
public:
  RGBA32DataFromYUV420PData()
  : RGBA32Image(3, 3, 4)
  {
//    libyuv: (16, 119, 156) --> (44, 0, 0, 255)
//    libyuv: (82, 119, 156) --> (120, 57, 58, 255)
//    libyuv: (144, 110, 25) --> (0, 238, 112, 255)
//    libyuv: (41, 119, 156) --> (73, 9, 11, 255)
//    libyuv: (210, 119, 156) --> (255, 205, 206, 255)
//    libyuv: (169, 110, 25) --> (12, 255, 141, 255)
//    libyuv: (107, 165, 175) --> (180, 52, 178, 255)
//    libyuv: (235, 165, 175) --> (255, 200, 255, 255)
//    libyuv: (126, 128, 128) --> (127, 127, 127, 255)

    int i = 0;
    mData[i + 0] = 44;  mData[i + 1] = 0;   mData[i + 2] = 0;    mData[i + 3] = 255;
    mData[i + 4] = 120; mData[i + 5] = 57;  mData[i + 6] = 58;   mData[i + 7] = 255;
    mData[i + 8] = 0;   mData[i + 9] = 238; mData[i + 10] = 112;   mData[i + 11] = 255;

    i = 1 * mStride;
    mData[i + 0] = 73;  mData[i + 1] = 9;  mData[i + 2] = 11;   mData[i + 3] = 255;
    mData[i + 4] = 255; mData[i + 5] = 205; mData[i + 6] = 206;  mData[i + 7] = 255;
    mData[i + 8] = 12;   mData[i + 9] = 255; mData[i + 10] = 141; mData[i + 11] = 255;

    i = 2 * mStride;
    mData[i + 0] = 180; mData[i + 1] = 52;  mData[i + 2] = 178;  mData[i + 3] = 255;
    mData[i + 4] = 255; mData[i + 5] = 200; mData[i + 6] = 255;  mData[i + 7] = 255;
    mData[i + 8] = 127; mData[i + 9] = 127; mData[i + 10] = 127; mData[i + 11] = 255;
  }
};

class GrayData : public SimpleImage<uint8_t> {
public:
  GrayData()
  : SimpleImage(3, 3, 1)
  {
    int i = 0;
    mData[i + 0] = 0;   mData[i + 1] = 76;   mData[i + 2] = 149;

    i = 1 * mStride;
    mData[i + 0] = 29;   mData[i + 1] = 225;   mData[i + 2] = 178;

    i = 2 * mStride;
    mData[i + 0] = 105; mData[i + 1] = 255;   mData[i + 2] = 127;
  }
};

class YUVImage : public Image<uint8_t>
{
public:
  YUVImage(int aYWidth, int aYHeight, int aYStride,
           int aUWidth, int aUHeight, int aUStride,
           int aVWidth, int aVHeight, int aVStride)
  : Image(3)
  , mYWidth(aYWidth), mYHeight(aYHeight), mYStride(aYStride)
  , mUWidth(aUWidth), mUHeight(aUHeight), mUStride(aUStride)
  , mVWidth(aVWidth), mVHeight(aVHeight), mVStride(aVStride)
  {
    mData = new uint8_t[mYHeight * mYStride + mUHeight * mUStride + mVHeight * mVStride];
  }

  uint8_t* YBuffer() const { return mData; }
  uint8_t* UBuffer() const { return YBuffer() + mYHeight * mYStride; }
  uint8_t* VBuffer() const { return UBuffer() + mUHeight * mUStride; }

  int mYWidth;
  int mYHeight;
  int mYStride;
  int mUWidth;
  int mUHeight;
  int mUStride;
  int mVWidth;
  int mVHeight;
  int mVStride;
};

class NVImage : public Image<uint8_t>
{
public:
  NVImage(int aYWidth, int aYHeight, int aYStride,
          int aUVWidth, int aUVHeight, int aUVStride)
  : Image(3)
  , mYWidth(aYWidth), mYHeight(aYHeight), mYStride(aYStride)
  , mUVWidth(aUVWidth), mUVHeight(aUVHeight), mUVStride(aUVStride)
  {
    mData = new uint8_t[mYHeight * mYStride + mUVHeight * mUVStride];
  }

  uint8_t* YBuffer() const { return mData; }
  uint8_t* UVBuffer() const { return YBuffer() + mYHeight * mYStride; }

  int mYWidth;
  int mYHeight;
  int mYStride;
  int mUVWidth;
  int mUVHeight;
  int mUVStride;
};

class YUV444PData : public YUVImage
{
public:
  YUV444PData()
  :YUVImage(3, 3, 3, 3, 3, 3, 3, 3, 3)
  {
//    libyuv: (0, 0, 0) --> (16, 128, 128)
//    libyuv: (255, 0, 0) --> (82, 90, 240)
//    libyuv: (0, 255, 0) --> (144, 54, 34)
//    libyuv: (0, 0, 255) --> (41, 240, 110)
//    libyuv: (255, 255, 0) --> (210, 16, 146)
//    libyuv: (0, 255, 255) --> (169, 166, 16)
//    libyuv: (255, 0, 255) --> (107, 202, 222)
//    libyuv: (255, 255, 255) --> (235, 128, 128)
//    libyuv: (128, 128, 128) --> (126, 128, 128)

    YBuffer()[0] = 16;  YBuffer()[1] = 82;  YBuffer()[2] = 144;
    YBuffer()[3] = 41;  YBuffer()[4] = 210; YBuffer()[5] = 169;
    YBuffer()[6] = 107; YBuffer()[7] = 235; YBuffer()[8] = 126;

    UBuffer()[0] = 128; UBuffer()[1] = 90;  UBuffer()[2] = 54;
    UBuffer()[3] = 240; UBuffer()[4] = 16;  UBuffer()[5] = 166;
    UBuffer()[6] = 202; UBuffer()[7] = 128; UBuffer()[8] = 128;

    VBuffer()[0] = 128; VBuffer()[1] = 240; VBuffer()[2] = 34;
    VBuffer()[3] = 110; VBuffer()[4] = 146; VBuffer()[5] = 16;
    VBuffer()[6] = 222; VBuffer()[7] = 128; VBuffer()[8] = 128;
  }
};

class YUV422PData : public YUVImage
{
public:
  YUV422PData()
  :YUVImage(3, 3, 3, 2, 3, 2, 2, 3, 2)
  {
    YBuffer()[0] = 16;  YBuffer()[1] = 82;  YBuffer()[2] = 144;
    YBuffer()[3] = 41;  YBuffer()[4] = 210; YBuffer()[5] = 169;
    YBuffer()[6] = 107; YBuffer()[7] = 235; YBuffer()[8] = 126;

    UBuffer()[0] = 109; UBuffer()[1] = 54;
    UBuffer()[2] = 128; UBuffer()[3] = 166;
    UBuffer()[4] = 165; UBuffer()[5] = 128;

    VBuffer()[0] = 184; VBuffer()[1] = 34;
    VBuffer()[2] = 128; VBuffer()[3] = 16;
    VBuffer()[4] = 175; VBuffer()[5] = 128;
  }
};

class YUV420PData : public YUVImage
{
public:
  YUV420PData()
  :YUVImage(3, 3, 3, 2, 2, 2, 2, 2, 2)
  {
    YBuffer()[0] = 16;  YBuffer()[1] = 82;  YBuffer()[2] = 144;
    YBuffer()[3] = 41;  YBuffer()[4] = 210; YBuffer()[5] = 169;
    YBuffer()[6] = 107; YBuffer()[7] = 235; YBuffer()[8] = 126;

    UBuffer()[0] = 119; UBuffer()[1] = 110;
    UBuffer()[2] = 165; UBuffer()[3] = 128;

    VBuffer()[0] = 156; VBuffer()[1] = 25;
    VBuffer()[2] = 175; VBuffer()[3] = 128;
  }
};

class NV12Data : public NVImage
{
public:
  NV12Data()
  :NVImage(3, 3, 3, 2, 2, 4)
  {
    YBuffer()[0] = 16;  YBuffer()[1] = 82;  YBuffer()[2] = 144;
    YBuffer()[3] = 41;  YBuffer()[4] = 210; YBuffer()[5] = 169;
    YBuffer()[6] = 107; YBuffer()[7] = 235; YBuffer()[8] = 126;

    // U
    UVBuffer()[0] = 119;
    UVBuffer()[2] = 110;
    UVBuffer()[4] = 165;
    UVBuffer()[6] = 128;

    // V
    UVBuffer()[1] = 156;
    UVBuffer()[3] = 25;
    UVBuffer()[5] = 175;
    UVBuffer()[7] = 128;
  }
};

class NV21Data : public NVImage
{
public:
  NV21Data()
  :NVImage(3, 3, 3, 2, 2, 4)
  {
    YBuffer()[0] = 16;  YBuffer()[1] = 82;  YBuffer()[2] = 144;
    YBuffer()[3] = 41;  YBuffer()[4] = 210; YBuffer()[5] = 169;
    YBuffer()[6] = 107; YBuffer()[7] = 235; YBuffer()[8] = 126;

    // U
    UVBuffer()[1] = 119;
    UVBuffer()[3] = 110;
    UVBuffer()[5] = 165;
    UVBuffer()[7] = 128;

    // V
    UVBuffer()[0] = 156;
    UVBuffer()[2] = 25;
    UVBuffer()[4] = 175;
    UVBuffer()[6] = 128;
  }
};

class HSVData : public SimpleImage<float>
{
public:
  HSVData()
  : SimpleImage(3, 3, 3)
  {
    int i = 0;
    mData[i + 0] = 0.0f;   mData[i + 1] = 0.0f; mData[i + 2] = 0.0f;
    mData[i + 3] = 0.0f;   mData[i + 4] = 1.0f; mData[i + 5] = 1.0f;
    mData[i + 6] = 120.0f; mData[i + 7] = 1.0f; mData[i + 8] = 1.0f;

    i += mWidth * mChannelCount;
    mData[i + 0] = 240.0f; mData[i + 1] = 1.0f; mData[i + 2] = 1.0f;
    mData[i + 3] = 60.0f;  mData[i + 4] = 1.0f; mData[i + 5] = 1.0f;
    mData[i + 6] = 180.0f; mData[i + 7] = 1.0f; mData[i + 8] = 1.0f;

    i += mWidth * mChannelCount;
    mData[i + 0] = 300.0f; mData[i + 1] = 1.0f; mData[i + 2] = 1.0f;
    mData[i + 3] = 0.0f;   mData[i + 4] = 0.0f; mData[i + 5] = 1.0f;
    mData[i + 6] = 0.0f;   mData[i + 7] = 0.0f; mData[i + 8] = 0.50196081f;
  }
};

class LabData : public SimpleImage<float>
{
public:
  LabData()
  : SimpleImage(3, 3, 3)
  {
    int i = 0;
    mData[i + 0] = 0.0f;        mData[i + 1] = 0.0f;         mData[i + 2] = 0.0f;
    mData[i + 3] = 53.240585f;  mData[i + 4] = 80.094185f;   mData[i + 5] = 67.201538f;
    mData[i + 6] = 87.7351f;    mData[i + 7] = -86.181252f;  mData[i + 8] = 83.177483f;

    i += mWidth * mChannelCount;
    mData[i + 0] = 32.29567f;   mData[i + 1] = 79.186989f;   mData[i + 2] = -107.86176f;
    mData[i + 3] = 97.139511f;  mData[i + 4] = -21.552414f;  mData[i + 5] = 94.475792f;
    mData[i + 6] = 91.113297f;  mData[i + 7] = -48.088551f;  mData[i + 8] = -14.130962f;

    i += mWidth * mChannelCount;
    mData[i + 0] = 60.323502f;  mData[i + 1] = 98.235161f;   mData[i + 2] = -60.825493f;
    mData[i + 3] = 100.0f;      mData[i + 4] = 0.0f;         mData[i + 5] = 0.0f;
    mData[i + 6] = 53.585014f;  mData[i + 7] = 0.0f;         mData[i + 8] = 0.0f;
  }
};

/*
 * From RGB24.
 */
TEST(ImageBitmapColorUtils, RGB24ToBGR24_)
{
  const RGB24Data srcData;
  const BGR24Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = RGB24ToBGR24(srcData.mData, srcData.mStride,
                        result_.mData, result_.mStride,
                        result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 2]);
    }
  }
}

TEST(ImageBitmapColorUtils, RGB24ToRGBA32)
{
  const RGB24Data srcData;
  const RGBA32Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = RGB24ToRGBA32(srcData.mData, srcData.mStride,
                         result_.mData, result_.mStride,
                         result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 3],
                255);
    }
  }
}

TEST(ImageBitmapColorUtils, RGB24ToBGRA32)
{
  const RGB24Data srcData;
  const BGRA32Data dstData;

  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = RGB24ToBGRA32(srcData.mData, srcData.mStride,
                         result_.mData, result_.mStride,
                         result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);


  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 3],
                255);
    }
  }
}


//static void tryLibyuv(int r, int g, int b)
//{
//  const RGBA32DataSinglePixel srcData(r, g, b, 255);
//  YUVImage result_(1, 1, 1, 1, 1, 1, 1, 1, 1);
//
//  int length = result_.mYHeight * result_.mYStride +
//               result_.mUHeight * result_.mUStride +
//               result_.mVHeight * result_.mVStride;
//
//  int rv = libyuv::ABGRToI420(srcData.mData, srcData.mStride,
//                              result_.YBuffer(), result_.mYStride,
//                              result_.UBuffer(), result_.mUStride,
//                              result_.VBuffer(), result_.mVStride,
//                              result_.mWidth, result_.mHeight);
//
//  printf_stderr("\nlibyuv: (%hhu, %hhu, %hhu) --> (%hhu, %hhu, %hhu) \n",
//                r, g, b,
//                result_.YBuffer()[0],
//                result_.UBuffer()[0],
//                result_.VBuffer()[0]);
//}

//static void tryLibyuv(int y, int u, int v)
//{
//  uint8_t* yuvPixel  = new uint8_t[3];
//  uint8_t* rgbaPixel = new uint8_t[4];
//
//  yuvPixel[0] = y;
//  yuvPixel[1] = u;
//  yuvPixel[2] = v;
//
//  int rv = libyuv::I420ToABGR(yuvPixel + 0, 1,
//                              yuvPixel + 1, 1,
//                              yuvPixel + 2, 1,
//                              rgbaPixel, 4,
//                              1, 1);
//
//  printf_stderr("\nlibyuv: (%hhu, %hhu, %hhu) --> (%hhu, %hhu, %hhu, %hhu) \n",
//                yuvPixel[0], yuvPixel[1], yuvPixel[2],
//                rgbaPixel[0], rgbaPixel[1], rgbaPixel[2], rgbaPixel[3]);
//}

TEST(ImageBitmapColorUtils, RGB24ToYUV444P)
{
  const RGB24Data srcData;
  const YUV444PData dstData;

  YUVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth, srcData.mHeight, srcData.mStride);

  int rv = RGB24ToYUV444P(srcData.mData, srcData.mStride,
                          result_.YBuffer(), result_.mYStride,
                          result_.UBuffer(), result_.mUStride,
                          result_.VBuffer(), result_.mVStride,
                          result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
      EXPECT_EQ(dstData.UBuffer()[i * dstData.mUStride + j],
                result_.UBuffer()[i * result_.mUStride + j]);
      EXPECT_EQ(dstData.VBuffer()[i * dstData.mVStride + j],
                result_.VBuffer()[i * result_.mVStride + j]);
    }
  }
}

TEST(ImageBitmapColorUtils, RGB24ToYUV422P)
{
  const RGB24Data srcData;
  const YUV422PData dstData;

  YUVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight, srcData.mStride);

  int rv = RGB24ToYUV422P(srcData.mData, srcData.mStride,
                          result_.YBuffer(), result_.mYStride,
                          result_.UBuffer(), result_.mUStride,
                          result_.VBuffer(), result_.mVStride,
                          result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUHeight; ++i) {
    for (int j = 0; j < dstData.mUWidth; ++j) {
      EXPECT_EQ(dstData.UBuffer()[i * dstData.mUStride + j],
                result_.UBuffer()[i * result_.mUStride + j]);
      EXPECT_EQ(dstData.VBuffer()[i * dstData.mVStride + j],
                result_.VBuffer()[i * result_.mVStride + j]);
    }
  }
}

TEST(ImageBitmapColorUtils, RGB24ToYUV420P)
{
  const RGB24Data srcData;
  const YUV420PData dstData;

  YUVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride);

  int rv = RGB24ToYUV420P(srcData.mData, srcData.mStride,
                          result_.YBuffer(), result_.mYStride,
                          result_.UBuffer(), result_.mUStride,
                          result_.VBuffer(), result_.mVStride,
                          result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUHeight; ++i) {
    for (int j = 0; j < dstData.mUWidth; ++j) {
      EXPECT_EQ(dstData.UBuffer()[i * dstData.mUStride + j],
                result_.UBuffer()[i * result_.mUStride + j]);
      EXPECT_EQ(dstData.VBuffer()[i * dstData.mVStride + j],
                result_.VBuffer()[i * result_.mVStride + j]);
    }
  }
}

TEST(ImageBitmapColorUtils, RGB24ToNV12)
{
  const RGB24Data srcData;
  const NV12Data dstData;

  NVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                  srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride + 10);

  int rv = RGB24ToNV12(srcData.mData, srcData.mStride,
                       result_.YBuffer(), result_.mYStride,
                       result_.UVBuffer(), result_.mUVStride,
                       result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUVHeight; ++i) {
    for (int j = 0; j < dstData.mUVWidth; ++j) {
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 0],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 0]);
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 1],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 1]);
    }
  }
}

TEST(ImageBitmapColorUtils, RGB24ToNV21)
{
  const RGB24Data srcData;
  const NV21Data dstData;

  NVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                  srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride + 10);

  int rv = RGB24ToNV21(srcData.mData, srcData.mStride,
                       result_.YBuffer(), result_.mYStride,
                       result_.UVBuffer(), result_.mUVStride,
                       result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUVHeight; ++i) {
    for (int j = 0; j < dstData.mUVWidth; ++j) {
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 0],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 0]);
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 1],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 1]);
    }
  }
}

TEST(ImageBitmapColorUtils, RGB24ToHSV)
{
  const RGB24Data srcData;
  const HSVData dstData;

  SimpleImage<float> result_(srcData.mWidth, srcData.mHeight, 3);

  int rv = RGB24ToHSV(srcData.mData, srcData.mStride,
                      result_.mData, result_.mStride,
                      result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mHeight; ++i) {
    for (int j = 0; j < dstData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        const double actualValue = result_.GetPixelValue(i, j, c);
        const double expectedValue = dstData.GetPixelValue(i, j, c);
        const double diff = std::abs(actualValue - expectedValue);
        EXPECT_LT(diff, 1.0);
      }
    }
  }
}

TEST(ImageBitmapColorUtils, RGB24ToLab)
{
  const RGB24Data srcData;
  const LabData dstData;

  SimpleImage<float> result_(srcData.mWidth, srcData.mHeight, 3);

  int rv = RGB24ToLab(srcData.mData, srcData.mStride,
                      result_.mData, result_.mStride,
                      result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mHeight; ++i) {
    for (int j = 0; j < dstData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        const double actualValue = result_.GetPixelValue(i, j, c);
        const double expectedValue = dstData.GetPixelValue(i, j, c);
        const double diff = std::abs(actualValue - expectedValue);
        EXPECT_LT(diff, 1.0);
      }
    }
  }
}

TEST(ImageBitmapColorUtils, RGB24ToGray8)
{
  const RGB24Data srcData;
  const GrayData dstData;

  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, 1);

  int rv = RGB24ToGray8(srcData.mData, srcData.mStride,
                        result_.mData, result_.mStride,
                        result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mHeight; ++i) {
    for (int j = 0; j < dstData.mWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j],
                dstData.mData[i * dstData.mStride + j]);
    }
  }
}

/*
 * From BGR24.
 */

TEST(ImageBitmapColorUtils, BGR24ToRGB24_)
{
  const BGR24Data srcData;
  const RGB24Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = BGR24ToRGB24(srcData.mData, srcData.mStride,
                        result_.mData, result_.mStride,
                        result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 2]);
    }
  }
}

TEST(ImageBitmapColorUtils, BGR24ToRGBA32)
{
  const BGR24Data srcData;
  const RGBA32Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = BGR24ToRGBA32(srcData.mData, srcData.mStride,
                         result_.mData, result_.mStride,
                         result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 3],
                255);
    }
  }
}

TEST(ImageBitmapColorUtils, BGR24ToBGRA32)
{
  const BGR24Data srcData;
  const BGRA32Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = BGR24ToBGRA32(srcData.mData, srcData.mStride,
                         result_.mData, result_.mStride,
                         result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 3],
                255);
    }
  }
}

TEST(ImageBitmapColorUtils, BGR24ToYUV444P)
{
  const BGR24Data srcData;
  const YUV444PData dstData;

  YUVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth, srcData.mHeight, srcData.mStride);

  int rv = BGR24ToYUV444P(srcData.mData, srcData.mStride,
                          result_.YBuffer(), result_.mYStride,
                          result_.UBuffer(), result_.mUStride,
                          result_.VBuffer(), result_.mVStride,
                          result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
      EXPECT_EQ(dstData.UBuffer()[i * dstData.mUStride + j],
                result_.UBuffer()[i * result_.mUStride + j]);
      EXPECT_EQ(dstData.VBuffer()[i * dstData.mVStride + j],
                result_.VBuffer()[i * result_.mVStride + j]);
    }
  }
}

TEST(ImageBitmapColorUtils, BGR24ToYUV422P)
{
  const BGR24Data srcData;
  const YUV422PData dstData;

  YUVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight, srcData.mStride);

  int rv = BGR24ToYUV422P(srcData.mData, srcData.mStride,
                          result_.YBuffer(), result_.mYStride,
                          result_.UBuffer(), result_.mUStride,
                          result_.VBuffer(), result_.mVStride,
                          result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUHeight; ++i) {
    for (int j = 0; j < dstData.mUWidth; ++j) {
      EXPECT_EQ(dstData.UBuffer()[i * dstData.mUStride + j],
                result_.UBuffer()[i * result_.mUStride + j]);
      EXPECT_EQ(dstData.VBuffer()[i * dstData.mVStride + j],
                result_.VBuffer()[i * result_.mVStride + j]);
    }
  }
}

TEST(ImageBitmapColorUtils, BGR24ToYUV420P)
{
  const BGR24Data srcData;
  const YUV420PData dstData;

  YUVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride);

  int rv = BGR24ToYUV420P(srcData.mData, srcData.mStride,
                          result_.YBuffer(), result_.mYStride,
                          result_.UBuffer(), result_.mUStride,
                          result_.VBuffer(), result_.mVStride,
                          result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUHeight; ++i) {
    for (int j = 0; j < dstData.mUWidth; ++j) {
      EXPECT_EQ(dstData.UBuffer()[i * dstData.mUStride + j],
                result_.UBuffer()[i * result_.mUStride + j]);
      EXPECT_EQ(dstData.VBuffer()[i * dstData.mVStride + j],
                result_.VBuffer()[i * result_.mVStride + j]);
    }
  }
}

TEST(ImageBitmapColorUtils, BGR24ToNV12)
{
  const BGR24Data srcData;
  const NV12Data dstData;

  NVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                  srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride + 10);

  int rv = BGR24ToNV12(srcData.mData, srcData.mStride,
                       result_.YBuffer(), result_.mYStride,
                       result_.UVBuffer(), result_.mUVStride,
                       result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUVHeight; ++i) {
    for (int j = 0; j < dstData.mUVWidth; ++j) {
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 0],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 0]);
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 1],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 1]);
    }
  }
}

TEST(ImageBitmapColorUtils, BGR24ToNV21)
{
  const BGR24Data srcData;
  const NV21Data dstData;

  NVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                  srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride + 10);

  int rv = BGR24ToNV21(srcData.mData, srcData.mStride,
                       result_.YBuffer(), result_.mYStride,
                       result_.UVBuffer(), result_.mUVStride,
                       result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUVHeight; ++i) {
    for (int j = 0; j < dstData.mUVWidth; ++j) {
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 0],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 0]);
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 1],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 1]);
    }
  }
}

TEST(ImageBitmapColorUtils, BGR24ToHSV)
{
  const BGR24Data srcData;
  const HSVData dstData;

  SimpleImage<float> result_(srcData.mWidth, srcData.mHeight, 3);

  int rv = BGR24ToHSV(srcData.mData, srcData.mStride,
                      result_.mData, result_.mStride,
                      result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mHeight; ++i) {
    for (int j = 0; j < dstData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        const double actualValue = result_.GetPixelValue(i, j, c);
        const double expectedValue = dstData.GetPixelValue(i, j, c);
        const double diff = std::abs(actualValue - expectedValue);
        EXPECT_LT(diff, 1.0);
      }
    }
  }
}

TEST(ImageBitmapColorUtils, BGR24ToLab)
{
  const BGR24Data srcData;
  const LabData dstData;

  SimpleImage<float> result_(srcData.mWidth, srcData.mHeight, 3);

  int rv = BGR24ToLab(srcData.mData, srcData.mStride,
                      result_.mData, result_.mStride,
                      result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mHeight; ++i) {
    for (int j = 0; j < dstData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        const double actualValue = result_.GetPixelValue(i, j, c);
        const double expectedValue = dstData.GetPixelValue(i, j, c);
        const double diff = std::abs(actualValue - expectedValue);
        EXPECT_LT(diff, 1.0);
      }
    }
  }
}

TEST(ImageBitmapColorUtils, BGR24ToGray8)
{
  const BGR24Data srcData;
  const GrayData dstData;

  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, 1);

  int rv = BGR24ToGray8(srcData.mData, srcData.mStride,
                        result_.mData, result_.mStride,
                        result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mHeight; ++i) {
    for (int j = 0; j < dstData.mWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j],
                dstData.mData[i * dstData.mStride + j]);
    }
  }
}

/*
 * From RGBA32.
 */
TEST(ImageBitmapColorUtils, RGBA32ToRGB24)
{
  const RGBA32Data srcData;
  const RGB24Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = RGBA32ToRGB24(srcData.mData, srcData.mStride,
                         result_.mData, result_.mStride,
                         result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 2]);
    }
  }
}

TEST(ImageBitmapColorUtils, RGBA32ToBGR24)
{
  const RGBA32Data srcData;
  const BGR24Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = RGBA32ToBGR24(srcData.mData, srcData.mStride,
                         result_.mData, result_.mStride,
                         result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 2]);
    }
  }
}

TEST(ImageBitmapColorUtils, RGBA32ToYUV444P)
{
  const RGBA32Data srcData;
  const YUV444PData dstData;

  YUVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth, srcData.mHeight, srcData.mStride);

  int rv = RGBA32ToYUV444P(srcData.mData, srcData.mStride,
                           result_.YBuffer(), result_.mYStride,
                           result_.UBuffer(), result_.mUStride,
                           result_.VBuffer(), result_.mVStride,
                           result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
      EXPECT_EQ(dstData.UBuffer()[i * dstData.mUStride + j],
                result_.UBuffer()[i * result_.mUStride + j]);
      EXPECT_EQ(dstData.VBuffer()[i * dstData.mVStride + j],
                result_.VBuffer()[i * result_.mVStride + j]);
    }
  }
}

TEST(ImageBitmapColorUtils, RGBA32ToYUV422P)
{
  const RGBA32Data srcData;
  const YUV422PData dstData;

  YUVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight, srcData.mStride);

  int rv = RGBA32ToYUV422P(srcData.mData, srcData.mStride,
                           result_.YBuffer(), result_.mYStride,
                           result_.UBuffer(), result_.mUStride,
                           result_.VBuffer(), result_.mVStride,
                           result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUHeight; ++i) {
    for (int j = 0; j < dstData.mUWidth; ++j) {
      EXPECT_EQ(dstData.UBuffer()[i * dstData.mUStride + j],
                result_.UBuffer()[i * result_.mUStride + j]);
      EXPECT_EQ(dstData.VBuffer()[i * dstData.mVStride + j],
                result_.VBuffer()[i * result_.mVStride + j]);
    }
  }
}

TEST(ImageBitmapColorUtils, RGBA32ToYUV420P)
{
  const RGBA32Data srcData;
  const YUV420PData dstData;

  YUVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride);

  int rv = RGBA32ToYUV420P(srcData.mData, srcData.mStride,
                           result_.YBuffer(), result_.mYStride,
                           result_.UBuffer(), result_.mUStride,
                           result_.VBuffer(), result_.mVStride,
                           result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUHeight; ++i) {
    for (int j = 0; j < dstData.mUWidth; ++j) {
      EXPECT_EQ(dstData.UBuffer()[i * dstData.mUStride + j],
                result_.UBuffer()[i * result_.mUStride + j]);
      EXPECT_EQ(dstData.VBuffer()[i * dstData.mVStride + j],
                result_.VBuffer()[i * result_.mVStride + j]);
    }
  }
}

TEST(ImageBitmapColorUtils, RGBA32ToNV12)
{
  const RGBA32Data srcData;
  const NV12Data dstData;

  NVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                  srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride + 10);

  int rv = RGBA32ToNV12(srcData.mData, srcData.mStride,
                        result_.YBuffer(), result_.mYStride,
                        result_.UVBuffer(), result_.mUVStride,
                        result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUVHeight; ++i) {
    for (int j = 0; j < dstData.mUVWidth; ++j) {
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 0],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 0]);
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 1],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 1]);
    }
  }
}

TEST(ImageBitmapColorUtils, RGBA32ToNV21)
{
  const RGBA32Data srcData;
  const NV21Data dstData;

  NVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                  srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride + 10);

  int rv = RGBA32ToNV21(srcData.mData, srcData.mStride,
                        result_.YBuffer(), result_.mYStride,
                        result_.UVBuffer(), result_.mUVStride,
                        result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUVHeight; ++i) {
    for (int j = 0; j < dstData.mUVWidth; ++j) {
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 0],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 0]);
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 1],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 1]);
    }
  }
}

TEST(ImageBitmapColorUtils, RGBA32ToHSV)
{
  const RGBA32Data srcData;
  const HSVData dstData;

  SimpleImage<float> result_(srcData.mWidth, srcData.mHeight, 3);

  int rv = RGBA32ToHSV(srcData.mData, srcData.mStride,
                       result_.mData, result_.mStride,
                       result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mHeight; ++i) {
    for (int j = 0; j < dstData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        const double actualValue = result_.GetPixelValue(i, j, c);
        const double expectedValue = dstData.GetPixelValue(i, j, c);
        const double diff = std::abs(actualValue - expectedValue);
        EXPECT_LT(diff, 1.0);
      }
    }
  }
}

TEST(ImageBitmapColorUtils, RGBA32ToLab)
{
  const RGBA32Data srcData;
  const LabData dstData;

  SimpleImage<float> result_(srcData.mWidth, srcData.mHeight, 3);

  int rv = RGBA32ToLab(srcData.mData, srcData.mStride,
                       result_.mData, result_.mStride,
                       result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mHeight; ++i) {
    for (int j = 0; j < dstData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        const double actualValue = result_.GetPixelValue(i, j, c);
        const double expectedValue = dstData.GetPixelValue(i, j, c);
        const double diff = std::abs(actualValue - expectedValue);
        EXPECT_LT(diff, 1.0);
      }
    }
  }
}

TEST(ImageBitmapColorUtils, RGBA32ToGray8)
{
  const RGBA32Data srcData;
  const GrayData dstData;

  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, 1);

  int rv = RGBA32ToGray8(srcData.mData, srcData.mStride,
                         result_.mData, result_.mStride,
                         result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mHeight; ++i) {
    for (int j = 0; j < dstData.mWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j],
                dstData.mData[i * dstData.mStride + j]);
    }
  }
}

/*
 * From BGRA32.
 */
TEST(ImageBitmapColorUtils, BGRA32ToRGB24)
{
  const BGRA32Data srcData;
  const RGB24Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = BGRA32ToRGB24(srcData.mData, srcData.mStride,
                         result_.mData, result_.mStride,
                         result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 2]);
    }
  }
}

TEST(ImageBitmapColorUtils, BGRA32ToBGR24)
{
  const BGRA32Data srcData;
  const BGR24Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = BGRA32ToBGR24(srcData.mData, srcData.mStride,
                         result_.mData, result_.mStride,
                         result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.mData[i * dstData.mStride + j * dstData.mChannelCount + 2]);
    }
  }
}

TEST(ImageBitmapColorUtils, BGRA32ToYUV444P)
{
  const BGRA32Data srcData;
  const YUV444PData dstData;

  YUVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth, srcData.mHeight, srcData.mStride);

  int rv = BGRA32ToYUV444P(srcData.mData, srcData.mStride,
                           result_.YBuffer(), result_.mYStride,
                           result_.UBuffer(), result_.mUStride,
                           result_.VBuffer(), result_.mVStride,
                           result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
      EXPECT_EQ(dstData.UBuffer()[i * dstData.mUStride + j],
                result_.UBuffer()[i * result_.mUStride + j]);
      EXPECT_EQ(dstData.VBuffer()[i * dstData.mVStride + j],
                result_.VBuffer()[i * result_.mVStride + j]);
    }
  }
}

TEST(ImageBitmapColorUtils, BGRA32ToYUV422P)
{
  const BGRA32Data srcData;
  const YUV422PData dstData;

  YUVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight, srcData.mStride);

  int rv = BGRA32ToYUV422P(srcData.mData, srcData.mStride,
                           result_.YBuffer(), result_.mYStride,
                           result_.UBuffer(), result_.mUStride,
                           result_.VBuffer(), result_.mVStride,
                           result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUHeight; ++i) {
    for (int j = 0; j < dstData.mUWidth; ++j) {
      EXPECT_EQ(dstData.UBuffer()[i * dstData.mUStride + j],
                result_.UBuffer()[i * result_.mUStride + j]);
      EXPECT_EQ(dstData.VBuffer()[i * dstData.mVStride + j],
                result_.VBuffer()[i * result_.mVStride + j]);
    }
  }
}

TEST(ImageBitmapColorUtils, BGRA32ToYUV420P)
{
  const BGRA32Data srcData;
  const YUV420PData dstData;

  YUVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride,
                   srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride);

  int rv = BGRA32ToYUV420P(srcData.mData, srcData.mStride,
                           result_.YBuffer(), result_.mYStride,
                           result_.UBuffer(), result_.mUStride,
                           result_.VBuffer(), result_.mVStride,
                           result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUHeight; ++i) {
    for (int j = 0; j < dstData.mUWidth; ++j) {
      EXPECT_EQ(dstData.UBuffer()[i * dstData.mUStride + j],
                result_.UBuffer()[i * result_.mUStride + j]);
      EXPECT_EQ(dstData.VBuffer()[i * dstData.mVStride + j],
                result_.VBuffer()[i * result_.mVStride + j]);
    }
  }
}

TEST(ImageBitmapColorUtils, BGRA32ToNV12)
{
  const BGRA32Data srcData;
  const NV12Data dstData;

  NVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                  srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride + 10);

  int rv = BGRA32ToNV12(srcData.mData, srcData.mStride,
                        result_.YBuffer(), result_.mYStride,
                        result_.UVBuffer(), result_.mUVStride,
                        result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUVHeight; ++i) {
    for (int j = 0; j < dstData.mUVWidth; ++j) {
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 0],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 0]);
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 1],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 1]);
    }
  }
}

TEST(ImageBitmapColorUtils, BGRA32ToNV21)
{
  const BGRA32Data srcData;
  const NV21Data dstData;

  NVImage result_(srcData.mWidth, srcData.mHeight, srcData.mStride,
                  srcData.mWidth / 2 + 1, srcData.mHeight / 2 + 1, srcData.mStride + 10);

  int rv = BGRA32ToNV21(srcData.mData, srcData.mStride,
                        result_.YBuffer(), result_.mYStride,
                        result_.UVBuffer(), result_.mUVStride,
                        result_.mYWidth, result_.mYHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mYHeight; ++i) {
    for (int j = 0; j < dstData.mYWidth; ++j) {
      EXPECT_EQ(dstData.YBuffer()[i * dstData.mYStride + j],
                result_.YBuffer()[i * result_.mYStride + j]);
    }
  }

  for (int i = 0; i < dstData.mUVHeight; ++i) {
    for (int j = 0; j < dstData.mUVWidth; ++j) {
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 0],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 0]);
      EXPECT_EQ(dstData.UVBuffer()[i * dstData.mUVStride + j * 2 + 1],
                result_.UVBuffer()[i * result_.mUVStride + j * 2 + 1]);
    }
  }
}

TEST(ImageBitmapColorUtils, BGRA32ToHSV)
{
  const BGRA32Data srcData;
  const HSVData dstData;

  SimpleImage<float> result_(srcData.mWidth, srcData.mHeight, 3);

  int rv = BGRA32ToHSV(srcData.mData, srcData.mStride,
                       result_.mData, result_.mStride,
                       result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mHeight; ++i) {
    for (int j = 0; j < dstData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        const double actualValue = result_.GetPixelValue(i, j, c);
        const double expectedValue = dstData.GetPixelValue(i, j, c);
        const double diff = std::abs(actualValue - expectedValue);
        EXPECT_LT(diff, 1.0);
      }
    }
  }
}

TEST(ImageBitmapColorUtils, BGRA32ToLab)
{
  const BGRA32Data srcData;
  const LabData dstData;

  SimpleImage<float> result_(srcData.mWidth, srcData.mHeight, 3);

  int rv = BGRA32ToLab(srcData.mData, srcData.mStride,
                       result_.mData, result_.mStride,
                       result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mHeight; ++i) {
    for (int j = 0; j < dstData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        const double actualValue = result_.GetPixelValue(i, j, c);
        const double expectedValue = dstData.GetPixelValue(i, j, c);
        const double diff = std::abs(actualValue - expectedValue);
        EXPECT_LT(diff, 1.0);
      }
    }
  }
}

TEST(ImageBitmapColorUtils, BGRA32ToGray8)
{
  const BGRA32Data srcData;
  const GrayData dstData;

  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, 1);

  int rv = BGRA32ToGray8(srcData.mData, srcData.mStride,
                         result_.mData, result_.mStride,
                         result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < dstData.mHeight; ++i) {
    for (int j = 0; j < dstData.mWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j],
                dstData.mData[i * dstData.mStride + j]);
    }
  }
}

/*
 * From YUV444P.
 */
TEST(ImageBitmapColorUtils, YUV444PToRGB24)
{
  const YUV444PData srcData;
  const RGBA32DataFromYUV444PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 3);

  int rv = YUV444PToRGB24(srcData.YBuffer(), srcData.mYStride,
                          srcData.UBuffer(), srcData.mUStride,
                          srcData.VBuffer(), srcData.mVStride,
                          result_.mData, result_.mStride,
                          result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[2]);
    }
  }
}

TEST(ImageBitmapColorUtils, YUV444PToBGR24)
{
  const YUV444PData srcData;
  const RGBA32DataFromYUV444PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 3);

  int rv = YUV444PToBGR24(srcData.YBuffer(), srcData.mYStride,
                          srcData.UBuffer(), srcData.mUStride,
                          srcData.VBuffer(), srcData.mVStride,
                          result_.mData, result_.mStride,
                          result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[0]);
    }
  }
}

TEST(ImageBitmapColorUtils, YUV444PToRGBA32)
{
  const YUV444PData srcData;
  const RGBA32DataFromYUV444PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, dstData.mChannelCount);

  int rv = YUV444PToRGBA32(srcData.YBuffer(), srcData.mYStride,
                           srcData.UBuffer(), srcData.mUStride,
                           srcData.VBuffer(), srcData.mVStride,
                           result_.mData, result_.mStride,
                           result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 3],
                dstData.GetPixel(j, i)[3]);
    }
  }
}

TEST(ImageBitmapColorUtils, YUV444PToBGRA32)
{
  const YUV444PData srcData;
  const RGBA32DataFromYUV444PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, dstData.mChannelCount);

  int rv = YUV444PToBGRA32(srcData.YBuffer(), srcData.mYStride,
                           srcData.UBuffer(), srcData.mUStride,
                           srcData.VBuffer(), srcData.mVStride,
                           result_.mData, result_.mStride,
                           result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 3],
                dstData.GetPixel(j, i)[3]);
    }
  }
}

TEST(ImageBitmapColorUtils, YUV444PToGray8)
{
  const YUV444PData srcData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 1);

  int rv = YUV444PToGray8(srcData.YBuffer(), srcData.mYStride,
                          srcData.UBuffer(), srcData.mUStride,
                          srcData.VBuffer(), srcData.mVStride,
                          result_.mData, result_.mStride,
                          result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j],
                srcData.YBuffer()[i * srcData.mYStride + j]);
    }
  }
}

/*
 * From YUV422P.
 */
TEST(ImageBitmapColorUtils, YUV422PToRGB24)
{
  const YUV422PData srcData;
  const RGBA32DataFromYUV422PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 3);

  int rv = YUV422PToRGB24(srcData.YBuffer(), srcData.mYStride,
                          srcData.UBuffer(), srcData.mUStride,
                          srcData.VBuffer(), srcData.mVStride,
                          result_.mData, result_.mStride,
                          result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[2]);
    }
  }
}

TEST(ImageBitmapColorUtils, YUV422PToBGR24)
{
  const YUV422PData srcData;
  const RGBA32DataFromYUV422PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 3);

  int rv = YUV422PToBGR24(srcData.YBuffer(), srcData.mYStride,
                          srcData.UBuffer(), srcData.mUStride,
                          srcData.VBuffer(), srcData.mVStride,
                          result_.mData, result_.mStride,
                          result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[0]);
    }
  }
}

TEST(ImageBitmapColorUtils, YUV422PToRGBA32)
{
  const YUV422PData srcData;
  const RGBA32DataFromYUV422PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, dstData.mChannelCount);

  int rv = YUV422PToRGBA32(srcData.YBuffer(), srcData.mYStride,
                           srcData.UBuffer(), srcData.mUStride,
                           srcData.VBuffer(), srcData.mVStride,
                           result_.mData, result_.mStride,
                           result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 3],
                dstData.GetPixel(j, i)[3]);
    }
  }
}

TEST(ImageBitmapColorUtils, YUV422PToBGRA32)
{
  const YUV422PData srcData;
  const RGBA32DataFromYUV422PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, dstData.mChannelCount);

  int rv = YUV422PToBGRA32(srcData.YBuffer(), srcData.mYStride,
                           srcData.UBuffer(), srcData.mUStride,
                           srcData.VBuffer(), srcData.mVStride,
                           result_.mData, result_.mStride,
                           result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 3],
                dstData.GetPixel(j, i)[3]);
    }
  }
}

TEST(ImageBitmapColorUtils, YUV422PToGray8)
{
  const YUV422PData srcData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 1);

  int rv = YUV422PToGray8(srcData.YBuffer(), srcData.mYStride,
                          srcData.UBuffer(), srcData.mUStride,
                          srcData.VBuffer(), srcData.mVStride,
                          result_.mData, result_.mStride,
                          result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j],
                srcData.YBuffer()[i * srcData.mYStride + j]);
    }
  }
}

/*
 * From YUV420P.
 */
TEST(ImageBitmapColorUtils, YUV420PToRGB24)
{
  const YUV420PData srcData;
  const RGBA32DataFromYUV420PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 3);

  int rv = YUV420PToRGB24(srcData.YBuffer(), srcData.mYStride,
                          srcData.UBuffer(), srcData.mUStride,
                          srcData.VBuffer(), srcData.mVStride,
                          result_.mData, result_.mStride,
                          result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[2]);
    }
  }
}

TEST(ImageBitmapColorUtils, YUV420PToBGR24)
{
  const YUV420PData srcData;
  const RGBA32DataFromYUV420PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 3);

  int rv = YUV420PToBGR24(srcData.YBuffer(), srcData.mYStride,
                          srcData.UBuffer(), srcData.mUStride,
                          srcData.VBuffer(), srcData.mVStride,
                          result_.mData, result_.mStride,
                          result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[0]);
    }
  }
}

TEST(ImageBitmapColorUtils, YUV420PToRGBA32)
{
  const YUV420PData srcData;
  const RGBA32DataFromYUV420PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, dstData.mChannelCount);

  int rv = YUV420PToRGBA32(srcData.YBuffer(), srcData.mYStride,
                           srcData.UBuffer(), srcData.mUStride,
                           srcData.VBuffer(), srcData.mVStride,
                           result_.mData, result_.mStride,
                           result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 3],
                dstData.GetPixel(j, i)[3]);
    }
  }
}

TEST(ImageBitmapColorUtils, YUV420PToBGRA32)
{
  const YUV420PData srcData;
  const RGBA32DataFromYUV420PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, dstData.mChannelCount);

  int rv = YUV420PToBGRA32(srcData.YBuffer(), srcData.mYStride,
                           srcData.UBuffer(), srcData.mUStride,
                           srcData.VBuffer(), srcData.mVStride,
                           result_.mData, result_.mStride,
                           result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 3],
                dstData.GetPixel(j, i)[3]);
    }
  }
}

TEST(ImageBitmapColorUtils, YUV420PToGray8)
{
  const YUV420PData srcData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 1);

  int rv = YUV420PToGray8(srcData.YBuffer(), srcData.mYStride,
                          srcData.UBuffer(), srcData.mUStride,
                          srcData.VBuffer(), srcData.mVStride,
                          result_.mData, result_.mStride,
                          result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j],
                srcData.YBuffer()[i * srcData.mYStride + j]);
    }
  }
}

/*
 * From NV12.
 */
TEST(ImageBitmapColorUtils, NV12ToRGB24)
{
  const NV12Data srcData;
  const RGBA32DataFromYUV420PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 3);

  int rv = NV12ToRGB24(srcData.YBuffer(), srcData.mYStride,
                          srcData.UVBuffer(), srcData.mUVStride,
                          result_.mData, result_.mStride,
                          result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[2]);
    }
  }
}

TEST(ImageBitmapColorUtils, NV12ToBGR24)
{
  const NV12Data srcData;
  const RGBA32DataFromYUV420PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 3);

  int rv = NV12ToBGR24(srcData.YBuffer(), srcData.mYStride,
                          srcData.UVBuffer(), srcData.mUVStride,
                          result_.mData, result_.mStride,
                          result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[0]);
    }
  }
}

TEST(ImageBitmapColorUtils, NV12ToRGBA32)
{
  const NV12Data srcData;
  const RGBA32DataFromYUV420PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, dstData.mChannelCount);

  int rv = NV12ToRGBA32(srcData.YBuffer(), srcData.mYStride,
                           srcData.UVBuffer(), srcData.mUVStride,
                           result_.mData, result_.mStride,
                           result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 3],
                dstData.GetPixel(j, i)[3]);
    }
  }
}

TEST(ImageBitmapColorUtils, NV12ToBGRA32)
{
  const NV12Data srcData;
  const RGBA32DataFromYUV420PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, dstData.mChannelCount);

  int rv = NV12ToBGRA32(srcData.YBuffer(), srcData.mYStride,
                           srcData.UVBuffer(), srcData.mUVStride,
                           result_.mData, result_.mStride,
                           result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 3],
                dstData.GetPixel(j, i)[3]);
    }
  }
}

TEST(ImageBitmapColorUtils, NV12ToGray8)
{
  const NV12Data srcData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 1);

  int rv = NV12ToGray8(srcData.YBuffer(), srcData.mYStride,
                       srcData.UVBuffer(), srcData.mUVStride,
                       result_.mData, result_.mStride,
                       result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j],
                srcData.YBuffer()[i * srcData.mYStride + j]);
    }
  }
}

/*
 * From NV21.
 */
TEST(ImageBitmapColorUtils, NV21ToRGB24)
{
  const NV21Data srcData;
  const RGBA32DataFromYUV420PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 3);

  int rv = NV21ToRGB24(srcData.YBuffer(), srcData.mYStride,
                          srcData.UVBuffer(), srcData.mUVStride,
                          result_.mData, result_.mStride,
                          result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[2]);
    }
  }
}

TEST(ImageBitmapColorUtils, NV21ToBGR24)
{
  const NV21Data srcData;
  const RGBA32DataFromYUV420PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 3);

  int rv = NV21ToBGR24(srcData.YBuffer(), srcData.mYStride,
                          srcData.UVBuffer(), srcData.mUVStride,
                          result_.mData, result_.mStride,
                          result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[0]);
    }
  }
}

TEST(ImageBitmapColorUtils, NV21ToRGBA32)
{
  const NV21Data srcData;
  const RGBA32DataFromYUV420PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, dstData.mChannelCount);

  int rv = NV21ToRGBA32(srcData.YBuffer(), srcData.mYStride,
                           srcData.UVBuffer(), srcData.mUVStride,
                           result_.mData, result_.mStride,
                           result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 3],
                dstData.GetPixel(j, i)[3]);
    }
  }
}

TEST(ImageBitmapColorUtils, NV21ToBGRA32)
{
  const NV21Data srcData;
  const RGBA32DataFromYUV420PData dstData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, dstData.mChannelCount);

  int rv = NV21ToBGRA32(srcData.YBuffer(), srcData.mYStride,
                           srcData.UVBuffer(), srcData.mUVStride,
                           result_.mData, result_.mStride,
                           result_.mWidth, result_.mHeight);
  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 0],
                dstData.GetPixel(j, i)[2]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 1],
                dstData.GetPixel(j, i)[1]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 2],
                dstData.GetPixel(j, i)[0]);
      EXPECT_EQ(result_.mData[i * result_.mStride + j * result_.mChannelCount + 3],
                dstData.GetPixel(j, i)[3]);
    }
  }
}

TEST(ImageBitmapColorUtils, NV21ToGray8)
{
  const NV21Data srcData;
  SimpleImage<uint8_t> result_(srcData.mYWidth, srcData.mYHeight, 1);

  int rv = NV21ToGray8(srcData.YBuffer(), srcData.mYStride,
                       srcData.UVBuffer(), srcData.mUVStride,
                       result_.mData, result_.mStride,
                       result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mYHeight; ++i) {
    for (int j = 0; j < srcData.mYWidth; ++j) {
      EXPECT_EQ(result_.mData[i * result_.mStride + j],
                srcData.YBuffer()[i * srcData.mYStride + j]);
    }
  }
}

/*
 * From HSV.
 */
TEST(ImageBitmapColorUtils, HSVToRGB24)
{
  const HSVData srcData;
  const RGB24Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = HSVToRGB24(srcData.mData, srcData.mStride,
                      result_.mData, result_.mStride,
                      result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        EXPECT_EQ(result_.GetPixelValue(i, j, c),
                  dstData.GetPixelValue(i, j, c));
      }
    }
  }
}

TEST(ImageBitmapColorUtils, HSVToBGR24)
{
  const HSVData srcData;
  const BGR24Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = HSVToBGR24(srcData.mData, srcData.mStride,
                      result_.mData, result_.mStride,
                      result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        EXPECT_EQ(result_.GetPixelValue(i, j, c),
                  dstData.GetPixelValue(i, j, c));
      }
    }
  }
}

TEST(ImageBitmapColorUtils, HSVToRGBA32)
{
  const HSVData srcData;
  const RGBA32Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = HSVToRGBA32(srcData.mData, srcData.mStride,
                       result_.mData, result_.mStride,
                       result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        EXPECT_EQ(result_.GetPixelValue(i, j, c),
                  dstData.GetPixelValue(i, j, c));
      }
    }
  }
}

TEST(ImageBitmapColorUtils, HSVToBGRA32)
{
  const HSVData srcData;
  const BGRA32Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = HSVToBGRA32(srcData.mData, srcData.mStride,
                       result_.mData, result_.mStride,
                       result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        EXPECT_EQ(result_.GetPixelValue(i, j, c),
                  dstData.GetPixelValue(i, j, c));
      }
    }
  }
}

/*
 * From Lab.
 */
TEST(ImageBitmapColorUtils, LabToRGB24)
{
  const LabData srcData;
  const RGB24Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = LabToRGB24(srcData.mData, srcData.mStride,
                      result_.mData, result_.mStride,
                      result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        const uint8_t actualValue = result_.GetPixelValue(i, j, c);
        const uint8_t expectedValue = dstData.GetPixelValue(i, j, c);
        const int diff = std::abs(actualValue - expectedValue);
        EXPECT_LE(diff, 1);
      }
    }
  }
}

TEST(ImageBitmapColorUtils, LabToBGR24)
{
  const LabData srcData;
  const BGR24Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = LabToBGR24(srcData.mData, srcData.mStride,
                      result_.mData, result_.mStride,
                      result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        const uint8_t actualValue = result_.GetPixelValue(i, j, c);
        const uint8_t expectedValue = dstData.GetPixelValue(i, j, c);
        const int diff = std::abs(actualValue - expectedValue);
        EXPECT_LE(diff, 1);
      }
    }
  }
}

TEST(ImageBitmapColorUtils, LabToRGBA32)
{
  const LabData srcData;
  const RGBA32Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = LabToRGBA32(srcData.mData, srcData.mStride,
                       result_.mData, result_.mStride,
                       result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        const uint8_t actualValue = result_.GetPixelValue(i, j, c);
        const uint8_t expectedValue = dstData.GetPixelValue(i, j, c);
        const int diff = std::abs(actualValue - expectedValue);
        EXPECT_LE(diff, 1);
      }
    }
  }
}

TEST(ImageBitmapColorUtils, LabToBGRA32)
{
  const LabData srcData;
  const BGRA32Data dstData;
  SimpleImage<uint8_t> result_(srcData.mWidth, srcData.mHeight, dstData.mChannelCount);

  int rv = LabToBGRA32(srcData.mData, srcData.mStride,
                       result_.mData, result_.mStride,
                       result_.mWidth, result_.mHeight);

  ASSERT_TRUE(rv == 0);

  for (int i = 0; i < srcData.mHeight; ++i) {
    for (int j = 0; j < srcData.mWidth; ++j) {
      for (int c = 0; c < dstData.mChannelCount; ++c) {
        const uint8_t actualValue = result_.GetPixelValue(i, j, c);
        const uint8_t expectedValue = dstData.GetPixelValue(i, j, c);
        const int diff = std::abs(actualValue - expectedValue);
        EXPECT_LE(diff, 1);
      }
    }
  }
}
