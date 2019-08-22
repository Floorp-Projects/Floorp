/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileStreams.h"

#include "QuotaManager.h"
#include "prio.h"

BEGIN_QUOTA_NAMESPACE

template <class FileStreamBase>
NS_IMETHODIMP FileQuotaStream<FileStreamBase>::SetEOF() {
  nsresult rv = FileStreamBase::SetEOF();
  NS_ENSURE_SUCCESS(rv, rv);

  if (mQuotaObject) {
    int64_t offset;
    nsresult rv = FileStreamBase::Tell(&offset);
    NS_ENSURE_SUCCESS(rv, rv);

    mQuotaObject->MaybeUpdateSize(offset, /* aTruncate */ true);
  }

  return NS_OK;
}

template <class FileStreamBase>
NS_IMETHODIMP FileQuotaStream<FileStreamBase>::Close() {
  nsresult rv = FileStreamBase::Close();
  NS_ENSURE_SUCCESS(rv, rv);

  mQuotaObject = nullptr;

  return NS_OK;
}

template <class FileStreamBase>
nsresult FileQuotaStream<FileStreamBase>::DoOpen() {
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  NS_ASSERTION(!mQuotaObject, "Creating quota object more than once?");
  mQuotaObject = quotaManager->GetQuotaObject(
      mPersistenceType, mGroup, mOrigin, mClientType,
      FileStreamBase::mOpenParams.localFile);

  nsresult rv = FileStreamBase::DoOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  if (mQuotaObject && (FileStreamBase::mOpenParams.ioFlags & PR_TRUNCATE)) {
    mQuotaObject->MaybeUpdateSize(0, /* aTruncate */ true);
  }

  return NS_OK;
}

template <class FileStreamBase>
NS_IMETHODIMP FileQuotaStreamWithWrite<FileStreamBase>::Write(
    const char* aBuf, uint32_t aCount, uint32_t* _retval) {
  nsresult rv;

  if (FileQuotaStreamWithWrite::mQuotaObject) {
    int64_t offset;
    rv = FileStreamBase::Tell(&offset);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_ASSERT(INT64_MAX - offset >= int64_t(aCount));

    if (!FileQuotaStreamWithWrite::mQuotaObject->MaybeUpdateSize(
            offset + int64_t(aCount),
            /* aTruncate */ false)) {
      return NS_ERROR_FILE_NO_DEVICE_SPACE;
    }
  }

  rv = FileStreamBase::Write(aBuf, aCount, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

already_AddRefed<FileInputStream> CreateFileInputStream(
    PersistenceType aPersistenceType, const nsACString& aGroup,
    const nsACString& aOrigin, Client::Type aClientType, nsIFile* aFile,
    int32_t aIOFlags, int32_t aPerm, int32_t aBehaviorFlags) {
  RefPtr<FileInputStream> stream =
      new FileInputStream(aPersistenceType, aGroup, aOrigin, aClientType);
  nsresult rv = stream->Init(aFile, aIOFlags, aPerm, aBehaviorFlags);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return stream.forget();
}

already_AddRefed<FileOutputStream> CreateFileOutputStream(
    PersistenceType aPersistenceType, const nsACString& aGroup,
    const nsACString& aOrigin, Client::Type aClientType, nsIFile* aFile,
    int32_t aIOFlags, int32_t aPerm, int32_t aBehaviorFlags) {
  RefPtr<FileOutputStream> stream =
      new FileOutputStream(aPersistenceType, aGroup, aOrigin, aClientType);
  nsresult rv = stream->Init(aFile, aIOFlags, aPerm, aBehaviorFlags);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return stream.forget();
}

already_AddRefed<FileStream> CreateFileStream(
    PersistenceType aPersistenceType, const nsACString& aGroup,
    const nsACString& aOrigin, Client::Type aClientType, nsIFile* aFile,
    int32_t aIOFlags, int32_t aPerm, int32_t aBehaviorFlags) {
  RefPtr<FileStream> stream =
      new FileStream(aPersistenceType, aGroup, aOrigin, aClientType);
  nsresult rv = stream->Init(aFile, aIOFlags, aPerm, aBehaviorFlags);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return stream.forget();
}

END_QUOTA_NAMESPACE
