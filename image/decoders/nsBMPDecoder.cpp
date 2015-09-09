/* vim:set tw=80 expandtab softtabstop=4 ts=4 sw=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// I got the format description from http://www.daubnet.com/formats/BMP.html

// This is a Cross-Platform BMP Decoder, which should work everywhere, including
// Big-Endian machines like the PowerPC.

#include <stdlib.h>

#include "ImageLogging.h"
#include "mozilla/Endian.h"
#include "mozilla/Likely.h"
#include "nsBMPDecoder.h"

#include "nsIInputStream.h"
#include "RasterImage.h"
#include <algorithm>

namespace mozilla {
namespace image {

static PRLogModuleInfo*
GetBMPLog()
{
  static PRLogModuleInfo* sBMPLog;
  if (!sBMPLog) {
    sBMPLog = PR_NewLogModule("BMPDecoder");
  }
  return sBMPLog;
}

// Convert from row (1..height) to absolute line (0..height-1)
#define LINE(row) ((mBIH.height < 0) ? (-mBIH.height - (row)) : ((row) - 1))
#define PIXEL_OFFSET(row, col) (LINE(row) * mBIH.width + col)

nsBMPDecoder::nsBMPDecoder(RasterImage* aImage)
  : Decoder(aImage)
  , mPos(0)
  , mLOH(BMP_HEADER_LENGTH::WIN_V3)
  , mNumColors(0)
  , mColors(nullptr)
  , mRow(nullptr)
  , mRowBytes(0)
  , mCurLine(1)  // Otherwise decoder will never start.
  , mOldLine(1)
  , mCurPos(0)
  , mState(eRLEStateInitial)
  , mStateData(0)
  , mProcessedHeader(false)
  , mUseAlphaData(false)
  , mHaveAlphaData(false)
{ }

nsBMPDecoder::~nsBMPDecoder()
{
  delete[] mColors;
  if (mRow) {
      free(mRow);
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
  // For everything except BMPINFOHEADER::RGB the header field must be defined
  if (mBIH.compression != BMPINFOHEADER::RGB) {
    return mBIH.image_size;
  }

  // mBIH.image_size isn't always filled for BMPINFOHEADER::RGB so calculate it
  // manually. The pixel array size is calculated based on extra 4 byte
  // boundary padding.
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

void
nsBMPDecoder::FinishInternal()
{
    // We shouldn't be called in error cases
    MOZ_ASSERT(!HasError(), "Can't call FinishInternal on error!");

    // We should never make multiple frames
    MOZ_ASSERT(GetFrameCount() <= 1, "Multiple BMP frames?");

    // Send notifications if appropriate
    if (!IsMetadataDecode() && HasSize()) {

        // Invalidate
        nsIntRect r(0, 0, mBIH.width, GetHeight());
        PostInvalidation(r);

        if (mUseAlphaData && mHaveAlphaData) {
          PostFrameStop(Opacity::SOME_TRANSPARENCY);
        } else {
          PostFrameStop(Opacity::OPAQUE);
        }
        PostDecodeDone();
    }
}

// ----------------------------------------
// Actual Data Processing
// ----------------------------------------

static void
calcBitmask(uint32_t aMask, uint8_t& aBegin, uint8_t& aLength)
{
    // find the rightmost 1
    uint8_t pos;
    bool started = false;
    aBegin = aLength = 0;
    for (pos = 0; pos <= 31; pos++) {
        if (!started && (aMask & (1 << pos))) {
            aBegin = pos;
            started = true;
        } else if (started && !(aMask & (1 << pos))) {
            aLength = pos - aBegin;
            break;
        }
    }
}

NS_METHOD
nsBMPDecoder::CalcBitShift()
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
  MOZ_ASSERT(!HasError(), "Shouldn't call WriteInternal after error!");

  // aCount=0 means EOF, mCurLine=0 means we're past end of image
  if (!aCount || !mCurLine) {
      return;
  }

  // This code assumes that mRawBuf == BIH_INTERNAL_LENGTH::WIN_V3
  // and that sizeof(mRawBuf) >= BMPFILEHEADER::INTERNAL_LENGTH
  MOZ_ASSERT(sizeof(mRawBuf) == BIH_INTERNAL_LENGTH::WIN_V3);
  MOZ_ASSERT(sizeof(mRawBuf) >= BMPFILEHEADER::INTERNAL_LENGTH);
  MOZ_ASSERT(BIH_INTERNAL_LENGTH::OS2 < BIH_INTERNAL_LENGTH::WIN_V3);

  // This code also assumes it's working with a byte array
  MOZ_ASSERT(sizeof(mRawBuf[0]) == 1);

  if (mPos < BMPFILEHEADER::INTERNAL_LENGTH) { /* In BITMAPFILEHEADER */
      // BMPFILEHEADER::INTERNAL_LENGTH < sizeof(mRawBuf)
      // mPos < BMPFILEHEADER::INTERNAL_LENGTH
      // BMPFILEHEADER::INTERNAL_LENGTH - mPos < sizeof(mRawBuf)
      // so toCopy <= BMPFILEHEADER::INTERNAL_LENGTH
      // so toCopy < sizeof(mRawBuf)
      // so toCopy > 0 && toCopy <= BMPFILEHEADER::INTERNAL_LENGTH
      uint32_t toCopy = BMPFILEHEADER::INTERNAL_LENGTH - mPos;
      if (toCopy > aCount) {
          toCopy = aCount;
      }

      // mRawBuf is a byte array of size BIH_INTERNAL_LENGTH::WIN_V3
      // (verified above)
      // mPos is < BMPFILEHEADER::INTERNAL_LENGTH
      // BMPFILEHEADER::INTERNAL_LENGTH < BIH_INTERNAL_LENGTH::WIN_V3
      // so mPos < sizeof(mRawBuf)
      //
      // Therefore this assert should hold
      MOZ_ASSERT(mPos < sizeof(mRawBuf));

      // toCopy <= BMPFILEHEADER::INTERNAL_LENGTH
      // mPos >= 0 && mPos < BMPFILEHEADER::INTERNAL_LENGTH
      // sizeof(mRawBuf) >= BMPFILEHEADER::INTERNAL_LENGTH (verified above)
      //
      // Therefore this assert should hold
      MOZ_ASSERT(mPos + toCopy <= sizeof(mRawBuf));

      memcpy(mRawBuf + mPos, aBuffer, toCopy);
      mPos += toCopy;
      aCount -= toCopy;
      aBuffer += toCopy;
  }
  if (mPos == BMPFILEHEADER::INTERNAL_LENGTH) {
      ProcessFileHeader();
      if (mBFH.signature[0] != 'B' || mBFH.signature[1] != 'M') {
          PostDataError();
          return;
      }
      if (mBFH.bihsize == BIH_LENGTH::OS2) {
          mLOH = BMP_HEADER_LENGTH::OS2;
      }
  }
  if (mPos >= BMPFILEHEADER::INTERNAL_LENGTH && mPos < mLOH) { /* In BITMAPINFOHEADER */
      // mLOH == BMP_HEADER_LENGTH::WIN_V3 || mLOH == BMP_HEADER_LENGTH::OS2
      // BMP_HEADER_LENGTH::OS2 < BMP_HEADER_LENGTH::WIN_V3
      // BMPFILEHEADER::INTERNAL_LENGTH < BMP_HEADER_LENGTH::OS2
      // BMPFILEHEADER::INTERNAL_LENGTH < BMP_HEADER_LENGTH::WIN_V3
      //
      // So toCopy is in the range
      //      1 to (BMP_HEADER_LENGTH::WIN_V3 - BMPFILEHEADER::INTERNAL_LENGTH)
      // or   1 to (BMP_HEADER_LENGTH::OS2 - BMPFILEHEADER::INTERNAL_LENGTH)
      //
      // But BMP_HEADER_LENGTH::WIN_V3 =
      //     BMPFILEHEADER::INTERNAL_LENGTH + BIH_INTERNAL_LENGTH::WIN_V3
      // and BMP_HEADER_LENGTH::OS2 = BMPFILEHEADER::INTERNAL_LENGTH + BIH_INTERNAL_LENGTH::OS2
      //
      // So toCopy is in the range
      //
      //      1 to BIH_INTERNAL_LENGTH::WIN_V3
      // or   1 to BIH_INTERNAL_LENGTH::OS2
      // and  BIH_INTERNAL_LENGTH::OS2 < BIH_INTERNAL_LENGTH::WIN_V3
      //
      // sizeof(mRawBuf) = BIH_INTERNAL_LENGTH::WIN_V3
      // so toCopy <= sizeof(mRawBuf)
      uint32_t toCopy = mLOH - mPos;
      if (toCopy > aCount) {
          toCopy = aCount;
      }

      // mPos is in the range
      //      BMPFILEHEADER::INTERNAL_LENGTH to (BMP_HEADER_LENGTH::WIN_V3 - 1)
      //
      // offset is then in the range (see toCopy comments for more details)
      //      0 to (BIH_INTERNAL_LENGTH::WIN_V3 - 1)
      //
      // sizeof(mRawBuf) is BIH_INTERNAL_LENGTH::WIN_V3 so this
      // offset stays within bounds and this assert should hold
      const uint32_t offset = mPos - BMPFILEHEADER::INTERNAL_LENGTH;
      MOZ_ASSERT(offset < sizeof(mRawBuf));

      // Two cases:
      //      mPos = BMPFILEHEADER::INTERNAL_LENGTH
      //      mLOH = BMP_HEADER_LENGTH::WIN_V3
      //
      // offset = 0
      // toCopy = BIH_INTERNAL_LENGTH::WIN_V3
      //
      //      This will be in the bounds of sizeof(mRawBuf)
      //
      // Second Case:
      //      mPos = BMP_HEADER_LENGTH::WIN_V3 - 1
      //      mLOH = BMP_HEADER_LENGTH::WIN_V3
      //
      // offset = BIH_INTERNAL_LENGTH::WIN_V3 - 1
      // toCopy = 1
      //
      //      This will be in the bounds of sizeof(mRawBuf)
      //
      // As sizeof(mRawBuf) == BIH_INTERNAL_LENGTH::WIN_V3 (verified above)
      // and BMP_HEADER_LENGTH::WIN_V3 is the largest range of values. If mLOH
      // was equal to BMP_HEADER_LENGTH::OS2 then the ranges are smaller.
      MOZ_ASSERT(offset + toCopy <= sizeof(mRawBuf));

      memcpy(mRawBuf + offset, aBuffer, toCopy);
      mPos += toCopy;
      aCount -= toCopy;
      aBuffer += toCopy;
  }

  // At this point mPos should be >= mLOH unless aBuffer did not have enough
  // data. In the latter case aCount should be 0.
  MOZ_ASSERT(mPos >= mLOH || aCount == 0);

  // mProcessedHeader is checked to ensure that if at this point mPos == mLOH
  // but we have no data left to process, the next time WriteInternal is called
  // we won't enter this condition again.
  if (mPos == mLOH && !mProcessedHeader) {
      mProcessedHeader = true;

      ProcessInfoHeader();
      MOZ_LOG(GetBMPLog(), LogLevel::Debug,
             ("BMP is %lix%lix%lu. compression=%lu\n",
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

      // We treat BMPs as transparent if they're 32bpp and alpha is enabled, but
      // also if they use RLE encoding, because the 'delta' mode can skip pixels
      // and cause implicit transparency.
      if ((mBIH.compression == BMPINFOHEADER::RLE8) ||
          (mBIH.compression == BMPINFOHEADER::RLE4) ||
          (mBIH.bpp == 32 && mUseAlphaData)) {
        PostHasTransparency();
      }

      // We have the size. If we're doing a metadata decode, we're done.
      if (IsMetadataDecode()) {
        return;
      }

      // We're doing a real decode.
      mOldLine = mCurLine = real_height;

      if (mBIH.bpp <= 8) {
        mNumColors = 1 << mBIH.bpp;
        if (mBIH.colors && mBIH.colors < mNumColors) {
            mNumColors = mBIH.colors;
        }

        // Always allocate 256 even though mNumColors might be smaller
        mColors = new colorTable[256];
        memset(mColors, 0, 256 * sizeof(colorTable));
      } else if (mBIH.compression != BMPINFOHEADER::BITFIELDS &&
                 mBIH.bpp == 16) {
        // Use default 5-5-5 format
        mBitFields.red   = 0x7C00;
        mBitFields.green = 0x03E0;
        mBitFields.blue  = 0x001F;
        CalcBitShift();
      }

      // Make sure we have a valid value for our supported compression modes
      // before adding the frame
      if (mBIH.compression != BMPINFOHEADER::RGB &&
          mBIH.compression != BMPINFOHEADER::RLE8 &&
          mBIH.compression != BMPINFOHEADER::RLE4 &&
          mBIH.compression != BMPINFOHEADER::BITFIELDS) {
        PostDataError();
        return;
      }

      // If we have RLE4 or RLE8 or BMPINFOHEADER::ALPHABITFIELDS, then ensure
      // we have valid BPP values before adding the frame.
      if (mBIH.compression == BMPINFOHEADER::RLE8 && mBIH.bpp != 8) {
        MOZ_LOG(GetBMPLog(), LogLevel::Debug,
               ("BMP RLE8 compression only supports 8 bits per pixel\n"));
        PostDataError();
        return;
      }
      if (mBIH.compression == BMPINFOHEADER::RLE4 &&
          mBIH.bpp != 4 && mBIH.bpp != 1) {
        MOZ_LOG(GetBMPLog(), LogLevel::Debug,
               ("BMP RLE4 compression only supports 4 bits per pixel\n"));
        PostDataError();
        return;
      }
      if (mBIH.compression == BMPINFOHEADER::ALPHABITFIELDS &&
          mBIH.bpp != 16 && mBIH.bpp != 32) {
        MOZ_LOG(GetBMPLog(), LogLevel::Debug,
               ("BMP ALPHABITFIELDS only supports 16 or 32 bits per pixel\n"
                ));
        PostDataError();
        return;
      }

      if (mBIH.compression != BMPINFOHEADER::RLE8 &&
          mBIH.compression != BMPINFOHEADER::RLE4 &&
          mBIH.compression != BMPINFOHEADER::ALPHABITFIELDS) {
        // mRow is not used for RLE encoded images
        mRow = (uint8_t*)malloc((mBIH.width * mBIH.bpp) / 8 + 4);
        // + 4 because the line is padded to a 4 bit boundary, but
        // I don't want to make exact calculations here, that's unnecessary.
        // Also, it compensates rounding error.
        if (!mRow) {
          PostDataError();
          return;
        }
      }

      MOZ_ASSERT(!mImageData, "Already have a buffer allocated?");
      nsresult rv = AllocateBasicFrame();
      if (NS_FAILED(rv)) {
          return;
      }

      MOZ_ASSERT(mImageData, "Should have a buffer now");

      // Prepare for transparency
      if ((mBIH.compression == BMPINFOHEADER::RLE8) ||
          (mBIH.compression == BMPINFOHEADER::RLE4)) {
        // Clear the image, as the RLE may jump over areas
        memset(mImageData, 0, mImageDataLength);
      }
  }

  if (mColors && mPos >= mLOH) {
    // OS/2 Bitmaps have no padding byte
    uint8_t bytesPerColor = (mBFH.bihsize == BIH_LENGTH::OS2) ? 3 : 4;
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
            if (bytesPerColor == 3) {
              colorNum++;
            }
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
  } else if (aCount &&
             mBIH.compression == BMPINFOHEADER::BITFIELDS &&
             mPos < (BMP_HEADER_LENGTH::WIN_V3 + bitFields::LENGTH)) {
    // If compression is used, this is a windows bitmap (compression can't be
    // used with OS/2 bitmaps), hence we can use BMP_HEADER_LENGTH::WIN_V3
    // instead of mLOH.  (verified below)

    // If aCount != 0 then mPos should be >= mLOH due to the if statements
    // at the beginning of the function
    MOZ_ASSERT(mPos >= mLOH);
    MOZ_ASSERT(mLOH == BMP_HEADER_LENGTH::WIN_V3);

    // mLOH == BMP_HEADER_LENGTH::WIN_V3 (verified above)
    // mPos >= mLOH (verified above)
    // mPos < BMP_HEADER_LENGTH::WIN_V3 + bitFields::LENGTH
    //
    // So toCopy is in the range
    //      0 to (bitFields::LENGTH - 1)
    uint32_t toCopy = (BMP_HEADER_LENGTH::WIN_V3 + bitFields::LENGTH) - mPos;
    if (toCopy > aCount) {
      toCopy = aCount;
    }

    // mPos >= BMP_HEADER_LENGTH::WIN_V3
    // mPos < BMP_HEADER_LENGTH::WIN_V3 + bitFields::LENGTH
    //
    // offset is in the range
    //      0 to (bitFields::LENGTH - 1)
    //
    // bitFields::LENGTH < BIH_INTERNAL_LENGTH::WIN_V3
    // and sizeof(mRawBuf) == BIH_INTERNAL_LENGTH::WIN_V3 (verified at
    // top of function)
    //
    // Therefore this assert should hold
    const uint32_t offset = mPos - BMP_HEADER_LENGTH::WIN_V3;
    MOZ_ASSERT(offset < sizeof(mRawBuf));

    // Two cases:
    //      mPos = BMP_HEADER_LENGTH::WIN_V3
    //
    // offset = 0
    // toCopy = bitFields::LENGTH
    //
    //      This will be in the bounds of sizeof(mRawBuf)
    //
    // Second case:
    //
    //      mPos = BMP_HEADER_LENGTH::WIN_V3 + bitFields::LENGTH - 1
    //
    // offset = bitFields::LENGTH - 1
    // toCopy = 1
    //
    //      This will be in the bounds of sizeof(mRawBuf)
    //
    // As bitFields::LENGTH < BIH_INTERNAL_LENGTH::WIN_V3 and
    // sizeof(mRawBuf) == BIH_INTERNAL_LENGTH::WIN_V3
    //
    // Therefore this assert should hold
    MOZ_ASSERT(offset + toCopy <= sizeof(mRawBuf));

    memcpy(mRawBuf + offset, aBuffer, toCopy);
    mPos += toCopy;
    aBuffer += toCopy;
    aCount -= toCopy;
  }
  if (mPos == BMP_HEADER_LENGTH::WIN_V3 + bitFields::LENGTH &&
      mBIH.compression == BMPINFOHEADER::BITFIELDS) {
    mBitFields.red = LittleEndian::readUint32(reinterpret_cast<uint32_t*>
                                              (mRawBuf));
    mBitFields.green = LittleEndian::readUint32(reinterpret_cast<uint32_t*>
                                                (mRawBuf + 4));
    mBitFields.blue = LittleEndian::readUint32(reinterpret_cast<uint32_t*>
                                               (mRawBuf + 8));
    CalcBitShift();
  }
  while (aCount && (mPos < mBFH.dataoffset)) { // Skip whatever is between
                                               // header and data
    mPos++; aBuffer++; aCount--;
  }
  if (aCount && ++mPos >= mBFH.dataoffset) {
    // Need to increment mPos, else we might get to mPos==mLOH again
    // From now on, mPos is irrelevant
    if (!mBIH.compression || mBIH.compression == BMPINFOHEADER::BITFIELDS) {
        uint32_t rowSize = (mBIH.bpp * mBIH.width + 7) / 8; // + 7 to
                                                            // round up
        if (rowSize % 4) {
          rowSize += (4 - (rowSize % 4)); // Pad to DWORD Boundary
        }
        uint32_t toCopy;
        do {
          toCopy = rowSize - mRowBytes;
          if (toCopy) {
            if (toCopy > aCount) {
              toCopy = aCount;
            }
            memcpy(mRow + mRowBytes, aBuffer, toCopy);
            aCount -= toCopy;
            aBuffer += toCopy;
            mRowBytes += toCopy;
        }
        if (rowSize == mRowBytes) {
          // Collected a whole row into mRow, process it
          uint8_t* p = mRow;
          uint32_t* d = reinterpret_cast<uint32_t*>(mImageData) +
                        PIXEL_OFFSET(mCurLine, 0);
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
                uint16_t val = LittleEndian::
                               readUint16(reinterpret_cast<uint16_t*>(p));
                SetPixel(d,
                         (val & mBitFields.red) >>
                         mBitFields.redRightShift <<
                         mBitFields.redLeftShift,
                         (val & mBitFields.green) >>
                         mBitFields.greenRightShift <<
                         mBitFields.greenLeftShift,
                         (val & mBitFields.blue) >>
                         mBitFields.blueRightShift <<
                         mBitFields.blueLeftShift);
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
                  if (MOZ_UNLIKELY(!mHaveAlphaData && p[3])) {
                    PostHasTransparency();
                    mHaveAlphaData = true;
                  }
                  SetPixel(d, p[2], p[1], p[0], p[3]);
                } else {
                  SetPixel(d, p[2], p[1], p[0]);
                }
                p += 4;
                --lpos;
              }
              break;
            default:
              NS_NOTREACHED("Unsupported color depth,"
                            " but earlier check didn't catch it");
          }
          mCurLine --;
          if (mCurLine == 0) { // Finished last line
            break;
          }
          mRowBytes = 0;
        }
      } while (aCount > 0);
    } else if ((mBIH.compression == BMPINFOHEADER::RLE8) ||
               (mBIH.compression == BMPINFOHEADER::RLE4)) {
      if (((mBIH.compression == BMPINFOHEADER::RLE8) && (mBIH.bpp != 8)) ||
          ((mBIH.compression == BMPINFOHEADER::RLE4) && (mBIH.bpp != 4) &&
           (mBIH.bpp != 1))) {
        MOZ_LOG(GetBMPLog(), LogLevel::Debug,
               ("BMP RLE8/RLE4 compression only supports 8/4 bits per"
               " pixel\n"));
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
            if (mStateData != RLE::ESCAPE) { // encoded mode
              // Encoded mode consists of two bytes:
              // the first byte (mStateData) specifies the
              // number of consecutive pixels to be drawn
              // using the color index contained in
              // the second byte
              // Work around bitmaps that specify too many pixels
              mState = eRLEStateInitial;
              uint32_t pixelsNeeded = std::min<uint32_t>(mBIH.width - mCurPos,
                                    mStateData);
              if (pixelsNeeded) {
                uint32_t* d = reinterpret_cast<uint32_t*>
                              (mImageData) + PIXEL_OFFSET(mCurLine, mCurPos);
                mCurPos += pixelsNeeded;
                if (mBIH.compression == BMPINFOHEADER::RLE8) {
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
              case RLE::ESCAPE_EOL:
                // End of Line: Go to next row
                mCurLine --;
                mCurPos = 0;
                mState = eRLEStateInitial;
                break;

              case RLE::ESCAPE_EOF: // EndOfFile
                mCurPos = mCurLine = 0;
                break;

              case RLE::ESCAPE_DELTA:
                mState = eRLEStateNeedXDelta;
                continue;

              default : // absolute mode
                // Save the number of pixels to read
                mStateData = byte;
                if (mCurPos + mStateData > (uint32_t)mBIH.width) {
                  // We can work around bitmaps that specify
                  // one pixel too many, but only if their
                  // width is odd.
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
              if (((mStateData - 1) & mBIH.compression) != 0) {
                  mState = eRLEStateAbsoluteMode;
              } else {
                  mState = eRLEStateAbsoluteModePadded;
              }
              continue;
            }
            break;

          case eRLEStateNeedXDelta:
            // Handle the XDelta and proceed to get Y Delta
            byte = *aBuffer++;
            aCount--;
            mCurPos += byte;
            // Delta encoding makes it possible to skip pixels
            // making the image transparent.
            if (MOZ_UNLIKELY(!mHaveAlphaData)) {
                PostHasTransparency();
            }
            mUseAlphaData = mHaveAlphaData = true;
            if (mCurPos > mBIH.width) {
                mCurPos = mBIH.width;
            }

            mState = eRLEStateNeedYDelta;
            continue;

          case eRLEStateNeedYDelta:
            // Get the Y Delta and then "handle" the move
            byte = *aBuffer++;
            aCount--;
            mState = eRLEStateInitial;
            // Delta encoding makes it possible to skip pixels
            // making the image transparent.
            if (MOZ_UNLIKELY(!mHaveAlphaData)) {
                PostHasTransparency();
            }
            mUseAlphaData = mHaveAlphaData = true;
            mCurLine -= std::min<int32_t>(byte, mCurLine);
            break;

          case eRLEStateAbsoluteMode: // Absolute Mode
          case eRLEStateAbsoluteModePadded:
            if (mStateData) {
              // In absolute mode, the second byte (mStateData)
              // represents the number of pixels
              // that follow, each of which contains
              // the color index of a single pixel.
              uint32_t* d = reinterpret_cast<uint32_t*>
                            (mImageData) +
                            PIXEL_OFFSET(mCurLine, mCurPos);
              uint32_t* oldPos = d;
              if (mBIH.compression == BMPINFOHEADER::RLE8) {
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

              if (mState == eRLEStateAbsoluteMode) {
                // word aligned
                mState = eRLEStateInitial;
              } else if (aCount > 0) {
                // not word aligned
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
            MOZ_ASSERT(0, "BMP RLE decompression: unknown state!");
            PostDecoderError(NS_ERROR_UNEXPECTED);
            return;
        }
        // Because of the use of the continue statement
        // we only get here for eol, eof or y delta
        if (mCurLine == 0) {
          // Finished last line
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

void
nsBMPDecoder::ProcessFileHeader()
{
  memset(&mBFH, 0, sizeof(mBFH));
  memcpy(&mBFH.signature, mRawBuf, sizeof(mBFH.signature));
  memcpy(&mBFH.filesize, mRawBuf + 2, sizeof(mBFH.filesize));
  memcpy(&mBFH.reserved, mRawBuf + 6, sizeof(mBFH.reserved));
  memcpy(&mBFH.dataoffset, mRawBuf + 10, sizeof(mBFH.dataoffset));
  memcpy(&mBFH.bihsize, mRawBuf + 14, sizeof(mBFH.bihsize));

  // Now correct the endianness of the header
  mBFH.filesize = LittleEndian::readUint32(&mBFH.filesize);
  mBFH.dataoffset = LittleEndian::readUint32(&mBFH.dataoffset);
  mBFH.bihsize = LittleEndian::readUint32(&mBFH.bihsize);
}

void
nsBMPDecoder::ProcessInfoHeader()
{
  memset(&mBIH, 0, sizeof(mBIH));
  if (mBFH.bihsize == 12) { // OS/2 Bitmap
    memcpy(&mBIH.width, mRawBuf, 2);
    memcpy(&mBIH.height, mRawBuf + 2, 2);
    memcpy(&mBIH.planes, mRawBuf + 4, sizeof(mBIH.planes));
    memcpy(&mBIH.bpp, mRawBuf + 6, sizeof(mBIH.bpp));
  } else {
    memcpy(&mBIH.width, mRawBuf, sizeof(mBIH.width));
    memcpy(&mBIH.height, mRawBuf + 4, sizeof(mBIH.height));
    memcpy(&mBIH.planes, mRawBuf + 8, sizeof(mBIH.planes));
    memcpy(&mBIH.bpp, mRawBuf + 10, sizeof(mBIH.bpp));
    memcpy(&mBIH.compression, mRawBuf + 12, sizeof(mBIH.compression));
    memcpy(&mBIH.image_size, mRawBuf + 16, sizeof(mBIH.image_size));
    memcpy(&mBIH.xppm, mRawBuf + 20, sizeof(mBIH.xppm));
    memcpy(&mBIH.yppm, mRawBuf + 24, sizeof(mBIH.yppm));
    memcpy(&mBIH.colors, mRawBuf + 28, sizeof(mBIH.colors));
    memcpy(&mBIH.important_colors, mRawBuf + 32,
           sizeof(mBIH.important_colors));
  }

  // Convert endianness
  mBIH.width = LittleEndian::readUint32(&mBIH.width);
  mBIH.height = LittleEndian::readUint32(&mBIH.height);
  mBIH.planes = LittleEndian::readUint16(&mBIH.planes);
  mBIH.bpp = LittleEndian::readUint16(&mBIH.bpp);

  mBIH.compression = LittleEndian::readUint32(&mBIH.compression);
  mBIH.image_size = LittleEndian::readUint32(&mBIH.image_size);
  mBIH.xppm = LittleEndian::readUint32(&mBIH.xppm);
  mBIH.yppm = LittleEndian::readUint32(&mBIH.yppm);
  mBIH.colors = LittleEndian::readUint32(&mBIH.colors);
  mBIH.important_colors = LittleEndian::readUint32(&mBIH.important_colors);
}

} // namespace image
} // namespace mozilla
