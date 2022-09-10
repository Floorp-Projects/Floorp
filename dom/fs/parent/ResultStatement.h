/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_RESULTSTATEMENT_H_
#define DOM_FS_PARENT_RESULTSTATEMENT_H_

#include "mozIStorageStatement.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class mozIStorageConnection;

namespace mozilla::dom::fs {

using Column = uint32_t;

using ResultConnection = nsCOMPtr<mozIStorageConnection>;

/**
 * @brief ResultStatement
 * - provides error monad Result<T, E> compatible interface to the lower level
 * error code-based statement implementation in order to enable remote
 * debugging with error stack traces
 * - converts between OPFS internal data types and the generic data types of
 * the lower level implementation
 * - provides a customization point for requests aimed at the lower level
 * implementation allowing for example to remap errors or implement mocks
 */
class ResultStatement {
 public:
  using underlying_t = nsCOMPtr<mozIStorageStatement>;

  explicit ResultStatement(underlying_t aStmt) : mStmt(std::move(aStmt)) {}

  ResultStatement(const ResultStatement& aOther)
      : ResultStatement(aOther.mStmt) {}

  ResultStatement(ResultStatement&& aOther) noexcept
      : ResultStatement(std::move(aOther.mStmt)) {}

  ResultStatement& operator=(const ResultStatement& aOther) = default;

  ResultStatement& operator=(ResultStatement&& aOther) noexcept {
    mStmt = std::move(aOther.mStmt);
    return *this;
  }

  static Result<ResultStatement, QMResult> Create(
      const ResultConnection& aConnection, const nsACString& aSQLStatement);

  // XXX Consider moving all these "inline" methods into a separate file
  // called ResultStatementInlines.h. ResultStatement.h wouldn't have to then
  // include ResultExtensions.h, QuotaCommon.h and mozIStorageStatement.h
  // which are quite large and should be preferable only included from cpp
  // files or special headers like ResultStatementInlines.h. So in the end,
  // other headers would include ResultStatement.h only and other cpp files
  // would include ResultStatementInlines.h. See also IndedexDababase.h and
  // IndexedDatabaseInlines.h to see how it's done.

  inline nsresult BindEntryIdByName(const nsACString& aField,
                                    const EntryId& aValue) {
    return mStmt->BindUTF8StringAsBlobByName(aField, aValue);
  }

  inline nsresult BindContentTypeByName(const nsACString& aField,
                                        const ContentType& aValue) {
    return mStmt->BindStringByName(aField, aValue);
  }

  inline nsresult BindNameByName(const nsACString& aField, const Name& aValue) {
    return mStmt->BindStringAsBlobByName(aField, aValue);
  }

  inline nsresult BindPageNumberByName(const nsACString& aField,
                                       PageNumber aValue) {
    return mStmt->BindInt32ByName(aField, aValue);
  }

  inline nsresult BindUsageByName(const nsACString& aField, Usage aValue) {
    return mStmt->BindInt64ByName(aField, aValue);
  }

  inline Result<bool, QMResult> GetBoolByColumn(Column aColumn) {
    int32_t value = 0;
    QM_TRY(QM_TO_RESULT(mStmt->GetInt32(aColumn, &value)));

    return 0 != value;
  }

  inline Result<ContentType, QMResult> GetContentTypeByColumn(Column aColumn) {
    ContentType value;
    QM_TRY(QM_TO_RESULT(mStmt->GetString(aColumn, value)));

    return value;
  }

  inline Result<DatabaseVersion, QMResult> GetDatabaseVersion() {
    bool hasEntries = false;
    QM_TRY(QM_TO_RESULT(mStmt->ExecuteStep(&hasEntries)));
    MOZ_ALWAYS_TRUE(hasEntries);

    DatabaseVersion value = 0;
    QM_TRY(QM_TO_RESULT(mStmt->GetInt32(0u, &value)));

    return value;
  }

  inline Result<EntryId, QMResult> GetEntryIdByColumn(Column aColumn) {
    EntryId value;
    QM_TRY(QM_TO_RESULT(mStmt->GetBlobAsUTF8String(aColumn, value)));

    return value;
  }

  inline Result<Name, QMResult> GetNameByColumn(Column aColumn) {
    Name value;
    QM_TRY(QM_TO_RESULT(mStmt->GetBlobAsString(aColumn, value)));

    return value;
  }

  inline Result<Usage, QMResult> GetUsageByColumn(Column aColumn) {
    Usage value = 0;
    QM_TRY(QM_TO_RESULT(mStmt->GetInt64(aColumn, &value)));

    return value;
  }

  inline bool IsNullByColumn(Column aColumn) const {
    bool value = mStmt->IsNull(aColumn);

    return value;
  }

  inline nsresult Execute() { return mStmt->Execute(); }

  inline Result<bool, QMResult> ExecuteStep() {
    bool hasEntries = false;
    QM_TRY(QM_TO_RESULT(mStmt->ExecuteStep(&hasEntries)));

    return hasEntries;
  }

  inline Result<bool, QMResult> YesOrNoQuery() {
    bool hasEntries = false;
    QM_TRY(QM_TO_RESULT(mStmt->ExecuteStep(&hasEntries)));
    MOZ_ALWAYS_TRUE(hasEntries);
    return GetBoolByColumn(0u);
  }

 private:
  underlying_t mStmt;
};

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_PARENT_RESULTSTATEMENT_H_
