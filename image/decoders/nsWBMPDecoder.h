/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWBMPDecoder_h__
#define nsWBMPDecoder_h__

#include "Decoder.h"
#include "nsCOMPtr.h"
#include "imgDecoderObserver.h"
#include "gfxColor.h"

namespace mozilla {
namespace image {
class RasterImage;

/* WBMP is a monochrome graphics file format optimized for mobile computing devices.
 * Format description from http://www.wapforum.org/what/technical/SPEC-WAESpec-19990524.pdf
 */

typedef enum {
  WbmpStateStart,
  DecodingFixHeader,
  DecodingWidth,
  DecodingHeight,
  DecodingImageData,
  DecodingFailed,
  WbmpStateFinished
} WbmpDecodingState;

typedef enum {
  IntParseSucceeded,
  IntParseFailed,
  IntParseInProgress
} WbmpIntDecodeStatus;

class nsWBMPDecoder : public Decoder
{
public:

  nsWBMPDecoder(RasterImage &aImage);
  virtual ~nsWBMPDecoder();

  virtual void WriteInternal(const char* aBuffer, uint32_t aCount);

private:
  uint32_t mWidth;
  uint32_t mHeight;

  uint8_t* mRow;                    // Holds one raw line of the image
  uint32_t mRowBytes;               // How many bytes of the row were already received
  uint32_t mCurLine;                // The current line being decoded (0 to mHeight - 1)

  WbmpDecodingState mState;         // Describes what part of the file we are decoding now.
};

} // namespace image
} // namespace mozilla

#endif // nsWBMPDecoder_h__
