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
  , mLOH(WIN_V3_HEADER_LENGTH)
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
    MOZ_ASSERT(!HasError(), "Can't call FinishInternal on error!");

    // We should never make multiple frames
    MOZ_ASSERT(GetFrameCount() <= 1, "Multiple BMP frames?");

    // Send notifications if appropriate
    if (!IsSizeDecode() && HasSize()) {

        // Invalidate
        nsIntRect r(0, 0, mBIH.width, GetHeight());
        PostInvalidation(r);

        if (mUseAlphaData) {
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

  // This code assumes that mRawBuf == WIN_V3_INTERNAL_BIH_LENGTH
  // and that sizeof(mRawBuf) >= BFH_INTERNAL_LENGTH
  MOZ_ASSERT(sizeof(mRawBuf) == WIN_V3_INTERNAL_BIH_LENGTH);
  MOZ_ASSERT(sizeof(mRawBuf) >= BFH_INTERNAL_LENGTH);
  MOZ_ASSERT(OS2_INTERNAL_BIH_LENGTH < WIN_V3_INTERNAL_BIH_LENGTH);

  // This code also assumes it's working with a byte array
  MOZ_ASSERT(sizeof(mRawBuf[0]) == 1);

  if (mPos < BFH_INTERNAL_LENGTH) { /* In BITMAPFILEHEADER */
      // BFH_INTERNAL_LENGTH < sizeof(mRawBuf)
      // mPos < BFH_INTERNAL_LENGTH
      // BFH_INTERNAL_LENGTH - mPos < sizeof(mRawBuf)
      // so toCopy <= BFH_INTERNAL_LENGTH
      // so toCopy < sizeof(mRawBuf)
      // so toCopy > 0 && toCopy <= BFH_INTERNAL_LENGTH
      uint32_t toCopy = BFH_INTERNAL_LENGTH - mPos;
      if (toCopy > aCount) {
          toCopy = aCount;
      }

      // mRawBuf is a byte array of size WIN_V3_INTERNAL_BIH_LENGTH
      // (verified above)
      // mPos is < BFH_INTERNAL_LENGTH
      // BFH_INTERNAL_LENGTH < WIN_V3_INTERNAL_BIH_LENGTH
      // so mPos < sizeof(mRawBuf)
      //
      // Therefore this assert should hold
      MOZ_ASSERT(mPos < sizeof(mRawBuf));

      // toCopy <= BFH_INTERNAL_LENGTH
      // mPos >= 0 && mPos < BFH_INTERNAL_LENGTH
      // sizeof(mRawBuf) >= BFH_INTERNAL_LENGTH (verified above)
      //
      // Therefore this assert should hold
      MOZ_ASSERT(mPos + toCopy <= sizeof(mRawBuf));

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
      if (mBFH.bihsize == OS2_BIH_LENGTH) {
          mLOH = OS2_HEADER_LENGTH;
      }
  }
  if (mPos >= BFH_INTERNAL_LENGTH && mPos < mLOH) { /* In BITMAPINFOHEADER */
      // mLOH == WIN_V3_HEADER_LENGTH || mLOH == OS2_HEADER_LENGTH
      // OS2_HEADER_LENGTH < WIN_V3_HEADER_LENGTH
      // BFH_INTERNAL_LENGTH < OS2_HEADER_LENGTH
      // BFH_INTERNAL_LENGTH < WIN_V3_HEADER_LENGTH
      //
      // So toCopy is in the range
      //      1 to (WIN_V3_HEADER_LENGTH - BFH_INTERNAL_LENGTH)
      // or   1 to (OS2_HEADER_LENGTH - BFH_INTERNAL_LENGTH)
      //
      // But WIN_V3_HEADER_LENGTH =
      //     BFH_INTERNAL_LENGTH + WIN_V3_INTERNAL_BIH_LENGTH
      // and OS2_HEADER_LENGTH = BFH_INTERNAL_LENGTH + OS2_INTERNAL_BIH_LENGTH
      //
      // So toCopy is in the range
      //
      //      1 to WIN_V3_INTERNAL_BIH_LENGTH
      // or   1 to OS2_INTERNAL_BIH_LENGTH
      // and  OS2_INTERNAL_BIH_LENGTH < WIN_V3_INTERNAL_BIH_LENGTH
      //
      // sizeof(mRawBuf) = WIN_V3_INTERNAL_BIH_LENGTH
      // so toCopy <= sizeof(mRawBuf)
      uint32_t toCopy = mLOH - mPos;
      if (toCopy > aCount) {
          toCopy = aCount;
      }

      // mPos is in the range
      //      BFH_INTERNAL_LENGTH to (WIN_V3_HEADER_LENGTH - 1)
      //
      // offset is then in the range (see toCopy comments for more details)
      //      0 to (WIN_V3_INTERNAL_BIH_LENGTH - 1)
      //
      // sizeof(mRawBuf) is WIN_V3_INTERNAL_BIH_LENGTH so this
      // offset stays within bounds and this assert should hold
      const uint32_t offset = mPos - BFH_INTERNAL_LENGTH;
      MOZ_ASSERT(offset < sizeof(mRawBuf));

      // Two cases:
      //      mPos = BFH_INTERNAL_LENGTH
      //      mLOH = WIN_V3_HEADER_LENGTH
      //
      // offset = 0
      // toCopy = WIN_V3_INTERNAL_BIH_LENGTH
      //
      //      This will be in the bounds of sizeof(mRawBuf)
      //
      // Second Case:
      //      mPos = WIN_V3_HEADER_LENGTH - 1
      //      mLOH = WIN_V3_HEADER_LENGTH
      //
      // offset = WIN_V3_INTERNAL_BIH_LENGTH - 1
      // toCopy = 1
      //
      //      This will be in the bounds of sizeof(mRawBuf)
      //
      // As sizeof(mRawBuf) == WIN_V3_INTERNAL_BIH_LENGTH (verified above)
      // and WIN_V3_HEADER_LENGTH is the largest range of values. If mLOH
      // was equal to OS2_HEADER_LENGTH then the ranges are smaller.
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
      PR_LOG(GetBMPLog(), PR_LOG_DEBUG,
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

      // We have the size. If we're doing a size decode, we got what
      // we came for.
      if (IsSizeDecode()) {
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
      } else if (mBIH.compression != BI_BITFIELDS && mBIH.bpp == 16) {
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
               ("BMP ALPHABITFIELDS only supports 16 or 32 bits per pixel\n"
                ));
        PostDataError();
        return;
      }

      if (mBIH.compression != BI_RLE8 && mBIH.compression != BI_RLE4 &&
          mBIH.compression != BI_ALPHABITFIELDS) {
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
  } else if (aCount && mBIH.compression == BI_BITFIELDS && mPos <
         (WIN_V3_HEADER_LENGTH + BITFIELD_LENGTH)) {
    // If compression is used, this is a windows bitmap (compression
    // can't be used with OS/2 bitmaps),
    // hence we can use WIN_V3_HEADER_LENGTH instead of mLOH.
    // (verified below)

    // If aCount != 0 then mPos should be >= mLOH due to the if statements
    // at the beginning of the function
    MOZ_ASSERT(mPos >= mLOH);
    MOZ_ASSERT(mLOH == WIN_V3_HEADER_LENGTH);

    // mLOH == WIN_V3_HEADER_LENGTH (verified above)
    // mPos >= mLOH (verified above)
    // mPos < WIN_V3_HEADER_LENGTH + BITFIELD_LENGTH
    //
    // So toCopy is in the range
    //      0 to (BITFIELD_LENGTH - 1)
    uint32_t toCopy = (WIN_V3_HEADER_LENGTH + BITFIELD_LENGTH) - mPos;
    if (toCopy > aCount) {
      toCopy = aCount;
    }

    // mPos >= WIN_V3_HEADER_LENGTH
    // mPos < WIN_V3_HEADER_LENGTH + BITFIELD_LENGTH
    //
    // offset is in the range
    //      0 to (BITFIELD_LENGTH - 1)
    //
    // BITFIELD_LENGTH < WIN_V3_INTERNAL_BIH_LENGTH
    // and sizeof(mRawBuf) == WIN_V3_INTERNAL_BIH_LENGTH (verified at
    // top of function)
    //
    // Therefore this assert should hold
    const uint32_t offset = mPos - WIN_V3_HEADER_LENGTH;
    MOZ_ASSERT(offset < sizeof(mRawBuf));

    // Two cases:
    //      mPos = WIN_V3_HEADER_LENGTH
    //
    // offset = 0
    // toCopy = BITFIELD_LENGTH
    //
    //      This will be in the bounds of sizeof(mRawBuf)
    //
    // Second case:
    //
    //      mPos = WIN_V3_HEADER_LENGTH + BITFIELD_LENGTH - 1
    //
    // offset = BITFIELD_LENGTH - 1
    // toCopy = 1
    //
    //      This will be in the bounds of sizeof(mRawBuf)
    //
    // As BITFIELD_LENGTH < WIN_V3_INTERNAL_BIH_LENGTH and
    // sizeof(mRawBuf) == WIN_V3_INTERNAL_BIH_LENGTH
    //
    // Therefore this assert should hold
    MOZ_ASSERT(offset + toCopy <= sizeof(mRawBuf));

    memcpy(mRawBuf + offset, aBuffer, toCopy);
    mPos += toCopy;
    aBuffer += toCopy;
    aCount -= toCopy;
  }
  if (mPos == WIN_V3_HEADER_LENGTH + BITFIELD_LENGTH &&
    mBIH.compression == BI_BITFIELDS) {
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
    if (!mBIH.compression || mBIH.compression == BI_BITFIELDS) {
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
                  if (!mHaveAlphaData && p[3]) {
                    // Non-zero alpha byte detected! Clear previous
                    // pixels that we have already processed.
                    // This works because we know that if we
                    // are reaching here then the alpha data in byte
                    // 4 has been right all along.  And we know it
                    // has been set to 0 the whole time, so that
                    // means that everything is transparent so far.
                    uint32_t* start = reinterpret_cast<uint32_t*>
                                      (mImageData) + GetWidth() *
                                      (mCurLine - 1);
                    uint32_t heightDifference = GetHeight() -
                                                mCurLine + 1;
                    uint32_t pixelCount = GetWidth() *
                                          heightDifference;

                    memset(start, 0, pixelCount * sizeof(uint32_t));

                    PostHasTransparency();
                    mHaveAlphaData = true;
                  }
                  SetPixel(d, p[2], p[1], p[0], mHaveAlphaData ?  p[3] : 0xFF);
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
    } else if ((mBIH.compression == BI_RLE8) ||
             (mBIH.compression == BI_RLE4)) {
      if (((mBIH.compression == BI_RLE8) && (mBIH.bpp != 8)) ||
          ((mBIH.compression == BI_RLE4) && (mBIH.bpp != 4) &&
           (mBIH.bpp != 1))) {
        PR_LOG(GetBMPLog(), PR_LOG_DEBUG,
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
            if (mStateData != RLE_ESCAPE) { // encoded mode
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
