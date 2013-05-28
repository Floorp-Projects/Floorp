/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_BLOBYCBCRSURFACE_H
#define MOZILLA_LAYERS_BLOBYCBCRSURFACE_H

#include "mozilla/DebugOnly.h"

#include "base/basictypes.h"
#include "Shmem.h"
#include "gfxPoint.h"

namespace mozilla {
namespace ipc {
  class Shmem;
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
  bool IsValid();

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
  gfxIntSize GetYSize();

  /**
   * Returns the dimensions of the Cb and Cr Channel.
   */
  gfxIntSize GetCbCrSize();

  /**
   * Return a pointer to the begining of the data buffer.
   */
  uint8_t* GetData();
protected:
  YCbCrImageDataDeserializerBase(uint8_t* aData)
  : mData (aData) {}

  uint8_t* mData;
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
  YCbCrImageDataSerializer(uint8_t* aData) : YCbCrImageDataDeserializerBase(aData) {}

  /**
   * This function is meant as a helper to know how much shared memory we need
   * to allocate in a shmem in order to place a shared YCbCr image blob of
   * given dimensions.
   */
  static size_t ComputeMinBufferSize(const gfx::IntSize& aYSize,
                                     const gfx::IntSize& aCbCrSize);
  static size_t ComputeMinBufferSize(const gfxIntSize& aYSize,
                                     const gfxIntSize& aCbCrSize);
  static size_t ComputeMinBufferSize(uint32_t aSize);

  /**
   * Write the image informations in the buffer for given dimensions.
   * The provided pointer should point to the beginning of the (chunk of)
   * buffer on which we want to store the image.
   */
  void InitializeBufferInfo(const gfx::IntSize& aYSize,
                            const gfx::IntSize& aCbCrSize);
  void InitializeBufferInfo(const gfxIntSize& aYSize,
                            const gfxIntSize& aCbCrSize);

  bool CopyData(const uint8_t* aYData,
                const uint8_t* aCbData, const uint8_t* aCrData,
                gfxIntSize aYSize, uint32_t aYStride,
                gfxIntSize aCbCrSize, uint32_t aCbCrStride,
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
  YCbCrImageDataDeserializer(uint8_t* aData) : YCbCrImageDataDeserializerBase(aData) {}
};

} // namespace
} // namespace

#endif
