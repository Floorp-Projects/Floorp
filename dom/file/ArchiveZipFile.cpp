/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ArchiveZipFile.h"
#include "ArchiveZipEvent.h"

#include "nsIInputStream.h"
#include "zlib.h"
#include "mozilla/Attributes.h"

USING_FILE_NAMESPACE

#define ZIP_CHUNK 16384

// a internat input stream object

class ArchiveInputStream MOZ_FINAL : public nsIInputStream
{
public:
  ArchiveInputStream(ArchiveReader* aReader,
                     nsString& aFilename,
                     PRUint32 aStart,
                     PRUint32 aLength,
                     ZipCentral& aCentral)
  : mArchiveReader(aReader),
    mCentral(aCentral),
    mFilename(aFilename),
    mStart(aStart),
    mLength(aLength),
    mRunning(false)
  {}

  ~ArchiveInputStream()
  {
    Close();
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM

private:
  nsresult Init();

private: // data
  nsRefPtr<ArchiveReader> mArchiveReader;
  ZipCentral mCentral;
  nsString mFilename;
  PRUint32 mStart;
  PRUint32 mLength;

  z_stream mZs;

  bool mRunning;

  struct {
    nsCOMPtr<nsIInputStream> inputStream;
    unsigned char input[ZIP_CHUNK];
    PRUint32 sizeToBeRead;
    bool compressed; // a zip file can contain stored or compressed files
  } mData;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(ArchiveInputStream, nsIInputStream)

nsresult
ArchiveInputStream::Init()
{
  nsresult rv;

  memset(&mZs, 0, sizeof(z_stream));
  int zerr = inflateInit2(&mZs, -MAX_WBITS);
  if (zerr != Z_OK)
    return NS_ERROR_OUT_OF_MEMORY;

  // Reset the data:
  memset(&mData, 0, sizeof(mData));
  mData.sizeToBeRead = ArchiveZipItem::StrToInt32(mCentral.size);

  PRUint32 offset = ArchiveZipItem::StrToInt32(mCentral.localhdr_offset);
  PRUint64 size;
  rv = mArchiveReader->GetSize(&size);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // The file is corrupt
  if (offset + ZIPLOCAL_SIZE > size)
    return NS_ERROR_UNEXPECTED;

  mArchiveReader->GetInputStream(getter_AddRefs(mData.inputStream));
  if (rv != NS_OK || !mData.inputStream)
    return NS_ERROR_UNEXPECTED;

  // From the input stream to a seekable stream
  nsCOMPtr<nsISeekableStream> seekableStream;
  seekableStream = do_QueryInterface(mData.inputStream);
  if (!seekableStream)
    return NS_ERROR_UNEXPECTED;

  // Seek + read the ZipLocal struct
  seekableStream->Seek(nsISeekableStream::NS_SEEK_SET, offset);
  PRUint8 buffer[ZIPLOCAL_SIZE];
  PRUint32 ret;

  rv = mData.inputStream->Read((char*)buffer, ZIPLOCAL_SIZE, &ret);
  if (rv != NS_OK || ret != ZIPLOCAL_SIZE)
    return NS_ERROR_UNEXPECTED;

  // Signature check:
  if (ArchiveZipItem::StrToInt32(buffer) != LOCALSIG)
    return NS_ERROR_UNEXPECTED;

  ZipLocal local;
  memcpy(&local, buffer, ZIPLOCAL_SIZE);

  // Seek to the real data:
  offset += ZIPLOCAL_SIZE +
            ArchiveZipItem::StrToInt16(local.filename_len) +
            ArchiveZipItem::StrToInt16(local.extrafield_len);

  // The file is corrupt if there is not enough data
  if (offset + mData.sizeToBeRead > size)
    return NS_ERROR_UNEXPECTED;

  // Data starts here:
  seekableStream->Seek(nsISeekableStream::NS_SEEK_SET, offset);

  // The file is compressed or not?
  mData.compressed = (ArchiveZipItem::StrToInt16(mCentral.method) != 0);

  // We have to skip the first mStart bytes:
  if (mStart != 0) {
    PRUint32 done(mStart);
    PRUint32 ret;
    char buffer[1024];

    while (done > 0) {
      rv = Read(buffer, done > sizeof(buffer) ? sizeof(buffer) : done, &ret);
      if (rv != NS_OK)
        return rv;

      if (ret == 0)
        return NS_ERROR_UNEXPECTED;

      done -= ret;
    }
  }

  mRunning = true;
  return NS_OK;
}

NS_IMETHODIMP
ArchiveInputStream::Close()
{
  if (mRunning) {
    inflateEnd(&mZs);
    mRunning = false;
  }

  return NS_OK;
}

NS_IMETHODIMP
ArchiveInputStream::Available(PRUint32* _retval)
{
  *_retval = mLength - mZs.total_out - mStart;
  return NS_OK;
}

NS_IMETHODIMP
ArchiveInputStream::Read(char* aBuffer,
                         PRUint32 aCount,
                         PRUint32* _retval)
{
  NS_ENSURE_ARG_POINTER(aBuffer);
  NS_ENSURE_ARG_POINTER(_retval);

  PRUint32 ret;
  nsresult rv;

  // This is the first time:
  if (!mRunning) {
    rv = Init();
    if (rv != NS_OK)
      return rv;
  }

  // Nothing more can be read
  if (mData.sizeToBeRead == 0) {
    *_retval = 0;
    return NS_OK;
  }

  // Stored file:
  if (!mData.compressed)
  {
    rv = mData.inputStream->Read(aBuffer,
                                 (mData.sizeToBeRead > aCount ?
                                      aCount : mData.sizeToBeRead),
                                 _retval);
    if (rv == NS_OK)
      mData.sizeToBeRead -= *_retval;

    return rv;
  }

  // We have nothing ready to be processed:
  if (mZs.avail_out == 0)
  {
    rv = mData.inputStream->Read((char*)mData.input,
                                 (mData.sizeToBeRead > sizeof(mData.input) ?
                                      sizeof(mData.input) : mData.sizeToBeRead),
                                 &ret);
    if (rv != NS_OK)
      return rv;

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

  ret = inflate(&mZs, mData.sizeToBeRead ? Z_NO_FLUSH : Z_FINISH);
  if (ret != Z_OK && ret != Z_STREAM_END)
    return NS_ERROR_UNEXPECTED;

  *_retval = aCount - mZs.avail_out;
  return NS_OK;
}

NS_IMETHODIMP
ArchiveInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                                 void* aClosure,
                                 PRUint32 aCount,
                                 PRUint32* _retval)
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


// ArchiveZipFile

NS_IMETHODIMP
ArchiveZipFile::GetInternalStream(nsIInputStream** aStream)
{
  if (mLength > PR_INT32_MAX)
    return NS_ERROR_FAILURE;

  nsRefPtr<ArchiveInputStream> stream = new ArchiveInputStream(mArchiveReader,
                                                               mFilename,
                                                               mStart,
                                                               mLength,
                                                               mCentral);
  NS_ADDREF(stream);

  *aStream = stream;
  return NS_OK;
}

already_AddRefed<nsIDOMBlob>
ArchiveZipFile::CreateSlice(PRUint64 aStart,
                            PRUint64 aLength,
                            const nsAString& aContentType)
{
  nsCOMPtr<nsIDOMBlob> t = new ArchiveZipFile(mFilename,
                                              mContentType,
                                              aStart,
                                              mLength,
                                              mCentral,
                                              mArchiveReader);
  return t.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(ArchiveZipFile)

// C++ traverse
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ArchiveZipFile)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mArchiveReader, nsIDOMArchiveReader)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// Unlink
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ArchiveZipFile)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mArchiveReader)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ArchiveZipFile)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFile)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBlob)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIDOMFile, mIsFile)
  NS_INTERFACE_MAP_ENTRY(nsIXHRSendable)
  NS_INTERFACE_MAP_ENTRY(nsIMutable)
NS_INTERFACE_MAP_END_INHERITING(nsDOMFileCC)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ArchiveZipFile)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ArchiveZipFile)
