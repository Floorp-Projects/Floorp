/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * DecodePool manages the threads used for decoding raster images.
 */

#ifndef mozilla_image_DecodePool_h
#define mozilla_image_DecodePool_h

#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsIEventTarget.h"
#include "nsIObserver.h"
#include "mozilla/RefPtr.h"

class nsIThread;
class nsIThreadPool;

namespace mozilla {
namespace image {

class Decoder;
class DecodePoolImpl;
class IDecodingTask;

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

  /// @return the number of processor cores we have available. This is not the
  /// same as the number of decoding threads we're actually using.
  static uint32_t NumberOfCores();

  /// Ask the DecodePool to run @aTask asynchronously and return immediately.
  void AsyncRun(IDecodingTask* aTask);

  /**
   * Run @aTask synchronously if the task would prefer it. It's up to the task
   * itself to make this decision; @see IDecodingTask::ShouldPreferSyncRun(). If
   * @aTask doesn't prefer it, just run @aTask asynchronously and return
   * immediately.
   * @return true if the task was run sync, false otherwise.
   */
  bool SyncRunIfPreferred(IDecodingTask* aTask);

  /**
   * Run @aTask synchronously. This does not guarantee that @aTask will complete
   * synchronously. If, for example, @aTask doesn't yet have the data it needs to
   * run synchronously, it may recover by scheduling an async task to finish up
   * the work when the remaining data is available.
   */
  void SyncRunIfPossible(IDecodingTask* aTask);

  /**
   * Returns an event target interface to the DecodePool's I/O thread. Callers
   * who want to deliver data to workers on the DecodePool can use this event
   * target.
   *
   * @return An nsIEventTarget interface to the thread pool's I/O thread.
   */
  already_AddRefed<nsIEventTarget> GetIOEventTarget();

private:
  friend class DecodePoolWorker;

  DecodePool();
  virtual ~DecodePool();

  static StaticRefPtr<DecodePool> sSingleton;
  static uint32_t sNumCores;

  RefPtr<DecodePoolImpl> mImpl;

  // mMutex protects mThreads and mIOThread.
  Mutex                         mMutex;
  nsTArray<nsCOMPtr<nsIThread>> mThreads;
  nsCOMPtr<nsIThread>           mIOThread;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_DecodePool_h
