/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Y_CB_CR_UTILS_H_
#define Y_CB_CR_UTILS_H_

#include "mozilla/gfx/Types.h"
#include "ImageContainer.h"

namespace mozilla {
namespace gfx {

void
GetYCbCrToRGBDestFormatAndSize(const layers::PlanarYCbCrData& aData,
                               SurfaceFormat& aSuggestedFormat,
                               IntSize& aSuggestedSize);

void
ConvertYCbCrToRGB(const layers::PlanarYCbCrData& aData,
                  const SurfaceFormat& aDestFormat,
                  const IntSize& aDestSize,
                  unsigned char* aDestBuffer,
                  int32_t aStride);

// Currently this function only has support for I420 type.
void
ConvertYCbCrAToARGB(const uint8_t* aSrcY,
                    const uint8_t* aSrcU,
                    const uint8_t* aSrcV,
                    const uint8_t* aSrcA,
                    int aSrcStrideYA, int aSrcStrideUV,
                    uint8_t* aDstARGB, int aDstStrideARGB,
                    int aWidth, int aHeight);

} // namespace gfx
} // namespace mozilla

#endif /* Y_CB_CR_UTILS_H_ */
