/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJPEGDecoder_h__
#define nsJPEGDecoder_h__

#include "RasterImage.h"
/* On Windows systems, RasterImage.h brings in 'windows.h', which defines INT32.
 * But the jpeg decoder has its own definition of INT32. To avoid build issues,
 * we need to undefine the version from 'windows.h'. */
#undef INT32

#include "Decoder.h"

#include "nsAutoPtr.h"

#include "imgDecoderObserver.h"
#include "nsIInputStream.h"
#include "nsIPipe.h"
#include "qcms.h"

extern "C" {
#include "jpeglib.h"
}

#include <setjmp.h>

namespace mozilla {
namespace image {

typedef struct {
    struct jpeg_error_mgr pub;  /* "public" fields for IJG library*/
    jmp_buf setjmp_buffer;      /* For handling catastropic errors */
} decoder_error_mgr;

typedef enum {
    JPEG_HEADER,                          /* Reading JFIF headers */
    JPEG_START_DECOMPRESS,
    JPEG_DECOMPRESS_PROGRESSIVE,          /* Output progressive pixels */
    JPEG_DECOMPRESS_SEQUENTIAL,           /* Output sequential pixels */
    JPEG_DONE,
    JPEG_SINK_NON_JPEG_TRAILER,          /* Some image files have a */
                                         /* non-JPEG trailer */
    JPEG_ERROR    
} jstate;

class RasterImage;

class nsJPEGDecoder : public Decoder
{
public:
  nsJPEGDecoder(RasterImage &aImage, imgDecoderObserver* aObserver, Decoder::DecodeStyle aDecodeStyle);
  virtual ~nsJPEGDecoder();

  virtual void InitInternal();
  virtual void WriteInternal(const char* aBuffer, uint32_t aCount);
  virtual void FinishInternal();

  virtual Telemetry::ID SpeedHistogram();
  void NotifyDone();

protected:
  void OutputScanlines(bool* suspend);

public:
  uint8_t *mImageData;

  struct jpeg_decompress_struct mInfo;
  struct jpeg_source_mgr mSourceMgr;
  decoder_error_mgr mErr;
  jstate mState;

  uint32_t mBytesToSkip;

  const JOCTET *mSegment;   // The current segment we are decoding from
  uint32_t mSegmentLen;     // amount of data in mSegment

  JOCTET *mBackBuffer;
  uint32_t mBackBufferLen; // Offset of end of active backtrack data
  uint32_t mBackBufferSize; // size in bytes what mBackBuffer was created with
  uint32_t mBackBufferUnreadLen; // amount of data currently in mBackBuffer

  JOCTET  *mProfile;
  uint32_t mProfileLength;

  qcms_profile *mInProfile;
  qcms_transform *mTransform;

  bool mReading;

  const Decoder::DecodeStyle mDecodeStyle;

  uint32_t mCMSMode;
};

} // namespace image
} // namespace mozilla

#endif // nsJPEGDecoder_h__
