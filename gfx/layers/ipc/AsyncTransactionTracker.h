/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AsyncTransactionTracker_h
#define mozilla_layers_AsyncTransactionTracker_h

#include <map>

#include "mozilla/Atomics.h"
#include "mozilla/Monitor.h"      // for Monitor
#include "mozilla/RefPtr.h"       // for AtomicRefCounted

namespace mozilla {
namespace layers {

class TextureClient;

/**
 * AsyncTransactionTracker tracks asynchronous transaction.
 * It is typically used for asynchronous layer transaction handling.
 */
class AsyncTransactionTracker
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncTransactionTracker)

  AsyncTransactionTracker();

  static void Initialize()
  {
    if (!sLock) {
      sLock = new Mutex("AsyncTransactionTracker::sLock");
    }
  }

  static void Finalize()
  {
    if (sLock) {
      delete sLock;
      sLock = nullptr;
    }
  }

  Monitor& GetReentrantMonitor()
  {
    return mCompletedMonitor;
  }

  /**
   * Wait until asynchronous transaction complete.
   */
  void WaitComplete();

  /**
   * Notify async transaction complete.
   */
  void NotifyComplete();

  /**
   * Notify async transaction cancel.
   */
  void NotifyCancel();

  uint64_t GetId()
  {
    return mSerial;
  }

  /**
   * Called when asynchronous transaction complete.
   */
  virtual void Complete()= 0;

  /**
   * Called when asynchronous transaction is cancelled.
   * The cancel typically happens when IPC is disconnected
   */
  virtual void Cancel()= 0;

  virtual void SetTextureClient(TextureClient* aTextureClient) {}

protected:
  virtual ~AsyncTransactionTracker();

  static uint64_t GetNextSerial()
  {
    MOZ_ASSERT(sLock);
    if(sLock) {
      sLock->Lock();
    }
    ++sSerialCounter;
    if(sLock) {
      sLock->Unlock();
    }
    return sSerialCounter;
  }

  uint64_t mSerial;
  Monitor mCompletedMonitor;
  bool    mCompleted;

  /**
   * gecko does not provide atomic operation for uint64_t.
   * Ensure atomicity by using Mutex.
   */
  static uint64_t sSerialCounter;
  static Mutex* sLock;
};

class AsyncTransactionTrackersHolder
{
public:
  AsyncTransactionTrackersHolder();
  virtual ~AsyncTransactionTrackersHolder();

  void HoldUntilComplete(AsyncTransactionTracker* aTransactionTracker);

  void TransactionCompleteted(uint64_t aTransactionId);

protected:
  void ClearAllAsyncTransactionTrackers();

  void DestroyAsyncTransactionTrackersHolder();

  bool mIsTrackersHolderDestroyed;
  std::map<uint64_t, RefPtr<AsyncTransactionTracker> > mAsyncTransactionTrackeres;

};

} // namespace layers
} // namespace mozilla

#endif  // mozilla_layers_AsyncTransactionTracker_h
