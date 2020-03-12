/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIconDecoder.h"
#include "RasterImage.h"
#include "SurfacePipeFactory.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace image {

static const uint32_t ICON_HEADER_SIZE = 4;

nsIconDecoder::nsIconDecoder(RasterImage* aImage)
    : Decoder(aImage),
      mLexer(Transition::To(State::HEADER, ICON_HEADER_SIZE),
             Transition::TerminateSuccess()),
      mBytesPerRow()  // set by ReadHeader()
{
  // Nothing to do
}

nsIconDecoder::~nsIconDecoder() {}

LexerResult nsIconDecoder::DoDecode(SourceBufferIterator& aIterator,
                                    IResumable* aOnResume) {
  MOZ_ASSERT(!HasError(), "Shouldn't call DoDecode after error!");

  return mLexer.Lex(aIterator, aOnResume,
                    [=](State aState, const char* aData, size_t aLength) {
                      switch (aState) {
                        case State::HEADER:
                          return ReadHeader(aData);
                        case State::ROW_OF_PIXELS:
                          return ReadRowOfPixels(aData, aLength);
                        case State::FINISH:
                          return Finish();
                        default:
                          MOZ_CRASH("Unknown State");
                      }
                    });
}

LexerTransition<nsIconDecoder::State> nsIconDecoder::ReadHeader(
    const char* aData) {
  // Grab the width and height.
  uint8_t width = uint8_t(aData[0]);
  uint8_t height = uint8_t(aData[1]);
  SurfaceFormat format = SurfaceFormat(aData[2]);
  bool transform = bool(aData[3]);

  // FIXME(aosmond): On OSX we get the icon in device space and already
  // premultiplied, so we can't support the surface flags with icons right now.
  SurfacePipeFlags pipeFlags = SurfacePipeFlags();
  if (transform) {
    if (mCMSMode == eCMSMode_All) {
      mTransform = GetCMSsRGBTransform(format);
    }

    if (!(GetSurfaceFlags() & SurfaceFlags::NO_PREMULTIPLY_ALPHA)) {
      pipeFlags |= SurfacePipeFlags::PREMULTIPLY_ALPHA;
    }
  }

  // The input is 32bpp, so we expect 4 bytes of data per pixel.
  mBytesPerRow = width * 4;

  // Post our size to the superclass.
  PostSize(width, height);

  // Icons have alpha.
  PostHasTransparency();

  // If we're doing a metadata decode, we're done.
  if (IsMetadataDecode()) {
    return Transition::TerminateSuccess();
  }

  MOZ_ASSERT(!mImageData, "Already have a buffer allocated?");
  Maybe<SurfacePipe> pipe = SurfacePipeFactory::CreateSurfacePipe(
      this, Size(), OutputSize(), FullFrame(), format, SurfaceFormat::OS_RGBA,
      /* aAnimParams */ Nothing(), mTransform, pipeFlags);
  if (!pipe) {
    return Transition::TerminateFailure();
  }

  mPipe = std::move(*pipe);

  MOZ_ASSERT(mImageData, "Should have a buffer now");

  return Transition::To(State::ROW_OF_PIXELS, mBytesPerRow);
}

LexerTransition<nsIconDecoder::State> nsIconDecoder::ReadRowOfPixels(
    const char* aData, size_t aLength) {
  MOZ_ASSERT(aLength % 4 == 0, "Rows should contain a multiple of four bytes");

  auto result = mPipe.WriteBuffer(reinterpret_cast<const uint32_t*>(aData));
  MOZ_ASSERT(result != WriteState::FAILURE);

  Maybe<SurfaceInvalidRect> invalidRect = mPipe.TakeInvalidRect();
  if (invalidRect) {
    PostInvalidation(invalidRect->mInputSpaceRect,
                     Some(invalidRect->mOutputSpaceRect));
  }

  return result == WriteState::FINISHED
             ? Transition::To(State::FINISH, 0)
             : Transition::To(State::ROW_OF_PIXELS, mBytesPerRow);
}

LexerTransition<nsIconDecoder::State> nsIconDecoder::Finish() {
  PostFrameStop();
  PostDecodeDone();

  return Transition::TerminateSuccess();
}

}  // namespace image
}  // namespace mozilla
