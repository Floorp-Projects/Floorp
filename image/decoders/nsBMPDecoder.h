/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_nsBMPDecoder_h
#define mozilla_image_decoders_nsBMPDecoder_h

#include "BMPFileHeaders.h"
#include "Decoder.h"
#include "gfxColor.h"
#include "nsAutoPtr.h"
#include "StreamingLexer.h"

namespace mozilla {
namespace image {

class RasterImage;

/// Decoder for BMP-Files, as used by Windows and OS/2

class nsBMPDecoder : public Decoder
{
public:
    ~nsBMPDecoder();

    // Specifies whether or not the BMP file will contain alpha data
    // If set to true and the BMP is 32BPP, the alpha data will be
    // retrieved from the 4th byte of image data per pixel
    void SetUseAlphaData(bool useAlphaData);

    // Obtains the bits per pixel from the internal BIH header
    int32_t GetBitsPerPixel() const;

    // Obtains the width from the internal BIH header
    int32_t GetWidth() const;

    // Obtains the abs-value of the height from the internal BIH header
    int32_t GetHeight() const;

    // Obtains the internal output image buffer
    uint32_t* GetImageData();
    size_t GetImageDataLength() const { return mImageDataLength; }

    // Obtains the size of the compressed image resource
    int32_t GetCompressedImageSize() const;

    // Obtains whether or not a BMP file had alpha data in its 4th byte
    // for 32BPP bitmaps.  Only use after the bitmap has been processed.
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

    /// Calculates the red-, green- and blueshift in mBitFields using
    /// the bitmasks from mBitFields
    void CalcBitShift();

    void DoReadBitfields(const char* aData);

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
    bmp::ColorTable* mColors; // The color table, if it's present.
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

/// Sets the pixel data in aDecoded to the given values.
/// @param aDecoded pointer to pixel to be set, will be incremented to point to
/// the next pixel.
static inline void
SetPixel(uint32_t*& aDecoded, uint8_t aRed, uint8_t aGreen,
         uint8_t aBlue, uint8_t aAlpha = 0xFF)
{
    *aDecoded++ = gfxPackedPixel(aAlpha, aRed, aGreen, aBlue);
}

static inline void
SetPixel(uint32_t*& aDecoded, uint8_t idx, bmp::ColorTable* aColors)
{
    SetPixel(aDecoded, aColors[idx].red, aColors[idx].green, aColors[idx].blue);
}

/// Sets two (or one if aCount = 1) pixels
/// @param aDecoded where the data is stored. Will be moved 4 resp 8 bytes
/// depending on whether one or two pixels are written.
/// @param aData The values for the two pixels
/// @param aCount Current count. Is decremented by one or two.
inline void
Set4BitPixel(uint32_t*& aDecoded, uint8_t aData, uint32_t& aCount,
             bmp::ColorTable* aColors)
{
    uint8_t idx = aData >> 4;
    SetPixel(aDecoded, idx, aColors);
    if (--aCount > 0) {
        idx = aData & 0xF;
        SetPixel(aDecoded, idx, aColors);
        --aCount;
    }
}

} // namespace image
} // namespace mozilla

#endif // mozilla_image_decoders_nsBMPDecoder_h
