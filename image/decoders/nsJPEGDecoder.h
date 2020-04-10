/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_nsJPEGDecoder_h
#define mozilla_image_decoders_nsJPEGDecoder_h

#include "RasterImage.h"
#include "SurfacePipe.h"

// On Windows systems, RasterImage.h brings in 'windows.h', which defines INT32.
// But the jpeg decoder has its own definition of INT32. To avoid build issues,
// we need to undefine the version from 'windows.h'.
#undef INT32

#include "Decoder.h"

extern "C" {
#include "jpeglib.h"
}

#include <setjmp.h>

namespace mozilla {
namespace image {

typedef struct {
  struct jpeg_error_mgr pub;  // "public" fields for IJG library
  jmp_buf setjmp_buffer;      // For handling catastropic errors
} decoder_error_mgr;

typedef enum {
  JPEG_HEADER,  // Reading JFIF headers
  JPEG_START_DECOMPRESS,
  JPEG_DECOMPRESS_PROGRESSIVE,  // Output progressive pixels
  JPEG_DECOMPRESS_SEQUENTIAL,   // Output sequential pixels
  JPEG_DONE,
  JPEG_SINK_NON_JPEG_TRAILER,  // Some image files have a
                               // non-JPEG trailer
  JPEG_ERROR
} jstate;

class RasterImage;
struct Orientation;

class nsJPEGDecoder : public Decoder {
 public:
  virtual ~nsJPEGDecoder();

  DecoderType GetType() const override { return DecoderType::JPEG; }

  void NotifyDone();

 protected:
  nsresult InitInternal() override;
  LexerResult DoDecode(SourceBufferIterator& aIterator,
                       IResumable* aOnResume) override;
  nsresult FinishInternal() override;

  Maybe<Telemetry::HistogramID> SpeedHistogram() const override;

 protected:
  Orientation ReadOrientationFromEXIF();
  WriteState OutputScanlines();

 private:
  friend class DecoderFactory;

  // Decoders should only be instantiated via DecoderFactory.
  nsJPEGDecoder(RasterImage* aImage, Decoder::DecodeStyle aDecodeStyle);

  enum class State { JPEG_DATA, FINISHED_JPEG_DATA };

  void FinishRow(uint32_t aLastSourceRow);
  LexerTransition<State> ReadJPEGData(const char* aData, size_t aLength);
  LexerTransition<State> FinishedJPEGData();

  StreamingLexer<State> mLexer;

 public:
  struct jpeg_decompress_struct mInfo;
  struct jpeg_source_mgr mSourceMgr;
  decoder_error_mgr mErr;
  jstate mState;

  uint32_t mBytesToSkip;

  const JOCTET* mSegment;  // The current segment we are decoding from
  uint32_t mSegmentLen;    // amount of data in mSegment

  JOCTET* mBackBuffer;
  uint32_t mBackBufferLen;   // Offset of end of active backtrack data
  uint32_t mBackBufferSize;  // size in bytes what mBackBuffer was created with
  uint32_t mBackBufferUnreadLen;  // amount of data currently in mBackBuffer

  JOCTET* mProfile;
  uint32_t mProfileLength;

  uint32_t* mCMSLine;

  bool mReading;

  const Decoder::DecodeStyle mDecodeStyle;

  SurfacePipe mPipe;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_decoders_nsJPEGDecoder_h
