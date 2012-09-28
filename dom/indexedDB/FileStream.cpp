/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileStream.h"

#include "nsIFile.h"

#include "nsThreadUtils.h"
#include "test_quota.h"

USING_INDEXEDDB_NAMESPACE

NS_IMPL_THREADSAFE_ADDREF(FileStream)
NS_IMPL_THREADSAFE_RELEASE(FileStream)

NS_INTERFACE_MAP_BEGIN(FileStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStandardFileStream)
  NS_INTERFACE_MAP_ENTRY(nsISeekableStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsIStandardFileStream)
  NS_INTERFACE_MAP_ENTRY(nsIFileMetadata)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
FileStream::Seek(int32_t aWhence, int64_t aOffset)
{
  // TODO: Add support for 64 bit file sizes, bug 752431
  NS_ENSURE_TRUE(aOffset <= INT32_MAX, NS_ERROR_INVALID_ARG);

  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mQuotaFile) {
    return NS_BASE_STREAM_CLOSED;
  }

  int whence;
  switch (aWhence) {
    case nsISeekableStream::NS_SEEK_SET:
      whence = SEEK_SET;
      break;
    case nsISeekableStream::NS_SEEK_CUR:
      whence = SEEK_CUR;
      break;
    case nsISeekableStream::NS_SEEK_END:
      whence = SEEK_END;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  NS_ASSERTION(!NS_IsMainThread(), "Performing sync IO on the main thread!");

  int rc = sqlite3_quota_fseek(mQuotaFile, aOffset, whence);
  NS_ENSURE_TRUE(rc == 0, NS_BASE_STREAM_OSERROR);

  return NS_OK;
}

NS_IMETHODIMP
FileStream::Tell(int64_t* aResult)
{
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mQuotaFile) {
    return NS_BASE_STREAM_CLOSED;
  }

  NS_ASSERTION(!NS_IsMainThread(), "Performing sync IO on the main thread!");

  long rc = sqlite3_quota_ftell(mQuotaFile);
  NS_ENSURE_TRUE(rc >= 0, NS_BASE_STREAM_OSERROR);

  *aResult = rc;
  return NS_OK;
}

NS_IMETHODIMP
FileStream::SetEOF()
{
  int64_t pos;
  nsresult rv = Tell(&pos);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(!NS_IsMainThread(), "Performing sync IO on the main thread!");

  int rc = sqlite3_quota_ftruncate(mQuotaFile, pos);
  NS_ENSURE_TRUE(rc == 0, NS_BASE_STREAM_OSERROR);

  return NS_OK;
}


NS_IMETHODIMP
FileStream::Close()
{
  CleanUpOpen();

  if (mQuotaFile) {
    NS_ASSERTION(!NS_IsMainThread(), "Performing sync IO on the main thread!");

    int rc = sqlite3_quota_fclose(mQuotaFile);
    mQuotaFile = nullptr;

    NS_ENSURE_TRUE(rc == 0, NS_BASE_STREAM_OSERROR);
  }

  return NS_OK;
}

NS_IMETHODIMP
FileStream::Available(uint64_t* aResult)
{
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mQuotaFile) {
    return NS_BASE_STREAM_CLOSED;
  }

  NS_ASSERTION(!NS_IsMainThread(), "Performing sync IO on the main thread!");

  long rc = sqlite3_quota_file_available(mQuotaFile);
  NS_ENSURE_TRUE(rc >= 0, NS_BASE_STREAM_OSERROR);

  *aResult = rc;
  return NS_OK;
}

NS_IMETHODIMP
FileStream::Read(char* aBuf, uint32_t aCount, uint32_t* aResult)
{
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mQuotaFile) {
    return NS_BASE_STREAM_CLOSED;
  }

  NS_ASSERTION(!NS_IsMainThread(), "Performing sync IO on the main thread!");

  size_t bytesRead = sqlite3_quota_fread(aBuf, 1, aCount, mQuotaFile);
  if (bytesRead < aCount && sqlite3_quota_ferror(mQuotaFile)) {
    return NS_BASE_STREAM_OSERROR;
  }

  *aResult = bytesRead;
  return NS_OK;
}

NS_IMETHODIMP
FileStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                         uint32_t aCount, uint32_t* aResult)
{
  NS_NOTREACHED("Don't call me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileStream::IsNonBlocking(bool *aNonBlocking)
{
  *aNonBlocking = false;
  return NS_OK;
}

NS_IMETHODIMP
FileStream::Write(const char* aBuf, uint32_t aCount, uint32_t *aResult)
{
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mQuotaFile) {
    return NS_BASE_STREAM_CLOSED;
  }

  NS_ASSERTION(!NS_IsMainThread(), "Performing sync IO on the main thread!");

  size_t bytesWritten = sqlite3_quota_fwrite(aBuf, 1, aCount, mQuotaFile);
  if (bytesWritten < aCount) {
    return NS_BASE_STREAM_OSERROR;
  }

  *aResult = bytesWritten;
  return NS_OK;
}

NS_IMETHODIMP
FileStream::Flush()
{
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mQuotaFile) {
    return NS_BASE_STREAM_CLOSED;
  }

  NS_ASSERTION(!NS_IsMainThread(), "Performing sync IO on the main thread!");

  int rc = sqlite3_quota_fflush(mQuotaFile, 1);
  NS_ENSURE_TRUE(rc == 0, NS_BASE_STREAM_OSERROR);

  return NS_OK;
}

NS_IMETHODIMP
FileStream::WriteFrom(nsIInputStream *inStr, uint32_t count, uint32_t *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileStream::WriteSegments(nsReadSegmentFun reader, void * closure, uint32_t count, uint32_t *_retval)
{
  NS_NOTREACHED("Don't call me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileStream::Init(nsIFile* aFile, const nsAString& aMode, int32_t aFlags)
{
  NS_ASSERTION(!mQuotaFile && !mDeferredOpen, "Already initialized!");

  nsresult rv = aFile->GetPath(mFilePath);
  NS_ENSURE_SUCCESS(rv, rv);

  mMode = aMode;
  mFlags = aFlags;
 
  if (mFlags & nsIStandardFileStream::FLAGS_DEFER_OPEN) {
    mDeferredOpen = true;
    return NS_OK;
  }

  return DoOpen();
}

NS_IMETHODIMP
FileStream::GetSize(int64_t* _retval)
{
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mQuotaFile) {
    return NS_BASE_STREAM_CLOSED;
  }

  NS_ASSERTION(!NS_IsMainThread(), "Performing sync IO on the main thread!");

  // TODO: Use sqlite3_quota_file_size() here, bug 760783
  int64_t rc = sqlite3_quota_file_truesize(mQuotaFile);

  NS_ASSERTION(rc >= 0, "The file is not under quota management!");

  *_retval = rc;
  return NS_OK;
}

NS_IMETHODIMP
FileStream::GetLastModified(int64_t* _retval)
{
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mQuotaFile) {
    return NS_BASE_STREAM_CLOSED;
  }

  NS_ASSERTION(!NS_IsMainThread(), "Performing sync IO on the main thread!");

  time_t mtime;
  int rc = sqlite3_quota_file_mtime(mQuotaFile, &mtime);
  NS_ENSURE_TRUE(rc == 0, NS_BASE_STREAM_OSERROR);

  *_retval = mtime * PR_MSEC_PER_SEC;
  return NS_OK;
}

NS_IMETHODIMP
FileStream::FlushBuffers()
{
  nsresult rv = DoPendingOpen();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mQuotaFile) {
    return NS_BASE_STREAM_CLOSED;
  }

  NS_ASSERTION(!NS_IsMainThread(), "Performing sync IO on the main thread!");

  int rc = sqlite3_quota_fflush(mQuotaFile, 0);
  NS_ENSURE_TRUE(rc == 0, NS_BASE_STREAM_OSERROR);

  return NS_OK;
}

nsresult
FileStream::DoOpen()
{
  NS_ASSERTION(!mFilePath.IsEmpty(), "Must have a file path");

  NS_ASSERTION(!NS_IsMainThread(), "Performing sync IO on the main thread!");

  quota_FILE* quotaFile =
    sqlite3_quota_fopen(NS_ConvertUTF16toUTF8(mFilePath).get(),
                        NS_ConvertUTF16toUTF8(mMode).get());

  CleanUpOpen();

  if (!quotaFile) {
    return NS_BASE_STREAM_OSERROR;
  }

  mQuotaFile = quotaFile;

  return NS_OK;
}
