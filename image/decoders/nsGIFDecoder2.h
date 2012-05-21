/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsGIFDecoder2_h
#define _nsGIFDecoder2_h

#include "nsCOMPtr.h"
#include "Decoder.h"
#include "imgIDecoderObserver.h"

#include "GIF2.h"

namespace mozilla {
namespace image {
class RasterImage;

//////////////////////////////////////////////////////////////////////
// nsGIFDecoder2 Definition

class nsGIFDecoder2 : public Decoder
{
public:

  nsGIFDecoder2(RasterImage &aImage, imgIDecoderObserver* aObserver);
  ~nsGIFDecoder2();

  virtual void WriteInternal(const char* aBuffer, PRUint32 aCount);
  virtual void FinishInternal();
  virtual Telemetry::ID SpeedHistogram();

private:
  /* These functions will be called when the decoder has a decoded row,
   * frame size information, etc. */

  void      BeginGIF();
  nsresult  BeginImageFrame(PRUint16 aDepth);
  void      EndImageFrame();
  void      FlushImageData();
  void      FlushImageData(PRUint32 fromRow, PRUint32 rows);

  nsresult  GifWrite(const PRUint8 * buf, PRUint32 numbytes);
  PRUint32  OutputRow();
  bool      DoLzw(const PRUint8 *q);

  inline int ClearCode() const { return 1 << mGIFStruct.datasize; }

  PRInt32 mCurrentRow;
  PRInt32 mLastFlushedRow;

  PRUint8 *mImageData;       // Pointer to image data in either Cairo or 8bit format
  PRUint32 *mColormap;       // Current colormap to be used in Cairo format
  PRUint32 mColormapSize;
  PRUint32 mOldColor;        // The old value of the transparent pixel

  // The frame number of the currently-decoding frame when we're in the middle
  // of decoding it, and -1 otherwise.
  PRInt32 mCurrentFrame;

  PRUint8 mCurrentPass;
  PRUint8 mLastFlushedPass;
  PRUint8 mColorMask;        // Apply this to the pixel to keep within colormap
  bool mGIFOpen;
  bool mSawTransparency;

  gif_struct mGIFStruct;
};

} // namespace image
} // namespace mozilla

#endif
