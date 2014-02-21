/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_BLOBYCBCRSURFACE_H
#define MOZILLA_LAYERS_BLOBYCBCRSURFACE_H

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint8_t, uint32_t
#include "ImageTypes.h"                 // for StereoMode
#include "mozilla/Attributes.h"         // for MOZ_STACK_CLASS
#include "mozilla/RefPtr.h"             // for TemporaryRef
#include "mozilla/gfx/Point.h"          // for IntSize

namespace mozilla {
namespace gfx {
class DataSourceSurface;
}

namespace layers {

class Image;

/**
 * Convenience class to share code between YCbCrImageDataSerializer
 * and YCbCrImageDataDeserializer.
 * Do not use it.
 */
class YCbCrImageDataDeserializerBase
{
public:
  bool IsValid() const { return mIsValid; }

  /**
   * Returns the Y channel data pointer.
   */
  uint8_t* GetYData();
  /**
   * Returns the Cb channel data pointer.
   */
  uint8_t* GetCbData();
  /**
   * Returns the Cr channel data pointer.
   */
  uint8_t* GetCrData();

  /**
   * Returns the Y channel stride.
   */
  uint32_t GetYStride();
  /**
   * Returns the stride of the Cb and Cr channels.
   */
  uint32_t GetCbCrStride();

  /**
   * Returns the dimensions of the Y Channel.
   */
  gfx::IntSize GetYSize();

  /**
   * Returns the dimensions of the Cb and Cr Channel.
   */
  gfx::IntSize GetCbCrSize();

  /**
   * Stereo mode for the image.
   */
  StereoMode GetStereoMode();

  /**
   * Return a pointer to the begining of the data buffer.
   */
  uint8_t* GetData();

  /**
   * This function is meant as a helper to know how much shared memory we need
   * to allocate in a shmem in order to place a shared YCbCr image blob of
   * given dimensions.
   */
  static size_t ComputeMinBufferSize(const gfx::IntSize& aYSize,
                                     uint32_t aYStride,
                                     const gfx::IntSize& aCbCrSize,
                                     uint32_t aCbCrStride);
  static size_t ComputeMinBufferSize(const gfx::IntSize& aYSize,
                                     const gfx::IntSize& aCbCrSize);
  static size_t ComputeMinBufferSize(uint32_t aSize);

protected:
  YCbCrImageDataDeserializerBase(uint8_t* aData, size_t aDataSize)
    : mData (aData)
    , mDataSize(aDataSize)
    , mIsValid(false)
  {}

  void Validate();

  uint8_t* mData;
  size_t mDataSize;
  bool mIsValid;
};

/**
 * A view on a YCbCr image stored with its metadata in a blob of memory.
 * It is only meant as a convenience to access the image data, and does not own
 * the data. The instance can live on the stack and used as follows:
 *
 * const YCbCrImage& yuv = sharedImage.get_YCbCrImage();
 * YCbCrImageDataDeserializer deserializer(yuv.data().get<uint8_t>());
 * if (!deserializer.IsValid()) {
 *   // handle error
 * }
 * size = deserializer.GetYSize(); // work with the data, etc...
 */
class MOZ_STACK_CLASS YCbCrImageDataSerializer : public YCbCrImageDataDeserializerBase
{
public:
  YCbCrImageDataSerializer(uint8_t* aData, size_t aDataSize)
    : YCbCrImageDataDeserializerBase(aData, aDataSize)
  {
    // a serializer needs to be usable before correct buffer info has been written to it
    mIsValid = !!mData;
  }

  /**
   * Write the image informations in the buffer for given dimensions.
   * The provided pointer should point to the beginning of the (chunk of)
   * buffer on which we want to store the image.
   */
  void InitializeBufferInfo(uint32_t aYOffset,
                            uint32_t aCbOffset,
                            uint32_t aCrOffset,
                            uint32_t aYStride,
                            uint32_t aCbCrStride,
                            const gfx::IntSize& aYSize,
                            const gfx::IntSize& aCbCrSize,
                            StereoMode aStereoMode);
  void InitializeBufferInfo(uint32_t aYStride,
                            uint32_t aCbCrStride,
                            const gfx::IntSize& aYSize,
                            const gfx::IntSize& aCbCrSize,
                            StereoMode aStereoMode);
  void InitializeBufferInfo(const gfx::IntSize& aYSize,
                            const gfx::IntSize& aCbCrSize,
                            StereoMode aStereoMode);
  bool CopyData(const uint8_t* aYData,
                const uint8_t* aCbData, const uint8_t* aCrData,
                gfx::IntSize aYSize, uint32_t aYStride,
                gfx::IntSize aCbCrSize, uint32_t aCbCrStride,
                uint32_t aYSkip, uint32_t aCbCrSkip);
};

/**
 * A view on a YCbCr image stored with its metadata in a blob of memory.
 * It is only meant as a convenience to access the image data, and does not own
 * the data. The instance can live on the stack and used as follows:
 *
 * const YCbCrImage& yuv = sharedImage.get_YCbCrImage();
 * YCbCrImageDataDeserializer deserializer(yuv.data().get<uint8_t>());
 * if (!deserializer.IsValid()) {
 *   // handle error
 * }
 * size = deserializer.GetYSize(); // work with the data, etc...
 */
class MOZ_STACK_CLASS YCbCrImageDataDeserializer : public YCbCrImageDataDeserializerBase
{
public:
  YCbCrImageDataDeserializer(uint8_t* aData, size_t aDataSize)
    : YCbCrImageDataDeserializerBase(aData, aDataSize)
  {
    Validate();
  }

  /**
   * Convert the YCbCr data into RGB and return a DataSourceSurface.
   * This is a costly operation, so use it only when YCbCr compositing is
   * not supported.
   */
  TemporaryRef<gfx::DataSourceSurface> ToDataSourceSurface();
};

} // namespace
} // namespace

#endif
