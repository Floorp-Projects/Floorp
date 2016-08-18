/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SurfacePipe.h"

#include <utility>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "Decoder.h"

namespace mozilla {
namespace image {

using namespace gfx;

using std::min;

/* static */ UniquePtr<NullSurfaceSink> NullSurfaceSink::sSingleton;

/* static */ NullSurfaceSink*
NullSurfaceSink::Singleton()
{
  if (!sSingleton) {
    MOZ_ASSERT(NS_IsMainThread());
    sSingleton = MakeUnique<NullSurfaceSink>();
    ClearOnShutdown(&sSingleton);

    DebugOnly<nsresult> rv = sSingleton->Configure(NullSurfaceConfig { });
    MOZ_ASSERT(NS_SUCCEEDED(rv), "Couldn't configure a NullSurfaceSink?");
  }

  return sSingleton.get();
}

nsresult
NullSurfaceSink::Configure(const NullSurfaceConfig& aConfig)
{
  // Note that the choice of uint32_t as the pixel size here is more or less
  // arbitrary, since you cannot write to a NullSurfaceSink anyway, but uint32_t
  // is a natural choice since most SurfacePipes will be for BGRA/BGRX surfaces.
  ConfigureFilter(IntSize(), sizeof(uint32_t));
  return NS_OK;
}

Maybe<SurfaceInvalidRect>
AbstractSurfaceSink::TakeInvalidRect()
{
  if (mInvalidRect.IsEmpty()) {
    return Nothing();
  }

  SurfaceInvalidRect invalidRect;
  invalidRect.mInputSpaceRect = invalidRect.mOutputSpaceRect = mInvalidRect;

  // Forget about the invalid rect we're returning.
  mInvalidRect = IntRect();

  return Some(invalidRect);
}

void
AbstractSurfaceSink::DoZeroOutRestOfSurface()
{
  // If there is no explicit clear value, we know the surface was already
  // zeroed out by default.
  if (!mClearRequired) {
    return;
  }

  // The reason we do not use the typical WritePixels methods here is
  // because if it is a progressively decoded image, we know that we
  // already have valid data written to the buffer, just not the final
  // result. There is no need to clear the buffers in that case.
  const int32_t width = InputSize().width;
  const int32_t height = InputSize().height;
  const int32_t pixelSize = IsValidPalettedPipe() ? sizeof(uint8_t)
                                                  : sizeof(uint32_t);
  const int32_t stride = width * pixelSize;

  // In addition to fully unwritten rows, the current row may be partially
  // written (i.e. mCol > 0). Since the row was not advanced, we have yet to
  // update mWrittenRect so it would be cleared without the exception below.
  const int32_t col = CurrentColumn();
  if (MOZ_UNLIKELY(col > 0 && col <= width)) {
    uint8_t* rowPtr = CurrentRowPointer();
    MOZ_ASSERT(rowPtr);
    memset(rowPtr + col * pixelSize, mClearValue, (width - col) * pixelSize);
    AdvanceRow(); // updates mInvalidRect and mWrittenRect
  }

  MOZ_ASSERT(mWrittenRect.x == 0);
  MOZ_ASSERT(mWrittenRect.width == 0 || mWrittenRect.width == width);

  if (MOZ_UNLIKELY(mWrittenRect.y > 0)) {
    const uint32_t length = mWrittenRect.y * stride;
    auto updateRect = IntRect(0, 0, width, mWrittenRect.y);
    MOZ_ASSERT(mImageDataLength >= length);
    memset(mImageData, mClearValue, length);
    mInvalidRect.UnionRect(mInvalidRect, updateRect);
    mWrittenRect.UnionRect(mWrittenRect, updateRect);
  }

  const int32_t top = mWrittenRect.y + mWrittenRect.height;
  if (MOZ_UNLIKELY(top < height)) {
    const int32_t remainder = height - top;
    auto updateRect = IntRect(0, top, width, remainder);
    MOZ_ASSERT(mImageDataLength >= (uint32_t)(height * stride));
    memset(mImageData + top * stride, mClearValue, remainder * stride);
    mInvalidRect.UnionRect(mInvalidRect, updateRect);
    mWrittenRect.UnionRect(mWrittenRect, updateRect);
  }
}

uint8_t*
AbstractSurfaceSink::DoResetToFirstRow()
{
  mRow = 0;
  return GetRowPointer();
}

uint8_t*
AbstractSurfaceSink::DoAdvanceRow()
{
  if (mRow >= uint32_t(InputSize().height)) {
    return nullptr;
  }

  // If we're vertically flipping the output, we need to flip the invalid rect. Since we're
  // dealing with an axis-aligned rect, only the y coordinate needs to change.
  int32_t invalidY = mFlipVertically
                   ? InputSize().height - (mRow + 1)
                   : mRow;
  mInvalidRect.UnionRect(mInvalidRect,
                         IntRect(0, invalidY, InputSize().width, 1));
  mWrittenRect.UnionRect(mWrittenRect, mInvalidRect);

  mRow = min(uint32_t(InputSize().height), mRow + 1);

  return mRow < uint32_t(InputSize().height) ? GetRowPointer()
                                             : nullptr;
}

nsresult
SurfaceSink::Configure(const SurfaceConfig& aConfig)
{
  // For non-paletted surfaces, the surface size is just the output size.
  IntSize surfaceSize = aConfig.mOutputSize;

  // Non-paletted surfaces should not have frame rects, so we just pass
  // AllocateFrame() a frame rect which covers the entire surface.
  IntRect frameRect(0, 0, surfaceSize.width, surfaceSize.height);

  // Allocate the frame.
  // XXX(seth): Once every Decoder subclass uses SurfacePipe, we probably want
  // to allocate the frame directly here and get rid of Decoder::AllocateFrame
  // altogether.
  nsresult rv = aConfig.mDecoder->AllocateFrame(aConfig.mFrameNum,
                                                surfaceSize,
                                                frameRect,
                                                aConfig.mFormat);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mImageData = aConfig.mDecoder->mImageData;
  mImageDataLength = aConfig.mDecoder->mImageDataLength;
  mFlipVertically = aConfig.mFlipVertically;

  if (aConfig.mFormat == SurfaceFormat::B8G8R8X8) {
    mClearRequired = true;
    mClearValue = 0xFF;
  } else if (aConfig.mDecoder->mCurrentFrame->OnHeap()) {
    mClearRequired = true;
    mClearValue = 0;
  }

  MOZ_ASSERT(mImageData);
  MOZ_ASSERT(mImageDataLength ==
               uint32_t(surfaceSize.width * surfaceSize.height * sizeof(uint32_t)));

  ConfigureFilter(surfaceSize, sizeof(uint32_t));
  return NS_OK;
}

uint8_t*
SurfaceSink::GetRowPointer() const
{
  // If we're flipping vertically, reverse the order in which we traverse the
  // rows.
  uint32_t row = mFlipVertically
               ? InputSize().height - (mRow + 1)
               : mRow;

  uint8_t* rowPtr = mImageData + row * InputSize().width * sizeof(uint32_t);

  MOZ_ASSERT(rowPtr >= mImageData);
  MOZ_ASSERT(rowPtr < mImageData + mImageDataLength);
  MOZ_ASSERT(rowPtr + InputSize().width * sizeof(uint32_t) <=
               mImageData + mImageDataLength);

  return rowPtr;
}


nsresult
PalettedSurfaceSink::Configure(const PalettedSurfaceConfig& aConfig)
{
  MOZ_ASSERT(aConfig.mFormat == SurfaceFormat::B8G8R8A8);
  MOZ_ASSERT(mClearValue == 0);

  // For paletted surfaces, the surface size is the size of the frame rect.
  IntSize surfaceSize = aConfig.mFrameRect.Size();

  // Allocate the frame.
  // XXX(seth): Once every Decoder subclass uses SurfacePipe, we probably want
  // to allocate the frame directly here and get rid of Decoder::AllocateFrame
  // altogether.
  nsresult rv = aConfig.mDecoder->AllocateFrame(aConfig.mFrameNum,
                                                aConfig.mOutputSize,
                                                aConfig.mFrameRect,
                                                aConfig.mFormat,
                                                aConfig.mPaletteDepth);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mImageData = aConfig.mDecoder->mImageData;
  mImageDataLength = aConfig.mDecoder->mImageDataLength;
  mFlipVertically = aConfig.mFlipVertically;
  mFrameRect = aConfig.mFrameRect;
  mClearRequired = true;

  MOZ_ASSERT(mImageData);
  MOZ_ASSERT(mImageDataLength ==
               uint32_t(mFrameRect.width * mFrameRect.height * sizeof(uint8_t)));

  ConfigureFilter(surfaceSize, sizeof(uint8_t));
  return NS_OK;
}

uint8_t*
PalettedSurfaceSink::GetRowPointer() const
{
  // If we're flipping vertically, reverse the order in which we traverse the
  // rows.
  uint32_t row = mFlipVertically
               ? InputSize().height - (mRow + 1)
               : mRow;

  uint8_t* rowPtr = mImageData + row * InputSize().width * sizeof(uint8_t);

  MOZ_ASSERT(rowPtr >= mImageData);
  MOZ_ASSERT(rowPtr < mImageData + mImageDataLength);
  MOZ_ASSERT(rowPtr + InputSize().width * sizeof(uint8_t) <=
               mImageData + mImageDataLength);

  return rowPtr;
}

} // namespace image
} // namespace mozilla
