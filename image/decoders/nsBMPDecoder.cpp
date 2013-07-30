/* vim:set tw=80 expandtab softtabstop=4 ts=4 sw=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* I got the format description from http://www.daubnet.com/formats/BMP.html */

/* This is a Cross-Platform BMP Decoder, which should work everywhere, including
 * Big-Endian machines like the PowerPC. */

#include <stdlib.h>

#include "ImageLogging.h"
#include "EndianMacros.h"
#include "nsBMPDecoder.h"

#include "nsIInputStream.h"
#include "RasterImage.h"
#include <algorithm>

namespace mozilla {
namespace image {

#ifdef PR_LOGGING
static PRLogModuleInfo *
GetBMPLog()
{
  static PRLogModuleInfo *sBMPLog;
  if (!sBMPLog)
    sBMPLog = PR_NewLogModule("BMPDecoder");
  return sBMPLog;
}
#endif

// Convert from row (1..height) to absolute line (0..height-1)
#define LINE(row) ((mBIH.height < 0) ? (-mBIH.height - (row)) : ((row) - 1))
#define PIXEL_OFFSET(row, col) (LINE(row) * mBIH.width + col)

nsBMPDecoder::nsBMPDecoder(RasterImage &aImage)
 : Decoder(aImage)
{
  mColors = nullptr;
  mRow = nullptr;
  mCurPos = mPos = mNumColors = mRowBytes = 0;
  mOldLine = mCurLine = 1; // Otherwise decoder will never start
  mState = eRLEStateInitial;
  mStateData = 0;
  mLOH = WIN_V3_HEADER_LENGTH;
  mUseAlphaData = mHaveAlphaData = false;
}

nsBMPDecoder::~nsBMPDecoder()
{
  delete[] mColors;
  if (mRow) {
      moz_free(mRow);
  }
}

// Sets whether or not the BMP will use alpha data
void 
nsBMPDecoder::SetUseAlphaData(bool useAlphaData) 
{
  mUseAlphaData = useAlphaData;
}

// Obtains the bits per pixel from the internal BIH header
int32_t 
nsBMPDecoder::GetBitsPerPixel() const
{
  return mBIH.bpp;
}

// Obtains the width from the internal BIH header
int32_t 
nsBMPDecoder::GetWidth() const
{
  return mBIH.width; 
}

// Obtains the abs-value of the height from the internal BIH header
int32_t 
nsBMPDecoder::GetHeight() const
{
  return abs(mBIH.height);
}

// Obtains the internal output image buffer
uint32_t* 
nsBMPDecoder::GetImageData() 
{
  return reinterpret_cast<uint32_t*>(mImageData);
}

// Obtains the size of the compressed image resource
int32_t 
nsBMPDecoder::GetCompressedImageSize() const
{
  // For everything except BI_RGB the header field must be defined
  if (mBIH.compression != BI_RGB) {
    return mBIH.image_size;
  }

  // mBIH.image_size isn't always filled for BI_RGB so calculate it manually
  // The pixel array size is calculated based on extra 4 byte boundary padding
  uint32_t rowSize = (mBIH.bpp * mBIH.width + 7) / 8; // + 7 to round up
  // Pad to DWORD Boundary
  if (rowSize % 4) {
    rowSize += (4 - (rowSize % 4));
  }

  // The height should be the absolute value of what the height is in the BIH.
  // If positive the bitmap is stored bottom to top, otherwise top to bottom
  int32_t pixelArraySize = rowSize * GetHeight();
  return pixelArraySize;
}

// Obtains whether or not a BMP file had alpha data in its 4th byte
// for 32BPP bitmaps.  Only use after the bitmap has been processed.
bool 
nsBMPDecoder::HasAlphaData() const 
{
  return mHaveAlphaData;
}


void
nsBMPDecoder::FinishInternal()
{
    // We shouldn't be called in error cases
    NS_ABORT_IF_FALSE(!HasError(), "Can't call FinishInternal on error!");

    // We should never make multiple frames
    NS_ABORT_IF_FALSE(GetFrameCount() <= 1, "Multiple BMP frames?");

    // Send notifications if appropriate
    if (!IsSizeDecode() && HasSize()) {

        // Invalidate
        nsIntRect r(0, 0, mBIH.width, GetHeight());
        PostInvalidation(r);

        if (mUseAlphaData) {
          PostFrameStop(FrameBlender::kFrameHasAlpha);
        } else {
          PostFrameStop(FrameBlender::kFrameOpaque);
        }
        PostDecodeDone();
    }
}

// ----------------------------------------
// Actual Data Processing
// ----------------------------------------

static void calcBitmask(uint32_t aMask, uint8_t& aBegin, uint8_t& aLength)
{
    // find the rightmost 1
    uint8_t pos;
    bool started = false;
    aBegin = aLength = 0;
    for (pos = 0; pos <= 31; pos++) {
        if (!started && (aMask & (1 << pos))) {
            aBegin = pos;
            started = true;
        }
        else if (started && !(aMask & (1 << pos))) {
            aLength = pos - aBegin;
            break;
        }
    }
}

NS_METHOD nsBMPDecoder::CalcBitShift()
{
    uint8_t begin, length;
    // red
    calcBitmask(mBitFields.red, begin, length);
    mBitFields.redRightShift = begin;
    mBitFields.redLeftShift = 8 - length;
    // green
    calcBitmask(mBitFields.green, begin, length);
    mBitFields.greenRightShift = begin;
    mBitFields.greenLeftShift = 8 - length;
    // blue
    calcBitmask(mBitFields.blue, begin, length);
    mBitFields.blueRightShift = begin;
    mBitFields.blueLeftShift = 8 - length;
    return NS_OK;
}

void
nsBMPDecoder::WriteInternal(const char* aBuffer, uint32_t aCount)
{
    NS_ABORT_IF_FALSE(!HasError(), "Shouldn't call WriteInternal after error!");

    // aCount=0 means EOF, mCurLine=0 means we're past end of image
    if (!aCount || !mCurLine)
        return;

    if (mPos < BFH_INTERNAL_LENGTH) { /* In BITMAPFILEHEADER */
        uint32_t toCopy = BFH_INTERNAL_LENGTH - mPos;
        if (toCopy > aCount)
            toCopy = aCount;
        memcpy(mRawBuf + mPos, aBuffer, toCopy);
        mPos += toCopy;
        aCount -= toCopy;
        aBuffer += toCopy;
    }
    if (mPos == BFH_INTERNAL_LENGTH) {
        ProcessFileHeader();
        if (mBFH.signature[0] != 'B' || mBFH.signature[1] != 'M') {
            PostDataError();
            return;
        }
        if (mBFH.bihsize == OS2_BIH_LENGTH)
            mLOH = OS2_HEADER_LENGTH;
    }
    if (mPos >= BFH_INTERNAL_LENGTH && mPos < mLOH) { /* In BITMAPINFOHEADER */
        uint32_t toCopy = mLOH - mPos;
        if (toCopy > aCount)
            toCopy = aCount;
        memcpy(mRawBuf + (mPos - BFH_INTERNAL_LENGTH), aBuffer, toCopy);
        mPos += toCopy;
        aCount -= toCopy;
        aBuffer += toCopy;
    }

    // HasSize is called to ensure that if at this point mPos == mLOH but
    // we have no data left to process, the next time WriteInternal is called
    // we won't enter this condition again.
    if (mPos == mLOH && !HasSize()) {
        ProcessInfoHeader();
        PR_LOG(GetBMPLog(), PR_LOG_DEBUG, ("BMP is %lix%lix%lu. compression=%lu\n",
               mBIH.width, mBIH.height, mBIH.bpp, mBIH.compression));
        // Verify we support this bit depth
        if (mBIH.bpp != 1 && mBIH.bpp != 4 && mBIH.bpp != 8 &&
            mBIH.bpp != 16 && mBIH.bpp != 24 && mBIH.bpp != 32) {
          PostDataError();
          return;
        }

        // BMPs with negative width are invalid
        // Reject extremely wide images to keep the math sane
        const int32_t k64KWidth = 0x0000FFFF;
        if (mBIH.width < 0 || mBIH.width > k64KWidth) {
            PostDataError();
            return;
        }

        if (mBIH.height == INT_MIN) {
            PostDataError();
            return;
        }

        uint32_t real_height = GetHeight();

        // Post our size to the superclass
        PostSize(mBIH.width, real_height);
        if (HasError()) {
          // Setting the size led to an error.
          return;
        }

        // We have the size. If we're doing a size decode, we got what
        // we came for.
        if (IsSizeDecode())
            return;

        // We're doing a real decode.
        mOldLine = mCurLine = real_height;

        if (mBIH.bpp <= 8) {
            mNumColors = 1 << mBIH.bpp;
            if (mBIH.colors && mBIH.colors < mNumColors)
                mNumColors = mBIH.colors;

            // Always allocate 256 even though mNumColors might be smaller
            mColors = new colorTable[256];
            memset(mColors, 0, 256 * sizeof(colorTable));
        }
        else if (mBIH.compression != BI_BITFIELDS && mBIH.bpp == 16) {
            // Use default 5-5-5 format
            mBitFields.red   = 0x7C00;
            mBitFields.green = 0x03E0;
            mBitFields.blue  = 0x001F;
            CalcBitShift();
        }

        // Make sure we have a valid value for our supported compression modes
        // before adding the frame
        if (mBIH.compression != BI_RGB && mBIH.compression != BI_RLE8 && 
            mBIH.compression != BI_RLE4 && mBIH.compression != BI_BITFIELDS) {
          PostDataError();
          return;
        }

        // If we have RLE4 or RLE8 or BI_ALPHABITFIELDS, then ensure we
        // have valid BPP values before adding the frame
        if (mBIH.compression == BI_RLE8 && mBIH.bpp != 8) {
          PR_LOG(GetBMPLog(), PR_LOG_DEBUG, 
                 ("BMP RLE8 compression only supports 8 bits per pixel\n"));
          PostDataError();
          return;
        }
        if (mBIH.compression == BI_RLE4 && mBIH.bpp != 4 && mBIH.bpp != 1) {
          PR_LOG(GetBMPLog(), PR_LOG_DEBUG, 
                 ("BMP RLE4 compression only supports 4 bits per pixel\n"));
          PostDataError();
          return;
        }
        if (mBIH.compression == BI_ALPHABITFIELDS && 
            mBIH.bpp != 16 && mBIH.bpp != 32) {
          PR_LOG(GetBMPLog(), PR_LOG_DEBUG, 
                 ("BMP ALPHABITFIELDS only supports 16 or 32 bits per pixel\n"));
          PostDataError();
          return;
        }

        if (mBIH.compression != BI_RLE8 && mBIH.compression != BI_RLE4 &&
            mBIH.compression != BI_ALPHABITFIELDS) {
            // mRow is not used for RLE encoded images
            mRow = (uint8_t*)moz_malloc((mBIH.width * mBIH.bpp) / 8 + 4);
            // + 4 because the line is padded to a 4 bit boundary, but I don't want
            // to make exact calculations here, that's unnecessary.
            // Also, it compensates rounding error.
            if (!mRow) {
              PostDataError();
              return;
            }
        }
        if (!mImageData) {
            PostDecoderError(NS_ERROR_FAILURE);
            return;
        }

        // Prepare for transparency
        if ((mBIH.compression == BI_RLE8) || (mBIH.compression == BI_RLE4)) {
            // Clear the image, as the RLE may jump over areas
            memset(mImageData, 0, mImageDataLength);
        }
    }

    if (mColors && mPos >= mLOH) {
      // OS/2 Bitmaps have no padding byte
      uint8_t bytesPerColor = (mBFH.bihsize == OS2_BIH_LENGTH) ? 3 : 4;
      if (mPos < (mLOH + mNumColors * bytesPerColor)) {
        // Number of bytes already received
        uint32_t colorBytes = mPos - mLOH; 
        // Color which is currently received
        uint8_t colorNum = colorBytes / bytesPerColor;
        uint8_t at = colorBytes % bytesPerColor;
        while (aCount && (mPos < (mLOH + mNumColors * bytesPerColor))) {
            switch (at) {
                case 0:
                    mColors[colorNum].blue = *aBuffer;
                    break;
                case 1:
                    mColors[colorNum].green = *aBuffer;
                    break;
                case 2:
                    mColors[colorNum].red = *aBuffer;
                    // If there is no padding byte, increment the color index
                    // since we're done with the current color.
                    if (bytesPerColor == 3)
                      colorNum++;
                    break;
                case 3:
                    // This is a padding byte only in Windows BMPs. Increment
                    // the color index since we're done with the current color.
                    colorNum++;
                    break;
            }
            mPos++; aBuffer++; aCount--;
            at = (at + 1) % bytesPerColor;
        }
      }
    }
    else if (aCount && mBIH.compression == BI_BITFIELDS && mPos < (WIN_V3_HEADER_LENGTH + BITFIELD_LENGTH)) {
        // If compression is used, this is a windows bitmap, hence we can
        // use WIN_HEADER_LENGTH instead of mLOH
        uint32_t toCopy = (WIN_V3_HEADER_LENGTH + BITFIELD_LENGTH) - mPos;
        if (toCopy > aCount)
            toCopy = aCount;
        memcpy(mRawBuf + (mPos - WIN_V3_HEADER_LENGTH), aBuffer, toCopy);
        mPos += toCopy;
        aBuffer += toCopy;
        aCount -= toCopy;
    }
    if (mPos == WIN_V3_HEADER_LENGTH + BITFIELD_LENGTH && 
        mBIH.compression == BI_BITFIELDS) {
        mBitFields.red = LITTLE_TO_NATIVE32(*(uint32_t*)mRawBuf);
        mBitFields.green = LITTLE_TO_NATIVE32(*(uint32_t*)(mRawBuf + 4));
        mBitFields.blue = LITTLE_TO_NATIVE32(*(uint32_t*)(mRawBuf + 8));
        CalcBitShift();
    }
    while (aCount && (mPos < mBFH.dataoffset)) { // Skip whatever is between header and data
        mPos++; aBuffer++; aCount--;
    }
    if (aCount && ++mPos >= mBFH.dataoffset) {
        // Need to increment mPos, else we might get to mPos==mLOH again
        // From now on, mPos is irrelevant
        if (!mBIH.compression || mBIH.compression == BI_BITFIELDS) {
            uint32_t rowSize = (mBIH.bpp * mBIH.width + 7) / 8; // + 7 to round up
            if (rowSize % 4) {
                rowSize += (4 - (rowSize % 4)); // Pad to DWORD Boundary
            }
            uint32_t toCopy;
            do {
                toCopy = rowSize - mRowBytes;
                if (toCopy) {
                    if (toCopy > aCount)
                        toCopy = aCount;
                    memcpy(mRow + mRowBytes, aBuffer, toCopy);
                    aCount -= toCopy;
                    aBuffer += toCopy;
                    mRowBytes += toCopy;
                }
                if (rowSize == mRowBytes) {
                    // Collected a whole row into mRow, process it
                    uint8_t* p = mRow;
                    uint32_t* d = reinterpret_cast<uint32_t*>(mImageData) + PIXEL_OFFSET(mCurLine, 0);
                    uint32_t lpos = mBIH.width;
                    switch (mBIH.bpp) {
                      case 1:
                        while (lpos > 0) {
                          int8_t bit;
                          uint8_t idx;
                          for (bit = 7; bit >= 0 && lpos > 0; bit--) {
                              idx = (*p >> bit) & 1;
                              SetPixel(d, idx, mColors);
                              --lpos;
                          }
                          ++p;
                        }
                        break;
                      case 4:
                        while (lpos > 0) {
                          Set4BitPixel(d, *p, lpos, mColors);
                          ++p;
                        }
                        break;
                      case 8:
                        while (lpos > 0) {
                          SetPixel(d, *p, mColors);
                          --lpos;
                          ++p;
                        }
                        break;
                      case 16:
                        while (lpos > 0) {
                          uint16_t val = LITTLE_TO_NATIVE16(*(uint16_t*)p);
                          SetPixel(d,
                                  (val & mBitFields.red) >> mBitFields.redRightShift << mBitFields.redLeftShift,
                                  (val & mBitFields.green) >> mBitFields.greenRightShift << mBitFields.greenLeftShift,
                                  (val & mBitFields.blue) >> mBitFields.blueRightShift << mBitFields.blueLeftShift);
                          --lpos;
                          p+=2;
                        }
                        break;
                      case 24:
                        while (lpos > 0) {
                          SetPixel(d, p[2], p[1], p[0]);
                          p += 2;
                          --lpos;
                          ++p;
                        }
                        break;
                      case 32:
                        while (lpos > 0) {
                          if (mUseAlphaData) {
                            if (!mHaveAlphaData && p[3]) {
                              // Non-zero alpha byte detected! Clear previous
                              // pixels that we have already processed.
                              // This works because we know that if we 
                              // are reaching here then the alpha data in byte 
                              // 4 has been right all along.  And we know it
                              // has been set to 0 the whole time, so that 
                              // means that everything is transparent so far.
                              uint32_t* start = reinterpret_cast<uint32_t*>(mImageData) + GetWidth() * (mCurLine - 1);
                              uint32_t heightDifference = GetHeight() - mCurLine + 1;
                              uint32_t pixelCount = GetWidth() * heightDifference;

                              memset(start, 0, pixelCount * sizeof(uint32_t));

                              mHaveAlphaData = true;
                            }
                            SetPixel(d, p[2], p[1], p[0], mHaveAlphaData ? p[3] : 0xFF);
                          } else {
                            SetPixel(d, p[2], p[1], p[0]);
                          }
                          p += 4;
                          --lpos;
                        }
                        break;
                      default:
                        NS_NOTREACHED("Unsupported color depth, but earlier check didn't catch it");
                    }
                    mCurLine --;
                    if (mCurLine == 0) { // Finished last line
                      break;
                    }
                    mRowBytes = 0;

                }
            } while (aCount > 0);
        }
        else if ((mBIH.compression == BI_RLE8) || (mBIH.compression == BI_RLE4)) {
            if (((mBIH.compression == BI_RLE8) && (mBIH.bpp != 8)) || 
                ((mBIH.compression == BI_RLE4) && (mBIH.bpp != 4) && (mBIH.bpp != 1))) {
              PR_LOG(GetBMPLog(), PR_LOG_DEBUG, ("BMP RLE8/RLE4 compression only supports 8/4 bits per pixel\n"));
              PostDataError();
              return;
            }

            while (aCount > 0) {
                uint8_t byte;

                switch(mState) {
                    case eRLEStateInitial:
                        mStateData = (uint8_t)*aBuffer++;
                        aCount--;

                        mState = eRLEStateNeedSecondEscapeByte;
                        continue;

                    case eRLEStateNeedSecondEscapeByte:
                        byte = *aBuffer++;
                        aCount--;
                        if (mStateData != RLE_ESCAPE) { // encoded mode
                            // Encoded mode consists of two bytes: 
                            // the first byte (mStateData) specifies the
                            // number of consecutive pixels to be drawn 
                            // using the color index contained in
                            // the second byte
                            // Work around bitmaps that specify too many pixels
                            mState = eRLEStateInitial;
                            uint32_t pixelsNeeded = std::min<uint32_t>(mBIH.width - mCurPos, mStateData);
                            if (pixelsNeeded) {
                                uint32_t* d = reinterpret_cast<uint32_t*>(mImageData) + PIXEL_OFFSET(mCurLine, mCurPos);
                                mCurPos += pixelsNeeded;
                                if (mBIH.compression == BI_RLE8) {
                                    do {
                                        SetPixel(d, byte, mColors);
                                        pixelsNeeded --;
                                    } while (pixelsNeeded);
                                } else {
                                    do {
                                        Set4BitPixel(d, byte, pixelsNeeded, mColors);
                                    } while (pixelsNeeded);
                                }
                            }
                            continue;
                        }

                        switch(byte) {
                            case RLE_ESCAPE_EOL:
                                // End of Line: Go to next row
                                mCurLine --;
                                mCurPos = 0;
                                mState = eRLEStateInitial;
                                break;

                            case RLE_ESCAPE_EOF: // EndOfFile
                                mCurPos = mCurLine = 0;
                                break;

                            case RLE_ESCAPE_DELTA:
                                mState = eRLEStateNeedXDelta;
                                continue;

                            default : // absolute mode
                                // Save the number of pixels to read
                                mStateData = byte;
                                if (mCurPos + mStateData > (uint32_t)mBIH.width) {
                                    // We can work around bitmaps that specify one
                                    // pixel too many, but only if their width is odd.
                                    mStateData -= mBIH.width & 1;
                                    if (mCurPos + mStateData > (uint32_t)mBIH.width) {
                                        PostDataError();
                                        return;
                                    }
                                }

                                // See if we will need to skip a byte
                                // to word align the pixel data
                                // mStateData is a number of pixels
                                // so allow for the RLE compression type
                                // Pixels RLE8=1 RLE4=2
                                //    1    Pad    Pad
                                //    2    No     Pad
                                //    3    Pad    No
                                //    4    No     No
                                if (((mStateData - 1) & mBIH.compression) != 0)
                                    mState = eRLEStateAbsoluteMode;
                                else
                                    mState = eRLEStateAbsoluteModePadded;
                                continue;
                        }
                        break;

                    case eRLEStateNeedXDelta:
                        // Handle the XDelta and proceed to get Y Delta
                        byte = *aBuffer++;
                        aCount--;
                        mCurPos += byte;
                        if (mCurPos > mBIH.width)
                            mCurPos = mBIH.width;

                        mState = eRLEStateNeedYDelta;
                        continue;

                    case eRLEStateNeedYDelta:
                        // Get the Y Delta and then "handle" the move
                        byte = *aBuffer++;
                        aCount--;
                        mState = eRLEStateInitial;
                        mCurLine -= std::min<int32_t>(byte, mCurLine);
                        break;

                    case eRLEStateAbsoluteMode: // Absolute Mode
                    case eRLEStateAbsoluteModePadded:
                        if (mStateData) {
                            // In absolute mode, the second byte (mStateData)
                            // represents the number of pixels 
                            // that follow, each of which contains 
                            // the color index of a single pixel.
                            uint32_t* d = reinterpret_cast<uint32_t*>(mImageData) + PIXEL_OFFSET(mCurLine, mCurPos);
                            uint32_t* oldPos = d;
                            if (mBIH.compression == BI_RLE8) {
                                while (aCount > 0 && mStateData > 0) {
                                    byte = *aBuffer++;
                                    aCount--;
                                    SetPixel(d, byte, mColors);
                                    mStateData--;
                                }
                            } else {
                                while (aCount > 0 && mStateData > 0) {
                                    byte = *aBuffer++;
                                    aCount--;
                                    Set4BitPixel(d, byte, mStateData, mColors);
                                }
                            }
                            mCurPos += d - oldPos;
                        }

                        if (mStateData == 0) {
                            // In absolute mode, each run must 
                            // be aligned on a word boundary

                            if (mState == eRLEStateAbsoluteMode) { // Word Aligned
                                mState = eRLEStateInitial;
                            } else if (aCount > 0) {               // Not word Aligned
                                // "next" byte is just a padding byte
                                // so "move" past it and we can continue
                                aBuffer++;
                                aCount--;
                                mState = eRLEStateInitial;
                            }
                        }
                        // else state is still eRLEStateAbsoluteMode
                        continue;

                    default :
                        NS_ABORT_IF_FALSE(0, "BMP RLE decompression: unknown state!");
                        PostDecoderError(NS_ERROR_UNEXPECTED);
                        return;
                }
                // Because of the use of the continue statement
                // we only get here for eol, eof or y delta
                if (mCurLine == 0) { // Finished last line
                    break;
                }
            }
        }
    }

    const uint32_t rows = mOldLine - mCurLine;
    if (rows) {

        // Invalidate
        nsIntRect r(0, mBIH.height < 0 ? -mBIH.height - mOldLine : mCurLine,
                    mBIH.width, rows);
        PostInvalidation(r);

        mOldLine = mCurLine;
    }

    return;
}

void nsBMPDecoder::ProcessFileHeader()
{
    memset(&mBFH, 0, sizeof(mBFH));
    memcpy(&mBFH.signature, mRawBuf, sizeof(mBFH.signature));
    memcpy(&mBFH.filesize, mRawBuf + 2, sizeof(mBFH.filesize));
    memcpy(&mBFH.reserved, mRawBuf + 6, sizeof(mBFH.reserved));
    memcpy(&mBFH.dataoffset, mRawBuf + 10, sizeof(mBFH.dataoffset));
    memcpy(&mBFH.bihsize, mRawBuf + 14, sizeof(mBFH.bihsize));

    // Now correct the endianness of the header
    mBFH.filesize = LITTLE_TO_NATIVE32(mBFH.filesize);
    mBFH.dataoffset = LITTLE_TO_NATIVE32(mBFH.dataoffset);
    mBFH.bihsize = LITTLE_TO_NATIVE32(mBFH.bihsize);
}

void nsBMPDecoder::ProcessInfoHeader()
{
    memset(&mBIH, 0, sizeof(mBIH));
    if (mBFH.bihsize == 12) { // OS/2 Bitmap
        memcpy(&mBIH.width, mRawBuf, 2);
        memcpy(&mBIH.height, mRawBuf + 2, 2);
        memcpy(&mBIH.planes, mRawBuf + 4, sizeof(mBIH.planes));
        memcpy(&mBIH.bpp, mRawBuf + 6, sizeof(mBIH.bpp));
    }
    else {
        memcpy(&mBIH.width, mRawBuf, sizeof(mBIH.width));
        memcpy(&mBIH.height, mRawBuf + 4, sizeof(mBIH.height));
        memcpy(&mBIH.planes, mRawBuf + 8, sizeof(mBIH.planes));
        memcpy(&mBIH.bpp, mRawBuf + 10, sizeof(mBIH.bpp));
        memcpy(&mBIH.compression, mRawBuf + 12, sizeof(mBIH.compression));
        memcpy(&mBIH.image_size, mRawBuf + 16, sizeof(mBIH.image_size));
        memcpy(&mBIH.xppm, mRawBuf + 20, sizeof(mBIH.xppm));
        memcpy(&mBIH.yppm, mRawBuf + 24, sizeof(mBIH.yppm));
        memcpy(&mBIH.colors, mRawBuf + 28, sizeof(mBIH.colors));
        memcpy(&mBIH.important_colors, mRawBuf + 32, sizeof(mBIH.important_colors));
    }

    // Convert endianness
    mBIH.width = LITTLE_TO_NATIVE32(mBIH.width);
    mBIH.height = LITTLE_TO_NATIVE32(mBIH.height);
    mBIH.planes = LITTLE_TO_NATIVE16(mBIH.planes);
    mBIH.bpp = LITTLE_TO_NATIVE16(mBIH.bpp);

    mBIH.compression = LITTLE_TO_NATIVE32(mBIH.compression);
    mBIH.image_size = LITTLE_TO_NATIVE32(mBIH.image_size);
    mBIH.xppm = LITTLE_TO_NATIVE32(mBIH.xppm);
    mBIH.yppm = LITTLE_TO_NATIVE32(mBIH.yppm);
    mBIH.colors = LITTLE_TO_NATIVE32(mBIH.colors);
    mBIH.important_colors = LITTLE_TO_NATIVE32(mBIH.important_colors);
}

} // namespace image
} // namespace mozilla
