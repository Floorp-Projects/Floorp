/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/StorageHelpers.h"

#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"

namespace mozilla::dom::quota {

AutoDatabaseAttacher::AutoDatabaseAttacher(
    nsCOMPtr<mozIStorageConnection> aConnection,
    nsCOMPtr<nsIFile> aDatabaseFile, const nsLiteralCString& aSchemaName)
    : mConnection(std::move(aConnection)),
      mDatabaseFile(std::move(aDatabaseFile)),
      mSchemaName(aSchemaName),
      mAttached(false) {}

AutoDatabaseAttacher::~AutoDatabaseAttacher() {
  if (mAttached) {
    QM_WARNONLY_TRY(MOZ_TO_RESULT(Detach()));
  }
}

nsresult AutoDatabaseAttacher::Attach() {
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(mDatabaseFile);
  MOZ_ASSERT(!mAttached);

#ifdef DEBUG
  {
    QM_TRY_INSPECT(const bool& exists,
                   MOZ_TO_RESULT_INVOKE_MEMBER(mDatabaseFile, Exists));

    MOZ_ASSERT(exists);
  }
#endif

  QM_TRY_INSPECT(const auto& path, MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
                                       nsString, mDatabaseFile, GetPath));

  QM_TRY_INSPECT(
      const auto& stmt,
      MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
          nsCOMPtr<mozIStorageStatement>, mConnection, CreateStatement,
          "ATTACH DATABASE :path AS "_ns + mSchemaName + ";"_ns));

  QM_TRY(MOZ_TO_RESULT(stmt->BindStringByName("path"_ns, path)));
  QM_TRY(MOZ_TO_RESULT(stmt->Execute()));

  mAttached = true;

  return NS_OK;
}

nsresult AutoDatabaseAttacher::Detach() {
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(mAttached);

  QM_TRY(MOZ_TO_RESULT(
      mConnection->ExecuteSimpleSQL("DETACH DATABASE "_ns + mSchemaName)));

  mAttached = false;
  return NS_OK;
}

}  // namespace mozilla::dom::quota
