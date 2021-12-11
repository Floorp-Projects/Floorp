/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_encoders_jpeg_nsJPEGEncoder_h
#define mozilla_image_encoders_jpeg_nsJPEGEncoder_h

#include "imgIEncoder.h"

#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Attributes.h"

#include "nsCOMPtr.h"

struct jpeg_compress_struct;
struct jpeg_common_struct;

#define NS_JPEGENCODER_CID                           \
  {                                                  \
    /* ac2bb8fe-eeeb-4572-b40f-be03932b56e0 */       \
    0xac2bb8fe, 0xeeeb, 0x4572, {                    \
      0xb4, 0x0f, 0xbe, 0x03, 0x93, 0x2b, 0x56, 0xe0 \
    }                                                \
  }

// Provides JPEG encoding functionality. Use InitFromData() to do the
// encoding. See that function definition for encoding options.
class nsJPEGEncoderInternal;

class nsJPEGEncoder final : public imgIEncoder {
  friend class nsJPEGEncoderInternal;
  typedef mozilla::ReentrantMonitor ReentrantMonitor;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_IMGIENCODER
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  nsJPEGEncoder();

 private:
  ~nsJPEGEncoder();

 protected:
  void ConvertHostARGBRow(const uint8_t* aSrc, uint8_t* aDest,
                          uint32_t aPixelWidth);
  void ConvertRGBARow(const uint8_t* aSrc, uint8_t* aDest,
                      uint32_t aPixelWidth);

  void NotifyListener();

  bool mFinished;

  // image buffer
  uint8_t* mImageBuffer;
  uint32_t mImageBufferSize;
  uint32_t mImageBufferUsed;

  uint32_t mImageBufferReadPoint;

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;
  uint32_t mNotifyThreshold;

  // nsJPEGEncoder is designed to allow one thread to pump data into it while
  // another reads from it.  We lock to ensure that the buffer remains
  // append-only while we read from it (that it is not realloced) and to ensure
  // that only one thread dispatches a callback for each call to AsyncWait.
  ReentrantMonitor mReentrantMonitor;
};

#endif  // mozilla_image_encoders_jpeg_nsJPEGEncoder_h
