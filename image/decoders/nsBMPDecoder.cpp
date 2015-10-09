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
//   file size, and offset to the pixel data (|dataoffset|).
// - Next is a 12 byte info header which includes: the info header size
//   (bihsize), width, height, number of color planes, and bits-per-pixel
//   (|bpp|) which must be 1, 4, 8 or 24.
// - Next is the semi-optional color table, which has length 2^|bpp| and has 3
//   bytes per value (BGR). The color table is required if |bpp| is 1, 4, or 8.
// - Next is an optional gap.
// - Next is the pixel data, which is pointed to by |dataoffset|.
//
// WinBMPv3. This is the most widely used version.
// - It changed the info header to 40 bytes by taking the WinBMPv2 info
//   header, enlargening its width and height fields, and adding more fields
//   including: a compression type (|compression|) and number of colors
//   (|colors|).
// - The semi-optional color table is now 4 bytes per value (BGR0), and its
//   length is |colors|, or 2^|bpp| if |colors| is zero.
// - |compression| can be RGB (i.e. no compression), RLE4 (if bpp==4) or RLE8
//   (if bpp==8) values.
//
// WinBMPv3-NT. A variant of WinBMPv3.
// - It did not change the info header layout from WinBMPv3.
// - |bpp| can now be 16 or 32, in which case |compression| can be RGB or the
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

#include <stdlib.h>

#include "ImageLogging.h"
#include "mozilla/Endian.h"
#include "mozilla/Likely.h"
#include "nsBMPDecoder.h"

#include "nsIInputStream.h"
#include "RasterImage.h"
#include <algorithm>

using namespace mozilla::gfx;

namespace mozilla {
namespace image {

using namespace bmp;

static PRLogModuleInfo*
GetBMPLog()
{
  static PRLogModuleInfo* sBMPLog;
  if (!sBMPLog) {
    sBMPLog = PR_NewLogModule("BMPDecoder");
  }
  return sBMPLog;
}

// Constants relating to RLE compression.
static const size_t RLE_SEGMENT_LENGTH = 2;
static const size_t RLE_DELTA_LENGTH = 2;

nsBMPDecoder::nsBMPDecoder(RasterImage* aImage)
  : Decoder(aImage)
  , mLexer(Transition::To(State::FILE_HEADER, FileHeader::LENGTH))
  , mNumColors(0)
  , mColors(nullptr)
  , mBytesPerColor(0)
  , mPreGapLength(0)
  , mCurrentRow(0)
  , mCurrentPos(0)
  , mAbsoluteModeNumPixels(0)
  , mUseAlphaData(false)
  , mHaveAlphaData(false)
{
  memset(&mBFH, 0, sizeof(mBFH));
  memset(&mBIH, 0, sizeof(mBIH));
}

nsBMPDecoder::~nsBMPDecoder()
{
  delete[] mColors;
}

// Sets whether or not the BMP will use alpha data.
void
nsBMPDecoder::SetUseAlphaData(bool useAlphaData)
{
  mUseAlphaData = useAlphaData;
}

// Obtains the bits per pixel from the internal BIH header.
int32_t
nsBMPDecoder::GetBitsPerPixel() const
{
  return mBIH.bpp;
}

// Obtains the width from the internal BIH header.
int32_t
nsBMPDecoder::GetWidth() const
{
  return mBIH.width;
}

// Obtains the absolute value of the height from the internal BIH header.
// If it's positive the bitmap is stored bottom to top, otherwise top to bottom.
int32_t
nsBMPDecoder::GetHeight() const
{
  return abs(mBIH.height);
}

// Obtains the internal output image buffer.
uint32_t*
nsBMPDecoder::GetImageData()
{
  return reinterpret_cast<uint32_t*>(mImageData);
}

// Obtains the size of the compressed image resource.
int32_t
nsBMPDecoder::GetCompressedImageSize() const
{
  // In the RGB case image_size might not be set, so compute it manually.
  MOZ_ASSERT(mPixelRowSize != 0);
  return mBIH.compression == Compression::RGB
       ? mPixelRowSize * GetHeight()
       : mBIH.image_size;
}

void
nsBMPDecoder::FinishInternal()
{
  // We shouldn't be called in error cases.
  MOZ_ASSERT(!HasError(), "Can't call FinishInternal on error!");

  // We should never make multiple frames.
  MOZ_ASSERT(GetFrameCount() <= 1, "Multiple BMP frames?");

  // Send notifications if appropriate.
  if (!IsMetadataDecode() && HasSize()) {

    // Invalidate.
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
  // Find the rightmost 1.
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

void
nsBMPDecoder::CalcBitShift()
{
  uint8_t begin, length;

  calcBitmask(mBitFields.red, begin, length);
  mBitFields.redRightShift = begin;
  mBitFields.redLeftShift = 8 - length;

  calcBitmask(mBitFields.green, begin, length);
  mBitFields.greenRightShift = begin;
  mBitFields.greenLeftShift = 8 - length;

  calcBitmask(mBitFields.blue, begin, length);
  mBitFields.blueRightShift = begin;
  mBitFields.blueLeftShift = 8 - length;
}

uint32_t*
nsBMPDecoder::RowBuffer()
{
  if (mDownscaler) {
    return reinterpret_cast<uint32_t*>(mDownscaler->RowBuffer()) + mCurrentPos;
  }

  // Convert from row (1..height) to absolute line (0..height-1).
  int32_t line = (mBIH.height < 0)
               ? -mBIH.height - mCurrentRow
               : mCurrentRow - 1;
  int32_t offset = line * mBIH.width + mCurrentPos;
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
    PostInvalidation(IntRect(0, mCurrentRow, mBIH.width, 1));
  }
  mCurrentRow--;
}

void
nsBMPDecoder::WriteInternal(const char* aBuffer, uint32_t aCount)
{
  MOZ_ASSERT(!HasError(), "Shouldn't call WriteInternal after error!");
  MOZ_ASSERT(aBuffer);
  MOZ_ASSERT(aCount > 0);

  Maybe<State> terminalState =
    mLexer.Lex(aBuffer, aCount, [=](State aState,
                                    const char* aData, size_t aLength) {
      switch (aState) {
        case State::FILE_HEADER:      return ReadFileHeader(aData, aLength);
        case State::INFO_HEADER_SIZE: return ReadInfoHeaderSize(aData, aLength);
        case State::INFO_HEADER_REST: return ReadInfoHeaderRest(aData, aLength);
        case State::BITFIELDS:        return ReadBitfields(aData, aLength);
        case State::COLOR_TABLE:      return ReadColorTable(aData, aLength);
        case State::GAP:              return SkipGap();
        case State::PIXEL_ROW:        return ReadPixelRow(aData);
        case State::RLE_SEGMENT:      return ReadRLESegment(aData);
        case State::RLE_DELTA:        return ReadRLEDelta(aData);
        case State::RLE_ABSOLUTE:     return ReadRLEAbsolute(aData, aLength);
        default:
          MOZ_ASSERT_UNREACHABLE("Unknown State");
          return Transition::Terminate(State::FAILURE);
      }
    });

  if (!terminalState) {
    return;  // Need more data.
  }

  if (*terminalState == State::FAILURE) {
    PostDataError();
    return;
  }

  MOZ_ASSERT(*terminalState == State::SUCCESS);

  return;
}

// The length of the bihsize field in the info header.
static const uint32_t BIHSIZE_FIELD_LENGTH = 4;

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadFileHeader(const char* aData, size_t aLength)
{
  mPreGapLength += aLength;

  mBFH.signature[0] = aData[0];
  mBFH.signature[1] = aData[1];
  bool signatureOk = mBFH.signature[0] == 'B' && mBFH.signature[1] == 'M';
  if (!signatureOk) {
    PostDataError();
    return Transition::Terminate(State::FAILURE);
  }

  // Nb: this field is unreliable. In Windows BMPs it's the file size, but in
  // OS/2 BMPs it's sometimes the size of the file and info headers. It doesn't
  // matter because we don't consult it.
  mBFH.filesize = LittleEndian::readUint32(aData + 2);

  mBFH.reserved = 0;

  mBFH.dataoffset = LittleEndian::readUint32(aData + 10);

  return Transition::To(State::INFO_HEADER_SIZE, BIHSIZE_FIELD_LENGTH);
}

// We read the info header in two steps: (a) read the bihsize field to
// determine how long the header is; (b) read the rest of the header.
LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadInfoHeaderSize(const char* aData, size_t aLength)
{
  mPreGapLength += aLength;

  mBIH.bihsize = LittleEndian::readUint32(aData);

  bool bihsizeOk = mBIH.bihsize == InfoHeaderLength::WIN_V2 ||
                   mBIH.bihsize == InfoHeaderLength::WIN_V3 ||
                   mBIH.bihsize == InfoHeaderLength::WIN_V4 ||
                   mBIH.bihsize == InfoHeaderLength::WIN_V5 ||
                   (mBIH.bihsize >= InfoHeaderLength::OS2_V2_MIN &&
                    mBIH.bihsize <= InfoHeaderLength::OS2_V2_MAX);
  if (!bihsizeOk) {
    PostDataError();
    return Transition::Terminate(State::FAILURE);
  }

  return Transition::To(State::INFO_HEADER_REST,
                        mBIH.bihsize - BIHSIZE_FIELD_LENGTH);
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadInfoHeaderRest(const char* aData, size_t aLength)
{
  mPreGapLength += aLength;

  // |width| and |height| may be signed (Windows) or unsigned (OS/2). We just
  // read as unsigned because in practice that's good enough.
  if (mBIH.bihsize == InfoHeaderLength::WIN_V2) {
    mBIH.width       = LittleEndian::readUint16(aData + 0);
    mBIH.height      = LittleEndian::readUint16(aData + 2);
    mBIH.planes      = LittleEndian::readUint16(aData + 4);
    mBIH.bpp         = LittleEndian::readUint16(aData + 6);
  } else {
    mBIH.width       = LittleEndian::readUint32(aData + 0);
    mBIH.height      = LittleEndian::readUint32(aData + 4);
    mBIH.planes      = LittleEndian::readUint16(aData + 8);
    mBIH.bpp         = LittleEndian::readUint16(aData + 10);

    // For OS2-BMPv2 the info header may be as little as 16 bytes, so be
    // careful for these fields.
    mBIH.compression = aLength >= 16 ? LittleEndian::readUint32(aData + 12) : 0;
    mBIH.image_size  = aLength >= 20 ? LittleEndian::readUint32(aData + 16) : 0;
    mBIH.xppm        = aLength >= 24 ? LittleEndian::readUint32(aData + 20) : 0;
    mBIH.yppm        = aLength >= 28 ? LittleEndian::readUint32(aData + 24) : 0;
    mBIH.colors      = aLength >= 32 ? LittleEndian::readUint32(aData + 28) : 0;
    mBIH.important_colors
                     = aLength >= 36 ? LittleEndian::readUint32(aData + 32) : 0;

    // For WinBMPv4, WinBMPv5 and (possibly) OS2-BMPv2 there are additional
    // fields in the info header which we ignore, with the possible exception
    // of the color bitfields (see below).
  }

  // Run with NSPR_LOG_MODULES=BMPDecoder:4 set to see this output.
  MOZ_LOG(GetBMPLog(), LogLevel::Debug,
          ("BMP: bihsize=%u, %d x %d, bpp=%u, compression=%u, colors=%u\n",
          mBIH.bihsize, mBIH.width, mBIH.height, uint32_t(mBIH.bpp),
          mBIH.compression, mBIH.colors));

  // BMPs with negative width are invalid. Also, reject extremely wide images
  // to keep the math sane. And reject INT_MIN as a height because you can't
  // get its absolute value (because -INT_MIN is one more than INT_MAX).
  const int32_t k64KWidth = 0x0000FFFF;
  bool sizeOk = 0 <= mBIH.width && mBIH.width <= k64KWidth &&
                mBIH.height != INT_MIN;
  if (!sizeOk) {
    PostDataError();
    return Transition::Terminate(State::FAILURE);
  }

  // Check bpp and compression.
  bool bppCompressionOk =
    (mBIH.compression == Compression::RGB &&
      (mBIH.bpp ==  1 || mBIH.bpp ==  4 || mBIH.bpp ==  8 ||
       mBIH.bpp == 16 || mBIH.bpp == 24 || mBIH.bpp == 32)) ||
    (mBIH.compression == Compression::RLE8 && mBIH.bpp == 8) ||
    (mBIH.compression == Compression::RLE4 && mBIH.bpp == 4) ||
    (mBIH.compression == Compression::BITFIELDS &&
      (mBIH.bpp == 16 || mBIH.bpp == 32));
  if (!bppCompressionOk) {
    PostDataError();
    return Transition::Terminate(State::FAILURE);
  }

  // Post our size to the superclass.
  uint32_t realHeight = GetHeight();
  PostSize(mBIH.width, realHeight);

  // Round it up to the nearest byte count, then pad to 4-byte boundary.
  // Compute this even for a metadate decode because GetCompressedImageSize()
  // relies on it.
  mPixelRowSize = (mBIH.bpp * mBIH.width + 7) / 8;
  uint32_t surplus = mPixelRowSize % 4;
  if (surplus != 0) {
    mPixelRowSize += 4 - surplus;
  }

  // We treat BMPs as transparent if they're 32bpp and alpha is enabled, but
  // also if they use RLE encoding, because the 'delta' mode can skip pixels
  // and cause implicit transparency.
  bool hasTransparency = (mBIH.compression == Compression::RLE8) ||
                         (mBIH.compression == Compression::RLE4) ||
                         (mBIH.bpp == 32 && mUseAlphaData);
  if (hasTransparency) {
    PostHasTransparency();
  }

  // If we're doing a metadata decode, we're done.
  if (IsMetadataDecode()) {
    return Transition::Terminate(State::SUCCESS);
  }

  // We're doing a real decode.
  mCurrentRow = realHeight;

  // Set up the color table, if present; it'll be filled in by ReadColorTable().
  if (mBIH.bpp <= 8) {
    mNumColors = 1 << mBIH.bpp;
    if (0 < mBIH.colors && mBIH.colors < mNumColors) {
      mNumColors = mBIH.colors;
    }

    // Always allocate and zero 256 entries, even though mNumColors might be
    // smaller, because the file might erroneously index past mNumColors.
    mColors = new ColorTable[256];
    memset(mColors, 0, 256 * sizeof(ColorTable));

    // OS/2 Bitmaps have no padding byte.
    mBytesPerColor = (mBIH.bihsize == InfoHeaderLength::WIN_V2) ? 3 : 4;
  }

  MOZ_ASSERT(!mImageData, "Already have a buffer allocated?");
  IntSize targetSize = mDownscaler ? mDownscaler->TargetSize() : GetSize();
  nsresult rv = AllocateFrame(/* aFrameNum = */ 0, targetSize,
                              IntRect(IntPoint(), targetSize),
                              SurfaceFormat::B8G8R8A8);
  if (NS_FAILED(rv)) {
    return Transition::Terminate(State::FAILURE);
  }
  MOZ_ASSERT(mImageData, "Should have a buffer now");

  if (mDownscaler) {
    // BMPs store their rows in reverse order, so the downscaler needs to
    // reverse them again when writing its output.
    rv = mDownscaler->BeginFrame(GetSize(), Nothing(),
                                 mImageData, hasTransparency,
                                 /* aFlipVertically = */ true);
    if (NS_FAILED(rv)) {
      return Transition::Terminate(State::FAILURE);
    }
  }

  size_t bitFieldsLengthStillToRead = 0;
  if (mBIH.compression == Compression::BITFIELDS) {
    // Need to read bitfields.
    if (mBIH.bihsize >= InfoHeaderLength::WIN_V4) {
      // Bitfields are present in the info header, so we can read them
      // immediately.
      DoReadBitfields(aData + 36);
    } else {
      // Bitfields are present after the info header, so we will read them in
      // ReadBitfields().
      bitFieldsLengthStillToRead = BitFields::LENGTH;
    }
  } else if (mBIH.bpp == 16) {
    // No bitfields specified; use the default 5-5-5 values.
    mBitFields.red   = 0x7C00;
    mBitFields.green = 0x03E0;
    mBitFields.blue  = 0x001F;
    CalcBitShift();
  }

  return Transition::To(State::BITFIELDS, bitFieldsLengthStillToRead);
}

void
nsBMPDecoder::DoReadBitfields(const char* aData)
{
  mBitFields.red   = LittleEndian::readUint32(aData + 0);
  mBitFields.green = LittleEndian::readUint32(aData + 4);
  mBitFields.blue  = LittleEndian::readUint32(aData + 8);
  CalcBitShift();
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadBitfields(const char* aData, size_t aLength)
{
  mPreGapLength += aLength;

  // If aLength is zero there are no bitfields to read, or we already read them
  // in ReadInfoHeader().
  if (aLength != 0) {
    DoReadBitfields(aData);
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
    mColors[i].blue  = uint8_t(aData[0]);
    mColors[i].green = uint8_t(aData[1]);
    mColors[i].red   = uint8_t(aData[2]);
    aData += mBytesPerColor;
  }

  // We know how many bytes we've read so far (mPreGapLength) and we know the
  // offset of the pixel data (mBFH.dataoffset), so we can determine the length
  // of the gap (possibly zero) between the color table and the pixel data.
  //
  // If the gap is negative the file must be malformed (e.g. mBFH.dataoffset
  // points into the middle of the color palette instead of past the end) and
  // we give up.
  if (mPreGapLength > mBFH.dataoffset) {
    PostDataError();
    return Transition::Terminate(State::FAILURE);
  }
  uint32_t gapLength = mBFH.dataoffset - mPreGapLength;
  return Transition::To(State::GAP, gapLength);
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::SkipGap()
{
  bool hasRLE = mBIH.compression == Compression::RLE8 ||
                mBIH.compression == Compression::RLE4;
  return hasRLE
       ? Transition::To(State::RLE_SEGMENT, RLE_SEGMENT_LENGTH)
       : Transition::To(State::PIXEL_ROW, mPixelRowSize);
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadPixelRow(const char* aData)
{
  MOZ_ASSERT(mCurrentPos == 0);

  const uint8_t* src = reinterpret_cast<const uint8_t*>(aData);
  uint32_t* dst = RowBuffer();
  uint32_t lpos = mBIH.width;
  switch (mBIH.bpp) {
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
      while (lpos > 0) {
        uint16_t val = LittleEndian::readUint16(src);
        SetPixel(dst,
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
        src += 2;
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
      while (lpos > 0) {
        if (mUseAlphaData) {
          if (MOZ_UNLIKELY(!mHaveAlphaData && src[3])) {
            PostHasTransparency();
            mHaveAlphaData = true;
          }
          SetPixel(dst, src[2], src[1], src[0], src[3]);
        } else {
          SetPixel(dst, src[2], src[1], src[0]);
        }
        --lpos;
        src += 4;
      }
      break;

    default:
      MOZ_CRASH("Unsupported color depth; earlier check didn't catch it?");
  }

  FinishRow();
  return mCurrentRow == 0
       ? Transition::Terminate(State::SUCCESS)
       : Transition::To(State::PIXEL_ROW, mPixelRowSize);
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadRLESegment(const char* aData)
{
  if (mCurrentRow == 0) {
    return Transition::Terminate(State::SUCCESS);
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
      std::min<uint32_t>(mBIH.width - mCurrentPos, byte1);
    if (pixelsNeeded) {
      uint32_t* dst = RowBuffer();
      mCurrentPos += pixelsNeeded;
      if (mBIH.compression == Compression::RLE8) {
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
    return Transition::To(State::RLE_SEGMENT, RLE_SEGMENT_LENGTH);
  }

  if (byte2 == RLE::ESCAPE_EOL) {
    mCurrentPos = 0;
    FinishRow();
    return mCurrentRow == 0
         ? Transition::Terminate(State::SUCCESS)
         : Transition::To(State::RLE_SEGMENT, RLE_SEGMENT_LENGTH);
  }

  if (byte2 == RLE::ESCAPE_EOF) {
    return Transition::Terminate(State::SUCCESS);
  }

  if (byte2 == RLE::ESCAPE_DELTA) {
    return Transition::To(State::RLE_DELTA, RLE_DELTA_LENGTH);
  }

  // Absolute mode. |byte2| gives the number of pixels. The length depends on
  // whether it's 4-bit or 8-bit RLE. Also, the length must be even (and zero
  // padding is used to achieve this when necessary).
  MOZ_ASSERT(mAbsoluteModeNumPixels == 0);
  mAbsoluteModeNumPixels = byte2;
  uint32_t length = byte2;
  if (mBIH.compression == Compression::RLE4) {
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
  // Delta encoding makes it possible to skip pixels making the image
  // transparent.
  if (MOZ_UNLIKELY(!mHaveAlphaData)) {
    PostHasTransparency();
    mHaveAlphaData = true;
  }
  mUseAlphaData = mHaveAlphaData = true;

  if (mDownscaler) {
    // Clear the skipped pixels. (This clears to the end of the row,
    // which is perfect if there's a Y delta and harmless if not).
    mDownscaler->ClearRow(/* aStartingAtCol = */ mCurrentPos);
  }

  // Handle the XDelta.
  mCurrentPos += uint8_t(aData[0]);
  if (mCurrentPos > mBIH.width) {
    mCurrentPos = mBIH.width;
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
       ? Transition::Terminate(State::SUCCESS)
       : Transition::To(State::RLE_SEGMENT, RLE_SEGMENT_LENGTH);
}

LexerTransition<nsBMPDecoder::State>
nsBMPDecoder::ReadRLEAbsolute(const char* aData, size_t aLength)
{
  uint32_t n = mAbsoluteModeNumPixels;
  mAbsoluteModeNumPixels = 0;

  if (mCurrentPos + n > uint32_t(mBIH.width)) {
    // Bad data. Stop decoding; at least part of the image may have been
    // decoded.
    return Transition::Terminate(State::SUCCESS);
  }

  // In absolute mode, n represents the number of pixels that follow, each of
  // which contains the color index of a single pixel.
  uint32_t* dst = RowBuffer();
  uint32_t iSrc = 0;
  uint32_t* oldPos = dst;
  if (mBIH.compression == Compression::RLE8) {
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

  return Transition::To(State::RLE_SEGMENT, RLE_SEGMENT_LENGTH);
}

} // namespace image
} // namespace mozilla
