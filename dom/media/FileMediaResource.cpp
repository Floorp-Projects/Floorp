/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileMediaResource.h"

#include "nsContentUtils.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsIFileChannel.h"
#include "nsIFileStreams.h"
#include "nsNetUtil.h"

namespace mozilla {

void
FileMediaResource::EnsureSizeInitialized()
{
  mLock.AssertCurrentThreadOwns();
  NS_ASSERTION(mInput, "Must have file input stream");
  if (mSizeInitialized) {
    return;
  }
  mSizeInitialized = true;
  // Get the file size and inform the decoder.
  uint64_t size;
  nsresult res = mInput->Available(&size);
  if (NS_SUCCEEDED(res) && size <= INT64_MAX) {
    mSize = (int64_t)size;
    mCallback->AbstractMainThread()->Dispatch(
      NewRunnableMethod<nsresult>("MediaResourceCallback::NotifyDataEnded",
                                  mCallback.get(),
                                  &MediaResourceCallback::NotifyDataEnded,
                                  NS_OK));
  }
}

nsresult
FileMediaResource::GetCachedRanges(MediaByteRangeSet& aRanges)
{
  MutexAutoLock lock(mLock);

  EnsureSizeInitialized();
  if (mSize == -1) {
    return NS_ERROR_FAILURE;
  }
  aRanges += MediaByteRange(0, mSize);
  return NS_OK;
}

nsresult
FileMediaResource::Open(nsIStreamListener** aStreamListener)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  MOZ_ASSERT(aStreamListener);

  *aStreamListener = nullptr;
  nsresult rv = NS_OK;

  // The channel is already open. We need a synchronous stream that
  // implements nsISeekableStream, so we have to find the underlying
  // file and reopen it
  nsCOMPtr<nsIFileChannel> fc(do_QueryInterface(mChannel));
  if (fc) {
    nsCOMPtr<nsIFile> file;
    rv = fc->GetFile(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_NewLocalFileInputStream(
      getter_AddRefs(mInput), file, -1, -1, nsIFileInputStream::SHARE_DELETE);
  } else if (IsBlobURI(mURI)) {
    rv = NS_GetStreamForBlobURI(mURI, getter_AddRefs(mInput));
  }

  NS_ENSURE_SUCCESS(rv, rv);

  mSeekable = do_QueryInterface(mInput);
  if (!mSeekable) {
    // XXX The file may just be a .url or similar
    // shortcut that points to a Web site. We need to fix this by
    // doing an async open and waiting until we locate the real resource,
    // then using that (if it's still a file!).
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
FileMediaResource::Close()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  // Since mChennel is only accessed by main thread, there is no necessary to
  // take the lock.
  if (mChannel) {
    mChannel->Cancel(NS_ERROR_PARSED_DATA_CACHED);
    mChannel = nullptr;
  }

  return NS_OK;
}

already_AddRefed<nsIPrincipal>
FileMediaResource::GetCurrentPrincipal()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  nsCOMPtr<nsIPrincipal> principal;
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  if (!secMan || !mChannel)
    return nullptr;
  secMan->GetChannelResultPrincipal(mChannel, getter_AddRefs(principal));
  return principal.forget();
}

nsresult
FileMediaResource::ReadFromCache(char* aBuffer, int64_t aOffset,
                                 uint32_t aCount)
{
  MutexAutoLock lock(mLock);

  EnsureSizeInitialized();
  if (!aCount) {
    return NS_OK;
  }
  int64_t offset = 0;
  nsresult res = mSeekable->Tell(&offset);
  NS_ENSURE_SUCCESS(res,res);
  res = mSeekable->Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  NS_ENSURE_SUCCESS(res,res);
  uint32_t bytesRead = 0;
  do {
    uint32_t x = 0;
    uint32_t bytesToRead = aCount - bytesRead;
    res = mInput->Read(aBuffer, bytesToRead, &x);
    bytesRead += x;
    if (!x) {
      res = NS_ERROR_FAILURE;
    }
  } while (bytesRead != aCount && res == NS_OK);

  // Reset read head to original position so we don't disturb any other
  // reading thread.
  nsresult seekres = mSeekable->Seek(nsISeekableStream::NS_SEEK_SET, offset);

  // If a read failed in the loop above, we want to return its failure code.
  NS_ENSURE_SUCCESS(res,res);

  // Else we succeed if the reset-seek succeeds.
  return seekres;
}

nsresult
FileMediaResource::UnsafeRead(char* aBuffer, uint32_t aCount, uint32_t* aBytes)
{
  EnsureSizeInitialized();
  return mInput->Read(aBuffer, aCount, aBytes);
}

nsresult
FileMediaResource::ReadAt(int64_t aOffset, char* aBuffer, uint32_t aCount,
                          uint32_t* aBytes)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  nsresult rv;
  {
    MutexAutoLock lock(mLock);
    rv = UnsafeSeek(nsISeekableStream::NS_SEEK_SET, aOffset);
    if (NS_FAILED(rv)) return rv;
    rv = UnsafeRead(aBuffer, aCount, aBytes);
  }
  return rv;
}

already_AddRefed<MediaByteBuffer>
FileMediaResource::UnsafeMediaReadAt(int64_t aOffset, uint32_t aCount)
{
  RefPtr<MediaByteBuffer> bytes = new MediaByteBuffer();
  bool ok = bytes->SetLength(aCount, fallible);
  NS_ENSURE_TRUE(ok, nullptr);
  nsresult rv = UnsafeSeek(nsISeekableStream::NS_SEEK_SET, aOffset);
  NS_ENSURE_SUCCESS(rv, nullptr);
  char* curr = reinterpret_cast<char*>(bytes->Elements());
  const char* start = curr;
  while (aCount > 0) {
    uint32_t bytesRead;
    rv = UnsafeRead(curr, aCount, &bytesRead);
    NS_ENSURE_SUCCESS(rv, nullptr);
    if (!bytesRead) {
      break;
    }
    aCount -= bytesRead;
    curr += bytesRead;
  }
  bytes->SetLength(curr - start);
  return bytes.forget();
}

nsresult
FileMediaResource::UnsafeSeek(int32_t aWhence, int64_t aOffset)
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");

  if (!mSeekable)
    return NS_ERROR_FAILURE;
  EnsureSizeInitialized();
  return mSeekable->Seek(aWhence, aOffset);
}

} // mozilla namespace
