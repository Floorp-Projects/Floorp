/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_nsGIFDecoder2_h
#define mozilla_image_decoders_nsGIFDecoder2_h

#include "Decoder.h"
#include "GIF2.h"
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

  virtual void WriteInternal(const char* aBuffer, uint32_t aCount) override;
  virtual void FinishInternal() override;
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

  nsresult  GifWrite(const uint8_t* buf, uint32_t numbytes);

  /// Transforms a palette index into a pixel.
  template <typename PixelSize> PixelSize
  ColormapIndexToPixel(uint8_t aIndex);

  /// A generator function that performs LZW decompression and yields pixels.
  template <typename PixelSize> NextPixel<PixelSize>
  YieldPixel(const uint8_t*& aCurrentByte);

  /// The entry point for LZW decompression.
  bool      DoLzw(const uint8_t* aData);

  bool      SetHold(const uint8_t* buf, uint32_t count,
                    const uint8_t* buf2 = nullptr, uint32_t count2 = 0);
  bool      CheckForTransparency(const gfx::IntRect& aFrameRect);
  gfx::IntRect ClampToImageRect(const gfx::IntRect& aFrameRect);

  inline int ClearCode() const { return 1 << mGIFStruct.datasize; }

  uint32_t mOldColor;        // The old value of the transparent pixel

  // The frame number of the currently-decoding frame when we're in the middle
  // of decoding it, and -1 otherwise.
  int32_t mCurrentFrameIndex;

  uint8_t mColorMask;        // Apply this to the pixel to keep within colormap
  bool mGIFOpen;
  bool mSawTransparency;

  gif_struct mGIFStruct;

  SurfacePipe mPipe;  /// The SurfacePipe used to write to the output surface.
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_decoders_nsGIFDecoder2_h
