/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_encoders_webp_nsWebPEncoder_h
#define mozilla_image_encoders_webp_nsWebPEncoder_h

#include "webp/encode.h"

#include "imgIEncoder.h"
#include "nsCOMPtr.h"

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"

#define NS_WEBPENCODER_CID                           \
  { /* a8e5a8e5-bebf-4512-9f50-e41e4748ce28 */       \
    0xa8e5a8e5, 0xbebf, 0x4512, {                    \
      0x9f, 0x50, 0xe4, 0x1e, 0x47, 0x48, 0xce, 0x28 \
    }                                                \
  }

// Provides WEBP encoding functionality. Use InitFromData() to do the
// encoding. See that function definition for encoding options.

class nsWebPEncoder final : public imgIEncoder {
  typedef mozilla::ReentrantMonitor ReentrantMonitor;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_IMGIENCODER
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  nsWebPEncoder();

 protected:
  ~nsWebPEncoder();

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

  // nsWebPEncoder is designed to allow one thread to pump data into it while
  // another reads from it.  We lock to ensure that the buffer remains
  // append-only while we read from it (that it is not realloced) and to
  // ensure that only one thread dispatches a callback for each call to
  // AsyncWait.
  ReentrantMonitor mReentrantMonitor MOZ_UNANNOTATED;
};
#endif  // mozilla_image_encoders_webp_nsWebPEncoder_h
