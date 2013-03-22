/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWBMPDecoder.h"
#include "RasterImage.h"
#include "nspr.h"
#include "nsRect.h"
#include "gfxPlatform.h"

#include "nsError.h"

namespace mozilla {
namespace image {

static inline void SetPixel(uint32_t*& aDecoded, bool aPixelWhite)
{
  uint8_t pixelValue = aPixelWhite ? 255 : 0;

  *aDecoded++ = gfxPackedPixel(0xFF, pixelValue, pixelValue, pixelValue);
}

/** Parses a WBMP encoded int field.  Returns IntParseInProgress (out of
 *  data), IntParseSucceeded if the field was read OK or IntParseFailed
 *  on an error.
 *  The encoding used for WBMP ints is per byte.  The high bit is a
 *  continuation flag saying (when set) that the next byte is part of the
 *  field, and the low seven bits are data.  New data bits are added in the
 *  low bit positions, i.e. the field is big-endian (ignoring the high bits).
 * @param aField Variable holds current value of field.  When this function
 *               returns IntParseInProgress, aField will hold the
 *               intermediate result of the decoding, so this function can be
 *               called repeatedly for new bytes on the same field and will
 *               operate correctly.
 * @param aBuffer Points to encoded field data.
 * @param aCount Number of bytes in aBuffer. */
static WbmpIntDecodeStatus DecodeEncodedInt (uint32_t& aField, const char*& aBuffer, uint32_t& aCount)
{
  while (aCount > 0) {
    // Check if the result would overflow if another seven bits were added.
    // The actual test performed is AND to check if any of the top seven bits are set.
    if (aField & 0xFE000000) {
      // Overflow :(
      return IntParseFailed;
    }

    // Get next encoded byte.
    char encodedByte = *aBuffer;

    // Update buffer state variables now we have read this byte.
    aBuffer++;
    aCount--;

    // Work out and store the new (valid) value of the encoded int with this byte added.
    aField = (aField << 7) + (uint32_t)(encodedByte & 0x7F);

    if (!(encodedByte & 0x80)) {
      // No more bytes, value is complete.
      return IntParseSucceeded;
    }
  }

  // Out of data but in the middle of an encoded int.
  return IntParseInProgress;
}

nsWBMPDecoder::nsWBMPDecoder(RasterImage &aImage)
 : Decoder(aImage),
   mWidth(0),
   mHeight(0),
   mRow(nullptr),
   mRowBytes(0),
   mCurLine(0),
   mState(WbmpStateStart)
{
  // Nothing to do
}

nsWBMPDecoder::~nsWBMPDecoder()
{
  moz_free(mRow);
}

void
nsWBMPDecoder::WriteInternal(const char *aBuffer, uint32_t aCount)
{
  NS_ABORT_IF_FALSE(!HasError(), "Shouldn't call WriteInternal after error!");

  // Loop until the input data is gone
  while (aCount > 0) {
    switch (mState) {
      case WbmpStateStart:
      {
        // Since we only accept a type 0 WBMP we can just check the first byte is 0.
        // (The specification says a well defined type 0 bitmap will start with a 0x00 byte).
        if (*aBuffer++ == 0x00) {
          // This is a type 0 WBMP.
          aCount--;
          mState = DecodingFixHeader;
        } else {
          // This is a new type of WBMP or a type 0 WBMP defined oddly (e.g. 0x80 0x00)
          PostDataError();
          mState = DecodingFailed;
          return;
        }
        break;
      }

      case DecodingFixHeader:
      {
        if ((*aBuffer++ & 0x9F) == 0x00) {
          // Fix header field is as expected
          aCount--;
          // For now, we skip the ext header field as it is not in a well-defined type 0 WBMP.
          mState = DecodingWidth;
        } else {
          // Can't handle this fix header field.
          PostDataError();
          mState = DecodingFailed;
          return;
        }
        break;
      }

      case DecodingWidth:
      {
        WbmpIntDecodeStatus widthReadResult = DecodeEncodedInt (mWidth, aBuffer, aCount);

        if (widthReadResult == IntParseSucceeded) {
          mState = DecodingHeight;
        } else if (widthReadResult == IntParseFailed) {
          // Encoded width was bigger than a uint32_t or equal to 0.
          PostDataError();
          mState = DecodingFailed;
          return;
        } else {
          // We are still parsing the encoded int field.
          NS_ABORT_IF_FALSE((widthReadResult == IntParseInProgress),
                            "nsWBMPDecoder got bad result from an encoded width field");
          return;
        }
        break;
      }

      case DecodingHeight:
      {
        WbmpIntDecodeStatus heightReadResult = DecodeEncodedInt (mHeight, aBuffer, aCount);

        if (heightReadResult == IntParseSucceeded) {
          // The header has now been entirely read.
          const uint32_t k64KWidth = 0x0000FFFF;
          if (mWidth == 0 || mWidth > k64KWidth
              || mHeight == 0 || mHeight > k64KWidth) {
            // consider 0 as an incorrect image size
            // reject the extremely wide/high images to keep the math sane
            PostDataError();
            mState = DecodingFailed;
            return;
          }

          // Post our size to the superclass
          PostSize(mWidth, mHeight);
          if (HasError()) {
            // Setting the size led to an error.
            mState = DecodingFailed;
            return;
          }

          // If We're doing a size decode, we're done
          if (IsSizeDecode()) {
            mState = WbmpStateFinished;
            return;
          }

          if (!mImageData) {
            PostDecoderError(NS_ERROR_FAILURE);
            mState = DecodingFailed;
            return;
          }

          // Create mRow, the buffer that holds one line of the raw image data
          mRow = (uint8_t*)moz_malloc((mWidth + 7) / 8);
          if (!mRow) {
            PostDecoderError(NS_ERROR_OUT_OF_MEMORY);
            mState = DecodingFailed;
            return;
          }

          mState = DecodingImageData;

        } else if (heightReadResult == IntParseFailed) {
          // Encoded height was bigger than a uint32_t.
          PostDataError();
          mState = DecodingFailed;
          return;
        } else {
          // We are still parsing the encoded int field.
          NS_ABORT_IF_FALSE((heightReadResult == IntParseInProgress),
                            "nsWBMPDecoder got bad result from an encoded height field");
          return;
        }
        break;
      }

      case DecodingImageData:
      {
        uint32_t rowSize = (mWidth + 7) / 8; // +7 to round up to nearest byte
        uint32_t top = mCurLine;

        // Process up to one row of data at a time until there is no more data.
        while ((aCount > 0) && (mCurLine < mHeight)) {
          // Calculate if we need to copy data to fill the next buffered row of raw data.
          uint32_t toCopy = rowSize - mRowBytes;

          // If required, copy raw data to fill a buffered row of raw data.
          if (toCopy) {
            if (toCopy > aCount)
              toCopy = aCount;
            memcpy(mRow + mRowBytes, aBuffer, toCopy);
            aCount -= toCopy;
            aBuffer += toCopy;
            mRowBytes += toCopy;
          }

          // If there is a filled buffered row of raw data, process the row.
          if (rowSize == mRowBytes) {
            uint8_t *p = mRow;
            uint32_t *d = reinterpret_cast<uint32_t*>(mImageData) + (mWidth * mCurLine); // position of the first pixel at mCurLine
            uint32_t lpos = 0;

            while (lpos < mWidth) {
              for (int8_t bit = 7; bit >= 0; bit--) {
                if (lpos >= mWidth)
                  break;
                bool pixelWhite = (*p >> bit) & 1;
                SetPixel(d, pixelWhite);
                ++lpos;
              }
              ++p;
            }

            mCurLine++;
            mRowBytes = 0;
          }
        }

        nsIntRect r(0, top, mWidth, mCurLine - top);
        // Invalidate
        PostInvalidation(r);

        // If we've got all the pixel bytes, we're finished
        if (mCurLine == mHeight) {
          PostFrameStop();
          PostDecodeDone();
          mState = WbmpStateFinished;
        }
        break;
      }

      case WbmpStateFinished:
      {
        // Consume all excess data silently
        aCount = 0;
        break;
      }

      case DecodingFailed:
      {
        NS_ABORT_IF_FALSE(0, "Shouldn't process any data after decode failed!");
        return;
      }
    }
  }
}

} // namespace image
} // namespace mozilla
