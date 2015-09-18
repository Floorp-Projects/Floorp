/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_nsIconDecoder_h
#define mozilla_image_decoders_nsIconDecoder_h

#include "Decoder.h"

#include "nsCOMPtr.h"

namespace mozilla {
namespace image {
class RasterImage;

////////////////////////////////////////////////////////////////////////////////
// The icon decoder is a decoder specifically tailored for loading icons
// from the OS. We've defined our own little format to represent these icons
// and this decoder takes that format and converts it into 24-bit RGB with
// alpha channel support. It was modeled a bit off the PPM decoder.
//
// Assumptions about the decoder:
// (1) We receive ALL of the data from the icon channel in one OnDataAvailable
//     call. We don't support multiple ODA calls yet.
// (2) the format of the incoming data is as follows:
//     The first two bytes contain the width and the height of the icon.
//     The remaining bytes contain the icon data, 4 bytes per pixel, in
//       ARGB order (platform endianness, A in highest bits, B in lowest
//       bits), row-primary, top-to-bottom, left-to-right, with
//       premultiplied alpha.
//
//
////////////////////////////////////////////////////////////////////////////////

class nsIconDecoder : public Decoder
{
public:
  virtual ~nsIconDecoder();

  virtual void WriteInternal(const char* aBuffer, uint32_t aCount) override;

private:
  friend class DecoderFactory;

  // Decoders should only be instantiated via DecoderFactory.
  explicit nsIconDecoder(RasterImage* aImage);

  uint32_t mExpectedDataLength;
  uint32_t mPixBytesRead;
  uint32_t mState;
  uint8_t mWidth;
  uint8_t mHeight;
};

enum {
  iconStateStart      = 0,
  iconStateHaveHeight = 1,
  iconStateReadPixels = 2,
  iconStateFinished   = 3
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_decoders_nsIconDecoder_h
