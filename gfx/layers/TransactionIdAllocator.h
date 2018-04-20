/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TRANSACTION_ID_ALLOCATOR_H
#define GFX_TRANSACTION_ID_ALLOCATOR_H

#include "nsISupportsImpl.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace layers {

class TransactionIdAllocator {
protected:
  virtual ~TransactionIdAllocator() {}

public:
  NS_INLINE_DECL_REFCOUNTING(TransactionIdAllocator)

  /**
   * Allocate a unique id number for the current refresh tick, can
   * only be called while IsInRefresh().
   *
   * If too many id's are allocated without being returned then
   * the refresh driver will suspend until they catch up. This
   * "throttling" behaviour can be skipped by passing aThrottle=false.
   * Otherwise call sites should generally be passing aThrottle=true.
   */
  virtual TransactionId GetTransactionId(bool aThrottle) = 0;

  /**
   * Return the transaction id that for the last non-revoked transaction.
   * This allows the caller to tell whether a composite was triggered by
   * a paint that occurred after a call to TransactionId().
   */
  virtual TransactionId LastTransactionId() const = 0;

  /**
   * Notify that all work (including asynchronous composites)
   * for a given transaction id has been completed.
   *
   * If the refresh driver has been suspended because
   * of having too many outstanding id's, then this may
   * resume it.
   */
  virtual void NotifyTransactionCompleted(TransactionId aTransactionId) = 0;

  /**
   * Revoke a transaction id that isn't needed to track
   * completion of asynchronous work. This is similar
   * to NotifyTransactionCompleted except avoids
   * return ordering issues.
   */
  virtual void RevokeTransactionId(TransactionId aTransactionId) = 0;

  /**
   * Stop waiting for pending transactions, if any.
   *
   * This is used when ClientLayerManager is assigning to another refresh
   * driver, and the current refresh driver may never receive transaction
   * completed notifications.
   */
  virtual void ClearPendingTransactions() = 0;

  /**
   * Transaction id is usually initialized as 0, however when ClientLayerManager
   * switches to another refresh driver, completed transactions of the previous
   * refresh driver could be delivered and confuse the newly adopted refresh
   * driver. To prevent this situation, use this function to reset transaction
   * id to the last transaction id from previous refresh driver, so that all
   * completed transactions of previous refresh driver will be ignored.
   */
  virtual void ResetInitialTransactionId(TransactionId aTransactionId) = 0;

  /**
   * Get the start time of the current refresh tick.
   */
  virtual mozilla::TimeStamp GetTransactionStart() = 0;
};

} // namespace layers
} // namespace mozilla


#endif /* GFX_TRANSACTION_ID_ALLOCATOR_H */
