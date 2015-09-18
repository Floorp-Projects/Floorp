/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIconDecoder.h"
#include "nsIInputStream.h"
#include "nspr.h"
#include "nsRect.h"
#include "nsError.h"
#include "RasterImage.h"
#include <algorithm>

using namespace mozilla::gfx;

using std::min;

namespace mozilla {
namespace image {

nsIconDecoder::nsIconDecoder(RasterImage* aImage)
 : Decoder(aImage)
 , mExpectedDataLength(0)
 , mPixBytesRead(0)
 , mState(iconStateStart)
 , mWidth(-1)
 , mHeight(-1)
{
  // Nothing to do
}

nsIconDecoder::~nsIconDecoder()
{ }

void
nsIconDecoder::WriteInternal(const char* aBuffer, uint32_t aCount)
{
  MOZ_ASSERT(!HasError(), "Shouldn't call WriteInternal after error!");

  // Loop until the input data is gone
  while (aCount > 0) {
    switch (mState) {
      case iconStateStart:

        // Grab the width
        mWidth = (uint8_t)*aBuffer;

        // Book Keeping
        aBuffer++;
        aCount--;
        mState = iconStateHaveHeight;
        break;

      case iconStateHaveHeight:

        // Grab the Height
        mHeight = (uint8_t)*aBuffer;

        // Post our size to the superclass
        PostSize(mWidth, mHeight);

        PostHasTransparency();

        if (HasError()) {
          // Setting the size led to an error.
          mState = iconStateFinished;
          return;
        }

        // If we're doing a metadata decode, we're done.
        if (IsMetadataDecode()) {
          mState = iconStateFinished;
          break;
        }

        // The input is 32bpp, so we expect 4 bytes of data per pixel.
        mExpectedDataLength = mWidth * mHeight * 4;

        {
          MOZ_ASSERT(!mImageData, "Already have a buffer allocated?");
          IntSize targetSize = mDownscaler ? mDownscaler->TargetSize()
                                           : GetSize();
          nsresult rv = AllocateFrame(0, targetSize,
                                      IntRect(IntPoint(), targetSize),
                                      gfx::SurfaceFormat::B8G8R8A8);
          if (NS_FAILED(rv)) {
            mState = iconStateFinished;
            return;
          }
        }

        MOZ_ASSERT(mImageData, "Should have a buffer now");

        if (mDownscaler) {
          nsresult rv = mDownscaler->BeginFrame(GetSize(), Nothing(),
                                                mImageData,
                                                /* aHasAlpha = */ true);
          if (NS_FAILED(rv)) {
            mState = iconStateFinished;
            return;
          }
        }

        // Book Keeping
        aBuffer++;
        aCount--;
        mState = iconStateReadPixels;
        break;

      case iconStateReadPixels: {

        // How many bytes are we reading?
        uint32_t bytesToRead = min(aCount, mExpectedDataLength - mPixBytesRead);

        if (mDownscaler) {
          uint8_t* row = mDownscaler->RowBuffer();
          const uint32_t bytesPerRow = mWidth * 4;
          const uint32_t rowOffset = mPixBytesRead % bytesPerRow;

          // Update global state; we're about to read |bytesToRead| bytes.
          aCount -= bytesToRead;
          mPixBytesRead += bytesToRead;

          if (rowOffset > 0) {
            // Finish the current row.
            const uint32_t remaining = bytesPerRow - rowOffset;
            memcpy(row + rowOffset, aBuffer, remaining);
            aBuffer += remaining;
            bytesToRead -= remaining;
            mDownscaler->CommitRow();
          }

          // Copy the bytes a row at a time.
          while (bytesToRead > bytesPerRow) {
            memcpy(row, aBuffer, bytesPerRow);
            aBuffer += bytesPerRow;
            bytesToRead -= bytesPerRow;
            mDownscaler->CommitRow();
          }

          // Copy any leftover bytes. (Leaving the current row incomplete.)
          if (bytesToRead > 0) {
            memcpy(row, aBuffer, bytesToRead);
            aBuffer += bytesPerRow;
            bytesToRead -= bytesPerRow;
          }

          if (mDownscaler->HasInvalidation()) {
            DownscalerInvalidRect invalidRect = mDownscaler->TakeInvalidRect();
            PostInvalidation(invalidRect.mOriginalSizeRect,
                             Some(invalidRect.mTargetSizeRect));
          }
        } else {
          // Copy all the bytes at once.
          memcpy(mImageData + mPixBytesRead, aBuffer, bytesToRead);
          aBuffer += bytesToRead;
          aCount -= bytesToRead;
          mPixBytesRead += bytesToRead;

          // Invalidate. Performance isn't critical here, so our update
          // rectangle is always the full icon.
          PostInvalidation(IntRect(0, 0, mWidth, mHeight));
        }

        // If we've got all the pixel bytes, we're finished
        if (mPixBytesRead == mExpectedDataLength) {
          PostFrameStop();
          PostDecodeDone();
          mState = iconStateFinished;
        }
        break;
      }

      case iconStateFinished:

        // Consume all excess data silently
        aCount = 0;

        break;
    }
  }
}

} // namespace image
} // namespace mozilla
