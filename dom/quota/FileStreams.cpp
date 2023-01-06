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
#include "RemoteQuotaObject.h"

// Global includes
#include <utility>
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Result.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/RandomAccessStreamParams.h"
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

  if (mQuotaObject) {
    if (auto* remoteQuotaObject = mQuotaObject->AsRemoteQuotaObject()) {
      remoteQuotaObject->Close();
    }

    mQuotaObject = nullptr;
  }

  return NS_OK;
}

template <class FileStreamBase>
nsresult FileQuotaStream<FileStreamBase>::DoOpen() {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  MOZ_RELEASE_ASSERT(!mDeserialized);

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
      *_retval = 0;
      return NS_ERROR_FILE_NO_DEVICE_SPACE;
    }
  }

  QM_TRY(MOZ_TO_RESULT(FileStreamBase::Write(aBuf, aCount, _retval)));

  return NS_OK;
}

mozilla::ipc::RandomAccessStreamParams FileRandomAccessStream::Serialize(
    nsIInterfaceRequestor* aCallbacks) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  MOZ_RELEASE_ASSERT(!mDeserialized);
  MOZ_ASSERT(mOpenParams.localFile);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  RefPtr<QuotaObject> quotaObject = quotaManager->GetQuotaObject(
      mPersistenceType, mOriginMetadata, mClientType, mOpenParams.localFile);
  MOZ_ASSERT(quotaObject);

  IPCQuotaObject ipcQuotaObject = quotaObject->Serialize(aCallbacks);

  mozilla::ipc::RandomAccessStreamParams randomAccessStreamParams =
      nsFileRandomAccessStream::Serialize(aCallbacks);

  MOZ_ASSERT(
      randomAccessStreamParams.type() ==
      mozilla::ipc::RandomAccessStreamParams::TFileRandomAccessStreamParams);

  mozilla::ipc::LimitingFileRandomAccessStreamParams
      limitingFileRandomAccessStreamParams;
  limitingFileRandomAccessStreamParams.fileRandomAccessStreamParams() =
      std::move(randomAccessStreamParams);
  limitingFileRandomAccessStreamParams.quotaObject() =
      std::move(ipcQuotaObject);

  return limitingFileRandomAccessStreamParams;
}

bool FileRandomAccessStream::Deserialize(
    mozilla::ipc::RandomAccessStreamParams& aParams) {
  MOZ_ASSERT(aParams.type() == mozilla::ipc::RandomAccessStreamParams::
                                   TLimitingFileRandomAccessStreamParams);

  auto& params = aParams.get_LimitingFileRandomAccessStreamParams();

  mozilla::ipc::RandomAccessStreamParams randomAccessStreamParams(
      std::move(params.fileRandomAccessStreamParams()));

  QM_TRY(MOZ_TO_RESULT(
             nsFileRandomAccessStream::Deserialize(randomAccessStreamParams)),
         false);

  mQuotaObject = QuotaObject::Deserialize(params.quotaObject());

  return true;
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
