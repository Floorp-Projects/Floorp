/* vim:set tw=80 expandtab softtabstop=4 ts=4 sw=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef mozilla_image_decoders_nsICODecoder_h
#define mozilla_image_decoders_nsICODecoder_h

#include "nsAutoPtr.h"
#include "StreamingLexer.h"
#include "Decoder.h"
#include "imgFrame.h"
#include "nsBMPDecoder.h"
#include "nsPNGDecoder.h"
#include "ICOFileHeaders.h"

namespace mozilla {
namespace image {

class RasterImage;

enum class ICOState
{
  SUCCESS,
  FAILURE,
  HEADER,
  DIR_ENTRY,
  SKIP_TO_RESOURCE,
  FOUND_RESOURCE,
  SNIFF_RESOURCE,
  READ_PNG,
  READ_BIH,
  READ_BMP,
  PREPARE_FOR_MASK,
  READ_MASK_ROW,
  SKIP_MASK,
  FINISHED_RESOURCE
};

class nsICODecoder : public Decoder
{
public:
  virtual ~nsICODecoder() { }

  nsresult SetTargetSize(const nsIntSize& aSize) override;

  /// @return the width of the icon directory entry @aEntry.
  static uint32_t GetRealWidth(const IconDirEntry& aEntry)
  {
    return aEntry.mWidth == 0 ? 256 : aEntry.mWidth;
  }

  /// @return the width of the selected directory entry (mDirEntry).
  uint32_t GetRealWidth() const { return GetRealWidth(mDirEntry); }

  /// @return the height of the icon directory entry @aEntry.
  static uint32_t GetRealHeight(const IconDirEntry& aEntry)
  {
    return aEntry.mHeight == 0 ? 256 : aEntry.mHeight;
  }

  /// @return the height of the selected directory entry (mDirEntry).
  uint32_t GetRealHeight() const { return GetRealHeight(mDirEntry); }

  /// @return the size of the selected directory entry (mDirEntry).
  gfx::IntSize GetRealSize() const
  {
    return gfx::IntSize(GetRealWidth(), GetRealHeight());
  }

  /// @return The offset from the beginning of the ICO to the first resource.
  size_t FirstResourceOffset() const;

  virtual void WriteInternal(const char* aBuffer, uint32_t aCount) override;
  virtual void FinishInternal() override;
  virtual void FinishWithErrorInternal() override;

private:
  friend class DecoderFactory;

  // Decoders should only be instantiated via DecoderFactory.
  explicit nsICODecoder(RasterImage* aImage);

  // Writes to the contained decoder and sets the appropriate errors
  // Returns true if there are no errors.
  bool WriteToContainedDecoder(const char* aBuffer, uint32_t aCount);

  // Gets decoder state from the contained decoder so it's visible externally.
  void GetFinalStateFromContainedDecoder();

  // Creates a bitmap file header buffer, returns true if successful
  bool FillBitmapFileHeaderBuffer(int8_t* bfh);
  // Fixes the ICO height to match that of the BIH.
  // and also fixes the BIH height to be /2 of what it was.
  // See definition for explanation.
  // Returns false if invalid information is contained within.
  bool FixBitmapHeight(int8_t* bih);
  // Fixes the ICO width to match that of the BIH.
  // Returns false if invalid information is contained within.
  bool FixBitmapWidth(int8_t* bih);
  // Extract bitmap info header size count from BMP information header
  int32_t ReadBIHSize(const char* aBIH);
  // Extract bit count from BMP information header
  int32_t ReadBPP(const char* aBIH);
  // Calculates the row size in bytes for the AND mask table
  uint32_t CalcAlphaRowSize();
  // Obtains the number of colors from the BPP, mBPP must be filled in
  uint16_t GetNumColors();

  LexerTransition<ICOState> ReadHeader(const char* aData);
  LexerTransition<ICOState> ReadDirEntry(const char* aData);
  LexerTransition<ICOState> SniffResource(const char* aData);
  LexerTransition<ICOState> ReadPNG(const char* aData, uint32_t aLen);
  LexerTransition<ICOState> ReadBIH(const char* aData);
  LexerTransition<ICOState> ReadBMP(const char* aData, uint32_t aLen);
  LexerTransition<ICOState> PrepareForMask();
  LexerTransition<ICOState> ReadMaskRow(const char* aData);
  LexerTransition<ICOState> FinishResource();

  StreamingLexer<ICOState, 32> mLexer; // The lexer.
  Maybe<Downscaler> mDownscaler;       // Our downscaler, if we're downscaling.
  nsRefPtr<Decoder> mContainedDecoder; // Either a BMP or PNG decoder.
  char mBIHraw[40];                    // The bitmap information header.
  IconDirEntry mDirEntry;              // The dir entry for the selected resource.
  IntSize mBiggestResourceSize;        // Used to select the intrinsic size.
  IntSize mBiggestResourceHotSpot;     // Used to select the intrinsic size.
  uint16_t mBiggestResourceColorDepth; // Used to select the intrinsic size.
  int32_t mBestResourceDelta;          // Used to select the best resource.
  uint16_t mBestResourceColorDepth;    // Used to select the best resource.
  uint16_t mNumIcons; // Stores the number of icons in the ICO file.
  uint16_t mCurrIcon; // Stores the current dir entry index we are processing.
  uint16_t mBPP;      // The BPP of the resource we're decoding.
  uint32_t mMaskRowSize;  // The size in bytes of each row in the BMP alpha mask.
  uint32_t mCurrMaskLine; // The line of the BMP alpha mask we're processing.
  bool mIsCursor;         // Is this ICO a cursor?
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_decoders_nsICODecoder_h
