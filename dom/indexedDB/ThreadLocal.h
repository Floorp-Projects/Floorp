/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_threadlocal_h__
#define mozilla_dom_indexeddb_threadlocal_h__

#include "IDBTransaction.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "ProfilerHelpers.h"

namespace mozilla::dom {

class IDBFactory;

namespace indexedDB {

class ThreadLocal {
  friend class DefaultDelete<ThreadLocal>;
  friend IDBFactory;

  LoggingInfo mLoggingInfo;
  Maybe<IDBTransaction&> mCurrentTransaction;
  LoggingIdString<false> mLoggingIdString;

  NS_DECL_OWNINGTHREAD

 public:
  ThreadLocal() = delete;
  ThreadLocal(const ThreadLocal& aOther) = delete;

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(ThreadLocal); }

  const LoggingInfo& GetLoggingInfo() const {
    AssertIsOnOwningThread();

    return mLoggingInfo;
  }

  const nsID& Id() const {
    AssertIsOnOwningThread();

    return mLoggingInfo.backgroundChildLoggingId();
  }

  const nsCString& IdString() const {
    AssertIsOnOwningThread();

    return mLoggingIdString;
  }

  int64_t NextTransactionSN(IDBTransaction::Mode aMode) {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mLoggingInfo.nextTransactionSerialNumber() < INT64_MAX);
    MOZ_ASSERT(mLoggingInfo.nextVersionChangeTransactionSerialNumber() >
               INT64_MIN);

    if (aMode == IDBTransaction::Mode::VersionChange) {
      return mLoggingInfo.nextVersionChangeTransactionSerialNumber()--;
    }

    return mLoggingInfo.nextTransactionSerialNumber()++;
  }

  uint64_t NextRequestSN() {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mLoggingInfo.nextRequestSerialNumber() < UINT64_MAX);

    return mLoggingInfo.nextRequestSerialNumber()++;
  }

  void SetCurrentTransaction(Maybe<IDBTransaction&> aCurrentTransaction) {
    AssertIsOnOwningThread();

    mCurrentTransaction = aCurrentTransaction;
  }

  Maybe<IDBTransaction&> MaybeCurrentTransactionRef() const {
    AssertIsOnOwningThread();

    return mCurrentTransaction;
  }

 private:
  explicit ThreadLocal(const nsID& aBackgroundChildLoggingId);
  ~ThreadLocal();
};

}  // namespace indexedDB
}  // namespace mozilla::dom

#endif  // mozilla_dom_indexeddb_threadlocal_h__
