/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_nsIconDecoder_h
#define mozilla_image_decoders_nsIconDecoder_h

#include "Decoder.h"
#include "StreamingLexer.h"
#include "SurfacePipe.h"

namespace mozilla {
namespace image {

class RasterImage;

////////////////////////////////////////////////////////////////////////////////
// The icon decoder is a decoder specifically tailored for loading icons
// from the OS. We've defined our own little format to represent these icons
// and this decoder takes that format and converts it into 24-bit RGB with
// alpha channel support. It was modeled a bit off the PPM decoder.
//
// The format of the incoming data is as follows:
//
// The first two bytes contain the width and the height of the icon.
// The remaining bytes contain the icon data, 4 bytes per pixel, in
// ARGB order (platform endianness, A in highest bits, B in lowest
// bits), row-primary, top-to-bottom, left-to-right, with
// premultiplied alpha.
//
////////////////////////////////////////////////////////////////////////////////

class nsIconDecoder : public Decoder
{
public:
  virtual ~nsIconDecoder();

  LexerResult DoDecode(SourceBufferIterator& aIterator,
                       IResumable* aOnResume) override;

private:
  friend class DecoderFactory;

  // Decoders should only be instantiated via DecoderFactory.
  explicit nsIconDecoder(RasterImage* aImage);

  enum class State {
    HEADER,
    ROW_OF_PIXELS,
    FINISH
  };

  LexerTransition<State> ReadHeader(const char* aData);
  LexerTransition<State> ReadRowOfPixels(const char* aData, size_t aLength);
  LexerTransition<State> Finish();

  StreamingLexer<State> mLexer;
  SurfacePipe mPipe;
  uint32_t mBytesPerRow;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_decoders_nsIconDecoder_h
