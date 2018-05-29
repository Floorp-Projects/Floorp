/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a cross-platform BMP Decoder, which should work everywhere,
// including big-endian machines like the PowerPC.
//
// BMP is a format that has been extended multiple times. To understand the
// decoder you need to understand this history. The summary of the history
// below was determined from the following documents.
//
// - http://www.fileformat.info/format/bmp/egff.htm
// - http://www.fileformat.info/format/os2bmp/egff.htm
// - http://fileformats.archiveteam.org/wiki/BMP
// - http://fileformats.archiveteam.org/wiki/OS/2_BMP
// - https://en.wikipedia.org/wiki/BMP_file_format
// - https://upload.wikimedia.org/wikipedia/commons/c/c4/BMPfileFormat.png
//
// WINDOWS VERSIONS OF THE BMP FORMAT
// ----------------------------------
// WinBMPv1.
// - This version is no longer used and can be ignored.
//
// WinBMPv2.
// - First is a 14 byte file header that includes: the magic number ("BM"),
//   file size, and offset to the pixel data (|mDataOffset|).
// - Next is a 12 byte info header which includes: the info header size
//   (mBIHSize), width, height, number of color planes, and bits-per-pixel
//   (|mBpp|) which must be 1, 4, 8 or 24.
// - Next is the semi-optional color table, which has length 2^|mBpp| and has 3
//   bytes per value (BGR). The color table is required if |mBpp| is 1, 4, or 8.
// - Next is an optional gap.
// - Next is the pixel data, which is pointed to by |mDataOffset|.
//
// WinBMPv3. This is the most widely used version.
// - It changed the info header to 40 bytes by taking the WinBMPv2 info
//   header, enlargening its width and height fields, and adding more fields
//   including: a compression type (|mCompression|) and number of colors
//   (|mNumColors|).
// - The semi-optional color table is now 4 bytes per value (BGR0), and its
//   length is |mNumColors|, or 2^|mBpp| if |mNumColors| is zero.
// - |mCompression| can be RGB (i.e. no compression), RLE4 (if |mBpp|==4) or
//   RLE8 (if |mBpp|==8) values.
//
// WinBMPv3-NT. A variant of WinBMPv3.
// - It did not change the info header layout from WinBMPv3.
// - |mBpp| can now be 16 or 32, in which case |mCompression| can be RGB or the
//   new BITFIELDS value; in the latter case an additional 12 bytes of color
//   bitfields follow the info header.
//
// WinBMPv4.
// - It extended the info header to 108 bytes, including the 12 bytes of color
//   mask data from WinBMPv3-NT, plus alpha mask data, and also color-space and
//   gamma correction fields.
//
// WinBMPv5.
// - It extended the info header to 124 bytes, adding color profile data.
// - It also added an optional color profile table after the pixel data (and
//   another optional gap).
//
// WinBMPv3-ICO. This is a variant of WinBMPv3.
// - It's the BMP format used for BMP images within ICO files.
// - The only difference with WinBMPv3 is that if an image is 32bpp and has no
//   compression, then instead of treating the pixel data as 0RGB it is treated
//   as ARGB, but only if one or more of the A values are non-zero.
//
// OS/2 VERSIONS OF THE BMP FORMAT
// -------------------------------
// OS2-BMPv1.
// - Almost identical to WinBMPv2; the differences are basically ignorable.
//
// OS2-BMPv2.
// - Similar to WinBMPv3.
// - The info header is 64 bytes but can be reduced to as little as 16; any
//   omitted fields are treated as zero. The first 40 bytes of these fields are
//   nearly identical to the WinBMPv3 info header; the remaining 24 bytes are
//   different.
// - Also adds compression types "Huffman 1D" and "RLE24", which we don't
//   support.
// - We treat OS2-BMPv2 files as if they are WinBMPv3 (i.e. ignore the extra 24
//   bytes in the info header), which in practice is good enough.

#include "ImageLogging.h"
#include "nsBMPDecoder.h"

#include <stdlib.h>

#include "mozilla/Attributes.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Likely.h"

#include "nsIInputStream.h"
#include "RasterImage.h"
#include <algorithm>

using namespace mozilla::gfx;

namespace mozilla {
namespace image {
namespace bmp {

struct Compression {
  enum {
    RGB = 0,
    RLE8 = 1,
    RLE4 = 2,
    BITFIELDS = 3
  };
};

// RLE escape codes and constants.
struct RLE {
  enum {
    ESCAPE = 0,
    ESCAPE_EOL = 0,
    ESCAPE_EOF = 1,
    ESCAPE_DELTA = 2,

    SEGMENT_LENGTH = 2,
    DELTA_LENGTH = 2
  };
};

} // namespace bmp

using namespace bmp;

/// Sets the pixel data in aDecoded to the given values.
/// @param aDecoded pointer to pixel to be set, will be incremented to point to
/// the next pixel.
static void
SetPixel(uint32_t*& aDecoded, uint8_t aRed, uint8_t aGreen,
         uint8_t aBlue, uint8_t aAlpha = 0xFF)
{
  *aDecoded++ = gfxPackedPixel(aAlpha, aRed, aGreen, aBlue);
}

static void
SetPixel(uint32_t*& aDecoded, uint8_t idx,
         const UniquePtr<ColorTableEntry[]>& aColors)
{
  SetPixel(aDecoded,
           aColors[idx].mRed, aColors[idx].mGreen, aColors[idx].mBlue);
}

/// Sets two (or one if aCount = 1) pixels
/// @param aDecoded where the data is stored. Will be moved 4 resp 8 bytes
/// depending on whether one or two pixels are written.
/// @param aData The values for the two pixels
/// @param aCount Current count. Is decremented by one or two.
static void
Set4BitPixel(uint32_t*& aDecoded, uint8_t aData, uint32_t& aCount,
             const UniquePtr<ColorTableEntry[]>& aColors)
{
  uint8_t idx = aData >> 4;
  SetPixel(aDecoded, idx, aColors);
  if (--aCount > 0) {
    idx = aData & 0xF;
    SetPixel(aDecoded, idx, aColors);
    --aCount;
  }
}

static mozilla::LazyLogModule sBMPLog("BMPDecoder");

// The length of the mBIHSize field in the info header.
static const uint32_t BIHSIZE_FIELD_LENGTH = 4;

nsBMPDecoder::nsBMPDecoder(RasterImage* aImage, State aState, size_t aLength)
  : Decoder(aImage)
  , mLexer(Transition::To(aState, aLength), Transition::TerminateSuccess())
  , mIsWithinICO(false)
  , mMayHaveTransparency(false)
  , mDoesHaveTransparency(false)
  , mNumColors(0)
  , mColors(nullptr)
  , mBytesPerColor(0)
  , mPreGapLength(0)
  , mPixelRowSize(0)
  , mCurrentRow(0)
  , mCurrentPos(0)
  , mAbsoluteModeNumPixels(0)
{
}

// Constructor for normal BMP files.
nsBMPDecoder::nsBMPDecoder(RasterImage* aImage)
  : nsBMPDecoder(aImage, State::FILE_HEADER, FILE_HEADER_LENGTH)
{
}

// Constructor used for WinBMPv3-ICO files, which lack a file header.
nsBMPDecoder::nsBMPDecoder(RasterImage* aImage, uint32_t aDataOffset)
  : nsBMPDecoder(aImage, State::INFO_HEADER_SIZE, BIHSIZE_FIELD_LENGTH)
{
  SetIsWithinICO();

  // Even though the file header isn't present in this case, the dataOffset
  // field is set as if it is, and so we must increment mPreGapLength
  // accordingly.
  mPreGapLength += FILE_HEADER_LENGTH;

  // This is the one piece of data we normally get from a BMP file header, so
  // it must be provided via an argument.
  mH.mDataOffset = aDataOffset;
}

nsBMPDecoder::~nsBMPDecoder()
{
}

// Obtains the size of the compressed image resource.
int32_t
nsBMPDecoder::GetCompressedImageSize() const
{
  // In the RGB case mImageSize might not be set, so compute it manually.
  MOZ_ASSERT(mPixelRowSize != 0);
  return mH.mCompression == Compression::RGB
       ? mPixelRowSize * AbsoluteHeight()
       : mH.mImageSize;
}

nsresult
nsBMPDecoder::BeforeFinishInternal()
{
  if (!IsMetadataDecode() && !mImageData) {
    return NS_ERROR_FAILURE;  // No image; something went wrong.
  }

  return NS_OK;
}

nsresult
nsBMPDecoder::FinishInternal()
{
  // We shouldn't be called in error cases.
  MOZ_ASSERT(!HasError(), "Can't call FinishInternal on error!");

  // We should never make multiple frames.
  MOZ_ASSERT(GetFrameCount() <= 1, "Multiple BMP frames?");

  // Send notifications if appropriate.
  if (!IsMetadataDecode() && HasSize()) {

    // We should have image data.
    MOZ_ASSERT(mImageData);

    // If it was truncated, fill in the missing pixels as black.
    while (mCurrentRow > 0) {
      uint32_t* dst = RowBuffer();
      while (mCurrentPos < mH.mWidth) {
        SetPixel(dst, 0, 0, 0);
        mCurrentPos++;
      }
      mCurrentPos = 0;
      FinishRow();
    }

    // Invalidate.
    nsIntRect r(0, 0, mH.mWidth, AbsoluteHeight());
    PostInvalidation(r);

    MOZ_ASSERT_IF(mDoesHaveTransparency, mMayHaveTransparency);

    // We have transparency if we either detected some in the image itself
    // (i.e., |mDoesHaveTransparency| is true) or we're in an ICO, which could
    // mean we have an AND mask that provides transparency (i.e., |mIsWithinICO|
    // is true).
    // XXX(seth): We can tell when we create the decoder if the AND mask is
    // present, so we could be more precise about this.
    const Opacity opacity = mDoesHaveTransparency || mIsWithinICO
                          ? Opacity::SOME_TRANSPARENCY
                          : Opacity::FULLY_OPAQUE;

    PostFrameStop(opacity);
    PostDecodeDone();
  }

  return NS_OK;
}

// ----------------------------------------
// Actual Data Processing
// ----------------------------------------

void
BitFields::Value::Set(uint32_t aMask)
{
  mMask = aMask;

  // Handle this exceptional case first. The chosen values don't matter
  // (because a mask of zero will always give a value of zero) except that
  // mBitWidth:
  // - shouldn't be zero, because that would cause an infinite loop in Get();
  // - shouldn't be 5 or 8, because that could cause a false positive match in
  //   IsR5G5B5() or IsR8G8B8().
  if (mMask == 0x0) {
    mRightShift = 0;
    mBitWidth = 1;
    return;
  }

  // Find the rightmost 1.
  uint8_t i;
  for (i = 0; i < 32; i++) {
    if (mMask & (1 << i)) {
      break;
    }
  }
  mRightShift = i;

  // Now find the leftmost 1 in the same run of 1s. (If there are multiple runs
  // of 1s -- which isn't valid -- we'll behave as if only the lowest run was
  // present, which seems reasonable.)
  for (i = i + 1; i < 32; i++) {
    if (!(mMask & (1 << i))) {
      break;
    }
  }
  mBitWidth = i - mRightShift;
}

MOZ_ALWAYS_INLINE uint8_t
BitFields::Value::Get(uint32_t aValue) const
{
  // Extract the unscaled value.
  uint32_t v = (aValue & mMask) >> mRightShift;

  // Idea: to upscale v precisely we need to duplicate its bits, possibly
  // repeatedly, possibly partially in the last case, from bit 7 down to bit 0
  // in v2. For example:
  //
  // - mBitWidth=1:  v2 = v<<7 | v<<6 | ... | v<<1 | v>>0     k -> kkkkkkkk
  // - mBitWidth=2:  v2 = v<<6 | v<<4 | v<<2 | v>>0          jk -> jkjkjkjk
  // - mBitWidth=3:  v2 = v<<5 | v<<2 | v>>1                ijk -> ijkijkij
  // - mBitWidth=4:  v2 = v<<4 | v>>0                      hijk -> hijkhijk
  // - mBitWidth=5:  v2 = v<<3 | v>>2                     ghijk -> ghijkghi
  // - mBitWidth=6:  v2 = v<<2 | v>>4                    fghijk -> fghijkfg
  // - mBitWidth=7:  v2 = v<<1 | v>>6                   efghijk -> efghijke
  // - mBitWidth=8:  v2 = v>>0                         defghijk -> defghijk
  // - mBitWidth=9:  v2 = v>>1                        cdefghijk -> cdefghij
  // - mBitWidth=10: v2 = v>>2                       bcdefghijk -> bcdefghi
  // - mBitWidth=11: v2 = v>>3                      abcdefghijk -> abcdefgh
  // - etc.
  //
  uint8_t v2 = 0;
  int32_t i;      // must be a signed integer
  for (i = 8 - mBitWidth; i > 0; i -= mBitWidth) {
    v2 |= v << uint32_t(i);
  }
  v2 |= v >> uint32_t(-i);
  return v2;
}

MOZ_ALWAYS_INLINE uint8_t
BitFields::Value::GetAlpha(uint32_t aValue, bool& aHasAlphaOut) const
{
  if (mMask == 0x0) {
    return 0xff;
  }
  aHasAlphaOut = true;
  return Get(aValue);
}

MOZ_ALWAYS_INLINE uint8_t
BitFields::Value::Get5(uint32_t aValue) const
{
  MOZ_ASSERT(mBitWidth == 5);
  uint32_t v = (aValue & mMask) >> mRightShift;
  return (v << 3u) | (v >> 2u);
}

MOZ_ALWAYS_INLINE uint8_t
BitFields::Value::Get8(uint32_t aValue) const
{
  MOZ_ASSERT(mBitWidth == 8);
  uint32_t v = (aValue & mMask) >> mRightShift;
  return v;
}

void
BitFields::SetR5G5B5()
{
  mRed.Set(0x7c00);
  mGreen.Set(0x03e0);
  mBlue.Set(0x001f);
}

void
BitFields::SetR8G8B8()
{
  mRed.Set(0xff0000);
  mGreen.Set(0xff00);
  mBlue.Set(0x00ff);
}

bool
BitFields::IsR5G5B5() const
{
  return mRed.mBitWidth == 5 &&
         mGreen.mBitWidth == 5 &&
         mBlue.mBitWidth == 5 &&
         mAlpha.mMask == 0x0;
}

bool
BitFields::IsR8G8B8() const
{
  return mRed.mBitWidth == 8 &&
         mGreen.mBitWidth == 8 &&
         mBlue.mBitWidth == 8 &&
         mAlpha.mMask == 0x0;
}

uint32_t*
nsBMPDecoder::RowBuffer()
{
  if (mDownscaler) {
    return reinterpret_cast<uint32_t*>(mDownscaler->RowBuffer()) + mCurrentPos;
  }

  // Convert from row (1..mHeight) to absolute line (0..mHeight-1).
  int32_t line = (mH.mHeight < 0)
               ? -mH.mHeight - mCurrentRow
               : mCurrentRow - 1;
  int32_t offset = line * mH.mWidth + mCurrentPos;
  return reinterpret_cast<uint32_t*>(mImageData) + offset;
}

void
nsBMPDecoder::FinishRow()
{
  if (mDownscaler) {
    mDownscaler->CommitRow();

    if (mDownscaler->HasInvalidation()) {
      DownscalerInvalidRect invalidRect = mDownscaler->TakeInvalidRect();
      PostInvalidation(invalidRect.mOriginalSizeRect,
                       Some(invalidRect.mTargetSizeRect));
    }
  } else {
    PostInvalidation(IntRect(0, mCurrentRow, mH.mWidth, 1));
  }
  mCurrentRow--;
}

LexerResult
nsBMPDecoder::DoDecode(SourceBufferIterator& aIterator, IResumable* aOnResume)
{
  MOZ_ASSERT(!HasError(), "Shouldn't call DoDecode after error!");

  return mLexer.Lex(aIterator, aOnResume,
                    [=](State aState, const char* aData, size_t aLength) {
    switch (aState) {
      case State::FILE_HEADER:      return ReadFileHeader(aData, aLength);
      case State::INFO_HEADER_SIZE: return ReadInfoHeaderSize(aData, aLength);
      case State::INFO_HEADER_REST: return ReadInfoHeaderRest(aData, aLength);
      case State::BITFIELDS:        return ReadBitfields(aData, aLength);
      case State::COLOR_TABLE:      return ReadColorTable(aData, aLength);
      case State::GAP:              return SkipGap();
      case State::AFTER_GAP:        return AfterGap();
      case State::PIXEL_ROW:        return ReadPixelRow(aData);
      case State::RLE_SEGMENT:      return ReadRLESegment(aData);
      case State::RLE_DELTA:        return ReadRLEDelta(aData);
      case State::RLE_ABSOLUTE:     return ReadRLEAbsolute(aData, aLength);
      default:
        MOZ_CRASH("Unknown State");
    }
  });
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadFileHeader(const char* aData, size_t aLength)
{
  mPreGapLength += aLength;

  bool signatureOk = aData[0] == 'B' && aData[1] == 'M';
  if (!signatureOk) {
    return Transition::TerminateFailure();
  }

  // We ignore the filesize (aData + 2) and reserved (aData + 6) fields.

  mH.mDataOffset = LittleEndian::readUint32(aData + 10);

  return Transition::To(State::INFO_HEADER_SIZE, BIHSIZE_FIELD_LENGTH);
}

// We read the info header in two steps: (a) read the mBIHSize field to
// determine how long the header is; (b) read the rest of the header.
LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadInfoHeaderSize(const char* aData, size_t aLength)
{
  mPreGapLength += aLength;

  mH.mBIHSize = LittleEndian::readUint32(aData);

  bool bihSizeOk = mH.mBIHSize == InfoHeaderLength::WIN_V2 ||
                   mH.mBIHSize == InfoHeaderLength::WIN_V3 ||
                   mH.mBIHSize == InfoHeaderLength::WIN_V4 ||
                   mH.mBIHSize == InfoHeaderLength::WIN_V5 ||
                   (mH.mBIHSize >= InfoHeaderLength::OS2_V2_MIN &&
                    mH.mBIHSize <= InfoHeaderLength::OS2_V2_MAX);
  if (!bihSizeOk) {
    return Transition::TerminateFailure();
  }
  // ICO BMPs must have a WinBMPv3 header. nsICODecoder should have already
  // terminated decoding if this isn't the case.
  MOZ_ASSERT_IF(mIsWithinICO, mH.mBIHSize == InfoHeaderLength::WIN_V3);

  return Transition::To(State::INFO_HEADER_REST,
                        mH.mBIHSize - BIHSIZE_FIELD_LENGTH);
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadInfoHeaderRest(const char* aData, size_t aLength)
{
  mPreGapLength += aLength;

  // |mWidth| and |mHeight| may be signed (Windows) or unsigned (OS/2). We just
  // read as unsigned because in practice that's good enough.
  if (mH.mBIHSize == InfoHeaderLength::WIN_V2) {
    mH.mWidth  = LittleEndian::readUint16(aData + 0);
    mH.mHeight = LittleEndian::readUint16(aData + 2);
    // We ignore the planes (aData + 4) field; it should always be 1.
    mH.mBpp    = LittleEndian::readUint16(aData + 6);
  } else {
    mH.mWidth  = LittleEndian::readUint32(aData + 0);
    mH.mHeight = LittleEndian::readUint32(aData + 4);
    // We ignore the planes (aData + 4) field; it should always be 1.
    mH.mBpp    = LittleEndian::readUint16(aData + 10);

    // For OS2-BMPv2 the info header may be as little as 16 bytes, so be
    // careful for these fields.
    mH.mCompression = aLength >= 16 ? LittleEndian::readUint32(aData + 12) : 0;
    mH.mImageSize   = aLength >= 20 ? LittleEndian::readUint32(aData + 16) : 0;
    // We ignore the xppm (aData + 20) and yppm (aData + 24) fields.
    mH.mNumColors   = aLength >= 32 ? LittleEndian::readUint32(aData + 28) : 0;
    // We ignore the important_colors (aData + 36) field.

    // For WinBMPv4, WinBMPv5 and (possibly) OS2-BMPv2 there are additional
    // fields in the info header which we ignore, with the possible exception
    // of the color bitfields (see below).
  }

  // The height for BMPs embedded inside an ICO includes spaces for the AND
  // mask even if it is not present, thus we need to adjust for that here.
  if (mIsWithinICO) {
    // XXX(seth): Should we really be writing the absolute value from
    // the BIH below? Seems like this could be problematic for inverted BMPs.
    mH.mHeight = abs(mH.mHeight) / 2;
  }

  // Run with MOZ_LOG=BMPDecoder:5 set to see this output.
  MOZ_LOG(sBMPLog, LogLevel::Debug,
          ("BMP: bihsize=%u, %d x %d, bpp=%u, compression=%u, colors=%u\n",
          mH.mBIHSize, mH.mWidth, mH.mHeight, uint32_t(mH.mBpp),
          mH.mCompression, mH.mNumColors));

  // BMPs with negative width are invalid. Also, reject extremely wide images
  // to keep the math sane. And reject INT_MIN as a height because you can't
  // get its absolute value (because -INT_MIN is one more than INT_MAX).
  const int32_t k64KWidth = 0x0000FFFF;
  bool sizeOk = 0 <= mH.mWidth && mH.mWidth <= k64KWidth &&
                mH.mHeight != INT_MIN;
  if (!sizeOk) {
    return Transition::TerminateFailure();
  }

  // Check mBpp and mCompression.
  bool bppCompressionOk =
    (mH.mCompression == Compression::RGB &&
      (mH.mBpp ==  1 || mH.mBpp ==  4 || mH.mBpp ==  8 ||
       mH.mBpp == 16 || mH.mBpp == 24 || mH.mBpp == 32)) ||
    (mH.mCompression == Compression::RLE8 && mH.mBpp == 8) ||
    (mH.mCompression == Compression::RLE4 && mH.mBpp == 4) ||
    (mH.mCompression == Compression::BITFIELDS &&
      // For BITFIELDS compression we require an exact match for one of the
      // WinBMP BIH sizes; this clearly isn't an OS2 BMP.
      (mH.mBIHSize == InfoHeaderLength::WIN_V3 ||
       mH.mBIHSize == InfoHeaderLength::WIN_V4 ||
       mH.mBIHSize == InfoHeaderLength::WIN_V5) &&
      (mH.mBpp == 16 || mH.mBpp == 32));
  if (!bppCompressionOk) {
    return Transition::TerminateFailure();
  }

  // Initialize our current row to the top of the image.
  mCurrentRow = AbsoluteHeight();

  // Round it up to the nearest byte count, then pad to 4-byte boundary.
  // Compute this even for a metadate decode because GetCompressedImageSize()
  // relies on it.
  mPixelRowSize = (mH.mBpp * mH.mWidth + 7) / 8;
  uint32_t surplus = mPixelRowSize % 4;
  if (surplus != 0) {
    mPixelRowSize += 4 - surplus;
  }

  size_t bitFieldsLengthStillToRead = 0;
  if (mH.mCompression == Compression::BITFIELDS) {
    // Need to read bitfields.
    if (mH.mBIHSize >= InfoHeaderLength::WIN_V4) {
      // Bitfields are present in the info header, so we can read them
      // immediately.
      mBitFields.ReadFromHeader(aData + 36, /* aReadAlpha = */ true);
    } else {
      // Bitfields are present after the info header, so we will read them in
      // ReadBitfields().
      bitFieldsLengthStillToRead = BitFields::LENGTH;
    }
  } else if (mH.mBpp == 16) {
    // No bitfields specified; use the default 5-5-5 values.
    mBitFields.SetR5G5B5();
  } else if (mH.mBpp == 32) {
    // No bitfields specified; use the default 8-8-8 values.
    mBitFields.SetR8G8B8();
  }

  return Transition::To(State::BITFIELDS, bitFieldsLengthStillToRead);
}

void
BitFields::ReadFromHeader(const char* aData, bool aReadAlpha)
{
  mRed.Set  (LittleEndian::readUint32(aData + 0));
  mGreen.Set(LittleEndian::readUint32(aData + 4));
  mBlue.Set (LittleEndian::readUint32(aData + 8));
  if (aReadAlpha) {
    mAlpha.Set(LittleEndian::readUint32(aData + 12));
  }
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadBitfields(const char* aData, size_t aLength)
{
  mPreGapLength += aLength;

  // If aLength is zero there are no bitfields to read, or we already read them
  // in ReadInfoHeader().
  if (aLength != 0) {
    mBitFields.ReadFromHeader(aData, /* aReadAlpha = */ false);
  }

  // Note that RLE-encoded BMPs might be transparent because the 'delta' mode
  // can skip pixels and cause implicit transparency.
  mMayHaveTransparency =
    mIsWithinICO ||
    mH.mCompression == Compression::RLE8 ||
    mH.mCompression == Compression::RLE4 ||
    (mH.mCompression == Compression::BITFIELDS &&
     mBitFields.mAlpha.IsPresent());
  if (mMayHaveTransparency) {
    PostHasTransparency();
  }

  // Post our size to the superclass.
  PostSize(mH.mWidth, AbsoluteHeight());
  if (HasError()) {
    return Transition::TerminateFailure();
  }

  // We've now read all the headers. If we're doing a metadata decode, we're
  // done.
  if (IsMetadataDecode()) {
    return Transition::TerminateSuccess();
  }

  // Set up the color table, if present; it'll be filled in by ReadColorTable().
  if (mH.mBpp <= 8) {
    mNumColors = 1 << mH.mBpp;
    if (0 < mH.mNumColors && mH.mNumColors < mNumColors) {
      mNumColors = mH.mNumColors;
    }

    // Always allocate and zero 256 entries, even though mNumColors might be
    // smaller, because the file might erroneously index past mNumColors.
    mColors = MakeUnique<ColorTableEntry[]>(256);
    memset(mColors.get(), 0, 256 * sizeof(ColorTableEntry));

    // OS/2 Bitmaps have no padding byte.
    mBytesPerColor = (mH.mBIHSize == InfoHeaderLength::WIN_V2) ? 3 : 4;
  }

  MOZ_ASSERT(!mImageData, "Already have a buffer allocated?");
  nsresult rv = AllocateFrame(OutputSize(), FullOutputFrame(),
                              mMayHaveTransparency ? SurfaceFormat::B8G8R8A8
                                                   : SurfaceFormat::B8G8R8X8);
  if (NS_FAILED(rv)) {
    return Transition::TerminateFailure();
  }
  MOZ_ASSERT(mImageData, "Should have a buffer now");

  if (mDownscaler) {
    // BMPs store their rows in reverse order, so the downscaler needs to
    // reverse them again when writing its output. Unless the height is
    // negative!
    rv = mDownscaler->BeginFrame(Size(), Nothing(),
                                 mImageData, mMayHaveTransparency,
                                 /* aFlipVertically = */ mH.mHeight >= 0);
    if (NS_FAILED(rv)) {
      return Transition::TerminateFailure();
    }
  }

  return Transition::To(State::COLOR_TABLE, mNumColors * mBytesPerColor);
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadColorTable(const char* aData, size_t aLength)
{
  MOZ_ASSERT_IF(aLength != 0, mNumColors > 0 && mColors);

  mPreGapLength += aLength;

  for (uint32_t i = 0; i < mNumColors; i++) {
    // The format is BGR or BGR0.
    mColors[i].mBlue  = uint8_t(aData[0]);
    mColors[i].mGreen = uint8_t(aData[1]);
    mColors[i].mRed   = uint8_t(aData[2]);
    aData += mBytesPerColor;
  }

  // We know how many bytes we've read so far (mPreGapLength) and we know the
  // offset of the pixel data (mH.mDataOffset), so we can determine the length
  // of the gap (possibly zero) between the color table and the pixel data.
  //
  // If the gap is negative the file must be malformed (e.g. mH.mDataOffset
  // points into the middle of the color palette instead of past the end) and
  // we give up.
  if (mPreGapLength > mH.mDataOffset) {
    return Transition::TerminateFailure();
  }

  uint32_t gapLength = mH.mDataOffset - mPreGapLength;
  return Transition::ToUnbuffered(State::AFTER_GAP, State::GAP, gapLength);
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::SkipGap()
{
  return Transition::ContinueUnbuffered(State::GAP);
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::AfterGap()
{
  // If there are no pixels we can stop.
  //
  // XXX: normally, if there are no pixels we will have stopped decoding before
  // now, outside of this decoder. However, if the BMP is within an ICO file,
  // it's possible that the ICO claimed the image had a non-zero size while the
  // BMP claims otherwise. This test is to catch that awkward case. If we ever
  // come up with a more general solution to this ICO-and-BMP-disagree-on-size
  // problem, this test can be removed.
  if (mH.mWidth == 0 || mH.mHeight == 0) {
    return Transition::TerminateSuccess();
  }

  bool hasRLE = mH.mCompression == Compression::RLE8 ||
                mH.mCompression == Compression::RLE4;
  return hasRLE
       ? Transition::To(State::RLE_SEGMENT, RLE::SEGMENT_LENGTH)
       : Transition::To(State::PIXEL_ROW, mPixelRowSize);
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadPixelRow(const char* aData)
{
  MOZ_ASSERT(mCurrentRow > 0);
  MOZ_ASSERT(mCurrentPos == 0);

  const uint8_t* src = reinterpret_cast<const uint8_t*>(aData);
  uint32_t* dst = RowBuffer();
  uint32_t lpos = mH.mWidth;
  switch (mH.mBpp) {
    case 1:
      while (lpos > 0) {
        int8_t bit;
        uint8_t idx;
        for (bit = 7; bit >= 0 && lpos > 0; bit--) {
          idx = (*src >> bit) & 1;
          SetPixel(dst, idx, mColors);
          --lpos;
        }
        ++src;
      }
      break;

    case 4:
      while (lpos > 0) {
        Set4BitPixel(dst, *src, lpos, mColors);
        ++src;
      }
      break;

    case 8:
      while (lpos > 0) {
        SetPixel(dst, *src, mColors);
        --lpos;
        ++src;
      }
      break;

    case 16:
      if (mBitFields.IsR5G5B5()) {
        // Specialize this common case.
        while (lpos > 0) {
          uint16_t val = LittleEndian::readUint16(src);
          SetPixel(dst, mBitFields.mRed.Get5(val),
                        mBitFields.mGreen.Get5(val),
                        mBitFields.mBlue.Get5(val));
          --lpos;
          src += 2;
        }
      } else {
        bool anyHasAlpha = false;
        while (lpos > 0) {
          uint16_t val = LittleEndian::readUint16(src);
          SetPixel(dst, mBitFields.mRed.Get(val),
                        mBitFields.mGreen.Get(val),
                        mBitFields.mBlue.Get(val),
                        mBitFields.mAlpha.GetAlpha(val, anyHasAlpha));
          --lpos;
          src += 2;
        }
        if (anyHasAlpha) {
          MOZ_ASSERT(mMayHaveTransparency);
          mDoesHaveTransparency = true;
        }
      }
      break;

    case 24:
      while (lpos > 0) {
        SetPixel(dst, src[2], src[1], src[0]);
        --lpos;
        src += 3;
      }
      break;

    case 32:
      if (mH.mCompression == Compression::RGB && mIsWithinICO &&
          mH.mBpp == 32) {
        // This is a special case only used for 32bpp WinBMPv3-ICO files, which
        // could be in either 0RGB or ARGB format. We start by assuming it's
        // an 0RGB image. If we hit a non-zero alpha value, then we know it's
        // actually an ARGB image, and change tack accordingly.
        // (Note: a fully-transparent ARGB image is indistinguishable from a
        // 0RGB image, and we will render such an image as a 0RGB image, i.e.
        // opaquely. This is unlikely to be a problem in practice.)
        while (lpos > 0) {
          if (!mDoesHaveTransparency && src[3] != 0) {
            // Up until now this looked like an 0RGB image, but we now know
            // it's actually an ARGB image. Which means every pixel we've seen
            // so far has been fully transparent. So we go back and redo them.

            // Tell the Downscaler to go back to the start.
            if (mDownscaler) {
              mDownscaler->ResetForNextProgressivePass();
            }

            // Redo the complete rows we've already done.
            MOZ_ASSERT(mCurrentPos == 0);
            int32_t currentRow = mCurrentRow;
            mCurrentRow = AbsoluteHeight();
            while (mCurrentRow > currentRow) {
              dst = RowBuffer();
              for (int32_t i = 0; i < mH.mWidth; i++) {
                SetPixel(dst, 0, 0, 0, 0);
              }
              FinishRow();
            }

            // Redo the part of this row we've already done.
            dst = RowBuffer();
            int32_t n = mH.mWidth - lpos;
            for (int32_t i = 0; i < n; i++) {
              SetPixel(dst, 0, 0, 0, 0);
            }

            MOZ_ASSERT(mMayHaveTransparency);
            mDoesHaveTransparency = true;
          }

          // If mDoesHaveTransparency is false, treat this as an 0RGB image.
          // Otherwise, treat this as an ARGB image.
          SetPixel(dst, src[2], src[1], src[0],
                   mDoesHaveTransparency ? src[3] : 0xff);
          src += 4;
          --lpos;
        }
      } else if (mBitFields.IsR8G8B8()) {
        // Specialize this common case.
        while (lpos > 0) {
          uint32_t val = LittleEndian::readUint32(src);
          SetPixel(dst, mBitFields.mRed.Get8(val),
                        mBitFields.mGreen.Get8(val),
                        mBitFields.mBlue.Get8(val));
          --lpos;
          src += 4;
        }
      } else {
        bool anyHasAlpha = false;
        while (lpos > 0) {
          uint32_t val = LittleEndian::readUint32(src);
          SetPixel(dst, mBitFields.mRed.Get(val),
                        mBitFields.mGreen.Get(val),
                        mBitFields.mBlue.Get(val),
                        mBitFields.mAlpha.GetAlpha(val, anyHasAlpha));
          --lpos;
          src += 4;
        }
        if (anyHasAlpha) {
          MOZ_ASSERT(mMayHaveTransparency);
          mDoesHaveTransparency = true;
        }
      }
      break;

    default:
      MOZ_CRASH("Unsupported color depth; earlier check didn't catch it?");
  }

  FinishRow();
  return mCurrentRow == 0
       ? Transition::TerminateSuccess()
       : Transition::To(State::PIXEL_ROW, mPixelRowSize);
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadRLESegment(const char* aData)
{
  if (mCurrentRow == 0) {
    return Transition::TerminateSuccess();
  }

  uint8_t byte1 = uint8_t(aData[0]);
  uint8_t byte2 = uint8_t(aData[1]);

  if (byte1 != RLE::ESCAPE) {
    // Encoded mode consists of two bytes: byte1 specifies the number of
    // consecutive pixels to be drawn using the color index contained in
    // byte2.
    //
    // Work around bitmaps that specify too many pixels.
    uint32_t pixelsNeeded =
      std::min<uint32_t>(mH.mWidth - mCurrentPos, byte1);
    if (pixelsNeeded) {
      uint32_t* dst = RowBuffer();
      mCurrentPos += pixelsNeeded;
      if (mH.mCompression == Compression::RLE8) {
        do {
          SetPixel(dst, byte2, mColors);
          pixelsNeeded --;
        } while (pixelsNeeded);
      } else {
        do {
          Set4BitPixel(dst, byte2, pixelsNeeded, mColors);
        } while (pixelsNeeded);
      }
    }
    return Transition::To(State::RLE_SEGMENT, RLE::SEGMENT_LENGTH);
  }

  if (byte2 == RLE::ESCAPE_EOL) {
    mCurrentPos = 0;
    FinishRow();
    return mCurrentRow == 0
         ? Transition::TerminateSuccess()
         : Transition::To(State::RLE_SEGMENT, RLE::SEGMENT_LENGTH);
  }

  if (byte2 == RLE::ESCAPE_EOF) {
    return Transition::TerminateSuccess();
  }

  if (byte2 == RLE::ESCAPE_DELTA) {
    return Transition::To(State::RLE_DELTA, RLE::DELTA_LENGTH);
  }

  // Absolute mode. |byte2| gives the number of pixels. The length depends on
  // whether it's 4-bit or 8-bit RLE. Also, the length must be even (and zero
  // padding is used to achieve this when necessary).
  MOZ_ASSERT(mAbsoluteModeNumPixels == 0);
  mAbsoluteModeNumPixels = byte2;
  uint32_t length = byte2;
  if (mH.mCompression == Compression::RLE4) {
    length = (length + 1) / 2;    // halve, rounding up
  }
  if (length & 1) {
    length++;
  }
  return Transition::To(State::RLE_ABSOLUTE, length);
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadRLEDelta(const char* aData)
{
  // Delta encoding makes it possible to skip pixels making part of the image
  // transparent.
  MOZ_ASSERT(mMayHaveTransparency);
  mDoesHaveTransparency = true;

  if (mDownscaler) {
    // Clear the skipped pixels. (This clears to the end of the row,
    // which is perfect if there's a Y delta and harmless if not).
    mDownscaler->ClearRestOfRow(/* aStartingAtCol = */ mCurrentPos);
  }

  // Handle the XDelta.
  mCurrentPos += uint8_t(aData[0]);
  if (mCurrentPos > mH.mWidth) {
    mCurrentPos = mH.mWidth;
  }

  // Handle the Y Delta.
  int32_t yDelta = std::min<int32_t>(uint8_t(aData[1]), mCurrentRow);
  mCurrentRow -= yDelta;

  if (mDownscaler && yDelta > 0) {
    // Commit the current row (the first of the skipped rows).
    mDownscaler->CommitRow();

    // Clear and commit the remaining skipped rows.
    for (int32_t line = 1; line < yDelta; line++) {
      mDownscaler->ClearRow();
      mDownscaler->CommitRow();
    }
  }

  return mCurrentRow == 0
       ? Transition::TerminateSuccess()
       : Transition::To(State::RLE_SEGMENT, RLE::SEGMENT_LENGTH);
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadRLEAbsolute(const char* aData, size_t aLength)
{
  uint32_t n = mAbsoluteModeNumPixels;
  mAbsoluteModeNumPixels = 0;

  if (mCurrentPos + n > uint32_t(mH.mWidth)) {
    // Bad data. Stop decoding; at least part of the image may have been
    // decoded.
    return Transition::TerminateSuccess();
  }

  // In absolute mode, n represents the number of pixels that follow, each of
  // which contains the color index of a single pixel.
  uint32_t* dst = RowBuffer();
  uint32_t iSrc = 0;
  uint32_t* oldPos = dst;
  if (mH.mCompression == Compression::RLE8) {
    while (n > 0) {
      SetPixel(dst, aData[iSrc], mColors);
      n--;
      iSrc++;
    }
  } else {
    while (n > 0) {
      Set4BitPixel(dst, aData[iSrc], n, mColors);
      iSrc++;
    }
  }
  mCurrentPos += dst - oldPos;

  // We should read all the data (unless the last byte is zero padding).
  MOZ_ASSERT(iSrc == aLength - 1 || iSrc == aLength);

  return Transition::To(State::RLE_SEGMENT, RLE::SEGMENT_LENGTH);
}

} // namespace image
} // namespace mozilla
