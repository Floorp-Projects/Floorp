/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_IMAGESCALING_H
#define _MOZILLA_GFX_IMAGESCALING_H

#include "Types.h"

#include <vector>
#include "Point.h"

namespace mozilla {
namespace gfx {

class ImageHalfScaler
{
public:
  ImageHalfScaler(uint8_t *aData, int32_t aStride, const IntSize &aSize)
    : mOrigData(aData), mOrigStride(aStride), mOrigSize(aSize)
    , mDataStorage(nullptr)
  {
  }

  ~ImageHalfScaler()
  {
    delete [] mDataStorage;
  }

  void ScaleForSize(const IntSize &aSize);

  uint8_t *GetScaledData() const { return mData; }
  IntSize GetSize() const { return mSize; }
  uint32_t GetStride() const { return mStride; }

private:
  void HalfImage2D(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                   uint8_t *aDest, uint32_t aDestStride);
  void HalfImageVertical(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                         uint8_t *aDest, uint32_t aDestStride);
  void HalfImageHorizontal(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                           uint8_t *aDest, uint32_t aDestStride);

  // This is our SSE2 scaling function. Our destination must always be 16-byte
  // aligned and use a 16-byte aligned stride.
  void HalfImage2D_SSE2(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                        uint8_t *aDest, uint32_t aDestStride);
  void HalfImageVertical_SSE2(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                              uint8_t *aDest, uint32_t aDestStride);
  void HalfImageHorizontal_SSE2(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                                uint8_t *aDest, uint32_t aDestStride);

  void HalfImage2D_C(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                     uint8_t *aDest, uint32_t aDestStride);
  void HalfImageVertical_C(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                           uint8_t *aDest, uint32_t aDestStride);
  void HalfImageHorizontal_C(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                             uint8_t *aDest, uint32_t aDestStride);

  uint8_t *mOrigData;
  int32_t mOrigStride;
  IntSize mOrigSize;

  uint8_t *mDataStorage;
  // Guaranteed 16-byte aligned
  uint8_t *mData;
  IntSize mSize;
  // Guaranteed 16-byte aligned
  uint32_t mStride;
};

}
}

#endif
