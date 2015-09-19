/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This is a Cross-Platform ICO Decoder, which should work everywhere, including
 * Big-Endian machines like the PowerPC. */

#include <stdlib.h>

#include "mozilla/Endian.h"
#include "mozilla/Move.h"
#include "nsICODecoder.h"

#include "RasterImage.h"

namespace mozilla {
namespace image {

// Constants.
static const uint32_t ICOHEADERSIZE = 6;
static const uint32_t BITMAPINFOSIZE = 40;
static const uint32_t PREFICONSIZE = 16;

// ----------------------------------------
// Actual Data Processing
// ----------------------------------------

uint32_t
nsICODecoder::CalcAlphaRowSize()
{
  // Calculate rowsize in DWORD's and then return in # of bytes
  uint32_t rowSize = (GetRealWidth() + 31) / 32; // + 31 to round up
  return rowSize * 4; // Return rowSize in bytes
}

// Obtains the number of colors from the bits per pixel
uint16_t
nsICODecoder::GetNumColors()
{
  uint16_t numColors = 0;
  if (mBPP <= 8) {
    switch (mBPP) {
    case 1:
      numColors = 2;
      break;
    case 4:
      numColors = 16;
      break;
    case 8:
      numColors = 256;
      break;
    default:
      numColors = (uint16_t)-1;
    }
  }
  return numColors;
}

nsICODecoder::nsICODecoder(RasterImage* aImage)
  : Decoder(aImage)
  , mLexer(Transition::To(ICOState::HEADER, ICOHEADERSIZE))
  , mBestResourceDelta(INT_MIN)
  , mBestResourceColorDepth(0)
  , mNumIcons(0)
  , mCurrIcon(0)
  , mBPP(0)
  , mMaskRowSize(0)
  , mCurrMaskLine(0)
{ }

void
nsICODecoder::FinishInternal()
{
  // We shouldn't be called in error cases
  MOZ_ASSERT(!HasError(), "Shouldn't call FinishInternal after error!");

  GetFinalStateFromContainedDecoder();
}

void
nsICODecoder::FinishWithErrorInternal()
{
  GetFinalStateFromContainedDecoder();
}

void
nsICODecoder::GetFinalStateFromContainedDecoder()
{
  if (!mContainedDecoder) {
    return;
  }

  // Finish the internally used decoder.
  mContainedDecoder->CompleteDecode();

  mDecodeDone = mContainedDecoder->GetDecodeDone();
  mDataError = mDataError || mContainedDecoder->HasDataError();
  mFailCode = NS_SUCCEEDED(mFailCode) ? mContainedDecoder->GetDecoderError()
                                      : mFailCode;
  mDecodeAborted = mContainedDecoder->WasAborted();
  mProgress |= mContainedDecoder->TakeProgress();
  mInvalidRect.UnionRect(mInvalidRect, mContainedDecoder->TakeInvalidRect());
  mCurrentFrame = mContainedDecoder->GetCurrentFrameRef();

  MOZ_ASSERT(HasError() || !mCurrentFrame || mCurrentFrame->IsImageComplete());
}

// Returns a buffer filled with the bitmap file header in little endian:
// Signature 2 bytes 'BM'
// FileSize      4 bytes File size in bytes
// reserved      4 bytes unused (=0)
// DataOffset    4 bytes File offset to Raster Data
// Returns true if successful
bool
nsICODecoder::FillBitmapFileHeaderBuffer(int8_t* bfh)
{
  memset(bfh, 0, 14);
  bfh[0] = 'B';
  bfh[1] = 'M';
  int32_t dataOffset = 0;
  int32_t fileSize = 0;
  dataOffset = BMPFILEHEADER::LENGTH + BITMAPINFOSIZE;

  // The color table is present only if BPP is <= 8
  if (mDirEntry.mBitCount <= 8) {
    uint16_t numColors = GetNumColors();
    if (numColors == (uint16_t)-1) {
      return false;
    }
    dataOffset += 4 * numColors;
    fileSize = dataOffset + GetRealWidth() * GetRealHeight();
  } else {
    fileSize = dataOffset + (mDirEntry.mBitCount * GetRealWidth() *
                             GetRealHeight()) / 8;
  }

  NativeEndian::swapToLittleEndianInPlace(&fileSize, 1);
  memcpy(bfh + 2, &fileSize, sizeof(fileSize));
  NativeEndian::swapToLittleEndianInPlace(&dataOffset, 1);
  memcpy(bfh + 10, &dataOffset, sizeof(dataOffset));
  return true;
}

// A BMP inside of an ICO has *2 height because of the AND mask
// that follows the actual bitmap.  The BMP shouldn't know about
// this difference though.
bool
nsICODecoder::FixBitmapHeight(int8_t* bih)
{
  // Get the height from the BMP file information header
  int32_t height;
  memcpy(&height, bih + 8, sizeof(height));
  NativeEndian::swapFromLittleEndianInPlace(&height, 1);
  // BMPs can be stored inverted by having a negative height
  height = abs(height);

  // The bitmap height is by definition * 2 what it should be to account for
  // the 'AND mask'. It is * 2 even if the `AND mask` is not present.
  height /= 2;

  if (height > 256) {
    return false;
  }

  // We should always trust the height from the bitmap itself instead of
  // the ICO height.  So fix the ICO height.
  if (height == 256) {
    mDirEntry.mHeight = 0;
  } else {
    mDirEntry.mHeight = (int8_t)height;
  }

  // Fix the BMP height in the BIH so that the BMP decoder can work properly
  NativeEndian::swapToLittleEndianInPlace(&height, 1);
  memcpy(bih + 8, &height, sizeof(height));
  return true;
}

// We should always trust the contained resource for the width
// information over our own information.
bool
nsICODecoder::FixBitmapWidth(int8_t* bih)
{
  // Get the width from the BMP file information header
  int32_t width;
  memcpy(&width, bih + 4, sizeof(width));
  NativeEndian::swapFromLittleEndianInPlace(&width, 1);
  if (width > 256) {
    return false;
  }

  // We should always trust the width  from the bitmap itself instead of
  // the ICO width.
  if (width == 256) {
    mDirEntry.mWidth = 0;
  } else {
    mDirEntry.mWidth = (int8_t)width;
  }
  return true;
}

// The BMP information header's bits per pixel should be trusted
// more than what we have.  Usually the ICO's BPP is set to 0.
int32_t
nsICODecoder::ReadBPP(const char* aBIH)
{
  const int8_t* bih = reinterpret_cast<const int8_t*>(aBIH);
  int32_t bitsPerPixel;
  memcpy(&bitsPerPixel, bih + 14, sizeof(bitsPerPixel));
  NativeEndian::swapFromLittleEndianInPlace(&bitsPerPixel, 1);
  return bitsPerPixel;
}

int32_t
nsICODecoder::ReadBIHSize(const char* aBIH)
{
  const int8_t* bih = reinterpret_cast<const int8_t*>(aBIH);
  int32_t headerSize;
  memcpy(&headerSize, bih, sizeof(headerSize));
  NativeEndian::swapFromLittleEndianInPlace(&headerSize, 1);
  return headerSize;
}

void
nsICODecoder::SetHotSpotIfCursor()
{
  if (!mIsCursor) {
    return;
  }

  mImageMetadata.SetHotspot(mDirEntry.mXHotspot, mDirEntry.mYHotspot);
}

LexerTransition<ICOState>
nsICODecoder::ReadHeader(const char* aData)
{
  // If the third byte is 1, this is an icon. If 2, a cursor.
  if ((aData[2] != 1) && (aData[2] != 2)) {
    return Transition::Terminate(ICOState::FAILURE);
  }
  mIsCursor = (aData[2] == 2);

  // The fifth and sixth bytes specify the number of resources in the file.
  mNumIcons =
    LittleEndian::readUint16(reinterpret_cast<const uint16_t*>(aData + 4));
  if (mNumIcons == 0) {
    return Transition::Terminate(ICOState::SUCCESS); // Nothing to do.
  }

  // If we didn't get a #-moz-resolution, default to PREFICONSIZE.
  if (mResolution.width == 0 && mResolution.height == 0) {
    mResolution.SizeTo(PREFICONSIZE, PREFICONSIZE);
  }

  return Transition::To(ICOState::DIR_ENTRY, ICODIRENTRYSIZE);
}

size_t
nsICODecoder::FirstResourceOffset() const
{
  MOZ_ASSERT(mNumIcons > 0,
             "Calling FirstResourceOffset before processing header");

  // The first resource starts right after the directory, which starts right
  // after the ICO header.
  return ICOHEADERSIZE + mNumIcons * ICODIRENTRYSIZE;
}

LexerTransition<ICOState>
nsICODecoder::ReadDirEntry(const char* aData)
{
  mCurrIcon++;

  // Read the directory entry.
  IconDirEntry e;
  memset(&e, 0, sizeof(e));
  memcpy(&e.mWidth, aData, sizeof(e.mWidth));
  memcpy(&e.mHeight, aData + 1, sizeof(e.mHeight));
  memcpy(&e.mColorCount, aData + 2, sizeof(e.mColorCount));
  memcpy(&e.mReserved, aData + 3, sizeof(e.mReserved));
  memcpy(&e.mPlanes, aData + 4, sizeof(e.mPlanes));
  e.mPlanes = LittleEndian::readUint16(&e.mPlanes);
  memcpy(&e.mBitCount, aData + 6, sizeof(e.mBitCount));
  e.mBitCount = LittleEndian::readUint16(&e.mBitCount);
  memcpy(&e.mBytesInRes, aData + 8, sizeof(e.mBytesInRes));
  e.mBytesInRes = LittleEndian::readUint32(&e.mBytesInRes);
  memcpy(&e.mImageOffset, aData + 12, sizeof(e.mImageOffset));
  e.mImageOffset = LittleEndian::readUint32(&e.mImageOffset);

  // Calculate the delta between this image's size and the desired size, so we
  // can see if it is better than our current-best option.  In the case of
  // several equally-good images, we use the last one. "Better" in this case is
  // determined by |delta|, a measure of the difference in size between the
  // entry we've found and the requested size. We will choose the smallest image
  // that is >= requested size (i.e. we assume it's better to downscale a larger
  // icon than to upscale a smaller one).
  int32_t delta = GetRealWidth(e) - mResolution.width +
                  GetRealHeight(e) - mResolution.height;
  if (e.mBitCount >= mBestResourceColorDepth &&
      ((mBestResourceDelta < 0 && delta >= mBestResourceDelta) ||
       (delta >= 0 && delta <= mBestResourceDelta))) {
    mBestResourceDelta = delta;

    // Ensure mImageOffset is >= size of the direntry headers (bug #245631).
    if (e.mImageOffset < FirstResourceOffset()) {
      return Transition::Terminate(ICOState::FAILURE);
    }

    mBestResourceColorDepth = e.mBitCount;
    mDirEntry = e;
  }

  if (mCurrIcon == mNumIcons) {
    PostSize(GetRealWidth(mDirEntry), GetRealHeight(mDirEntry));
    if (IsMetadataDecode()) {
      return Transition::Terminate(ICOState::SUCCESS);
    }

    size_t offsetToResource = mDirEntry.mImageOffset - FirstResourceOffset();
    return Transition::ToUnbuffered(ICOState::FOUND_RESOURCE,
                                    ICOState::SKIP_TO_RESOURCE,
                                    offsetToResource);
  }

  return Transition::To(ICOState::DIR_ENTRY, ICODIRENTRYSIZE);
}

LexerTransition<ICOState>
nsICODecoder::SniffResource(const char* aData)
{
  // We use the first PNGSIGNATURESIZE bytes to determine whether this resource
  // is a PNG or a BMP.
  bool isPNG = !memcmp(aData, nsPNGDecoder::pngSignatureBytes,
                       PNGSIGNATURESIZE);
  if (isPNG) {
    // Create a PNG decoder which will do the rest of the work for us.
    mContainedDecoder = new nsPNGDecoder(mImage);
    mContainedDecoder->SetMetadataDecode(IsMetadataDecode());
    mContainedDecoder->SetDecoderFlags(GetDecoderFlags());
    mContainedDecoder->SetSurfaceFlags(GetSurfaceFlags());
    mContainedDecoder->Init();

    if (!WriteToContainedDecoder(aData, PNGSIGNATURESIZE)) {
      return Transition::Terminate(ICOState::FAILURE);
    }

    if (mDirEntry.mBytesInRes <= PNGSIGNATURESIZE) {
      return Transition::Terminate(ICOState::FAILURE);
    }

    // Read in the rest of the PNG unbuffered.
    size_t toRead = mDirEntry.mBytesInRes - PNGSIGNATURESIZE;
    return Transition::ToUnbuffered(ICOState::FINISHED_RESOURCE,
                                    ICOState::READ_PNG,
                                    toRead);
  } else {
    // Create a BMP decoder which will do most of the work for us; the exception
    // is the AND mask, which isn't present in standalone BMPs.
    nsBMPDecoder* bmpDecoder = new nsBMPDecoder(mImage);
    mContainedDecoder = bmpDecoder;
    bmpDecoder->SetUseAlphaData(true);
    mContainedDecoder->SetMetadataDecode(IsMetadataDecode());
    mContainedDecoder->SetDecoderFlags(GetDecoderFlags());
    mContainedDecoder->SetSurfaceFlags(GetSurfaceFlags());
    mContainedDecoder->Init();

    // Make sure we have a sane size for the bitmap information header.
    int32_t bihSize = ReadBIHSize(aData);
    if (bihSize != static_cast<int32_t>(BITMAPINFOSIZE)) {
      return Transition::Terminate(ICOState::FAILURE);
    }

    // Buffer the first part of the bitmap information header.
    memcpy(mBIHraw, aData, PNGSIGNATURESIZE);

    // Read in the rest of the bitmap information header.
    return Transition::To(ICOState::READ_BIH,
                          BITMAPINFOSIZE - PNGSIGNATURESIZE);
  }
}

LexerTransition<ICOState>
nsICODecoder::ReadPNG(const char* aData, uint32_t aLen)
{
  if (!WriteToContainedDecoder(aData, aLen)) {
    return Transition::Terminate(ICOState::FAILURE);
  }

  // Raymond Chen says that 32bpp only are valid PNG ICOs
  // http://blogs.msdn.com/b/oldnewthing/archive/2010/10/22/10079192.aspx
  if (!IsMetadataDecode() &&
      !static_cast<nsPNGDecoder*>(mContainedDecoder.get())->IsValidICO()) {
    return Transition::Terminate(ICOState::FAILURE);
  }

  return Transition::ContinueUnbuffered(ICOState::READ_PNG);
}

LexerTransition<ICOState>
nsICODecoder::ReadBIH(const char* aData)
{
  // Buffer the rest of the bitmap information header.
  memcpy(mBIHraw + PNGSIGNATURESIZE, aData, BITMAPINFOSIZE - PNGSIGNATURESIZE);

  // Extracting the BPP from the BIH header; it should be trusted over the one
  // we have from the ICO header.
  mBPP = ReadBPP(mBIHraw);

  // The ICO format when containing a BMP does not include the 14 byte
  // bitmap file header. To use the code of the BMP decoder we need to
  // generate this header ourselves and feed it to the BMP decoder.
  int8_t bfhBuffer[BMPFILEHEADERSIZE];
  if (!FillBitmapFileHeaderBuffer(bfhBuffer)) {
    return Transition::Terminate(ICOState::FAILURE);
  }

  if (!WriteToContainedDecoder(reinterpret_cast<const char*>(bfhBuffer),
                               sizeof(bfhBuffer))) {
    return Transition::Terminate(ICOState::FAILURE);
  }

  // Set up the cursor hot spot if one is present.
  SetHotSpotIfCursor();

  // Fix the ICO height from the BIH. It needs to be halved so our BMP decoder
  // will understand, because the BMP decoder doesn't expect the alpha mask that
  // follows the BMP data in an ICO.
  if (!FixBitmapHeight(reinterpret_cast<int8_t*>(mBIHraw))) {
    return Transition::Terminate(ICOState::FAILURE);
  }

  // Fix the ICO width from the BIH.
  if (!FixBitmapWidth(reinterpret_cast<int8_t*>(mBIHraw))) {
    return Transition::Terminate(ICOState::FAILURE);
  }

  // Write out the BMP's bitmap info header.
  if (!WriteToContainedDecoder(mBIHraw, sizeof(mBIHraw))) {
    return Transition::Terminate(ICOState::FAILURE);
  }

  // Sometimes the ICO BPP header field is not filled out so we should trust the
  // contained resource over our own information.
  // XXX(seth): Is this ever different than the value we obtained from
  // ReadBPP() above?
  nsRefPtr<nsBMPDecoder> bmpDecoder =
    static_cast<nsBMPDecoder*>(mContainedDecoder.get());
  mBPP = bmpDecoder->GetBitsPerPixel();

  // Check to make sure we have valid color settings.
  uint16_t numColors = GetNumColors();
  if (numColors == uint16_t(-1)) {
    return Transition::Terminate(ICOState::FAILURE);
  }

  // Do we have an AND mask on this BMP? If so, we need to read it after we read
  // the BMP data itself.
  uint32_t bmpDataLength = bmpDecoder->GetCompressedImageSize() + 4 * numColors;
  bool hasANDMask = (BITMAPINFOSIZE + bmpDataLength) < mDirEntry.mBytesInRes;
  ICOState afterBMPState = hasANDMask ? ICOState::PREPARE_FOR_MASK
                                      : ICOState::FINISHED_RESOURCE;

  // Read in the rest of the BMP unbuffered.
  return Transition::ToUnbuffered(afterBMPState,
                                  ICOState::READ_BMP,
                                  bmpDataLength);
}

LexerTransition<ICOState>
nsICODecoder::ReadBMP(const char* aData, uint32_t aLen)
{
  if (!WriteToContainedDecoder(aData, aLen)) {
    return Transition::Terminate(ICOState::FAILURE);
  }

  return Transition::ContinueUnbuffered(ICOState::READ_BMP);
}

LexerTransition<ICOState>
nsICODecoder::PrepareForMask()
{
  nsRefPtr<nsBMPDecoder> bmpDecoder =
    static_cast<nsBMPDecoder*>(mContainedDecoder.get());

  uint16_t numColors = GetNumColors();
  MOZ_ASSERT(numColors != uint16_t(-1));

  // Determine the length of the AND mask.
  uint32_t bmpLengthWithHeader =
    BITMAPINFOSIZE + bmpDecoder->GetCompressedImageSize() + 4 * numColors;
  MOZ_ASSERT(bmpLengthWithHeader < mDirEntry.mBytesInRes);
  uint32_t maskLength = mDirEntry.mBytesInRes - bmpLengthWithHeader;

  // If we have a 32-bpp BMP with alpha data, we ignore the AND mask. We can
  // also obviously ignore it if the image has zero width or zero height.
  if ((bmpDecoder->GetBitsPerPixel() == 32 && bmpDecoder->HasAlphaData()) ||
      GetRealWidth() == 0 || GetRealHeight() == 0) {
    return Transition::ToUnbuffered(ICOState::FINISHED_RESOURCE,
                                    ICOState::SKIP_MASK,
                                    maskLength);
  }

  // Compute the row size for the mask.
  mMaskRowSize = ((GetRealWidth() + 31) / 32) * 4; // + 31 to round up

  // If the expected size of the AND mask is larger than its actual size, then
  // we must have a truncated (and therefore corrupt) AND mask.
  uint32_t expectedLength = mMaskRowSize * GetRealHeight();
  if (maskLength < expectedLength) {
    return Transition::Terminate(ICOState::FAILURE);
  }

  mCurrMaskLine = GetRealHeight();
  return Transition::To(ICOState::READ_MASK_ROW, mMaskRowSize);
}


LexerTransition<ICOState>
nsICODecoder::ReadMaskRow(const char* aData)
{
  mCurrMaskLine--;

  nsRefPtr<nsBMPDecoder> bmpDecoder =
    static_cast<nsBMPDecoder*>(mContainedDecoder.get());

  uint32_t* imageData = bmpDecoder->GetImageData();
  if (!imageData) {
    return Transition::Terminate(ICOState::FAILURE);
  }

  uint8_t sawTransparency = 0;
  uint32_t* decoded = imageData + mCurrMaskLine * GetRealWidth();
  uint32_t* decodedRowEnd = decoded + GetRealWidth();
  const uint8_t* mask = reinterpret_cast<const uint8_t*>(aData);
  const uint8_t* maskRowEnd = mask + mMaskRowSize;

  // Iterate simultaneously through the AND mask and the image data.
  while (mask < maskRowEnd) {
    uint8_t idx = *mask++;
    sawTransparency |= idx;
    for (uint8_t bit = 0x80; bit && decoded < decodedRowEnd; bit >>= 1) {
      // Clear pixel completely for transparency.
      if (idx & bit) {
        *decoded = 0;
      }
      decoded++;
    }
  }

  // If any bits are set in sawTransparency, then we know at least one pixel was
  // transparent.
  if (sawTransparency) {
    PostHasTransparency();
    bmpDecoder->SetHasAlphaData();
  }

  if (mCurrMaskLine == 0) {
    return Transition::To(ICOState::FINISHED_RESOURCE, 0);
  }

  return Transition::To(ICOState::READ_MASK_ROW, mMaskRowSize);
}

LexerTransition<ICOState>
nsICODecoder::FinishResource()
{
  // Make sure the actual size of the resource matches the size in the directory
  // entry. If not, we consider the image corrupt.
  IntSize expectedSize(GetRealWidth(mDirEntry), GetRealHeight(mDirEntry));
  if (mContainedDecoder->HasSize() &&
      mContainedDecoder->GetSize() != expectedSize) {
    return Transition::Terminate(ICOState::FAILURE);
  }

  return Transition::Terminate(ICOState::SUCCESS);
}

void
nsICODecoder::WriteInternal(const char* aBuffer, uint32_t aCount)
{
  MOZ_ASSERT(!HasError(), "Shouldn't call WriteInternal after error!");
  MOZ_ASSERT(aBuffer);
  MOZ_ASSERT(aCount > 0);

  Maybe<ICOState> terminalState =
    mLexer.Lex(aBuffer, aCount,
               [=](ICOState aState, const char* aData, size_t aLength) {
      switch (aState) {
        case ICOState::HEADER:
          return ReadHeader(aData);
        case ICOState::DIR_ENTRY:
          return ReadDirEntry(aData);
        case ICOState::SKIP_TO_RESOURCE:
          return Transition::ContinueUnbuffered(ICOState::SKIP_TO_RESOURCE);
        case ICOState::FOUND_RESOURCE:
          return Transition::To(ICOState::SNIFF_RESOURCE, PNGSIGNATURESIZE);
        case ICOState::SNIFF_RESOURCE:
          return SniffResource(aData);
        case ICOState::READ_PNG:
          return ReadPNG(aData, aLength);
        case ICOState::READ_BIH:
          return ReadBIH(aData);
        case ICOState::READ_BMP:
          return ReadBMP(aData, aLength);
        case ICOState::PREPARE_FOR_MASK:
          return PrepareForMask();
        case ICOState::READ_MASK_ROW:
          return ReadMaskRow(aData);
        case ICOState::SKIP_MASK:
          return Transition::ContinueUnbuffered(ICOState::SKIP_MASK);
        case ICOState::FINISHED_RESOURCE:
          return FinishResource();
        default:
          MOZ_ASSERT_UNREACHABLE("Unknown ICOState");
          return Transition::Terminate(ICOState::FAILURE);
      }
    });

  if (!terminalState) {
    return;  // Need more data.
  }

  if (*terminalState == ICOState::FAILURE) {
    PostDataError();
    return;
  }

  MOZ_ASSERT(*terminalState == ICOState::SUCCESS);
}

bool
nsICODecoder::WriteToContainedDecoder(const char* aBuffer, uint32_t aCount)
{
  mContainedDecoder->Write(aBuffer, aCount);
  mProgress |= mContainedDecoder->TakeProgress();
  mInvalidRect.UnionRect(mInvalidRect, mContainedDecoder->TakeInvalidRect());
  if (mContainedDecoder->HasDataError()) {
    PostDataError();
  }
  if (mContainedDecoder->HasDecoderError()) {
    PostDecoderError(mContainedDecoder->GetDecoderError());
  }
  return !HasError();
}

} // namespace image
} // namespace mozilla
