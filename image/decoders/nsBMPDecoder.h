/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_nsBMPDecoder_h
#define mozilla_image_decoders_nsBMPDecoder_h

#include "BMPFileHeaders.h"
#include "Decoder.h"
#include "gfxColor.h"
#include "StreamingLexer.h"

namespace mozilla {
namespace image {

namespace bmp {

/// An entry in the color table.
struct ColorTableEntry {
  uint8_t mRed;
  uint8_t mGreen;
  uint8_t mBlue;
};

/// All the color-related bitfields for 16bpp and 32bpp images. We use this
/// even for older format BMPs that don't have explicit bitfields.
class BitFields {
  class Value {
    friend class BitFields;

    uint32_t mMask;       // The mask for the value.
    uint8_t mRightShift;  // The amount to right-shift after masking.
    uint8_t mBitWidth;    // The width (in bits) of the value.

    /// Sets the mask (and thus the right-shift and bit-width as well).
    void Set(uint32_t aMask);

  public:
    /// Extracts the single color value from the multi-color value.
    uint8_t Get(uint32_t aVal) const;

    /// Specialized version of Get() for the case where the bit-width is 5.
    /// (It will assert if called and the bit-width is not 5.)
    uint8_t Get5(uint32_t aVal) const;
  };

public:
  /// The individual color channels.
  Value mRed;
  Value mGreen;
  Value mBlue;

  /// Set bitfields to the standard 5-5-5 16bpp values.
  void SetR5G5B5();

  /// Test if bitfields have the standard 5-5-5 16bpp values.
  bool IsR5G5B5() const;

  /// Read the bitfields from a header.
  void ReadFromHeader(const char* aData);

  /// Length of the bitfields structure in the BMP file.
  static const size_t LENGTH = 12;
};

} // namespace bmp

class RasterImage;

/// Decoder for BMP-Files, as used by Windows and OS/2.

class nsBMPDecoder : public Decoder
{
public:
  ~nsBMPDecoder();

  /// Specifies whether or not the BMP file will contain alpha data. If set to
  /// true and the BMP is 32BPP, the alpha data will be retrieved from the 4th
  /// byte of image data per pixel.
  void SetUseAlphaData(bool useAlphaData);

  /// Obtains the bits per pixel from the internal BIH header.
  int32_t GetBitsPerPixel() const;

  /// Obtains the width from the internal BIH header.
  int32_t GetWidth() const;

  /// Obtains the abs-value of the height from the internal BIH header.
  int32_t GetHeight() const;

  /// Obtains the internal output image buffer.
  uint32_t* GetImageData();
  size_t GetImageDataLength() const { return mImageDataLength; }

  /// Obtains the size of the compressed image resource.
  int32_t GetCompressedImageSize() const;

  /// Obtains whether or not a BMP file had alpha data in its 4th byte for 32BPP
  /// bitmaps. Use only after the bitmap has been processed.
  bool HasAlphaData() const { return mHaveAlphaData; }

  /// Marks this BMP as having alpha data (due to e.g. an ICO alpha mask).
  void SetHasAlphaData() { mHaveAlphaData = true; }

  virtual void WriteInternal(const char* aBuffer,
                             uint32_t aCount) override;
  virtual void FinishInternal() override;

private:
  friend class DecoderFactory;
  friend class nsICODecoder;

  // Decoders should only be instantiated via DecoderFactory.
  // XXX(seth): nsICODecoder is temporarily an exception to this rule.
  explicit nsBMPDecoder(RasterImage* aImage);

  uint32_t* RowBuffer();

  void FinishRow();

  enum class State {
    FILE_HEADER,
    INFO_HEADER_SIZE,
    INFO_HEADER_REST,
    BITFIELDS,
    COLOR_TABLE,
    GAP,
    PIXEL_ROW,
    RLE_SEGMENT,
    RLE_DELTA,
    RLE_ABSOLUTE,
    SUCCESS,
    FAILURE
  };

  LexerTransition<State> ReadFileHeader(const char* aData, size_t aLength);
  LexerTransition<State> ReadInfoHeaderSize(const char* aData, size_t aLength);
  LexerTransition<State> ReadInfoHeaderRest(const char* aData, size_t aLength);
  LexerTransition<State> ReadBitfields(const char* aData, size_t aLength);
  LexerTransition<State> ReadColorTable(const char* aData, size_t aLength);
  LexerTransition<State> SkipGap();
  LexerTransition<State> ReadPixelRow(const char* aData);
  LexerTransition<State> ReadRLESegment(const char* aData);
  LexerTransition<State> ReadRLEDelta(const char* aData);
  LexerTransition<State> ReadRLEAbsolute(const char* aData, size_t aLength);

  StreamingLexer<State> mLexer;

  bmp::FileHeader mBFH;
  bmp::V5InfoHeader mBIH;

  bmp::BitFields mBitFields;

  uint32_t mNumColors;      // The number of used colors, i.e. the number of
                            // entries in mColors, if it's present.
  bmp::ColorTableEntry* mColors; // The color table, if it's present.
  uint32_t mBytesPerColor;  // 3 or 4, depending on the format

  // The number of bytes prior to the optional gap that have been read. This
  // is used to find the start of the pixel data.
  uint32_t mPreGapLength;

  uint32_t mPixelRowSize;   // The number of bytes per pixel row.

  int32_t mCurrentRow;      // Index of the row of the image that's currently
                            // being decoded: [height,1].
  int32_t mCurrentPos;      // Index into the current line; only used when
                            // doing RLE decoding.

  // Only used in RLE_ABSOLUTE state: the number of pixels to read.
  uint32_t mAbsoluteModeNumPixels;

  // Stores whether the image data may store alpha data, or if the alpha data
  // is unspecified and filled with a padding byte of 0. When a 32BPP bitmap
  // is stored in an ICO or CUR file, its 4th byte is used for alpha
  // transparency.  When it is stored in a BMP, its 4th byte is reserved and
  // is always 0. Reference:
  //   http://en.wikipedia.org/wiki/ICO_(file_format)#cite_note-9
  // Bitmaps where the alpha bytes are all 0 should be fully visible.
  bool mUseAlphaData;

  // Whether the 4th byte alpha data was found to be non zero and hence used.
  bool mHaveAlphaData;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_decoders_nsBMPDecoder_h
