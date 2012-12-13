/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_SHMEMYCBCRIMAGE_H
#define MOZILLA_LAYERS_SHMEMYCBCRIMAGE_H

#include "base/basictypes.h"
#include "Shmem.h"
#include "gfxPoint.h"

namespace mozilla {
namespace ipc {
  class Shmem;
}
namespace layers {

/**
 * This class is a view on a YCbCrImage stored in a Shmem at a certain offset.
 * It is only meant as a convenience to access the image data, and does not own
 * the data. The instance can live on the stack and used as follows:
 *
 * const YCbCrImage& yuv = sharedImage.get_YCbCrImage();
 * ShmemYCbCrImage shmImg(yuv.data(), yuv.offset());
 * if (!shmImg.IsValid()) {
 *   // handle error
 * }
 * mYSize = shmImg.GetYSize(); // work with the data, etc...
 */
class ShmemYCbCrImage
{
public:
  typedef mozilla::ipc::Shmem Shmem;

  ShmemYCbCrImage() : mOffset(0) {}

  ShmemYCbCrImage(Shmem& shm, size_t offset = 0) {
    DebugOnly<bool> status = Open(shm,offset);
    NS_ASSERTION(status, "Invalid data in the shmem");
  }

  /**
   * This function is meant as a helper to know how much shared memory we need
   * to allocate in a shmem in order to place a shared YCbCr image blob of 
   * given dimensions.
   */
  static size_t ComputeMinBufferSize(const gfxIntSize& aYSize,
                                     const gfxIntSize& aCbCrSize);
  /**
   * Write the image informations in a buffer for given dimensions.
   * The provided pointer should point to the beginning of the (chunk of)
   * buffer on which we want to store th image.
   */
  static void InitializeBufferInfo(uint8_t* aBuffer,
                                   const gfxIntSize& aYSize,
                                   const gfxIntSize& aCbCrSize);

  /**
   * Returns true if the shmem's data blob contains a valid YCbCr image.
   */
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
   * Copies the data passed in parameter into the shmem.
   */
  bool CopyData(const uint8_t* aYData,
                const uint8_t* aCbData, const uint8_t* aCrData,
                gfxIntSize aYSize, uint32_t aYStride,
                gfxIntSize aCbCrSize, uint32_t aCbCrStride,
                uint32_t aYSkip, uint32_t aCbCrSkip);

private:
  bool Open(Shmem& aShmem, size_t aOffset = 0);

  ipc::Shmem mShmem;
  size_t mOffset;
};

} // namespace
} // namespace

#endif
