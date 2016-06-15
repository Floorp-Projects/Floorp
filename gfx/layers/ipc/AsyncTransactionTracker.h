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
 * Object that lets you wait for one or more async transactions to complete.
 */
class AsyncTransactionWaiter
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncTransactionWaiter)

  AsyncTransactionWaiter()
    : mCompletedMonitor("AsyncTransactionWaiter")
    , mWaitCount(0)
  {}

  void IncrementWaitCount()
  {
    MonitorAutoLock lock(mCompletedMonitor);
    ++mWaitCount;
  }
  void DecrementWaitCount()
  {
    MonitorAutoLock lock(mCompletedMonitor);
    MOZ_ASSERT(mWaitCount > 0);
    --mWaitCount;
    if (mWaitCount == 0) {
      mCompletedMonitor.Notify();
    }
  }

  /**
   * Wait until asynchronous transactions complete.
   */
  void WaitComplete();

  uint32_t GetWaitCount() { return mWaitCount; }

private:
  ~AsyncTransactionWaiter() {}

  Monitor mCompletedMonitor;
  uint32_t mWaitCount;
};

/**
 * AsyncTransactionTracker tracks asynchronous transaction.
 * It is typically used for asynchronous layer transaction handling.
 */
class AsyncTransactionTracker
{
  friend class AsyncTransactionTrackersHolder;
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncTransactionTracker)

  explicit AsyncTransactionTracker(AsyncTransactionWaiter* aWaiter = nullptr);

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

  static uint64_t GetNextSerial()
  {
    return ++sSerialCounter;
  }

  uint64_t mSerial;
  RefPtr<AsyncTransactionWaiter> mWaiter;
#ifdef DEBUG
  bool mCompleted;
#endif

  static Atomic<uint64_t> sSerialCounter;
};

class AsyncTransactionTrackersHolder
{
public:
  AsyncTransactionTrackersHolder();
  virtual ~AsyncTransactionTrackersHolder();

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

  void DestroyAsyncTransactionTrackersHolder();

protected:

  static uint64_t GetNextSerial()
  {
    return ++sSerialCounter;
  }

  void TransactionCompletetedInternal(uint64_t aTransactionId);

  void SetReleaseFenceHandle(FenceHandle& aReleaseFenceHandle, uint64_t aTransactionId);

  void ClearAllAsyncTransactionTrackers();

  const uint64_t mSerial;

  bool mIsTrackersHolderDestroyed;
  std::map<uint64_t, RefPtr<AsyncTransactionTracker> > mAsyncTransactionTrackers;

  static Atomic<uint64_t> sSerialCounter;
};

} // namespace layers
} // namespace mozilla

#endif  // mozilla_layers_AsyncTransactionTracker_h
