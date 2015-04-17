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
#include "mozilla/layers/FenceUtils.h"  // for FenceHandle
#include "mozilla/Monitor.h"      // for Monitor
#include "mozilla/RefPtr.h"       // for AtomicRefCounted

namespace mozilla {
namespace layers {

class TextureClient;
class AsyncTransactionTrackersHolder;

/**
 * AsyncTransactionTracker tracks asynchronous transaction.
 * It is typically used for asynchronous layer transaction handling.
 */
class AsyncTransactionTracker
{
  friend class AsyncTransactionTrackersHolder;
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncTransactionTracker)

  AsyncTransactionTracker();

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

  virtual void SetReleaseFenceHandle(FenceHandle& aReleaseFenceHandle) {}

protected:
  virtual ~AsyncTransactionTracker();

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

  static uint64_t GetNextSerial()
  {
    MOZ_ASSERT(sLock);
    MutexAutoLock lock(*sLock);
    ++sSerialCounter;
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

  static void Initialize()
  {
    if (!sHolderLock) {
      sHolderLock = new Mutex("AsyncTransactionTrackersHolder::sHolderLock");
    }
    AsyncTransactionTracker::Initialize();
  }

  static void Finalize()
  {
    if (sHolderLock) {
      delete sHolderLock;
      sHolderLock = nullptr;
    }
    AsyncTransactionTracker::Finalize();
  }

  void HoldUntilComplete(AsyncTransactionTracker* aTransactionTracker);

  void TransactionCompleteted(uint64_t aTransactionId);

  static void TransactionCompleteted(uint64_t aHolderId, uint64_t aTransactionId);

  static void SetReleaseFenceHandle(FenceHandle& aReleaseFenceHandle,
                                    uint64_t aHolderId,
                                    uint64_t aTransactionId);

  uint64_t GetId()
  {
    return mSerial;
  }

protected:

  static uint64_t GetNextSerial()
  {
    MOZ_ASSERT(sHolderLock);
    MutexAutoLock lock(*sHolderLock);
    ++sSerialCounter;
    return sSerialCounter;
  }

  void TransactionCompletetedInternal(uint64_t aTransactionId);

  void SetReleaseFenceHandle(FenceHandle& aReleaseFenceHandle, uint64_t aTransactionId);

  void ClearAllAsyncTransactionTrackers();

  void DestroyAsyncTransactionTrackersHolder();

  uint64_t mSerial;

  bool mIsTrackersHolderDestroyed;
  std::map<uint64_t, RefPtr<AsyncTransactionTracker> > mAsyncTransactionTrackeres;

  /**
   * gecko does not provide atomic operation for uint64_t.
   * Ensure atomicity by using Mutex.
   */
  static uint64_t sSerialCounter;
  static Mutex* sHolderLock;

  /**
   * Map of all living AsyncTransactionTrackersHolder instances
   */
  static std::map<uint64_t, AsyncTransactionTrackersHolder*> sTrackersHolders;
};

} // namespace layers
} // namespace mozilla

#endif  // mozilla_layers_AsyncTransactionTracker_h
