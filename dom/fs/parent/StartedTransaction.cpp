/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StartedTransaction.h"

#include "ResultConnection.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"

namespace mozilla::dom::fs {

/* static */
Result<StartedTransaction, QMResult> StartedTransaction::Create(
    const ResultConnection& aConn) {
  auto transaction = MakeUnique<mozStorageTransaction>(
      aConn.get(), /* aCommitOnComplete */ false,
      mozIStorageConnection::TRANSACTION_IMMEDIATE);

  QM_TRY(QM_TO_RESULT(transaction->Start()));

  return StartedTransaction(std::move(transaction));
}

nsresult StartedTransaction::Commit() { return mTransaction->Commit(); }

nsresult StartedTransaction::Rollback() { return mTransaction->Rollback(); }

StartedTransaction::StartedTransaction(
    UniquePtr<mozStorageTransaction>&& aTransaction)
    : mTransaction(std::move(aTransaction)) {}

}  // namespace mozilla::dom::fs
