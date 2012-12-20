/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsGIFDecoder2_h
#define _nsGIFDecoder2_h

#include "nsCOMPtr.h"
#include "Decoder.h"
#include "imgDecoderObserver.h"

#include "GIF2.h"

namespace mozilla {
namespace image {
class RasterImage;

//////////////////////////////////////////////////////////////////////
// nsGIFDecoder2 Definition

class nsGIFDecoder2 : public Decoder
{
public:

  nsGIFDecoder2(RasterImage &aImage, imgDecoderObserver* aObserver);
  ~nsGIFDecoder2();

  virtual void WriteInternal(const char* aBuffer, uint32_t aCount);
  virtual void FinishInternal();
  virtual Telemetry::ID SpeedHistogram();

private:
  /* These functions will be called when the decoder has a decoded row,
   * frame size information, etc. */

  void      BeginGIF();
  nsresult  BeginImageFrame(uint16_t aDepth);
  void      EndImageFrame();
  void      FlushImageData();
  void      FlushImageData(uint32_t fromRow, uint32_t rows);

  nsresult  GifWrite(const uint8_t * buf, uint32_t numbytes);
  uint32_t  OutputRow();
  bool      DoLzw(const uint8_t *q);

  inline int ClearCode() const { return 1 << mGIFStruct.datasize; }

  int32_t mCurrentRow;
  int32_t mLastFlushedRow;

  uint8_t *mImageData;       // Pointer to image data in either Cairo or 8bit format
  uint32_t *mColormap;       // Current colormap to be used in Cairo format
  uint32_t mColormapSize;
  uint32_t mOldColor;        // The old value of the transparent pixel

  // The frame number of the currently-decoding frame when we're in the middle
  // of decoding it, and -1 otherwise.
  int32_t mCurrentFrame;

  uint8_t mCurrentPass;
  uint8_t mLastFlushedPass;
  uint8_t mColorMask;        // Apply this to the pixel to keep within colormap
  bool mGIFOpen;
  bool mSawTransparency;

  gif_struct mGIFStruct;
};

} // namespace image
} // namespace mozilla

#endif
