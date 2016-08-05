/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This is a Cross-Platform ICO Decoder, which should work everywhere, including
 * Big-Endian machines like the PowerPC. */

#include "nsICODecoder.h"

#include <stdlib.h>

#include "mozilla/EndianUtils.h"
#include "mozilla/Move.h"

#include "RasterImage.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace image {

// Constants.
static const uint32_t ICOHEADERSIZE = 6;
static const uint32_t BITMAPINFOSIZE = bmp::InfoHeaderLength::WIN_ICO;

// ----------------------------------------
// Actual Data Processing
// ----------------------------------------

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
  , mLexer(Transition::To(ICOState::HEADER, ICOHEADERSIZE),
           Transition::TerminateSuccess())
  , mBiggestResourceColorDepth(0)
  , mBestResourceDelta(INT_MIN)
  , mBestResourceColorDepth(0)
  , mNumIcons(0)
  , mCurrIcon(0)
  , mBPP(0)
  , mMaskRowSize(0)
  , mCurrMaskLine(0)
  , mIsCursor(false)
  , mHasMaskAlpha(false)
{ }

nsresult
nsICODecoder::FinishInternal()
{
  // We shouldn't be called in error cases
  MOZ_ASSERT(!HasError(), "Shouldn't call FinishInternal after error!");

  return GetFinalStateFromContainedDecoder();
}

nsresult
nsICODecoder::FinishWithErrorInternal()
{
  return GetFinalStateFromContainedDecoder();
}

nsresult
nsICODecoder::GetFinalStateFromContainedDecoder()
{
  if (!mContainedDecoder) {
    return NS_OK;
  }

  MOZ_ASSERT(mContainedSourceBuffer,
             "Should have a SourceBuffer if we have a decoder");

  // Let the contained decoder finish up if necessary.
  if (!mContainedSourceBuffer->IsComplete()) {
    mContainedSourceBuffer->Complete(NS_OK);
    mContainedDecoder->Decode();
  }

  // Make our state the same as the state of the contained decoder.
  mDecodeDone = mContainedDecoder->GetDecodeDone();
  mDecodeAborted = mContainedDecoder->WasAborted();
  mProgress |= mContainedDecoder->TakeProgress();
  mInvalidRect.UnionRect(mInvalidRect, mContainedDecoder->TakeInvalidRect());
  mCurrentFrame = mContainedDecoder->GetCurrentFrameRef();

  // Propagate errors.
  nsresult rv = HasError() || mContainedDecoder->HasError()
              ? NS_ERROR_FAILURE
              : NS_OK;

  MOZ_ASSERT(NS_FAILED(rv) || !mCurrentFrame || mCurrentFrame->IsFinished());
  return rv;
}

bool
nsICODecoder::CheckAndFixBitmapSize(int8_t* aBIH)
{
  // Get the width from the BMP file information header. This is
  // (unintuitively) a signed integer; see the documentation at:
  //
  //   https://msdn.microsoft.com/en-us/library/windows/desktop/dd183376(v=vs.85).aspx
  //
  // However, we reject negative widths since they aren't meaningful.
  const int32_t width = LittleEndian::readInt32(aBIH + 4);
  if (width <= 0 || width > 256) {
    return false;
  }

  // Verify that the BMP width matches the width we got from the ICO directory
  // entry. If not, decoding fails, because if we were to allow it to continue
  // the intrinsic size of the image wouldn't match the size of the decoded
  // surface.
  if (width != int32_t(GetRealWidth())) {
    return false;
  }

  // Get the height from the BMP file information header. This is also signed,
  // but in this case negative values are meaningful; see below.
  int32_t height = LittleEndian::readInt32(aBIH + 8);
  if (height == 0) {
    return false;
  }

  // BMPs can be stored inverted by having a negative height.
  // XXX(seth): Should we really be writing the absolute value into the BIH
  // below? Seems like this could be problematic for inverted BMPs.
  height = abs(height);

  // The height field is double the actual height of the image to account for
  // the AND mask. This is true even if the AND mask is not present.
  height /= 2;
  if (height > 256) {
    return false;
  }

  // Verify that the BMP height matches the height we got from the ICO directory
  // entry. If not, again, decoding fails.
  if (height != int32_t(GetRealHeight())) {
    return false;
  }

  // Fix the BMP height in the BIH so that the BMP decoder, which does not know
  // about the AND mask that may follow the actual bitmap, can work properly.
  LittleEndian::writeInt32(aBIH + 8, GetRealHeight());

  return true;
}

LexerTransition<ICOState>
nsICODecoder::ReadHeader(const char* aData)
{
  // If the third byte is 1, this is an icon. If 2, a cursor.
  if ((aData[2] != 1) && (aData[2] != 2)) {
    return Transition::TerminateFailure();
  }
  mIsCursor = (aData[2] == 2);

  // The fifth and sixth bytes specify the number of resources in the file.
  mNumIcons = LittleEndian::readUint16(aData + 4);
  if (mNumIcons == 0) {
    return Transition::TerminateSuccess(); // Nothing to do.
  }

  // Downscale-during-decode can end up decoding different resources in the ICO
  // file depending on the target size. Since the resources are not necessarily
  // scaled versions of the same image, some may be transparent and some may not
  // be. We could be precise about transparency if we decoded the metadata of
  // every resource, but for now we don't and it's safest to assume that
  // transparency could be present.
  PostHasTransparency();

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
  e.mWidth       = aData[0];
  e.mHeight      = aData[1];
  e.mColorCount  = aData[2];
  e.mReserved    = aData[3];
  e.mPlanes      = LittleEndian::readUint16(aData + 4);
  e.mBitCount    = LittleEndian::readUint16(aData + 6);
  e.mBytesInRes  = LittleEndian::readUint32(aData + 8);
  e.mImageOffset = LittleEndian::readUint32(aData + 12);

  // If an explicit output size was specified, we'll try to select the resource
  // that matches it best below.
  const Maybe<IntSize> desiredSize = ExplicitOutputSize();

  // Determine if this is the biggest resource we've seen so far. We always use
  // the biggest resource for the intrinsic size, and if we don't have a
  // specific desired size, we select it as the best resource as well.
  IntSize entrySize(GetRealWidth(e), GetRealHeight(e));
  if (e.mBitCount >= mBiggestResourceColorDepth &&
      entrySize.width * entrySize.height >=
        mBiggestResourceSize.width * mBiggestResourceSize.height) {
    mBiggestResourceSize = entrySize;
    mBiggestResourceColorDepth = e.mBitCount;
    mBiggestResourceHotSpot = IntSize(e.mXHotspot, e.mYHotspot);

    if (!desiredSize) {
      mDirEntry = e;
    }
  }

  if (desiredSize) {
    // Calculate the delta between this resource's size and the desired size, so
    // we can see if it is better than our current-best option.  In the case of
    // several equally-good resources, we use the last one. "Better" in this
    // case is determined by |delta|, a measure of the difference in size
    // between the entry we've found and the desired size. We will choose the
    // smallest resource that is greater than or equal to the desired size (i.e.
    // we assume it's better to downscale a larger icon than to upscale a
    // smaller one).
    int32_t delta = std::min(entrySize.width - desiredSize->width,
                             entrySize.height - desiredSize->height);
    if (e.mBitCount >= mBestResourceColorDepth &&
        ((mBestResourceDelta < 0 && delta >= mBestResourceDelta) ||
         (delta >= 0 && delta <= mBestResourceDelta))) {
      mBestResourceDelta = delta;
      mBestResourceColorDepth = e.mBitCount;
      mDirEntry = e;
    }
  }

  if (mCurrIcon == mNumIcons) {
    // Ensure the resource we selected has an offset past the ICO headers.
    if (mDirEntry.mImageOffset < FirstResourceOffset()) {
      return Transition::TerminateFailure();
    }

    // If this is a cursor, set the hotspot. We use the hotspot from the biggest
    // resource since we also use that resource for the intrinsic size.
    if (mIsCursor) {
      mImageMetadata.SetHotspot(mBiggestResourceHotSpot.width,
                                mBiggestResourceHotSpot.height);
    }

    // We always report the biggest resource's size as the intrinsic size; this
    // is necessary for downscale-during-decode to work since we won't even
    // attempt to *upscale* while decoding.
    PostSize(mBiggestResourceSize.width, mBiggestResourceSize.height);
    if (IsMetadataDecode()) {
      return Transition::TerminateSuccess();
    }

    // If the resource we selected matches the output size perfectly, we don't
    // need to do any downscaling.
    if (GetRealSize() == OutputSize()) {
      MOZ_ASSERT_IF(desiredSize, GetRealSize() == *desiredSize);
      MOZ_ASSERT_IF(!desiredSize, GetRealSize() == Size());
      mDownscaler.reset();
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
    mContainedSourceBuffer = new SourceBuffer();
    mContainedSourceBuffer->ExpectLength(mDirEntry.mBytesInRes);
    mContainedDecoder =
      DecoderFactory::CreateDecoderForICOResource(DecoderType::PNG,
                                                  WrapNotNull(mContainedSourceBuffer),
                                                  WrapNotNull(this));

    if (!WriteToContainedDecoder(aData, PNGSIGNATURESIZE)) {
      return Transition::TerminateFailure();
    }

    if (mDirEntry.mBytesInRes <= PNGSIGNATURESIZE) {
      return Transition::TerminateFailure();
    }

    // Read in the rest of the PNG unbuffered.
    size_t toRead = mDirEntry.mBytesInRes - PNGSIGNATURESIZE;
    return Transition::ToUnbuffered(ICOState::FINISHED_RESOURCE,
                                    ICOState::READ_PNG,
                                    toRead);
  } else {
    // Make sure we have a sane size for the bitmap information header.
    int32_t bihSize = LittleEndian::readUint32(aData);
    if (bihSize != static_cast<int32_t>(BITMAPINFOSIZE)) {
      return Transition::TerminateFailure();
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
    return Transition::TerminateFailure();
  }

  // Raymond Chen says that 32bpp only are valid PNG ICOs
  // http://blogs.msdn.com/b/oldnewthing/archive/2010/10/22/10079192.aspx
  if (!static_cast<nsPNGDecoder*>(mContainedDecoder.get())->IsValidICO()) {
    return Transition::TerminateFailure();
  }

  return Transition::ContinueUnbuffered(ICOState::READ_PNG);
}

LexerTransition<ICOState>
nsICODecoder::ReadBIH(const char* aData)
{
  // Buffer the rest of the bitmap information header.
  memcpy(mBIHraw + PNGSIGNATURESIZE, aData, BITMAPINFOSIZE - PNGSIGNATURESIZE);

  // Extract the BPP from the BIH header; it should be trusted over the one
  // we have from the ICO header which is usually set to 0.
  mBPP = LittleEndian::readUint16(mBIHraw + 14);

  // The ICO format when containing a BMP does not include the 14 byte
  // bitmap file header. So we create the BMP decoder via the constructor that
  // tells it to skip this, and pass in the required data (dataOffset) that
  // would have been present in the header.
  uint32_t dataOffset = bmp::FILE_HEADER_LENGTH + BITMAPINFOSIZE;
  if (mDirEntry.mBitCount <= 8) {
    // The color table is present only if BPP is <= 8.
    uint16_t numColors = GetNumColors();
    if (numColors == (uint16_t)-1) {
      return Transition::TerminateFailure();
    }
    dataOffset += 4 * numColors;
  }

  // Create a BMP decoder which will do most of the work for us; the exception
  // is the AND mask, which isn't present in standalone BMPs.
  mContainedSourceBuffer = new SourceBuffer();
  mContainedSourceBuffer->ExpectLength(mDirEntry.mBytesInRes);
  mContainedDecoder =
    DecoderFactory::CreateDecoderForICOResource(DecoderType::BMP,
                                                WrapNotNull(mContainedSourceBuffer),
                                                WrapNotNull(this),
                                                Some(dataOffset));
  RefPtr<nsBMPDecoder> bmpDecoder =
    static_cast<nsBMPDecoder*>(mContainedDecoder.get());

  // Verify that the BIH width and height values match the ICO directory entry,
  // and fix the BIH height value to compensate for the fact that the underlying
  // BMP decoder doesn't know about AND masks.
  if (!CheckAndFixBitmapSize(reinterpret_cast<int8_t*>(mBIHraw))) {
    return Transition::TerminateFailure();
  }

  // Write out the BMP's bitmap info header.
  if (!WriteToContainedDecoder(mBIHraw, sizeof(mBIHraw))) {
    return Transition::TerminateFailure();
  }

  // Check to make sure we have valid color settings.
  uint16_t numColors = GetNumColors();
  if (numColors == uint16_t(-1)) {
    return Transition::TerminateFailure();
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
    return Transition::TerminateFailure();
  }

  return Transition::ContinueUnbuffered(ICOState::READ_BMP);
}

LexerTransition<ICOState>
nsICODecoder::PrepareForMask()
{
  RefPtr<nsBMPDecoder> bmpDecoder =
    static_cast<nsBMPDecoder*>(mContainedDecoder.get());

  uint16_t numColors = GetNumColors();
  MOZ_ASSERT(numColors != uint16_t(-1));

  // Determine the length of the AND mask.
  uint32_t bmpLengthWithHeader =
    BITMAPINFOSIZE + bmpDecoder->GetCompressedImageSize() + 4 * numColors;
  MOZ_ASSERT(bmpLengthWithHeader < mDirEntry.mBytesInRes);
  uint32_t maskLength = mDirEntry.mBytesInRes - bmpLengthWithHeader;

  // If the BMP provides its own transparency, we ignore the AND mask. We can
  // also obviously ignore it if the image has zero width or zero height.
  if (bmpDecoder->HasTransparency() ||
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
    return Transition::TerminateFailure();
  }

  // If we're downscaling, the mask is the wrong size for the surface we've
  // produced, so we need to downscale the mask into a temporary buffer and then
  // combine the mask's alpha values with the color values from the image.
  if (mDownscaler) {
    MOZ_ASSERT(bmpDecoder->GetImageDataLength() ==
                 mDownscaler->TargetSize().width *
                 mDownscaler->TargetSize().height *
                 sizeof(uint32_t));
    mMaskBuffer = MakeUnique<uint8_t[]>(bmpDecoder->GetImageDataLength());
    nsresult rv = mDownscaler->BeginFrame(GetRealSize(), Nothing(),
                                          mMaskBuffer.get(),
                                          /* aHasAlpha = */ true,
                                          /* aFlipVertically = */ true);
    if (NS_FAILED(rv)) {
      return Transition::TerminateFailure();
    }
  }

  mCurrMaskLine = GetRealHeight();
  return Transition::To(ICOState::READ_MASK_ROW, mMaskRowSize);
}


LexerTransition<ICOState>
nsICODecoder::ReadMaskRow(const char* aData)
{
  mCurrMaskLine--;

  uint8_t sawTransparency = 0;

  // Get the mask row we're reading.
  const uint8_t* mask = reinterpret_cast<const uint8_t*>(aData);
  const uint8_t* maskRowEnd = mask + mMaskRowSize;

  // Get the corresponding row of the mask buffer (if we're downscaling) or the
  // decoded image data (if we're not).
  uint32_t* decoded = nullptr;
  if (mDownscaler) {
    // Initialize the row to all white and fully opaque.
    memset(mDownscaler->RowBuffer(), 0xFF, GetRealWidth() * sizeof(uint32_t));

    decoded = reinterpret_cast<uint32_t*>(mDownscaler->RowBuffer());
  } else {
    RefPtr<nsBMPDecoder> bmpDecoder =
      static_cast<nsBMPDecoder*>(mContainedDecoder.get());
    uint32_t* imageData = bmpDecoder->GetImageData();
    if (!imageData) {
      return Transition::TerminateFailure();
    }

    decoded = imageData + mCurrMaskLine * GetRealWidth();
  }

  MOZ_ASSERT(decoded);
  uint32_t* decodedRowEnd = decoded + GetRealWidth();

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

  if (mDownscaler) {
    mDownscaler->CommitRow();
  }

  // If any bits are set in sawTransparency, then we know at least one pixel was
  // transparent.
  if (sawTransparency) {
    mHasMaskAlpha = true;
  }

  if (mCurrMaskLine == 0) {
    return Transition::To(ICOState::FINISH_MASK, 0);
  }

  return Transition::To(ICOState::READ_MASK_ROW, mMaskRowSize);
}

LexerTransition<ICOState>
nsICODecoder::FinishMask()
{
  // If we're downscaling, we now have the appropriate alpha values in
  // mMaskBuffer. We just need to transfer them to the image.
  if (mDownscaler) {
    // Retrieve the image data.
    RefPtr<nsBMPDecoder> bmpDecoder =
      static_cast<nsBMPDecoder*>(mContainedDecoder.get());
    uint8_t* imageData = reinterpret_cast<uint8_t*>(bmpDecoder->GetImageData());
    if (!imageData) {
      return Transition::TerminateFailure();
    }

    // Iterate through the alpha values, copying from mask to image.
    MOZ_ASSERT(mMaskBuffer);
    MOZ_ASSERT(bmpDecoder->GetImageDataLength() > 0);
    for (size_t i = 3 ; i < bmpDecoder->GetImageDataLength() ; i += 4) {
      imageData[i] = mMaskBuffer[i];
    }
  }

  return Transition::To(ICOState::FINISHED_RESOURCE, 0);
}

LexerTransition<ICOState>
nsICODecoder::FinishResource()
{
  // Make sure the actual size of the resource matches the size in the directory
  // entry. If not, we consider the image corrupt.
  if (mContainedDecoder->HasSize() &&
      mContainedDecoder->Size() != GetRealSize()) {
    return Transition::TerminateFailure();
  }

  return Transition::TerminateSuccess();
}

LexerResult
nsICODecoder::DoDecode(SourceBufferIterator& aIterator, IResumable* aOnResume)
{
  MOZ_ASSERT(!HasError(), "Shouldn't call DoDecode after error!");

  return mLexer.Lex(aIterator, aOnResume,
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
      case ICOState::FINISH_MASK:
        return FinishMask();
      case ICOState::SKIP_MASK:
        return Transition::ContinueUnbuffered(ICOState::SKIP_MASK);
      case ICOState::FINISHED_RESOURCE:
        return FinishResource();
      default:
        MOZ_CRASH("Unknown ICOState");
    }
  });
}

bool
nsICODecoder::WriteToContainedDecoder(const char* aBuffer, uint32_t aCount)
{
  MOZ_ASSERT(mContainedDecoder);
  MOZ_ASSERT(mContainedSourceBuffer);

  // Append the provided data to the SourceBuffer that the contained decoder is
  // reading from.
  mContainedSourceBuffer->Append(aBuffer, aCount);

  bool succeeded = true;

  // Write to the contained decoder. If we run out of data, the ICO decoder will
  // get resumed when there's more data available, as usual, so we don't need
  // the contained decoder to get resumed too. To avoid that, we provide an
  // IResumable which just does nothing.
  LexerResult result = mContainedDecoder->Decode();
  if (result == LexerResult(TerminalState::FAILURE)) {
    succeeded = false;
  }

  MOZ_ASSERT(result != LexerResult(Yield::OUTPUT_AVAILABLE),
             "Unexpected yield");

  // Make our state the same as the state of the contained decoder, and
  // propagate errors.
  mProgress |= mContainedDecoder->TakeProgress();
  mInvalidRect.UnionRect(mInvalidRect, mContainedDecoder->TakeInvalidRect());
  if (mContainedDecoder->HasError()) {
    succeeded = false;
  }

  return succeeded;
}

} // namespace image
} // namespace mozilla
