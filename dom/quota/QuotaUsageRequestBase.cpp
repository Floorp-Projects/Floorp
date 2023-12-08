/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaUsageRequestBase.h"

#include "mozilla/dom/quota/Assertions.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/FileUtils.h"
#include "mozilla/dom/quota/PQuotaRequest.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "nsIFile.h"

namespace mozilla::dom::quota {

Result<UsageInfo, nsresult> QuotaUsageRequestBase::GetUsageForOrigin(
    QuotaManager& aQuotaManager, PersistenceType aPersistenceType,
    const OriginMetadata& aOriginMetadata) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginMetadata.mPersistenceType == aPersistenceType);

  QM_TRY_INSPECT(const auto& directory,
                 aQuotaManager.GetOriginDirectory(aOriginMetadata));

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE_MEMBER(directory, Exists));

  if (!exists || mCanceled) {
    return UsageInfo();
  }

  // If the directory exists then enumerate all the files inside, adding up
  // the sizes to get the final usage statistic.
  bool initialized;

  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    initialized = aQuotaManager.IsOriginInitialized(aOriginMetadata.mOrigin);
  } else {
    initialized = aQuotaManager.IsTemporaryStorageInitializedInternal();
  }

  return GetUsageForOriginEntries(aQuotaManager, aPersistenceType,
                                  aOriginMetadata, *directory, initialized);
}

Result<UsageInfo, nsresult> QuotaUsageRequestBase::GetUsageForOriginEntries(
    QuotaManager& aQuotaManager, PersistenceType aPersistenceType,
    const OriginMetadata& aOriginMetadata, nsIFile& aDirectory,
    const bool aInitialized) {
  AssertIsOnIOThread();

  QM_TRY_RETURN((ReduceEachFileAtomicCancelable(
      aDirectory, mCanceled, UsageInfo{},
      [&](UsageInfo oldUsageInfo, const nsCOMPtr<nsIFile>& file)
          -> mozilla::Result<UsageInfo, nsresult> {
        QM_TRY_INSPECT(
            const auto& leafName,
            MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsAutoString, file, GetLeafName));

        QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*file));

        switch (dirEntryKind) {
          case nsIFileKind::ExistsAsDirectory: {
            Client::Type clientType;
            const bool ok =
                Client::TypeFromText(leafName, clientType, fallible);
            if (!ok) {
              // Unknown directories during getting usage for an origin (even
              // for an uninitialized origin) are now allowed. Just warn if we
              // find them.
              UNKNOWN_FILE_WARNING(leafName);
              break;
            }

            Client* const client = aQuotaManager.GetClient(clientType);
            MOZ_ASSERT(client);

            QM_TRY_INSPECT(
                const auto& usageInfo,
                aInitialized ? client->GetUsageForOrigin(
                                   aPersistenceType, aOriginMetadata, mCanceled)
                             : client->InitOrigin(aPersistenceType,
                                                  aOriginMetadata, mCanceled));
            return oldUsageInfo + usageInfo;
          }

          case nsIFileKind::ExistsAsFile:
            // We are maintaining existing behavior for unknown files here (just
            // continuing).
            // This can possibly be used by developers to add temporary backups
            // into origin directories without losing get usage functionality.
            if (IsTempMetadata(leafName)) {
              if (!aInitialized) {
                QM_TRY(MOZ_TO_RESULT(file->Remove(/* recursive */ false)));
              }

              break;
            }

            if (IsOriginMetadata(leafName) || IsOSMetadata(leafName) ||
                IsDotFile(leafName)) {
              break;
            }

            // Unknown files during getting usage for an origin (even for an
            // uninitialized origin) are now allowed. Just warn if we find them.
            UNKNOWN_FILE_WARNING(leafName);
            break;

          case nsIFileKind::DoesNotExist:
            // Ignore files that got removed externally while iterating.
            break;
        }

        return oldUsageInfo;
      })));
}

void QuotaUsageRequestBase::SendResults() {
  AssertIsOnOwningThread();

  if (IsActorDestroyed()) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = NS_ERROR_FAILURE;
    }
  } else {
    if (mCanceled) {
      mResultCode = NS_ERROR_FAILURE;
    }

    UsageRequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      GetResponse(response);
    } else {
      response = mResultCode;
    }

    Unused << PQuotaUsageRequestParent::Send__delete__(this, response);
  }
}

void QuotaUsageRequestBase::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

mozilla::ipc::IPCResult QuotaUsageRequestBase::RecvCancel() {
  AssertIsOnOwningThread();

  if (mCanceled.exchange(true)) {
    NS_WARNING("Canceled more than once?!");
    return IPC_FAIL(this, "Request canceled more than once");
  }

  return IPC_OK();
}

}  // namespace mozilla::dom::quota
