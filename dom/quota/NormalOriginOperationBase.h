/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_NORMALORIGINOPERATIONBASE_H_
#define DOM_QUOTA_NORMALORIGINOPERATIONBASE_H_

#include "OriginOperationBase.h"
#include "mozilla/Atomics.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/quota/CheckedUnsafePtr.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "mozilla/dom/quota/PersistenceType.h"

namespace mozilla::dom::quota {

class NormalOriginOperationBase
    : public OriginOperationBase,
      public OpenDirectoryListener,
      public SupportsCheckedUnsafePtr<CheckIf<DiagnosticAssertEnabled>> {
 protected:
  OriginScope mOriginScope;
  RefPtr<DirectoryLock> mDirectoryLock;
  Nullable<PersistenceType> mPersistenceType;
  Nullable<Client::Type> mClientType;
  mozilla::Atomic<bool> mCanceled;
  const bool mExclusive;

  NormalOriginOperationBase(const char* aRunnableName,
                            const Nullable<PersistenceType>& aPersistenceType,
                            const OriginScope& aOriginScope,
                            const Nullable<Client::Type> aClientType,
                            bool aExclusive)
      : OriginOperationBase(GetCurrentSerialEventTarget(), aRunnableName),
        mOriginScope(aOriginScope),
        mPersistenceType(aPersistenceType),
        mClientType(aClientType),
        mExclusive(aExclusive) {
    AssertIsOnOwningThread();
  }

  ~NormalOriginOperationBase() = default;

  virtual RefPtr<DirectoryLock> CreateDirectoryLock();

 private:
  // Need to declare refcounting unconditionally, because
  // OpenDirectoryListener has pure-virtual refcounting.
  NS_DECL_ISUPPORTS_INHERITED

  virtual void Open() override;

  virtual void UnblockOpen() override;

  // OpenDirectoryListener overrides.
  virtual void DirectoryLockAcquired(DirectoryLock* aLock) override;

  virtual void DirectoryLockFailed() override;

  // Used to send results before unblocking open.
  virtual void SendResults() = 0;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_NORMALORIGINOPERATIONBASE_H_
