/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIconDecoder.h"
#include "nsIInputStream.h"
#include "RasterImage.h"
#include "nspr.h"
#include "nsRect.h"

#include "nsError.h"
#include <algorithm>

namespace mozilla {
namespace image {

nsIconDecoder::nsIconDecoder(RasterImage &aImage)
 : Decoder(aImage),
   mWidth(-1),
   mHeight(-1),
   mPixBytesRead(0),
   mState(iconStateStart)
{
  // Nothing to do
}

nsIconDecoder::~nsIconDecoder()
{ }

void
nsIconDecoder::WriteInternal(const char *aBuffer, uint32_t aCount, DecodeStrategy)
{
  NS_ABORT_IF_FALSE(!HasError(), "Shouldn't call WriteInternal after error!");

  // We put this here to avoid errors about crossing initialization with case
  // jumps on linux.
  uint32_t bytesToRead = 0;

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
        if (HasError()) {
          // Setting the size led to an error.
          mState = iconStateFinished;
          return;
        }

        // If We're doing a size decode, we're done
        if (IsSizeDecode()) {
          mState = iconStateFinished;
          break;
        }

        if (!mImageData) {
          PostDecoderError(NS_ERROR_OUT_OF_MEMORY);
          return;
        }

        // Book Keeping
        aBuffer++;
        aCount--;
        mState = iconStateReadPixels;
        break;

      case iconStateReadPixels:
      {

        // How many bytes are we reading?
        bytesToRead = std::min(aCount, mImageDataLength - mPixBytesRead);

        // Copy the bytes
        memcpy(mImageData + mPixBytesRead, aBuffer, bytesToRead);

        // Performance isn't critical here, so our update rectangle is
        // always the full icon
        nsIntRect r(0, 0, mWidth, mHeight);

        // Invalidate
        PostInvalidation(r);

        // Book Keeping
        aBuffer += bytesToRead;
        aCount -= bytesToRead;
        mPixBytesRead += bytesToRead;

        // If we've got all the pixel bytes, we're finished
        if (mPixBytesRead == mImageDataLength) {
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
