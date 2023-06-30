/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_STARTEDTRANSACTION_H_
#define DOM_FS_PARENT_STARTEDTRANSACTION_H_

#include "ResultConnection.h"
#include "mozStorageHelper.h"
#include "mozilla/dom/QMResult.h"

namespace mozilla::dom::fs {

class StartedTransaction {
 public:
  static Result<StartedTransaction, QMResult> Create(
      const ResultConnection& aConn);

  StartedTransaction(StartedTransaction&& aOther) = default;

  StartedTransaction(const StartedTransaction& aOther) = delete;

  nsresult Commit();

  nsresult Rollback();

  ~StartedTransaction() = default;

 private:
  explicit StartedTransaction(UniquePtr<mozStorageTransaction>&& aTransaction);

  UniquePtr<mozStorageTransaction> mTransaction;
};

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_PARENT_STARTEDTRANSACTION_H_
