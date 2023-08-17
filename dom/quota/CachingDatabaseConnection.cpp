/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/CachingDatabaseConnection.h"

#include "mozilla/ProfilerLabels.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla::dom::quota {

CachingDatabaseConnection::CachingDatabaseConnection(
    MovingNotNull<nsCOMPtr<mozIStorageConnection>> aStorageConnection)
    :
#ifdef CACHING_DB_CONNECTION_CHECK_THREAD_OWNERSHIP
      mOwningThread{nsAutoOwningThread{}},
#endif
      mStorageConnection(std::move(aStorageConnection)) {
}

void CachingDatabaseConnection::LazyInit(
    MovingNotNull<nsCOMPtr<mozIStorageConnection>> aStorageConnection) {
#ifdef CACHING_DB_CONNECTION_CHECK_THREAD_OWNERSHIP
  mOwningThread.init();
#endif
  mStorageConnection.init(std::move(aStorageConnection));
}

Result<CachingDatabaseConnection::CachedStatement, nsresult>
CachingDatabaseConnection::GetCachedStatement(const nsACString& aQuery) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!aQuery.IsEmpty());
  MOZ_ASSERT(mStorageConnection);

  AUTO_PROFILER_LABEL("CachingDatabaseConnection::GetCachedStatement", DOM);

  QM_TRY_UNWRAP(
      auto stmt,
      mCachedStatements.TryLookupOrInsertWith(
          aQuery, [&]() -> Result<nsCOMPtr<mozIStorageStatement>, nsresult> {
            const auto extraInfo =
                ScopedLogExtraInfo{ScopedLogExtraInfo::kTagQuery, aQuery};

            QM_TRY_RETURN(
                MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
                    nsCOMPtr<mozIStorageStatement>, **mStorageConnection,
                    CreateStatement, aQuery),
                QM_PROPAGATE,
                ([&aQuery,
                  &storageConnection = **mStorageConnection](const auto&) {
#ifdef DEBUG
                  nsCString msg;
                  MOZ_ALWAYS_SUCCEEDS(
                      storageConnection.GetLastErrorString(msg));

                  nsAutoCString error =
                      "The statement '"_ns + aQuery +
                      "' failed to compile with the error message '"_ns + msg +
                      "'."_ns;

                  NS_WARNING(error.get());
#else
                    (void)aQuery;
#endif
                }));
          }));

  return CachedStatement{this, std::move(stmt), aQuery};
}

Result<CachingDatabaseConnection::BorrowedStatement, nsresult>
CachingDatabaseConnection::BorrowCachedStatement(const nsACString& aQuery) {
  QM_TRY_UNWRAP(auto cachedStatement, GetCachedStatement(aQuery));

  return cachedStatement.Borrow();
}

nsresult CachingDatabaseConnection::ExecuteCachedStatement(
    const nsACString& aQuery) {
  return ExecuteCachedStatement(
      aQuery, [](auto&) -> Result<Ok, nsresult> { return Ok{}; });
}

void CachingDatabaseConnection::Close() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(HasStorageConnection());

  AUTO_PROFILER_LABEL("CachingDatabaseConnection::Close", DOM);

  mCachedStatements.Clear();

  MOZ_ALWAYS_SUCCEEDS((*mStorageConnection)->Close());
  mStorageConnection.destroy();
}

#if defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING)
CachingDatabaseConnection::CachedStatement::CachedStatement()
#  ifdef DEBUG
    : mDEBUGConnection(nullptr)
#  endif
{
  AssertIsOnConnectionThread();

  MOZ_COUNT_CTOR(CachingDatabaseConnection::CachedStatement);
}

CachingDatabaseConnection::CachedStatement::~CachedStatement() {
  AssertIsOnConnectionThread();

  MOZ_COUNT_DTOR(CachingDatabaseConnection::CachedStatement);
}
#endif

CachingDatabaseConnection::CachedStatement::operator bool() const {
  AssertIsOnConnectionThread();

  return mStatement;
}

mozIStorageStatement& CachingDatabaseConnection::BorrowedStatement::operator*()
    const {
  return *operator->();
}

mozIStorageStatement* CachingDatabaseConnection::BorrowedStatement::operator->()
    const {
  MOZ_ASSERT(mStatement);

  return mStatement;
}

CachingDatabaseConnection::BorrowedStatement
CachingDatabaseConnection::CachedStatement::Borrow() const {
  AssertIsOnConnectionThread();

#ifdef QM_SCOPED_LOG_EXTRA_INFO_ENABLED
  return BorrowedStatement{WrapNotNull(mStatement), mQuery};
#else
  return BorrowedStatement{WrapNotNull(mStatement)};
#endif
}

CachingDatabaseConnection::CachedStatement::CachedStatement(
    CachingDatabaseConnection* aConnection,
    nsCOMPtr<mozIStorageStatement> aStatement, const nsACString& aQuery)
    : mStatement(std::move(aStatement))
#ifdef QM_SCOPED_LOG_EXTRA_INFO_ENABLED
      ,
      mQuery(aQuery)
#endif
#if defined(DEBUG)
      ,
      mDEBUGConnection(aConnection)
#endif
{
#ifdef DEBUG
  MOZ_ASSERT(aConnection);
  aConnection->AssertIsOnConnectionThread();
#endif
  MOZ_ASSERT(mStatement);
  AssertIsOnConnectionThread();

  MOZ_COUNT_CTOR(CachingDatabaseConnection::CachedStatement);
}

void CachingDatabaseConnection::CachedStatement::AssertIsOnConnectionThread()
    const {
#ifdef DEBUG
  if (mDEBUGConnection) {
    mDEBUGConnection->AssertIsOnConnectionThread();
  }
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mozilla::ipc::IsOnBackgroundThread());
#endif
}

Result<CachingDatabaseConnection::BorrowedStatement, nsresult>
CachingDatabaseConnection::LazyStatement::Borrow() {
  if (!mCachedStatement) {
    QM_TRY(Initialize());
  }

  return mCachedStatement.Borrow();
}

Result<Ok, nsresult> CachingDatabaseConnection::LazyStatement::Initialize() {
  QM_TRY_UNWRAP(mCachedStatement, mConnection.GetCachedStatement(mQueryString));
  return Ok{};
}

}  // namespace mozilla::dom::quota
