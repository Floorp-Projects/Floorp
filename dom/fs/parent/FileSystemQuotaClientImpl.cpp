/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemQuotaClientImpl.h"
#include "datamodel/FileSystemDataManager.h"

#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/UsageInfo.h"

namespace mozilla::dom::fs::usage {

Result<quota::UsageInfo, nsresult> GetUsageForOrigin(
    quota::PersistenceType aPersistenceType,
    const quota::OriginMetadata& aOriginMetadata,
    const Atomic<bool>& aCanceled) {
  quota::AssertIsOnIOThread();

  auto toNSResult = [](const auto& aRv) { return aRv.NSResult(); };

  QM_TRY_UNWRAP(fs::data::FileSystemDataManager * dm,
                fs::data::FileSystemDataManager::CreateFileSystemDataManager(
                    aOriginMetadata.mOrigin)
                    .mapErr(toNSResult));
  UniquePtr<fs::data::FileSystemDataManager> access(dm);

  QM_TRY_UNWRAP(int64_t usage, access->GetUsage().mapErr(toNSResult));

  quota::UsageInfo usageInfo{quota::DatabaseUsageType(Some(usage))};
  return usageInfo;
}

nsresult AboutToClearOrigins(
    const Nullable<quota::PersistenceType>& aPersistenceType,
    const quota::OriginScope& aOriginScope) {
  return NS_OK;
}

void StartIdleMaintenance() {}

void StopIdleMaintenance() {}

}  // namespace mozilla::dom::fs::usage
