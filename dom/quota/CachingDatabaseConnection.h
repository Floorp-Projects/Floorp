/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_CACHINGDATABASECONNECTION_H_
#define DOM_QUOTA_CACHINGDATABASECONNECTION_H_

#include "mozilla/dom/quota/Config.h"

#include "mozStorageHelper.h"
#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsHashKeys.h"
#include "nsInterfaceHashtable.h"
#include "nsString.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/InitializedOnce.h"
#include "mozilla/NotNull.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/ScopedLogExtraInfo.h"

namespace mozilla::dom::quota {

class CachingDatabaseConnection {
 public:
  class CachedStatement;

  // A stack-only RAII wrapper that resets its borrowed statement when the
  // wrapper goes out of scope. Note it's intentionally not declared MOZ_RAII,
  // because it actually is used as a temporary in simple cases like
  // `stmt.Borrow()->Execute()`. It also automatically exposes the current query
  // to ScopedLogExtraInfo as "query" in builds where this mechanism is active.
  class MOZ_STACK_CLASS BorrowedStatement : mozStorageStatementScoper {
   public:
    mozIStorageStatement& operator*() const;

    MOZ_NONNULL_RETURN mozIStorageStatement* operator->() const
        MOZ_NO_ADDREF_RELEASE_ON_RETURN;

    BorrowedStatement(BorrowedStatement&& aOther) = default;

    // No funny business allowed.
    BorrowedStatement& operator=(BorrowedStatement&&) = delete;
    BorrowedStatement(const BorrowedStatement&) = delete;
    BorrowedStatement& operator=(const BorrowedStatement&) = delete;

   private:
    friend class CachedStatement;

#ifdef QM_SCOPED_LOG_EXTRA_INFO_ENABLED
    BorrowedStatement(NotNull<mozIStorageStatement*> aStatement,
                      const nsACString& aQuery)
        : mozStorageStatementScoper(aStatement),
          mExtraInfo{ScopedLogExtraInfo::kTagQuery, aQuery} {}

    ScopedLogExtraInfo mExtraInfo;
#else
    MOZ_IMPLICIT BorrowedStatement(NotNull<mozIStorageStatement*> aStatement)
        : mozStorageStatementScoper(aStatement) {}
#endif
  };

  class LazyStatement;

  void AssertIsOnConnectionThread() const {
#ifdef CACHING_DB_CONNECTION_CHECK_THREAD_OWNERSHIP
    mOwningThread->AssertOwnership("CachingDatabaseConnection not thread-safe");
#endif
  }

  bool HasStorageConnection() const {
    return static_cast<bool>(mStorageConnection);
  }

  mozIStorageConnection& MutableStorageConnection() const {
    AssertIsOnConnectionThread();
    MOZ_ASSERT(mStorageConnection);

    return **mStorageConnection;
  }

  Result<CachedStatement, nsresult> GetCachedStatement(
      const nsACString& aQuery);

  Result<BorrowedStatement, nsresult> BorrowCachedStatement(
      const nsACString& aQuery);

  template <typename BindFunctor>
  nsresult ExecuteCachedStatement(const nsACString& aQuery,
                                  BindFunctor&& aBindFunctor) {
    QM_TRY_INSPECT(const auto& stmt, BorrowCachedStatement(aQuery));
    QM_TRY(std::forward<BindFunctor>(aBindFunctor)(*stmt));
    QM_TRY(MOZ_TO_RESULT(stmt->Execute()));

    return NS_OK;
  }

  nsresult ExecuteCachedStatement(const nsACString& aQuery);

  template <typename BindFunctor>
  Result<Maybe<BorrowedStatement>, nsresult>
  BorrowAndExecuteSingleStepStatement(const nsACString& aQuery,
                                      BindFunctor&& aBindFunctor);

#ifdef DEBUG
  ~CachingDatabaseConnection() {
    MOZ_ASSERT(!mStorageConnection);
    MOZ_ASSERT(!mCachedStatements.Count());
  }
#endif

 protected:
  explicit CachingDatabaseConnection(
      MovingNotNull<nsCOMPtr<mozIStorageConnection>> aStorageConnection);

  CachingDatabaseConnection() = default;

  void LazyInit(
      MovingNotNull<nsCOMPtr<mozIStorageConnection>> aStorageConnection);

  void Close();

 private:
#ifdef CACHING_DB_CONNECTION_CHECK_THREAD_OWNERSHIP
  LazyInitializedOnce<const nsAutoOwningThread> mOwningThread;
#endif

  LazyInitializedOnceEarlyDestructible<
      const NotNull<nsCOMPtr<mozIStorageConnection>>>
      mStorageConnection;
  nsInterfaceHashtable<nsCStringHashKey, mozIStorageStatement>
      mCachedStatements;
};

class CachingDatabaseConnection::CachedStatement final {
  friend class CachingDatabaseConnection;

  nsCOMPtr<mozIStorageStatement> mStatement;

#ifdef QM_SCOPED_LOG_EXTRA_INFO_ENABLED
  nsCString mQuery;
#endif

#ifdef DEBUG
  CachingDatabaseConnection* mDEBUGConnection;
#endif

 public:
#if defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING)
  CachedStatement();
  ~CachedStatement();
#else
  CachedStatement() = default;
#endif

  void AssertIsOnConnectionThread() const;

  explicit operator bool() const;

  BorrowedStatement Borrow() const;

 private:
  // Only called by CachingDatabaseConnection.
  CachedStatement(CachingDatabaseConnection* aConnection,
                  nsCOMPtr<mozIStorageStatement> aStatement,
                  const nsACString& aQuery);

 public:
#if defined(NS_BUILD_REFCNT_LOGGING)
  CachedStatement(CachedStatement&& aOther)
      : mStatement(std::move(aOther.mStatement))
#  ifdef QM_SCOPED_LOG_EXTRA_INFO_ENABLED
        ,
        mQuery(std::move(aOther.mQuery))
#  endif
#  ifdef DEBUG
        ,
        mDEBUGConnection(aOther.mDEBUGConnection)
#  endif
  {
    MOZ_COUNT_CTOR(CachingDatabaseConnection::CachedStatement);
  }
#else
  CachedStatement(CachedStatement&&) = default;
#endif

  CachedStatement& operator=(CachedStatement&&) = default;

  // No funny business allowed.
  CachedStatement(const CachedStatement&) = delete;
  CachedStatement& operator=(const CachedStatement&) = delete;
};

class CachingDatabaseConnection::LazyStatement final {
 public:
  LazyStatement(CachingDatabaseConnection& aConnection,
                const nsACString& aQueryString)
      : mConnection{aConnection}, mQueryString{aQueryString} {}

  Result<CachingDatabaseConnection::BorrowedStatement, nsresult> Borrow();

  template <typename BindFunctor>
  Result<Maybe<CachingDatabaseConnection::BorrowedStatement>, nsresult>
  BorrowAndExecuteSingleStep(BindFunctor&& aBindFunctor) {
    QM_TRY_UNWRAP(auto borrowedStatement, Borrow());

    QM_TRY(std::forward<BindFunctor>(aBindFunctor)(*borrowedStatement));

    QM_TRY_INSPECT(
        const bool& hasResult,
        MOZ_TO_RESULT_INVOKE_MEMBER(&*borrowedStatement, ExecuteStep));

    return hasResult ? Some(std::move(borrowedStatement)) : Nothing{};
  }

 private:
  Result<Ok, nsresult> Initialize();

  CachingDatabaseConnection& mConnection;
  const nsCString mQueryString;
  CachingDatabaseConnection::CachedStatement mCachedStatement;
};

template <typename BindFunctor>
Result<Maybe<CachingDatabaseConnection::BorrowedStatement>, nsresult>
CachingDatabaseConnection::BorrowAndExecuteSingleStepStatement(
    const nsACString& aQuery, BindFunctor&& aBindFunctor) {
  return LazyStatement{*this, aQuery}.BorrowAndExecuteSingleStep(
      std::forward<BindFunctor>(aBindFunctor));
}

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_CACHINGDATABASECONNECTION_H_
