/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ArchiveZipFile.h"
#include "ArchiveZipEvent.h"

#include "nsIInputStream.h"
#include "zlib.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/File.h"

using namespace mozilla::dom;
USING_ARCHIVEREADER_NAMESPACE

#define ZIP_CHUNK 16384

/**
 * Input stream object for zip files
 */
class ArchiveInputStream final : public nsIInputStream,
                                 public nsISeekableStream
{
public:
  ArchiveInputStream(uint64_t aParentSize,
                     nsIInputStream* aInputStream,
                     nsString& aFilename,
                     uint32_t aStart,
                     uint32_t aLength,
                     ZipCentral& aCentral)
  : mCentral(aCentral),
    mFilename(aFilename),
    mStart(aStart),
    mLength(aLength),
    mStatus(NotStarted)
  {
    MOZ_COUNT_CTOR(ArchiveInputStream);

    // Reset the data:
    memset(&mData, 0, sizeof(mData));

    mData.parentSize = aParentSize;
    mData.inputStream = aInputStream;
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM

private:
  virtual ~ArchiveInputStream()
  {
    MOZ_COUNT_DTOR(ArchiveInputStream);
    Close();
  }

  nsresult Init();

private: // data
  ZipCentral mCentral;
  nsString mFilename;
  uint32_t mStart;
  uint32_t mLength;

  z_stream mZs;

  enum {
    NotStarted,
    Started,
    Done
  } mStatus;

  struct {
    uint64_t parentSize;
    nsCOMPtr<nsIInputStream> inputStream;

    unsigned char input[ZIP_CHUNK];
    uint32_t sizeToBeRead;
    uint32_t cursor;

    bool compressed; // a zip file can contain stored or compressed files
  } mData;
};

NS_IMPL_ISUPPORTS(ArchiveInputStream,
                  nsIInputStream,
                  nsISeekableStream)

nsresult
ArchiveInputStream::Init()
{
  nsresult rv;

  memset(&mZs, 0, sizeof(z_stream));
  int zerr = inflateInit2(&mZs, -MAX_WBITS);
  if (zerr != Z_OK) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mData.sizeToBeRead = ArchiveZipItem::StrToInt32(mCentral.size);

  uint32_t offset = ArchiveZipItem::StrToInt32(mCentral.localhdr_offset);

  // The file is corrupt
  if (mData.parentSize < ZIPLOCAL_SIZE ||
      offset > mData.parentSize - ZIPLOCAL_SIZE) {
    return NS_ERROR_UNEXPECTED;
  }

  // From the input stream to a seekable stream
  nsCOMPtr<nsISeekableStream> seekableStream;
  seekableStream = do_QueryInterface(mData.inputStream);
  if (!seekableStream) {
    return NS_ERROR_UNEXPECTED;
  }

  // Seek + read the ZipLocal struct
  seekableStream->Seek(nsISeekableStream::NS_SEEK_SET, offset);
  uint8_t buffer[ZIPLOCAL_SIZE];
  uint32_t ret;

  rv = mData.inputStream->Read((char*)buffer, ZIPLOCAL_SIZE, &ret);
  if (NS_FAILED(rv) || ret != ZIPLOCAL_SIZE) {
    return NS_ERROR_UNEXPECTED;
  }

  // Signature check:
  if (ArchiveZipItem::StrToInt32(buffer) != LOCALSIG) {
    return NS_ERROR_UNEXPECTED;
  }

  ZipLocal local;
  memcpy(&local, buffer, ZIPLOCAL_SIZE);

  // Seek to the real data:
  offset += ZIPLOCAL_SIZE +
            ArchiveZipItem::StrToInt16(local.filename_len) +
            ArchiveZipItem::StrToInt16(local.extrafield_len);

  // The file is corrupt if there is not enough data
  if (mData.parentSize < mData.sizeToBeRead ||
      offset > mData.parentSize - mData.sizeToBeRead) {
    return NS_ERROR_UNEXPECTED;
  }

  // Data starts here:
  seekableStream->Seek(nsISeekableStream::NS_SEEK_SET, offset);

  // The file is compressed or not?
  mData.compressed = (ArchiveZipItem::StrToInt16(mCentral.method) != 0);

  // We have to skip the first mStart bytes:
  if (mStart != 0) {
    rv = Seek(NS_SEEK_SET, mStart);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
ArchiveInputStream::Close()
{
  if (mStatus != NotStarted) {
    inflateEnd(&mZs);
    mStatus = NotStarted;
  }

  return NS_OK;
}

NS_IMETHODIMP
ArchiveInputStream::Available(uint64_t* _retval)
{
  *_retval = mLength - mData.cursor - mStart;
  return NS_OK;
}

NS_IMETHODIMP
ArchiveInputStream::Read(char* aBuffer,
                         uint32_t aCount,
                         uint32_t* _retval)
{
  NS_ENSURE_ARG_POINTER(aBuffer);
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv;

  // This is the first time:
  if (mStatus == NotStarted) {
    mStatus = Started;

    rv = Init();
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Let's set avail_out to -1 so we read something from the stream.
    mZs.avail_out = (uInt)-1;
  }

  // Nothing more can be read
  if (mStatus == Done) {
    *_retval = 0;
    return NS_OK;
  }

  // Stored file:
  if (!mData.compressed) {
    rv = mData.inputStream->Read(aBuffer,
                                 (mData.sizeToBeRead > aCount ?
                                    aCount : mData.sizeToBeRead),
                                 _retval);
    if (NS_SUCCEEDED(rv)) {
      mData.sizeToBeRead -= *_retval;
      mData.cursor += *_retval;

      if (mData.sizeToBeRead == 0) {
        mStatus = Done;
      }
    }

    return rv;
  }

  // We have nothing ready to be processed:
  if (mZs.avail_out != 0 && mData.sizeToBeRead != 0) {
    uint32_t ret;
    rv = mData.inputStream->Read((char*)mData.input,
                                 (mData.sizeToBeRead > sizeof(mData.input) ?
                                      sizeof(mData.input) : mData.sizeToBeRead),
                                 &ret);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Terminator:
    if (ret == 0) {
      *_retval = 0;
      return NS_OK;
    }

    mData.sizeToBeRead -= ret;
    mZs.avail_in = ret;
    mZs.next_in = mData.input;
  }

  mZs.avail_out = aCount;
  mZs.next_out = (unsigned char*)aBuffer;

  int ret = inflate(&mZs, mData.sizeToBeRead ? Z_NO_FLUSH : Z_FINISH);
  if (ret != Z_BUF_ERROR && ret != Z_OK && ret != Z_STREAM_END) {
    return NS_ERROR_UNEXPECTED;
  }

  if (ret == Z_STREAM_END) {
    mStatus = Done;
  }

  *_retval = aCount - mZs.avail_out;
  mData.cursor += *_retval;
  return NS_OK;
}

NS_IMETHODIMP
ArchiveInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                                 void* aClosure,
                                 uint32_t aCount,
                                 uint32_t* _retval)
{
  // don't have a buffer to read from, so this better not be called!
  NS_NOTREACHED("Consumers should be using Read()!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ArchiveInputStream::IsNonBlocking(bool* _retval)
{
  // We are blocking
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
ArchiveInputStream::Seek(int32_t aWhence, int64_t aOffset)
{
  int64_t pos = aOffset;

  switch (aWhence) {
  case NS_SEEK_SET:
    break;

  case NS_SEEK_CUR:
    pos += mData.cursor;
    break;

  case NS_SEEK_END:
    pos += mLength;
    break;

  default:
    NS_NOTREACHED("unexpected whence value");
    return NS_ERROR_UNEXPECTED;
  }

  if (pos == int64_t(mData.cursor)) {
    return NS_OK;
  }

  if (pos < 0 || pos >= mLength) {
    return NS_ERROR_FAILURE;
  }

  // We have to terminate the previous operation:
  nsresult rv;
  if (mStatus != NotStarted) {
    rv = Close();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Reset the cursor:
  mData.cursor = 0;

  // Note: This code is heavy but inflate does not have any seek() support:
  uint32_t ret;
  char buffer[1024];
  while (pos > 0) {
    rv = Read(buffer, pos > int64_t(sizeof(buffer)) ? sizeof(buffer) : pos, &ret);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (ret == 0) {
      return NS_ERROR_UNEXPECTED;
    }

    pos -= ret;
  }

  return NS_OK;
}

NS_IMETHODIMP
ArchiveInputStream::Tell(int64_t *aResult)
{
  *aResult = mData.cursor;
  return NS_OK;
}

NS_IMETHODIMP
ArchiveInputStream::SetEOF()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// ArchiveZipBlobImpl

void
ArchiveZipBlobImpl::GetInternalStream(nsIInputStream** aStream,
                                      ErrorResult& aRv)
{
  if (mLength > INT32_MAX) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  uint64_t size = mBlobImpl->GetSize(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsIInputStream> inputStream;
  mBlobImpl->GetInternalStream(getter_AddRefs(inputStream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  RefPtr<ArchiveInputStream> stream = new ArchiveInputStream(size,
                                                               inputStream,
                                                               mFilename,
                                                               mStart,
                                                               mLength,
                                                               mCentral);

  stream.forget(aStream);
}

already_AddRefed<mozilla::dom::BlobImpl>
ArchiveZipBlobImpl::CreateSlice(uint64_t aStart,
                                uint64_t aLength,
                                const nsAString& aContentType,
                                mozilla::ErrorResult& aRv)
{
  RefPtr<BlobImpl> impl =
    new ArchiveZipBlobImpl(mFilename, mContentType, aStart, mLength, mCentral,
                           mBlobImpl);
  return impl.forget();
}

NS_IMPL_ISUPPORTS_INHERITED0(ArchiveZipBlobImpl, BlobImpl)
