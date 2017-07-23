/* vim:set tw=80 expandtab softtabstop=4 ts=4 sw=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef mozilla_image_decoders_nsICODecoder_h
#define mozilla_image_decoders_nsICODecoder_h

#include "StreamingLexer.h"
#include "Decoder.h"
#include "imgFrame.h"
#include "mozilla/gfx/2D.h"
#include "nsBMPDecoder.h"
#include "nsPNGDecoder.h"
#include "ICOFileHeaders.h"

namespace mozilla {
namespace image {

class RasterImage;

enum class ICOState
{
  HEADER,
  DIR_ENTRY,
  FINISHED_DIR_ENTRY,
  ITERATE_UNSIZED_DIR_ENTRY,
  SKIP_TO_RESOURCE,
  FOUND_RESOURCE,
  SNIFF_RESOURCE,
  READ_RESOURCE,
  PREPARE_FOR_MASK,
  READ_MASK_ROW,
  FINISH_MASK,
  SKIP_MASK,
  FINISHED_RESOURCE
};

class nsICODecoder : public Decoder
{
public:
  virtual ~nsICODecoder() { }

  /// @return The offset from the beginning of the ICO to the first resource.
  size_t FirstResourceOffset() const;

  LexerResult DoDecode(SourceBufferIterator& aIterator,
                       IResumable* aOnResume) override;
  nsresult FinishInternal() override;
  nsresult FinishWithErrorInternal() override;

private:
  friend class DecoderFactory;

  // Decoders should only be instantiated via DecoderFactory.
  explicit nsICODecoder(RasterImage* aImage);

  // Flushes the contained decoder to read all available data and sets the
  // appropriate errors. Returns true if there are no errors.
  bool FlushContainedDecoder();

  // Gets decoder state from the contained decoder so it's visible externally.
  nsresult GetFinalStateFromContainedDecoder();

  // Obtains the number of colors from the BPP, mBPP must be filled in
  uint16_t GetNumColors();

  LexerTransition<ICOState> ReadHeader(const char* aData);
  LexerTransition<ICOState> ReadDirEntry(const char* aData);
  LexerTransition<ICOState> IterateUnsizedDirEntry();
  LexerTransition<ICOState> FinishDirEntry();
  LexerTransition<ICOState> SniffResource(const char* aData);
  LexerTransition<ICOState> ReadResource();
  LexerTransition<ICOState> ReadBIH(const char* aData);
  LexerTransition<ICOState> PrepareForMask();
  LexerTransition<ICOState> ReadMaskRow(const char* aData);
  LexerTransition<ICOState> FinishMask();
  LexerTransition<ICOState> FinishResource();

  struct IconDirEntryEx : public IconDirEntry {
    gfx::IntSize mSize;
  };

  StreamingLexer<ICOState, 32> mLexer; // The lexer.
  RefPtr<Decoder> mContainedDecoder; // Either a BMP or PNG decoder.
  Maybe<SourceBufferIterator> mReturnIterator; // Iterator to save return point.
  UniquePtr<uint8_t[]> mMaskBuffer; // A temporary buffer for the alpha mask.
  nsTArray<IconDirEntryEx> mDirEntries; // Valid dir entries with a size.
  nsTArray<IconDirEntryEx> mUnsizedDirEntries; // Dir entries without a size.
  IconDirEntryEx* mDirEntry; // The dir entry for the selected resource.
  uint16_t mNumIcons;     // Stores the number of icons in the ICO file.
  uint16_t mCurrIcon;     // Stores the current dir entry index we are processing.
  uint16_t mBPP;          // The BPP of the resource we're decoding.
  uint32_t mMaskRowSize;  // The size in bytes of each row in the BMP alpha mask.
  uint32_t mCurrMaskLine; // The line of the BMP alpha mask we're processing.
  bool mIsCursor;         // Is this ICO a cursor?
  bool mHasMaskAlpha;     // Did the BMP alpha mask have any transparency?
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_decoders_nsICODecoder_h
