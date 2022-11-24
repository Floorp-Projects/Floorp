/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileStreams.h"

// Local includes
#include "QuotaCommon.h"
#include "QuotaManager.h"
#include "QuotaObject.h"

// Global includes
#include <utility>
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Result.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsDebug.h"
#include "prio.h"

namespace mozilla::dom::quota {

template <class FileStreamBase>
NS_IMETHODIMP FileQuotaStream<FileStreamBase>::SetEOF() {
  // If the stream is not quota tracked, or on an early or late stage in the
  // lifecycle, mQuotaObject is null. Under these circumstances,
  // we don't check the quota limit in order to avoid breakage.
  if (mQuotaObject) {
    int64_t offset = 0;
    QM_TRY(MOZ_TO_RESULT(FileStreamBase::Tell(&offset)));

    QM_TRY(OkIf(mQuotaObject->MaybeUpdateSize(offset, /* aTruncate */ true)),
           NS_ERROR_FILE_NO_DEVICE_SPACE);
  }

  QM_TRY(MOZ_TO_RESULT(FileStreamBase::SetEOF()));

  return NS_OK;
}

template <class FileStreamBase>
NS_IMETHODIMP FileQuotaStream<FileStreamBase>::Close() {
  QM_TRY(MOZ_TO_RESULT(FileStreamBase::Close()));

  mQuotaObject = nullptr;

  return NS_OK;
}

template <class FileStreamBase>
nsresult FileQuotaStream<FileStreamBase>::DoOpen() {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager, "Shouldn't be null!");

  MOZ_ASSERT(!mQuotaObject, "Creating quota object more than once?");
  mQuotaObject = quotaManager->GetQuotaObject(
      mPersistenceType, mOriginMetadata, mClientType,
      FileStreamBase::mOpenParams.localFile);

  QM_TRY(MOZ_TO_RESULT(FileStreamBase::DoOpen()));

  if (mQuotaObject && (FileStreamBase::mOpenParams.ioFlags & PR_TRUNCATE)) {
    DebugOnly<bool> res =
        mQuotaObject->MaybeUpdateSize(0, /* aTruncate */ true);
    MOZ_ASSERT(res);
  }

  return NS_OK;
}

template <class FileStreamBase>
NS_IMETHODIMP FileQuotaStreamWithWrite<FileStreamBase>::Write(
    const char* aBuf, uint32_t aCount, uint32_t* _retval) {
  if (FileQuotaStreamWithWrite::mQuotaObject) {
    int64_t offset;
    QM_TRY(MOZ_TO_RESULT(FileStreamBase::Tell(&offset)));

    MOZ_ASSERT(INT64_MAX - offset >= int64_t(aCount));

    if (!FileQuotaStreamWithWrite::mQuotaObject->MaybeUpdateSize(
            offset + int64_t(aCount),
            /* aTruncate */ false)) {
      return NS_ERROR_FILE_NO_DEVICE_SPACE;
    }
  }

  QM_TRY(MOZ_TO_RESULT(FileStreamBase::Write(aBuf, aCount, _retval)));

  return NS_OK;
}

Result<MovingNotNull<nsCOMPtr<nsIInputStream>>, nsresult> CreateFileInputStream(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    Client::Type aClientType, nsIFile* aFile, int32_t aIOFlags, int32_t aPerm,
    int32_t aBehaviorFlags) {
  auto stream = MakeRefPtr<FileInputStream>(aPersistenceType, aOriginMetadata,
                                            aClientType);

  QM_TRY(MOZ_TO_RESULT(stream->Init(aFile, aIOFlags, aPerm, aBehaviorFlags)));

  return WrapMovingNotNullUnchecked(
      nsCOMPtr<nsIInputStream>(std::move(stream)));
}

Result<MovingNotNull<nsCOMPtr<nsIOutputStream>>, nsresult>
CreateFileOutputStream(PersistenceType aPersistenceType,
                       const OriginMetadata& aOriginMetadata,
                       Client::Type aClientType, nsIFile* aFile,
                       int32_t aIOFlags, int32_t aPerm,
                       int32_t aBehaviorFlags) {
  auto stream = MakeRefPtr<FileOutputStream>(aPersistenceType, aOriginMetadata,
                                             aClientType);

  QM_TRY(MOZ_TO_RESULT(stream->Init(aFile, aIOFlags, aPerm, aBehaviorFlags)));

  return WrapMovingNotNullUnchecked(
      nsCOMPtr<nsIOutputStream>(std::move(stream)));
}

Result<MovingNotNull<nsCOMPtr<nsIRandomAccessStream>>, nsresult>
CreateFileRandomAccessStream(PersistenceType aPersistenceType,
                             const OriginMetadata& aOriginMetadata,
                             Client::Type aClientType, nsIFile* aFile,
                             int32_t aIOFlags, int32_t aPerm,
                             int32_t aBehaviorFlags) {
  auto stream = MakeRefPtr<FileRandomAccessStream>(
      aPersistenceType, aOriginMetadata, aClientType);

  QM_TRY(MOZ_TO_RESULT(stream->Init(aFile, aIOFlags, aPerm, aBehaviorFlags)));

  return WrapMovingNotNullUnchecked(
      nsCOMPtr<nsIRandomAccessStream>(std::move(stream)));
}

}  // namespace mozilla::dom::quota
