/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileStreams.h"

#include "QuotaManager.h"
#include "prio.h"

USING_QUOTA_NAMESPACE

template <class FileStreamBase>
NS_IMETHODIMP
FileQuotaStream<FileStreamBase>::SetEOF()
{
  nsresult rv = FileStreamBase::SetEOF();
  NS_ENSURE_SUCCESS(rv, rv);

  if (mQuotaObject) {
    int64_t offset;
    nsresult rv = FileStreamBase::Tell(&offset);
    NS_ENSURE_SUCCESS(rv, rv);

    mQuotaObject->UpdateSize(offset);
  }

  return NS_OK;
}

template <class FileStreamBase>
NS_IMETHODIMP
FileQuotaStream<FileStreamBase>::Close()
{
  nsresult rv = FileStreamBase::Close();
  NS_ENSURE_SUCCESS(rv, rv);

  mQuotaObject = nullptr;

  return NS_OK;
}

template <class FileStreamBase>
nsresult
FileQuotaStream<FileStreamBase>::DoOpen()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  NS_ASSERTION(!mQuotaObject, "Creating quota object more than once?");
  mQuotaObject = quotaManager->GetQuotaObject(mPersistenceType, mGroup, mOrigin,
    FileStreamBase::mOpenParams.localFile);

  nsresult rv = FileStreamBase::DoOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  if (mQuotaObject && (FileStreamBase::mOpenParams.ioFlags & PR_TRUNCATE)) {
    mQuotaObject->UpdateSize(0);
  }

  return NS_OK;
}

template <class FileStreamBase>
NS_IMETHODIMP
FileQuotaStreamWithWrite<FileStreamBase>::Write(const char* aBuf,
                                                uint32_t aCount,
                                                uint32_t* _retval)
{
  nsresult rv;

  if (FileQuotaStreamWithWrite::mQuotaObject) {
    int64_t offset;
    rv = FileStreamBase::Tell(&offset);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!FileQuotaStreamWithWrite::
         mQuotaObject->MaybeAllocateMoreSpace(offset, aCount)) {
      return NS_ERROR_FAILURE;
    }
  }

  rv = FileStreamBase::Write(aBuf, aCount, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED0(FileInputStream, nsFileInputStream)

already_AddRefed<FileInputStream>
FileInputStream::Create(PersistenceType aPersistenceType,
                        const nsACString& aGroup, const nsACString& aOrigin,
                        nsIFile* aFile, int32_t aIOFlags, int32_t aPerm,
                        int32_t aBehaviorFlags)
{
  nsRefPtr<FileInputStream> stream =
    new FileInputStream(aPersistenceType, aGroup, aOrigin);
  nsresult rv = stream->Init(aFile, aIOFlags, aPerm, aBehaviorFlags);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return stream.forget();
}

NS_IMPL_ISUPPORTS_INHERITED0(FileOutputStream, nsFileOutputStream)

already_AddRefed<FileOutputStream>
FileOutputStream::Create(PersistenceType aPersistenceType,
                         const nsACString& aGroup, const nsACString& aOrigin,
                         nsIFile* aFile, int32_t aIOFlags, int32_t aPerm,
                         int32_t aBehaviorFlags)
{
  nsRefPtr<FileOutputStream> stream =
    new FileOutputStream(aPersistenceType, aGroup, aOrigin);
  nsresult rv = stream->Init(aFile, aIOFlags, aPerm, aBehaviorFlags);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return stream.forget();
}

NS_IMPL_ISUPPORTS_INHERITED0(FileStream, nsFileStream)

already_AddRefed<FileStream>
FileStream::Create(PersistenceType aPersistenceType, const nsACString& aGroup,
                   const nsACString& aOrigin, nsIFile* aFile, int32_t aIOFlags,
                   int32_t aPerm, int32_t aBehaviorFlags)
{
  nsRefPtr<FileStream> stream =
    new FileStream(aPersistenceType, aGroup, aOrigin);
  nsresult rv = stream->Init(aFile, aIOFlags, aPerm, aBehaviorFlags);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return stream.forget();
}
