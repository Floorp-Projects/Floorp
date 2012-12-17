/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileStreamWrappers.h"

#include "nsIFileStorage.h"
#include "nsISeekableStream.h"
#include "mozilla/Attributes.h"

#include "FileHelper.h"

USING_FILE_NAMESPACE

namespace {

class ProgressRunnable MOZ_FINAL : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  ProgressRunnable(FileHelper* aFileHelper,
                   uint64_t aProgress,
                   uint64_t aProgressMax)
  : mFileHelper(aFileHelper),
    mProgress(aProgress),
    mProgressMax(aProgressMax)
  {
  }

private:
  nsRefPtr<FileHelper> mFileHelper;
  uint64_t mProgress;
  uint64_t mProgressMax;
};

class CloseRunnable MOZ_FINAL : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  CloseRunnable(FileHelper* aFileHelper)
  : mFileHelper(aFileHelper)
  { }

private:
  nsRefPtr<FileHelper> mFileHelper;
};

class DestroyRunnable MOZ_FINAL : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  DestroyRunnable(FileHelper* aFileHelper)
  : mFileHelper(aFileHelper)
  { }

private:
  nsRefPtr<FileHelper> mFileHelper;
};

} // anonymous namespace

FileStreamWrapper::FileStreamWrapper(nsISupports* aFileStream,
                                     FileHelper* aFileHelper,
                                     uint64_t aOffset,
                                     uint64_t aLimit,
                                     uint32_t aFlags)
: mFileStream(aFileStream),
  mFileHelper(aFileHelper),
  mOffset(aOffset),
  mLimit(aLimit),
  mFlags(aFlags),
  mFirstTime(true)
{
  NS_ASSERTION(mFileStream, "Must have a file stream!");
  NS_ASSERTION(mFileHelper, "Must have a file helper!");
}

FileStreamWrapper::~FileStreamWrapper()
{
  if (mFlags & NOTIFY_DESTROY) {
    if (NS_IsMainThread()) {
      mFileHelper->OnStreamDestroy();
    }
    else {
      nsCOMPtr<nsIRunnable> runnable =
        new DestroyRunnable(mFileHelper);

      nsresult rv = NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to dispatch to the main thread!");
      }
    }
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS0(FileStreamWrapper)

FileInputStreamWrapper::FileInputStreamWrapper(nsISupports* aFileStream,
                                               FileHelper* aFileHelper,
                                               uint64_t aOffset,
                                               uint64_t aLimit,
                                               uint32_t aFlags)
: FileStreamWrapper(aFileStream, aFileHelper, aOffset, aLimit, aFlags)
{
  mInputStream = do_QueryInterface(mFileStream);
  NS_ASSERTION(mInputStream, "This should always succeed!");
}

NS_IMPL_ISUPPORTS_INHERITED1(FileInputStreamWrapper,
                             FileStreamWrapper,
                             nsIInputStream)

NS_IMETHODIMP
FileInputStreamWrapper::Close()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mFlags & NOTIFY_CLOSE) {
    nsCOMPtr<nsIRunnable> runnable = new CloseRunnable(mFileHelper);

    if (NS_FAILED(NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL))) {
      NS_WARNING("Failed to dispatch to the main thread!");
    }
  }

  mOffset = 0;
  mLimit = 0;

  return NS_OK;
}

NS_IMETHODIMP
FileInputStreamWrapper::Available(uint64_t* _retval)
{
  // Performing sync IO on the main thread is generally not allowed.
  // However, the input stream wrapper is also used to track reads performed by
  // other APIs like FileReader, XHR, etc.
  // In that case nsInputStreamChannel::OpenContentStream() calls Available()
  // before setting the content length property. This property is not important
  // to perform reads properly, so we can just return NS_BASE_STREAM_CLOSED
  // here. It causes OpenContentStream() to set the content length property to
  // zero.

  if (NS_IsMainThread()) {
    return NS_BASE_STREAM_CLOSED;
  }

  return mInputStream->Available(_retval);
}

NS_IMETHODIMP
FileInputStreamWrapper::Read(char* aBuf, uint32_t aCount, uint32_t* _retval)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsresult rv;

  if (mFirstTime) {
    mFirstTime = false;

    if (mOffset != UINT64_MAX) {
      nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mInputStream);
      if (seekable) {
        rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, mOffset);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    mOffset = 0;
  }

  uint64_t max = mLimit - mOffset;
  if (max == 0) {
    *_retval = 0;
    return NS_OK;
  }

  if (aCount > max) {
    aCount = max;
  }

  rv = mInputStream->Read(aBuf, aCount, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  mOffset += *_retval;

  if (mFlags & NOTIFY_PROGRESS) {
    nsCOMPtr<nsIRunnable> runnable =
      new ProgressRunnable(mFileHelper, mOffset, mLimit);

    rv = NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch to the main thread!");
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
FileInputStreamWrapper::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                     uint32_t aCount, uint32_t* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileInputStreamWrapper::IsNonBlocking(bool* _retval)
{
  *_retval = false;
  return NS_OK;
}

FileOutputStreamWrapper::FileOutputStreamWrapper(nsISupports* aFileStream,
                                                 FileHelper* aFileHelper,
                                                 uint64_t aOffset,
                                                 uint64_t aLimit,
                                                 uint32_t aFlags)
: FileStreamWrapper(aFileStream, aFileHelper, aOffset, aLimit, aFlags)
#ifdef DEBUG
, mWriteThread(nullptr)
#endif
{
  mOutputStream = do_QueryInterface(mFileStream);
  NS_ASSERTION(mOutputStream, "This should always succeed!");
}

NS_IMPL_ISUPPORTS_INHERITED1(FileOutputStreamWrapper,
                             FileStreamWrapper,
                             nsIOutputStream)

NS_IMETHODIMP
FileOutputStreamWrapper::Close()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsresult rv = NS_OK;

  if (!mFirstTime) {
    NS_ASSERTION(PR_GetCurrentThread() == mWriteThread,
                 "Unsetting thread locals on wrong thread!");
    mFileHelper->mFileStorage->UnsetThreadLocals();
  }

  if (mFlags & NOTIFY_CLOSE) {
    nsCOMPtr<nsIRunnable> runnable = new CloseRunnable(mFileHelper);

    if (NS_FAILED(NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL))) {
      NS_WARNING("Failed to dispatch to the main thread!");
    }
  }

  mOffset = 0;
  mLimit = 0;

  return rv;
}

NS_IMETHODIMP
FileOutputStreamWrapper::Write(const char* aBuf, uint32_t aCount,
                               uint32_t* _retval)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsresult rv;

  if (mFirstTime) {
    mFirstTime = false;

#ifdef DEBUG
    mWriteThread = PR_GetCurrentThread();
#endif
    mFileHelper->mFileStorage->SetThreadLocals();

    nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mOutputStream);
    if (seekable) {
      if (mOffset == UINT64_MAX) {
        rv = seekable->Seek(nsISeekableStream::NS_SEEK_END, 0);
      }
      else {
        rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, mOffset);
      }
      NS_ENSURE_SUCCESS(rv, rv);
    }

    mOffset = 0;
  }

  uint64_t max = mLimit - mOffset;
  if (max == 0) {
    *_retval = 0;
    return NS_OK;
  }

  if (aCount > max) {
    aCount = max;
  }

  rv = mOutputStream->Write(aBuf, aCount, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  mOffset += *_retval;

  if (mFlags & NOTIFY_PROGRESS) {
    nsCOMPtr<nsIRunnable> runnable =
      new ProgressRunnable(mFileHelper, mOffset, mLimit);

    NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);
  }

  return NS_OK;
}

NS_IMETHODIMP
FileOutputStreamWrapper::Flush()
{
  return NS_OK;
}

NS_IMETHODIMP
FileOutputStreamWrapper::WriteFrom(nsIInputStream* aFromStream,
                                   uint32_t aCount, uint32_t* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileOutputStreamWrapper::WriteSegments(nsReadSegmentFun aReader,
                                       void* aClosure, uint32_t aCount,
                                       uint32_t* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileOutputStreamWrapper::IsNonBlocking(bool* _retval)
{
  *_retval = false;
  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(ProgressRunnable, nsIRunnable)

NS_IMETHODIMP
ProgressRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mFileHelper->OnStreamProgress(mProgress, mProgressMax);
  mFileHelper = nullptr;

  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(CloseRunnable, nsIRunnable)

NS_IMETHODIMP
CloseRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mFileHelper->OnStreamClose();
  mFileHelper = nullptr;

  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(DestroyRunnable, nsIRunnable)

NS_IMETHODIMP
DestroyRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mFileHelper->OnStreamDestroy();
  mFileHelper = nullptr;

  return NS_OK;
}
