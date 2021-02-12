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
#include "nsDebug.h"
#include "prio.h"

namespace mozilla::dom::quota {

template <class FileStreamBase>
NS_IMETHODIMP FileQuotaStream<FileStreamBase>::SetEOF() {
  QM_TRY(FileStreamBase::SetEOF());

  if (mQuotaObject) {
    int64_t offset;
    QM_TRY(FileStreamBase::Tell(&offset));

    DebugOnly<bool> res =
        mQuotaObject->MaybeUpdateSize(offset, /* aTruncate */ true);
    MOZ_ASSERT(res);
  }

  return NS_OK;
}

template <class FileStreamBase>
NS_IMETHODIMP FileQuotaStream<FileStreamBase>::Close() {
  QM_TRY(FileStreamBase::Close());

  mQuotaObject = nullptr;

  return NS_OK;
}

template <class FileStreamBase>
nsresult FileQuotaStream<FileStreamBase>::DoOpen() {
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  NS_ASSERTION(!mQuotaObject, "Creating quota object more than once?");
  mQuotaObject = quotaManager->GetQuotaObject(
      mPersistenceType, mOriginMetadata, mClientType,
      FileStreamBase::mOpenParams.localFile);

  QM_TRY(FileStreamBase::DoOpen());

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
    QM_TRY(FileStreamBase::Tell(&offset));

    MOZ_ASSERT(INT64_MAX - offset >= int64_t(aCount));

    if (!FileQuotaStreamWithWrite::mQuotaObject->MaybeUpdateSize(
            offset + int64_t(aCount),
            /* aTruncate */ false)) {
      return NS_ERROR_FILE_NO_DEVICE_SPACE;
    }
  }

  QM_TRY(FileStreamBase::Write(aBuf, aCount, _retval));

  return NS_OK;
}

Result<NotNull<RefPtr<FileInputStream>>, nsresult> CreateFileInputStream(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    Client::Type aClientType, nsIFile* aFile, int32_t aIOFlags, int32_t aPerm,
    int32_t aBehaviorFlags) {
  const auto stream = MakeNotNull<RefPtr<FileInputStream>>(
      aPersistenceType, aOriginMetadata, aClientType);

  QM_TRY(stream->Init(aFile, aIOFlags, aPerm, aBehaviorFlags));

  return stream;
}

Result<NotNull<RefPtr<FileOutputStream>>, nsresult> CreateFileOutputStream(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    Client::Type aClientType, nsIFile* aFile, int32_t aIOFlags, int32_t aPerm,
    int32_t aBehaviorFlags) {
  const auto stream = MakeNotNull<RefPtr<FileOutputStream>>(
      aPersistenceType, aOriginMetadata, aClientType);

  QM_TRY(stream->Init(aFile, aIOFlags, aPerm, aBehaviorFlags));

  return stream;
}

Result<NotNull<RefPtr<FileStream>>, nsresult> CreateFileStream(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    Client::Type aClientType, nsIFile* aFile, int32_t aIOFlags, int32_t aPerm,
    int32_t aBehaviorFlags) {
  const auto stream = MakeNotNull<RefPtr<FileStream>>(
      aPersistenceType, aOriginMetadata, aClientType);

  QM_TRY(stream->Init(aFile, aIOFlags, aPerm, aBehaviorFlags));

  return stream;
}

}  // namespace mozilla::dom::quota
