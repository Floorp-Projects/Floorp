/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * DecodePool manages the threads used for decoding raster images.
 */

#ifndef MOZILLA_IMAGELIB_DECODEPOOL_H_
#define MOZILLA_IMAGELIB_DECODEPOOL_H_

#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include <mozilla/TypedEnum.h>
#include "nsCOMPtr.h"
#include "nsIEventTarget.h"
#include "nsIObserver.h"

class nsIThreadPool;

namespace mozilla {
namespace image {

class Decoder;
class RasterImage;

MOZ_BEGIN_ENUM_CLASS(DecodeStatus, uint8_t)
  INACTIVE,
  PENDING,
  ACTIVE,
  WORK_DONE,
  STOPPED
MOZ_END_ENUM_CLASS(DecodeStatus)

MOZ_BEGIN_ENUM_CLASS(DecodeUntil, uint8_t)
  TIME,
  SIZE,
  DONE_BYTES
MOZ_END_ENUM_CLASS(DecodeUntil)

MOZ_BEGIN_ENUM_CLASS(ShutdownReason, uint8_t)
  DONE,
  NOT_NEEDED,
  FATAL_ERROR
MOZ_END_ENUM_CLASS(ShutdownReason)


/**
 * DecodePool is a singleton class we use when decoding large images.
 *
 * When we wish to decode an image larger than
 * image.mem.max_bytes_for_sync_decode, we call DecodePool::RequestDecode()
 * for the image.  This adds the image to a queue of pending requests and posts
 * the DecodePool singleton to the event queue, if it's not already pending
 * there.
 *
 * When the DecodePool is run from the event queue, it decodes the image (and
 * all others it's managing) in chunks, periodically yielding control back to
 * the event loop.
 */
class DecodePool : public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static DecodePool* Singleton();

  /**
   * Ask the DecodePool to asynchronously decode this image.
   */
  void RequestDecode(RasterImage* aImage);

  /**
   * Decode aImage for a short amount of time, and post the remainder to the
   * queue.
   */
  void DecodeABitOf(RasterImage* aImage);

  /**
   * Ask the DecodePool to stop decoding this image.  Internally, we also
   * call this function when we finish decoding an image.
   *
   * Since the DecodePool keeps raw pointers to RasterImages, make sure you
   * call this before a RasterImage is destroyed!
   */
  static void StopDecoding(RasterImage* aImage);

  /**
   * Synchronously decode the beginning of the image until we run out of
   * bytes or we get the image's size.  Note that this done on a best-effort
   * basis; if the size is burried too deep in the image, we'll give up.
   *
   * @return NS_ERROR if an error is encountered, and NS_OK otherwise.  (Note
   *         that we return NS_OK even when the size was not found.)
   */
  nsresult DecodeUntilSizeAvailable(RasterImage* aImage);

  /**
   * Returns an event target interface to the thread pool; primarily for
   * OnDataAvailable delivery off main thread.
   *
   * @return An nsIEventTarget interface to mThreadPool.
   */
  already_AddRefed<nsIEventTarget> GetEventTarget();

  /**
   * Decode some chunks of the given image.  If aDecodeUntil is SIZE,
   * decode until we have the image's size, then stop. If bytesToDecode is
   * non-0, at most bytesToDecode bytes will be decoded. if aDecodeUntil is
   * DONE_BYTES, decode until all bytesToDecode bytes are decoded.
   */
  nsresult DecodeSomeOfImage(RasterImage* aImage,
                             DecodeUntil aDecodeUntil = DecodeUntil::TIME,
                             uint32_t bytesToDecode = 0);

private:
  DecodePool();
  virtual ~DecodePool();

  static StaticRefPtr<DecodePool> sSingleton;

  // mThreadPoolMutex protects mThreadPool. For all RasterImages R,
  // R::mDecodingMonitor must be acquired before mThreadPoolMutex
  // if both are acquired; the other order may cause deadlock.
  Mutex                     mThreadPoolMutex;
  nsCOMPtr<nsIThreadPool>   mThreadPool;
};

} // namespace image
} // namespace mozilla

#endif // MOZILLA_IMAGELIB_DECODEPOOL_H_
