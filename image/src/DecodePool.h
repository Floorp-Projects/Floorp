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
#include "nsCOMPtr.h"
#include "nsIEventTarget.h"
#include "nsIObserver.h"

class nsIThread;
class nsIThreadPool;

namespace mozilla {
namespace image {

class Decoder;
class RasterImage;

/**
 * DecodePool is a singleton class that manages decoding of raster images. It
 * owns a pool of image decoding threads that are used for asynchronous
 * decoding.
 *
 * DecodePool allows callers to run a decoder, handling management of the
 * decoder's lifecycle and whether it executes on the main thread,
 * off-main-thread in the image decoding thread pool, or on some combination of
 * the two.
 */
class DecodePool : public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /// Initializes the singleton instance. Should be called from the main thread.
  static void Initialize();

  /// Returns the singleton instance.
  static DecodePool* Singleton();

  /// Ask the DecodePool to run @aDecoder asynchronously and return immediately.
  void AsyncDecode(Decoder* aDecoder);

  /**
   * Run @aDecoder synchronously if the image it's decoding is small. If the
   * image is too large, or if the source data isn't complete yet, run @aDecoder
   * asynchronously instead.
   */
  void SyncDecodeIfSmall(Decoder* aDecoder);

  /**
   * Run aDecoder synchronously if at all possible. If it can't complete
   * synchronously because the source data isn't complete, asynchronously decode
   * the rest.
   */
  void SyncDecodeIfPossible(Decoder* aDecoder);

  /**
   * Returns an event target interface to the DecodePool's underlying thread
   * pool. Callers can use this event target to submit work to the image
   * decoding thread pool.
   *
   * @return An nsIEventTarget interface to the thread pool.
   */
  already_AddRefed<nsIEventTarget> GetEventTarget();

  /**
   * Returns an event target interface to the DecodePool's I/O thread. Callers
   * who want to deliver data to workers on the DecodePool can use this event
   * target.
   *
   * @return An nsIEventTarget interface to the thread pool's I/O thread.
   */
  already_AddRefed<nsIEventTarget> GetIOEventTarget();

  /**
   * Creates a worker which can be used to attempt further decoding using the
   * provided decoder.
   *
   * @return The new worker, which should be posted to the event target returned
   *         by GetEventTarget.
   */
  already_AddRefed<nsIRunnable> CreateDecodeWorker(Decoder* aDecoder);

private:
  friend class DecodeWorker;

  DecodePool();
  virtual ~DecodePool();

  void Decode(Decoder* aDecoder);
  void NotifyDecodeComplete(Decoder* aDecoder);
  void NotifyProgress(Decoder* aDecoder);

  static StaticRefPtr<DecodePool> sSingleton;

  // mMutex protects mThreadPool and mIOThread.
  Mutex                     mMutex;
  nsCOMPtr<nsIThreadPool>   mThreadPool;
  nsCOMPtr<nsIThread>       mIOThread;
};

} // namespace image
} // namespace mozilla

#endif // MOZILLA_IMAGELIB_DECODEPOOL_H_
