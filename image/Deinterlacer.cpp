/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "Downscaler.h"
#include "mozilla/UniquePtrExtensions.h"

namespace mozilla {
namespace image {

Deinterlacer::Deinterlacer(const nsIntSize& aImageSize)
  : mImageSize(aImageSize)
{
  CheckedInt<size_t> bufferSize = mImageSize.width;
  bufferSize *= mImageSize.height;
  bufferSize *= sizeof(uint32_t);

  if (!bufferSize.isValid()) {
    return;
  }

  mBuffer = MakeUniqueFallible<uint8_t[]>(bufferSize.value());
}

uint32_t
Deinterlacer::RowSize() const
{
  return mImageSize.width * sizeof(uint32_t);
}

uint8_t*
Deinterlacer::RowBuffer(uint32_t aRow)
{
  uint32_t offset = aRow * RowSize();
  MOZ_ASSERT(IsValid(), "Deinterlacer in invalid state");
  MOZ_ASSERT(offset < mImageSize.width * mImageSize.height * sizeof(uint32_t),
             "Row is outside of image");
  return mBuffer.get() + offset;
}

void
Deinterlacer::PropagatePassToDownscaler(Downscaler& aDownscaler)
{
  MOZ_ASSERT(IsValid(), "Deinterlacer in invalid state");
  for (int32_t row = 0 ; row < mImageSize.height ; ++row) {
    memcpy(aDownscaler.RowBuffer(), RowBuffer(row), RowSize());
    aDownscaler.CommitRow();
  }
}

} // namespace image
} // namespace mozilla
