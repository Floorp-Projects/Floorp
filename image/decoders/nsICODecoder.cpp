/* vim:set tw=80 expandtab softtabstop=4 ts=4 sw=4: */
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

#define ICONCOUNTOFFSET 4
#define DIRENTRYOFFSET 6
#define BITMAPINFOSIZE 40
#define PREFICONSIZE 16

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
{
  mPos = mImageOffset = mCurrIcon = mNumIcons = mBPP = mRowBytes = 0;
  mIsPNG = false;
  mRow = nullptr;
  mOldLine = mCurLine = 1; // Otherwise decoder will never start
}

nsICODecoder::~nsICODecoder()
{
  if (mRow) {
    moz_free(mRow);
  }
}

void
nsICODecoder::FinishInternal()
{
  // We shouldn't be called in error cases
  MOZ_ASSERT(!HasError(), "Shouldn't call FinishInternal after error!");

  // Finish the internally used decoder as well
  if (mContainedDecoder) {
    mContainedDecoder->FinishSharedDecoder();
    mDecodeDone = mContainedDecoder->GetDecodeDone();
    mProgress |= mContainedDecoder->TakeProgress();
    mInvalidRect.Union(mContainedDecoder->TakeInvalidRect());
  }
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
  dataOffset = BFH_LENGTH + BITMAPINFOSIZE;

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
// more than what we have.  Usually the ICO's BPP is set to 0
int32_t
nsICODecoder::ExtractBPPFromBitmap(int8_t* bih)
{
  int32_t bitsPerPixel;
  memcpy(&bitsPerPixel, bih + 14, sizeof(bitsPerPixel));
  NativeEndian::swapFromLittleEndianInPlace(&bitsPerPixel, 1);
  return bitsPerPixel;
}

int32_t
nsICODecoder::ExtractBIHSizeFromBitmap(int8_t* bih)
{
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

void
nsICODecoder::WriteInternal(const char* aBuffer, uint32_t aCount)
{
  MOZ_ASSERT(!HasError(), "Shouldn't call WriteInternal after error!");

  if (!aCount) {
    if (mContainedDecoder) {
      WriteToContainedDecoder(aBuffer, aCount);
    }
    return;
  }

  while (aCount && (mPos < ICONCOUNTOFFSET)) { // Skip to the # of icons.
    if (mPos == 2) { // if the third byte is 1: This is an icon, 2: a cursor
      if ((*aBuffer != 1) && (*aBuffer != 2)) {
        PostDataError();
        return;
      }
      mIsCursor = (*aBuffer == 2);
    }
    mPos++; aBuffer++; aCount--;
  }

  if (mPos == ICONCOUNTOFFSET && aCount >= 2) {
    mNumIcons =
      LittleEndian::readUint16(reinterpret_cast<const uint16_t*>(aBuffer));
    aBuffer += 2;
    mPos += 2;
    aCount -= 2;
  }

  if (mNumIcons == 0) {
    return; // Nothing to do.
  }

  uint16_t colorDepth = 0;
  nsIntSize prefSize = mImage->GetRequestedResolution();
  if (prefSize.width == 0 && prefSize.height == 0) {
    prefSize.SizeTo(PREFICONSIZE, PREFICONSIZE);
  }

  // A measure of the difference in size between the entry we've found
  // and the requested size. We will choose the smallest image that is
  // >= requested size (i.e. we assume it's better to downscale a larger
  // icon than to upscale a smaller one).
  int32_t diff = INT_MIN;

  // Loop through each entry's dir entry
  while (mCurrIcon < mNumIcons) {
    if (mPos >= DIRENTRYOFFSET + (mCurrIcon * sizeof(mDirEntryArray)) &&
        mPos < DIRENTRYOFFSET + ((mCurrIcon + 1) * sizeof(mDirEntryArray))) {
      uint32_t toCopy = sizeof(mDirEntryArray) -
                        (mPos - DIRENTRYOFFSET - mCurrIcon *
                         sizeof(mDirEntryArray));
      if (toCopy > aCount) {
        toCopy = aCount;
      }
      memcpy(mDirEntryArray + sizeof(mDirEntryArray) - toCopy, aBuffer, toCopy);
      mPos += toCopy;
      aCount -= toCopy;
      aBuffer += toCopy;
    }
    if (aCount == 0) {
      return; // Need more data
    }

    IconDirEntry e;
    if (mPos == (DIRENTRYOFFSET + ICODIRENTRYSIZE) +
                (mCurrIcon * sizeof(mDirEntryArray))) {
      mCurrIcon++;
      ProcessDirEntry(e);
      // We can't use GetRealWidth and GetRealHeight here because those operate
      // on mDirEntry, here we are going through each item in the directory.
      // Calculate the delta between this image's size and the desired size,
      // so we can see if it is better than our current-best option.
      // In the case of several equally-good images, we use the last one.
      int32_t delta = (e.mWidth == 0 ? 256 : e.mWidth) - prefSize.width +
                      (e.mHeight == 0 ? 256 : e.mHeight) - prefSize.height;
      if (e.mBitCount >= colorDepth &&
          ((diff < 0 && delta >= diff) || (delta >= 0 && delta <= diff))) {
        diff = delta;
        mImageOffset = e.mImageOffset;

        // ensure mImageOffset is >= size of the direntry headers (bug #245631)
        uint32_t minImageOffset = DIRENTRYOFFSET +
                                  mNumIcons * sizeof(mDirEntryArray);
        if (mImageOffset < minImageOffset) {
          PostDataError();
          return;
        }

        colorDepth = e.mBitCount;
        memcpy(&mDirEntry, &e, sizeof(IconDirEntry));
      }
    }
  }

  if (mPos < mImageOffset) {
    // Skip to (or at least towards) the desired image offset
    uint32_t toSkip = mImageOffset - mPos;
    if (toSkip > aCount) {
      toSkip = aCount;
    }

    mPos    += toSkip;
    aBuffer += toSkip;
    aCount  -= toSkip;
  }

  // If we are within the first PNGSIGNATURESIZE bytes of the image data,
  // then we have either a BMP or a PNG.  We use the first PNGSIGNATURESIZE
  // bytes to determine which one we have.
  if (mCurrIcon == mNumIcons && mPos >= mImageOffset &&
      mPos < mImageOffset + PNGSIGNATURESIZE) {
    uint32_t toCopy = PNGSIGNATURESIZE - (mPos - mImageOffset);
    if (toCopy > aCount) {
      toCopy = aCount;
    }

    memcpy(mSignature + (mPos - mImageOffset), aBuffer, toCopy);
    mPos += toCopy;
    aCount -= toCopy;
    aBuffer += toCopy;

    mIsPNG = !memcmp(mSignature, nsPNGDecoder::pngSignatureBytes,
                     PNGSIGNATURESIZE);
    if (mIsPNG) {
      mContainedDecoder = new nsPNGDecoder(mImage);
      mContainedDecoder->SetSizeDecode(IsSizeDecode());
      mContainedDecoder->SetSendPartialInvalidations(mSendPartialInvalidations);
      mContainedDecoder->InitSharedDecoder(mImageData, mImageDataLength,
                                           mColormap, mColormapSize,
                                           Move(mRefForContainedDecoder));
      if (!WriteToContainedDecoder(mSignature, PNGSIGNATURESIZE)) {
        return;
      }
    }
  }

  // If we have a PNG, let the PNG decoder do all of the rest of the work
  if (mIsPNG && mContainedDecoder && mPos >= mImageOffset + PNGSIGNATURESIZE) {
    if (!WriteToContainedDecoder(aBuffer, aCount)) {
      return;
    }

    if (!HasSize() && mContainedDecoder->HasSize()) {
      PostSize(mContainedDecoder->GetImageMetadata().GetWidth(),
               mContainedDecoder->GetImageMetadata().GetHeight());
    }

    mPos += aCount;
    aBuffer += aCount;
    aCount = 0;

    // Raymond Chen says that 32bpp only are valid PNG ICOs
    // http://blogs.msdn.com/b/oldnewthing/archive/2010/10/22/10079192.aspx
    if (!IsSizeDecode() &&
        !static_cast<nsPNGDecoder*>(mContainedDecoder.get())->IsValidICO()) {
      PostDataError();
    }
    return;
  }

  // We've processed all of the icon dir entries and are within the
  // bitmap info size
  if (!mIsPNG && mCurrIcon == mNumIcons && mPos >= mImageOffset &&
      mPos >= mImageOffset + PNGSIGNATURESIZE &&
      mPos < mImageOffset + BITMAPINFOSIZE) {

    // As we were decoding, we did not know if we had a PNG signature or the
    // start of a bitmap information header.  At this point we know we had
    // a bitmap information header and not a PNG signature, so fill the bitmap
    // information header with the data it should already have.
    memcpy(mBIHraw, mSignature, PNGSIGNATURESIZE);

    // We've found the icon.
    uint32_t toCopy = sizeof(mBIHraw) - (mPos - mImageOffset);
    if (toCopy > aCount) {
      toCopy = aCount;
    }

    memcpy(mBIHraw + (mPos - mImageOffset), aBuffer, toCopy);
    mPos += toCopy;
    aCount -= toCopy;
    aBuffer += toCopy;
  }

  // If we have a BMP inside the ICO and we have read the BIH header
  if (!mIsPNG && mPos == mImageOffset + BITMAPINFOSIZE) {

    // Make sure we have a sane value for the bitmap information header
    int32_t bihSize = ExtractBIHSizeFromBitmap(reinterpret_cast<int8_t*>
                                               (mBIHraw));
    if (bihSize != BITMAPINFOSIZE) {
      PostDataError();
      return;
    }
    // We are extracting the BPP from the BIH header as it should be trusted
    // over the one we have from the icon header
    mBPP = ExtractBPPFromBitmap(reinterpret_cast<int8_t*>(mBIHraw));

    // Init the bitmap decoder which will do most of the work for us
    // It will do everything except the AND mask which isn't present in bitmaps
    // bmpDecoder is for local scope ease, it will be freed by mContainedDecoder
    nsBMPDecoder* bmpDecoder = new nsBMPDecoder(mImage);
    mContainedDecoder = bmpDecoder;
    bmpDecoder->SetUseAlphaData(true);
    mContainedDecoder->SetSizeDecode(IsSizeDecode());
    mContainedDecoder->SetSendPartialInvalidations(mSendPartialInvalidations);
    mContainedDecoder->InitSharedDecoder(mImageData, mImageDataLength,
                                         mColormap, mColormapSize,
                                         Move(mRefForContainedDecoder));

    // The ICO format when containing a BMP does not include the 14 byte
    // bitmap file header. To use the code of the BMP decoder we need to
    // generate this header ourselves and feed it to the BMP decoder.
    int8_t bfhBuffer[BMPFILEHEADERSIZE];
    if (!FillBitmapFileHeaderBuffer(bfhBuffer)) {
      PostDataError();
      return;
    }
    if (!WriteToContainedDecoder((const char*)bfhBuffer, sizeof(bfhBuffer))) {
      return;
    }

    // Setup the cursor hot spot if one is present
    SetHotSpotIfCursor();

    // Fix the ICO height from the BIH.
    // Fix the height on the BIH to be /2 so our BMP decoder will understand.
    if (!FixBitmapHeight(reinterpret_cast<int8_t*>(mBIHraw))) {
      PostDataError();
      return;
    }

    // Fix the ICO width from the BIH.
    if (!FixBitmapWidth(reinterpret_cast<int8_t*>(mBIHraw))) {
      PostDataError();
      return;
    }

    // Write out the BMP's bitmap info header
    if (!WriteToContainedDecoder(mBIHraw, sizeof(mBIHraw))) {
      return;
    }

    PostSize(mContainedDecoder->GetImageMetadata().GetWidth(),
             mContainedDecoder->GetImageMetadata().GetHeight());

    // We have the size. If we're doing a size decode, we got what
    // we came for.
    if (IsSizeDecode()) {
      return;
    }

    // Sometimes the ICO BPP header field is not filled out
    // so we should trust the contained resource over our own
    // information.
    mBPP = bmpDecoder->GetBitsPerPixel();

    // Check to make sure we have valid color settings
    uint16_t numColors = GetNumColors();
    if (numColors == (uint16_t)-1) {
      PostDataError();
      return;
    }
  }

  // If we have a BMP
  if (!mIsPNG && mContainedDecoder && mPos >= mImageOffset + BITMAPINFOSIZE) {
    uint16_t numColors = GetNumColors();
    if (numColors == (uint16_t)-1) {
      PostDataError();
      return;
    }
    // Feed the actual image data (not including headers) into the BMP decoder
    uint32_t bmpDataOffset = mDirEntry.mImageOffset + BITMAPINFOSIZE;
    uint32_t bmpDataEnd = mDirEntry.mImageOffset + BITMAPINFOSIZE +
                          static_cast<nsBMPDecoder*>(mContainedDecoder.get())->
                            GetCompressedImageSize() +
                          4 * numColors;

    // If we are feeding in the core image data, but we have not yet
    // reached the ICO's 'AND buffer mask'
    if (mPos >= bmpDataOffset && mPos < bmpDataEnd) {

      // Figure out how much data the BMP decoder wants
      uint32_t toFeed = bmpDataEnd - mPos;
      if (toFeed > aCount) {
        toFeed = aCount;
      }

      if (!WriteToContainedDecoder(aBuffer, toFeed)) {
        return;
      }

      mPos += toFeed;
      aCount -= toFeed;
      aBuffer += toFeed;
    }

    // If the bitmap is fully processed, treat any left over data as the ICO's
    // 'AND buffer mask' which appears after the bitmap resource.
    if (!mIsPNG && mPos >= bmpDataEnd) {
      // There may be an optional AND bit mask after the data.  This is
      // only used if the alpha data is not already set. The alpha data
      // is used for 32bpp bitmaps as per the comment in ICODecoder.h
      // The alpha mask should be checked in all other cases.
      if (static_cast<nsBMPDecoder*>(mContainedDecoder.get())->
            GetBitsPerPixel() != 32 ||
          !static_cast<nsBMPDecoder*>(mContainedDecoder.get())->
            HasAlphaData()) {
        uint32_t rowSize = ((GetRealWidth() + 31) / 32) * 4; // + 31 to round up
        if (mPos == bmpDataEnd) {
          mPos++;
          mRowBytes = 0;
          mCurLine = GetRealHeight();
          mRow = (uint8_t*)moz_realloc(mRow, rowSize);
          if (!mRow) {
            PostDecoderError(NS_ERROR_OUT_OF_MEMORY);
            return;
          }
        }

        // Ensure memory has been allocated before decoding.
        MOZ_ASSERT(mRow, "mRow is null");
        if (!mRow) {
          PostDataError();
          return;
        }

        uint8_t sawTransparency = 0;

        while (mCurLine > 0 && aCount > 0) {
          uint32_t toCopy = std::min(rowSize - mRowBytes, aCount);
          if (toCopy) {
            memcpy(mRow + mRowBytes, aBuffer, toCopy);
            aCount -= toCopy;
            aBuffer += toCopy;
            mRowBytes += toCopy;
          }
          if (rowSize == mRowBytes) {
            mCurLine--;
            mRowBytes = 0;

            uint32_t* imageData =
              static_cast<nsBMPDecoder*>(mContainedDecoder.get())->
                                           GetImageData();
            if (!imageData) {
              PostDataError();
              return;
            }
            uint32_t* decoded = imageData + mCurLine * GetRealWidth();
            uint32_t* decoded_end = decoded + GetRealWidth();
            uint8_t* p = mRow;
            uint8_t* p_end = mRow + rowSize;
            while (p < p_end) {
              uint8_t idx = *p++;
              sawTransparency |= idx;
              for (uint8_t bit = 0x80; bit && decoded<decoded_end; bit >>= 1) {
                // Clear pixel completely for transparency.
                if (idx & bit) {
                  *decoded = 0;
                }
                decoded++;
              }
            }
          }
        }

        // If any bits are set in sawTransparency, then we know at least one
        // pixel was transparent.
        if (sawTransparency) {
            PostHasTransparency();
        }
      }
    }
  }
}

bool
nsICODecoder::WriteToContainedDecoder(const char* aBuffer, uint32_t aCount)
{
  mContainedDecoder->Write(aBuffer, aCount);
  mProgress |= mContainedDecoder->TakeProgress();
  mInvalidRect.Union(mContainedDecoder->TakeInvalidRect());
  if (mContainedDecoder->HasDataError()) {
    mDataError = mContainedDecoder->HasDataError();
  }
  if (mContainedDecoder->HasDecoderError()) {
    PostDecoderError(mContainedDecoder->GetDecoderError());
  }
  return !HasError();
}

void
nsICODecoder::ProcessDirEntry(IconDirEntry& aTarget)
{
  memset(&aTarget, 0, sizeof(aTarget));
  memcpy(&aTarget.mWidth, mDirEntryArray, sizeof(aTarget.mWidth));
  memcpy(&aTarget.mHeight, mDirEntryArray + 1, sizeof(aTarget.mHeight));
  memcpy(&aTarget.mColorCount, mDirEntryArray + 2, sizeof(aTarget.mColorCount));
  memcpy(&aTarget.mReserved, mDirEntryArray + 3, sizeof(aTarget.mReserved));
  memcpy(&aTarget.mPlanes, mDirEntryArray + 4, sizeof(aTarget.mPlanes));
  aTarget.mPlanes = LittleEndian::readUint16(&aTarget.mPlanes);
  memcpy(&aTarget.mBitCount, mDirEntryArray + 6, sizeof(aTarget.mBitCount));
  aTarget.mBitCount = LittleEndian::readUint16(&aTarget.mBitCount);
  memcpy(&aTarget.mBytesInRes, mDirEntryArray + 8, sizeof(aTarget.mBytesInRes));
  aTarget.mBytesInRes = LittleEndian::readUint32(&aTarget.mBytesInRes);
  memcpy(&aTarget.mImageOffset, mDirEntryArray + 12,
         sizeof(aTarget.mImageOffset));
  aTarget.mImageOffset = LittleEndian::readUint32(&aTarget.mImageOffset);
}

bool
nsICODecoder::NeedsNewFrame() const
{
  if (mContainedDecoder) {
    return mContainedDecoder->NeedsNewFrame();
  }

  return Decoder::NeedsNewFrame();
}

nsresult
nsICODecoder::AllocateFrame(const nsIntSize& aTargetSize /* = nsIntSize() */)
{
  nsresult rv;

  if (mContainedDecoder) {
    rv = mContainedDecoder->AllocateFrame(aTargetSize);
    mCurrentFrame = mContainedDecoder->GetCurrentFrameRef();
    mProgress |= mContainedDecoder->TakeProgress();
    mInvalidRect.Union(mContainedDecoder->TakeInvalidRect());
    return rv;
  }

  // Grab a strong ref that we'll later hand over to the contained decoder. This
  // lets us avoid creating a RawAccessFrameRef off-main-thread.
  rv = Decoder::AllocateFrame(aTargetSize);
  mRefForContainedDecoder = GetCurrentFrameRef();
  return rv;
}

} // namespace image
} // namespace mozilla
