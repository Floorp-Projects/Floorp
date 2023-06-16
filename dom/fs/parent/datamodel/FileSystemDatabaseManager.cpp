/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDatabaseManager.h"

#include "FileSystemDatabaseManagerVersion001.h"
#include "FileSystemDatabaseManagerVersion002.h"
#include "FileSystemFileManager.h"
#include "ResultConnection.h"
#include "ResultStatement.h"
#include "SchemaVersion001.h"
#include "SchemaVersion002.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"

namespace mozilla::dom::fs::data {

namespace {

Result<Usage, QMResult> GetFileUsage(const ResultConnection& aConnection) {
  DatabaseVersion version = 0;
  QM_TRY(QM_TO_RESULT(aConnection->GetSchemaVersion(&version)));

  switch (version) {
    case 0: {
      return 0;
    }

    case 1: {
      QM_TRY_RETURN(
          FileSystemDatabaseManagerVersion001::GetFileUsage(aConnection));
    }

    case 2: {
      QM_TRY_RETURN(
          FileSystemDatabaseManagerVersion002::GetFileUsage(aConnection));
    }

    default:
      break;
  }

  return Err(QMResult(NS_ERROR_NOT_IMPLEMENTED));
}

}  // namespace

/* static */
nsresult FileSystemDatabaseManager::RescanUsages(
    const ResultConnection& aConnection,
    const quota::OriginMetadata& aOriginMetadata) {
  DatabaseVersion version = 0;
  QM_TRY(MOZ_TO_RESULT(aConnection->GetSchemaVersion(&version)));

  switch (version) {
    case 0: {
      return NS_OK;
    }

    case 1:
      return FileSystemDatabaseManagerVersion001::RescanTrackedUsages(
          aConnection, aOriginMetadata);

    case 2: {
      return FileSystemDatabaseManagerVersion002::RescanTrackedUsages(
          aConnection, aOriginMetadata);
    }

    default:
      break;
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* static */
Result<quota::UsageInfo, QMResult> FileSystemDatabaseManager::GetUsage(
    const ResultConnection& aConnection,
    const quota::OriginMetadata& aOriginMetadata) {
  QM_TRY_INSPECT(const auto& databaseFile, GetDatabaseFile(aOriginMetadata));

  // If database is deleted between connection creation and now, error
  int64_t dbSize = 0;
  QM_TRY(QM_TO_RESULT(databaseFile->GetFileSize(&dbSize)));

  quota::UsageInfo result(quota::DatabaseUsageType(Some(dbSize)));

  QM_TRY_INSPECT(const Usage& fileUsage, GetFileUsage(aConnection));

  // XXX: DatabaseUsage is currently total usage for most forms of storage
  result += quota::DatabaseUsageType(Some(fileUsage));

  return result;
}

}  // namespace mozilla::dom::fs::data
