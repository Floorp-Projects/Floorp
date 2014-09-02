/* vim:set tw=80 expandtab softtabstop=4 ts=4 sw=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef _nsBMPDecoder_h
#define _nsBMPDecoder_h

#include "nsAutoPtr.h"
#include "gfxColor.h"
#include "Decoder.h"
#include "BMPFileHeaders.h"

namespace mozilla {
namespace image {

class RasterImage;

/**
 * Decoder for BMP-Files, as used by Windows and OS/2
 */
class nsBMPDecoder : public Decoder
{
public:

    explicit nsBMPDecoder(RasterImage &aImage);
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
    // Obtains the size of the compressed image resource
    int32_t GetCompressedImageSize() const;
    // Obtains whether or not a BMP file had alpha data in its 4th byte
    // for 32BPP bitmaps.  Only use after the bitmap has been processed.
    bool HasAlphaData() const;

    virtual void WriteInternal(const char* aBuffer, uint32_t aCount, DecodeStrategy aStrategy);
    virtual void FinishInternal();

private:

    /** Calculates the red-, green- and blueshift in mBitFields using
     * the bitmasks from mBitFields */
    NS_METHOD CalcBitShift();

    uint32_t mPos; ///< Number of bytes read from aBuffer in WriteInternal()

    BMPFILEHEADER mBFH;
    BITMAPV5HEADER mBIH;
    char mRawBuf[WIN_V3_INTERNAL_BIH_LENGTH]; ///< If this is changed, WriteInternal() MUST be updated

    uint32_t mLOH; ///< Length of the header

    uint32_t mNumColors; ///< The number of used colors, i.e. the number of entries in mColors
    colorTable *mColors;

    bitFields mBitFields;

    uint8_t *mRow;      ///< Holds one raw line of the image
    uint32_t mRowBytes; ///< How many bytes of the row were already received
    int32_t mCurLine;   ///< Index of the line of the image that's currently being decoded: [height,1]
    int32_t mOldLine;   ///< Previous index of the line 
    int32_t mCurPos;    ///< Index in the current line of the image

    ERLEState mState;   ///< Maintains the current state of the RLE decoding
    uint32_t mStateData;///< Decoding information that is needed depending on mState

    /** Set mBFH from the raw data in mRawBuf, converting from little-endian
     * data to native data as necessary */
    void ProcessFileHeader();
    /** Set mBIH from the raw data in mRawBuf, converting from little-endian
     * data to native data as necessary */
    void ProcessInfoHeader();

    // Stores whether the image data may store alpha data, or if
    // the alpha data is unspecified and filled with a padding byte of 0. 
    // When a 32BPP bitmap is stored in an ICO or CUR file, its 4th byte
    // is used for alpha transparency.  When it is stored in a BMP, its
    // 4th byte is reserved and is always 0.
    // Reference: 
    // http://en.wikipedia.org/wiki/ICO_(file_format)#cite_note-9
    // Bitmaps where the alpha bytes are all 0 should be fully visible.
    bool mUseAlphaData;
    // Whether the 4th byte alpha data was found to be non zero and hence used.
    bool mHaveAlphaData;
};

/** Sets the pixel data in aDecoded to the given values.
 * @param aDecoded pointer to pixel to be set, will be incremented to point to the next pixel.
 */
static inline void SetPixel(uint32_t*& aDecoded, uint8_t aRed, uint8_t aGreen, uint8_t aBlue, uint8_t aAlpha = 0xFF)
{
    *aDecoded++ = gfxPackedPixel(aAlpha, aRed, aGreen, aBlue);
}

static inline void SetPixel(uint32_t*& aDecoded, uint8_t idx, colorTable* aColors)
{
    SetPixel(aDecoded, aColors[idx].red, aColors[idx].green, aColors[idx].blue);
}

/** Sets two (or one if aCount = 1) pixels
 * @param aDecoded where the data is stored. Will be moved 4 resp 8 bytes
 * depending on whether one or two pixels are written.
 * @param aData The values for the two pixels
 * @param aCount Current count. Is decremented by one or two.
 */
inline void Set4BitPixel(uint32_t*& aDecoded, uint8_t aData,
                         uint32_t& aCount, colorTable* aColors)
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


#endif

