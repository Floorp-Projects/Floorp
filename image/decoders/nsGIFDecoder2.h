/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_nsGIFDecoder2_h
#define mozilla_image_decoders_nsGIFDecoder2_h

#include "Decoder.h"
#include "GIF2.h"
#include "StreamingLexer.h"
#include "SurfacePipe.h"

namespace mozilla {
namespace image {
class RasterImage;

//////////////////////////////////////////////////////////////////////
// nsGIFDecoder2 Definition

class nsGIFDecoder2 : public Decoder
{
public:
  ~nsGIFDecoder2();

  LexerResult DoDecode(SourceBufferIterator& aIterator,
                       IResumable* aOnResume) override;
  nsresult FinishInternal() override;
  virtual Telemetry::ID SpeedHistogram() override;

private:
  friend class DecoderFactory;

  // Decoders should only be instantiated via DecoderFactory.
  explicit nsGIFDecoder2(RasterImage* aImage);

  /// Called when we begin decoding the image.
  void      BeginGIF();

  /**
   * Called when we begin decoding a frame.
   *
   * @param aFrameRect The region of the image that contains data. The region
   *                   outside this rect is transparent.
   * @param aDepth The palette depth of this frame.
   * @param aIsInterlaced If true, this frame is an interlaced frame.
   */
  nsresult  BeginImageFrame(const gfx::IntRect& aFrameRect,
                            uint16_t aDepth,
                            bool aIsInterlaced);

  /// Called when we finish decoding a frame.
  void      EndImageFrame();

  /// Called when we finish decoding the entire image.
  void      FlushImageData();

  /// Transforms a palette index into a pixel.
  template <typename PixelSize> PixelSize
  ColormapIndexToPixel(uint8_t aIndex);

  /// A generator function that performs LZW decompression and yields pixels.
  template <typename PixelSize> NextPixel<PixelSize>
  YieldPixel(const uint8_t* aData, size_t aLength, size_t* aBytesReadOut);

  /// Checks if we have transparency, either because the header indicates that
  /// there's alpha, or because the frame rect doesn't cover the entire image.
  bool CheckForTransparency(const gfx::IntRect& aFrameRect);

  // @return the clear code used for LZW decompression.
  int ClearCode() const { return 1 << mGIFStruct.datasize; }

  enum class State
  {
    FAILURE,
    SUCCESS,
    GIF_HEADER,
    SCREEN_DESCRIPTOR,
    GLOBAL_COLOR_TABLE,
    FINISHED_GLOBAL_COLOR_TABLE,
    BLOCK_HEADER,
    EXTENSION_HEADER,
    GRAPHIC_CONTROL_EXTENSION,
    APPLICATION_IDENTIFIER,
    NETSCAPE_EXTENSION_SUB_BLOCK,
    NETSCAPE_EXTENSION_DATA,
    IMAGE_DESCRIPTOR,
    FINISH_IMAGE_DESCRIPTOR,
    LOCAL_COLOR_TABLE,
    FINISHED_LOCAL_COLOR_TABLE,
    IMAGE_DATA_BLOCK,
    IMAGE_DATA_SUB_BLOCK,
    LZW_DATA,
    SKIP_LZW_DATA,
    FINISHED_LZW_DATA,
    SKIP_SUB_BLOCKS,
    SKIP_DATA_THEN_SKIP_SUB_BLOCKS,
    FINISHED_SKIPPING_DATA
  };

  LexerTransition<State> ReadGIFHeader(const char* aData);
  LexerTransition<State> ReadScreenDescriptor(const char* aData);
  LexerTransition<State> ReadGlobalColorTable(const char* aData, size_t aLength);
  LexerTransition<State> FinishedGlobalColorTable();
  LexerTransition<State> ReadBlockHeader(const char* aData);
  LexerTransition<State> ReadExtensionHeader(const char* aData);
  LexerTransition<State> ReadGraphicControlExtension(const char* aData);
  LexerTransition<State> ReadApplicationIdentifier(const char* aData);
  LexerTransition<State> ReadNetscapeExtensionSubBlock(const char* aData);
  LexerTransition<State> ReadNetscapeExtensionData(const char* aData);
  LexerTransition<State> ReadImageDescriptor(const char* aData);
  LexerTransition<State> FinishImageDescriptor(const char* aData);
  LexerTransition<State> ReadLocalColorTable(const char* aData, size_t aLength);
  LexerTransition<State> FinishedLocalColorTable();
  LexerTransition<State> ReadImageDataBlock(const char* aData);
  LexerTransition<State> ReadImageDataSubBlock(const char* aData);
  LexerTransition<State> ReadLZWData(const char* aData, size_t aLength);
  LexerTransition<State> SkipSubBlocks(const char* aData);

  // The StreamingLexer used to manage input. The initial size of the buffer is
  // chosen as a little larger than the maximum size of any fixed-length data we
  // have to read for a state. We read variable-length data in unbuffered mode
  // so the buffer shouldn't have to be resized during decoding.
  StreamingLexer<State, 16> mLexer;

  uint32_t mOldColor;        // The old value of the transparent pixel

  // The frame number of the currently-decoding frame when we're in the middle
  // of decoding it, and -1 otherwise.
  int32_t mCurrentFrameIndex;

  // When we're reading in the global or local color table, this records our
  // current position - i.e., the offset into which the next byte should be
  // written.
  size_t mColorTablePos;

  uint8_t mColorMask;        // Apply this to the pixel to keep within colormap
  bool mGIFOpen;
  bool mSawTransparency;

  gif_struct mGIFStruct;

  SurfacePipe mPipe;  /// The SurfacePipe used to write to the output surface.
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_decoders_nsGIFDecoder2_h
