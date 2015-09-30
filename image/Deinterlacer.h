/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * Deinterlacer is a utility class to allow Downscaler to work with interlaced
 * images.

 * Since Downscaler needs to receive rows in top-to-bottom or
 * bottom-to-top order, it can't natively handle interlaced images, in which the
 * rows arrive in an interleaved order. Deinterlacer solves this problem by
 * acting as an intermediate buffer that records decoded rows. Unlike
 * Downscaler, it allows the rows to be written in arbitrary order. After each
 * pass, calling PropagatePassToDownscaler() will downscale every buffered row
 * in a single operation. The rows remain in the buffer, so rows that were
 * written in one pass will be included in subsequent passes.
 */


#ifndef mozilla_image_Deinterlacer_h
#define mozilla_image_Deinterlacer_h

#include "Downscaler.h"

namespace mozilla {
namespace image {

class Deinterlacer
{
public:
  explicit Deinterlacer(const nsIntSize& aImageSize);

  uint8_t* RowBuffer(uint32_t aRow);
  void PropagatePassToDownscaler(Downscaler& aDownscaler);

private:
  uint32_t RowSize() const;

  nsIntSize mImageSize;
  UniquePtr<uint8_t[]> mBuffer;
};


} // namespace image
} // namespace mozilla

#endif // mozilla_image_Deinterlacer_h
