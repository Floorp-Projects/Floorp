/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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

static const uint32_t ICON_HEADER_SIZE = 2;

nsIconDecoder::nsIconDecoder(RasterImage* aImage)
 : Decoder(aImage)
 , mLexer(Transition::To(State::HEADER, ICON_HEADER_SIZE))
 , mWidth()         // set by ReadHeader()
 , mHeight()        // set by ReadHeader()
 , mBytesPerRow()   // set by ReadHeader()
 , mCurrentRow(0)
{
  // Nothing to do
}

nsIconDecoder::~nsIconDecoder()
{ }

void
nsIconDecoder::WriteInternal(const char* aBuffer, uint32_t aCount)
{
  MOZ_ASSERT(!HasError(), "Shouldn't call WriteInternal after error!");
  MOZ_ASSERT(aBuffer);
  MOZ_ASSERT(aCount > 0);

  Maybe<State> terminalState =
    mLexer.Lex(aBuffer, aCount, [=](State aState,
                                    const char* aData, size_t aLength) {
      switch (aState) {
        case State::HEADER:
          return ReadHeader(aData);
        case State::ROW_OF_PIXELS:
          return ReadRowOfPixels(aData, aLength);
        case State::FINISH:
          return Finish();
        default:
          MOZ_ASSERT_UNREACHABLE("Unknown State");
          return Transition::Terminate(State::FAILURE);
      }
    });

  if (!terminalState) {
    return;  // Need more data.
  }

  if (*terminalState == State::FAILURE) {
    PostDataError();
    return;
  }

  MOZ_ASSERT(*terminalState == State::SUCCESS);
}

LexerTransition<nsIconDecoder::State>
nsIconDecoder::ReadHeader(const char* aData)
{
  // Grab the width and height.
  mWidth  = uint8_t(aData[0]);
  mHeight = uint8_t(aData[1]);

  // The input is 32bpp, so we expect 4 bytes of data per pixel.
  mBytesPerRow = mWidth * 4;

  // Post our size to the superclass.
  PostSize(mWidth, mHeight);

  // Icons have alpha.
  PostHasTransparency();

  // If we're doing a metadata decode, we're done.
  if (IsMetadataDecode()) {
    return Transition::Terminate(State::SUCCESS);
  }

  MOZ_ASSERT(!mImageData, "Already have a buffer allocated?");
  IntSize targetSize = mDownscaler ? mDownscaler->TargetSize() : GetSize();
  nsresult rv = AllocateFrame(0, targetSize,
                              IntRect(IntPoint(), targetSize),
                              gfx::SurfaceFormat::B8G8R8A8);
  if (NS_FAILED(rv)) {
    return Transition::Terminate(State::FAILURE);
  }
  MOZ_ASSERT(mImageData, "Should have a buffer now");

  if (mDownscaler) {
    nsresult rv = mDownscaler->BeginFrame(GetSize(), Nothing(),
                                          mImageData, /* aHasAlpha = */ true);
    if (NS_FAILED(rv)) {
      return Transition::Terminate(State::FAILURE);
    }
  }

  return Transition::To(State::ROW_OF_PIXELS, mBytesPerRow);
}

LexerTransition<nsIconDecoder::State>
nsIconDecoder::ReadRowOfPixels(const char* aData, size_t aLength)
{
  if (mDownscaler) {
    memcpy(mDownscaler->RowBuffer(), aData, mBytesPerRow);
    mDownscaler->CommitRow();

    if (mDownscaler->HasInvalidation()) {
      DownscalerInvalidRect invalidRect = mDownscaler->TakeInvalidRect();
      PostInvalidation(invalidRect.mOriginalSizeRect,
                       Some(invalidRect.mTargetSizeRect));
    }
  } else {
    memcpy(mImageData + mCurrentRow * mBytesPerRow, aData, mBytesPerRow);

    PostInvalidation(IntRect(0, mCurrentRow, mWidth, 1));
  }
  mCurrentRow++;

  return (mCurrentRow < mHeight)
       ? Transition::To(State::ROW_OF_PIXELS, mBytesPerRow)
       : Transition::To(State::FINISH, 0);
}

LexerTransition<nsIconDecoder::State>
nsIconDecoder::Finish()
{
  PostFrameStop();
  PostDecodeDone();

  return Transition::Terminate(State::SUCCESS);
}

} // namespace image
} // namespace mozilla
