/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMFile.h"

#include "nsCExternalHandlerService.h"
#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsError.h"
#include "nsICharsetDetector.h"
#include "nsIClassInfo.h"
#include "nsIConverterInputStream.h"
#include "nsIDocument.h"
#include "nsIFileStreams.h"
#include "nsIInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsIMemoryReporter.h"
#include "nsIMIMEService.h"
#include "nsISeekableStream.h"
#include "nsIUnicharInputStream.h"
#include "nsIUnicodeDecoder.h"
#include "nsIRemoteBlob.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIUUIDGenerator.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsStringStream.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"
#include "mozilla/SHA1.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Preferences.h"
#include "mozilla/Attributes.h"
#include "nsThreadUtils.h"

#include "mozilla/dom/FileListBinding.h"

DOMCI_DATA(File, mozilla::dom::DOMFile)
DOMCI_DATA(Blob, mozilla::dom::DOMFile)

namespace mozilla {
namespace dom {

// XXXkhuey the input stream that we pass out of a DOMFile
// can outlive the actual DOMFile object.  Thus, we must
// ensure that the buffer underlying the stream we get
// from NS_NewByteInputStream is held alive as long as the
// stream is.  We do that by passing back this class instead.
class DataOwnerAdapter MOZ_FINAL : public nsIInputStream,
                                   public nsISeekableStream,
                                   public nsIIPCSerializableInputStream
{
  typedef DOMFileImplMemory::DataOwner DataOwner;
public:
  static nsresult Create(DataOwner* aDataOwner,
                         uint32_t aStart,
                         uint32_t aLength,
                         nsIInputStream** _retval);

  NS_DECL_THREADSAFE_ISUPPORTS

  // These are mandatory.
  NS_FORWARD_NSIINPUTSTREAM(mStream->)
  NS_FORWARD_NSISEEKABLESTREAM(mSeekableStream->)

  // This is optional. We use a conditional QI to keep it from being called
  // if the underlying stream doesn't support it.
  NS_FORWARD_NSIIPCSERIALIZABLEINPUTSTREAM(mSerializableInputStream->)

private:
  ~DataOwnerAdapter() {}

  DataOwnerAdapter(DataOwner* aDataOwner,
                   nsIInputStream* aStream)
    : mDataOwner(aDataOwner), mStream(aStream),
      mSeekableStream(do_QueryInterface(aStream)),
      mSerializableInputStream(do_QueryInterface(aStream))
  {
    NS_ASSERTION(mSeekableStream, "Somebody gave us the wrong stream!");
  }

  nsRefPtr<DataOwner> mDataOwner;
  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsISeekableStream> mSeekableStream;
  nsCOMPtr<nsIIPCSerializableInputStream> mSerializableInputStream;
};

NS_IMPL_ADDREF(DataOwnerAdapter)
NS_IMPL_RELEASE(DataOwnerAdapter)

NS_INTERFACE_MAP_BEGIN(DataOwnerAdapter)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsISeekableStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIIPCSerializableInputStream,
                                     mSerializableInputStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

nsresult DataOwnerAdapter::Create(DataOwner* aDataOwner,
                                  uint32_t aStart,
                                  uint32_t aLength,
                                  nsIInputStream** _retval)
{
  nsresult rv;
  NS_ASSERTION(aDataOwner, "Uh ...");

  nsCOMPtr<nsIInputStream> stream;

  rv = NS_NewByteInputStream(getter_AddRefs(stream),
                             static_cast<const char*>(aDataOwner->mData) +
                             aStart,
                             (int32_t)aLength,
                             NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_retval = new DataOwnerAdapter(aDataOwner, stream));

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////
// mozilla::dom::DOMFile implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMFile)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMFile)
  MOZ_ASSERT(tmp->mImpl);
  tmp->mImpl->Unlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMFile)
  MOZ_ASSERT(tmp->mImpl);
  tmp->mImpl->Traverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMFile)
  // This class should not receive any nsIRemoteBlob QI!
  MOZ_ASSERT(!aIID.Equals(NS_GET_IID(nsIRemoteBlob)));

  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFile)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBlob)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIDOMFile, IsFile())
  NS_INTERFACE_MAP_ENTRY(nsIXHRSendable)
  NS_INTERFACE_MAP_ENTRY(nsIMutable)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(File, IsFile())
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(Blob, !(IsFile()))
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMFile)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMFile)

/* static */ already_AddRefed<DOMFile>
DOMFile::Create(const nsAString& aName, const nsAString& aContentType,
                uint64_t aLength, uint64_t aLastModifiedDate)
{
  nsRefPtr<DOMFile> file = new DOMFile(
    new DOMFileImplBase(aName, aContentType, aLength, aLastModifiedDate));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::Create(const nsAString& aName, const nsAString& aContentType,
                uint64_t aLength)
{
  nsRefPtr<DOMFile> file = new DOMFile(
    new DOMFileImplBase(aName, aContentType, aLength));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::Create(const nsAString& aContentType, uint64_t aLength)
{
  nsRefPtr<DOMFile> file = new DOMFile(
    new DOMFileImplBase(aContentType, aLength));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::Create(const nsAString& aContentType, uint64_t aStart,
                uint64_t aLength)
{
  nsRefPtr<DOMFile> file = new DOMFile(
    new DOMFileImplBase(aContentType, aStart, aLength));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateMemoryFile(void* aMemoryBuffer, uint64_t aLength,
                          const nsAString& aName,
                          const nsAString& aContentType,
                          uint64_t aLastModifiedDate)
{
  nsRefPtr<DOMFile> file = new DOMFile(
    new DOMFileImplMemory(aMemoryBuffer, aLength, aName,
                          aContentType, aLastModifiedDate));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateMemoryFile(void* aMemoryBuffer, uint64_t aLength,
                          const nsAString& aContentType)
{
  nsRefPtr<DOMFile> file = new DOMFile(
    new DOMFileImplMemory(aMemoryBuffer, aLength, aContentType));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateTemporaryFileBlob(PRFileDesc* aFD, uint64_t aStartPos,
                                 uint64_t aLength,
                                      const nsAString& aContentType)
{
  nsRefPtr<DOMFile> file = new DOMFile(
    new DOMFileImplTemporaryFileBlob(aFD, aStartPos, aLength, aContentType));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateFromFile(nsIFile* aFile)
{
  nsRefPtr<DOMFile> file = new DOMFile(new DOMFileImplFile(aFile));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateFromFile(const nsAString& aContentType, uint64_t aLength,
                        nsIFile* aFile, indexedDB::FileInfo* aFileInfo)
{
  nsRefPtr<DOMFile> file = new DOMFile(
    new DOMFileImplFile(aContentType, aLength, aFile, aFileInfo));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateFromFile(const nsAString& aName,
                        const nsAString& aContentType,
                        uint64_t aLength, nsIFile* aFile,
                        indexedDB::FileInfo* aFileInfo)
{
  nsRefPtr<DOMFile> file = new DOMFile(
    new DOMFileImplFile(aName, aContentType, aLength, aFile, aFileInfo));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateFromFile(nsIFile* aFile, indexedDB::FileInfo* aFileInfo)
{
  nsRefPtr<DOMFile> file = new DOMFile(new DOMFileImplFile(aFile, aFileInfo));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateFromFile(nsIFile* aFile, const nsAString& aName,
                        const nsAString& aContentType)
{
  nsRefPtr<DOMFile> file = new DOMFile(
    new DOMFileImplFile(aFile, aName, aContentType));
  return file.forget();
}

const nsTArray<nsRefPtr<DOMFileImpl>>*
DOMFile::GetSubBlobImpls() const
{
  return mImpl->GetSubBlobImpls();
}

bool
DOMFile::IsSizeUnknown() const
{
  return mImpl->IsSizeUnknown();
}

bool
DOMFile::IsDateUnknown() const
{
  return mImpl->IsDateUnknown();
}

bool
DOMFile::IsFile() const
{
  return mImpl->IsFile();
}

void
DOMFile::SetLazyData(const nsAString& aName, const nsAString& aContentType,
                     uint64_t aLength, uint64_t aLastModifiedDate)
{
  return mImpl->SetLazyData(aName, aContentType, aLength, aLastModifiedDate);
}

already_AddRefed<nsIDOMBlob>
DOMFile::CreateSlice(uint64_t aStart, uint64_t aLength,
                     const nsAString& aContentType)
{
  return mImpl->CreateSlice(aStart, aLength, aContentType);
}

NS_IMETHODIMP
DOMFile::Initialize(nsISupports* aOwner, JSContext* aCx, JSObject* aObj,
                    const JS::CallArgs& aArgs)
{
  return mImpl->Initialize(aOwner, aCx, aObj, aArgs);
}

NS_IMETHODIMP
DOMFile::GetName(nsAString& aFileName)
{
  return mImpl->GetName(aFileName);
}

NS_IMETHODIMP
DOMFile::GetPath(nsAString& aPath)
{
  return mImpl->GetPath(aPath);
}

NS_IMETHODIMP
DOMFile::GetLastModifiedDate(JSContext* aCx,
                             JS::MutableHandle<JS::Value> aDate)
{
  return mImpl->GetLastModifiedDate(aCx, aDate);
}

NS_IMETHODIMP
DOMFile::GetMozFullPath(nsAString &aFileName)
{
  return mImpl->GetMozFullPath(aFileName);
}

NS_IMETHODIMP
DOMFile::GetMozFullPathInternal(nsAString &aFileName)
{
  return mImpl->GetMozFullPathInternal(aFileName);
}

NS_IMETHODIMP
DOMFile::GetSize(uint64_t* aSize)
{
  return mImpl->GetSize(aSize);
}

NS_IMETHODIMP
DOMFile::GetType(nsAString &aType)
{
  return mImpl->GetType(aType);
}

NS_IMETHODIMP
DOMFile::GetMozLastModifiedDate(uint64_t* aDate)
{
  return mImpl->GetMozLastModifiedDate(aDate);
}

// Makes sure that aStart and aEnd is less then or equal to aSize and greater
// than 0
static void
ParseSize(int64_t aSize, int64_t& aStart, int64_t& aEnd)
{
  CheckedInt64 newStartOffset = aStart;
  if (aStart < -aSize) {
    newStartOffset = 0;
  }
  else if (aStart < 0) {
    newStartOffset += aSize;
  }
  else if (aStart > aSize) {
    newStartOffset = aSize;
  }

  CheckedInt64 newEndOffset = aEnd;
  if (aEnd < -aSize) {
    newEndOffset = 0;
  }
  else if (aEnd < 0) {
    newEndOffset += aSize;
  }
  else if (aEnd > aSize) {
    newEndOffset = aSize;
  }

  if (!newStartOffset.isValid() || !newEndOffset.isValid() ||
      newStartOffset.value() >= newEndOffset.value()) {
    aStart = aEnd = 0;
  }
  else {
    aStart = newStartOffset.value();
    aEnd = newEndOffset.value();
  }
}

NS_IMETHODIMP
DOMFile::Slice(int64_t aStart, int64_t aEnd,
               const nsAString& aContentType, uint8_t aArgc,
               nsIDOMBlob **aBlob)
{
  MOZ_ASSERT(mImpl);
  return mImpl->Slice(aStart, aEnd, aContentType, aArgc, aBlob);
}

NS_IMETHODIMP
DOMFile::GetInternalStream(nsIInputStream** aStream)
{
 return mImpl->GetInternalStream(aStream);
}

NS_IMETHODIMP_(int64_t)
DOMFile::GetFileId()
{
  return mImpl->GetFileId();
}

NS_IMETHODIMP_(void)
DOMFile::AddFileInfo(indexedDB::FileInfo* aFileInfo)
{
  mImpl->AddFileInfo(aFileInfo);
}

indexedDB::FileInfo*
DOMFile::GetFileInfo(indexedDB::FileManager* aFileManager)
{
  return mImpl->GetFileInfo(aFileManager);
}

NS_IMETHODIMP
DOMFile::GetSendInfo(nsIInputStream** aBody,
                     uint64_t* aContentLength,
                     nsACString& aContentType,
                     nsACString& aCharset)
{
  return mImpl->GetSendInfo(aBody, aContentLength, aContentType, aCharset);
}

NS_IMETHODIMP
DOMFile::GetMutable(bool* aMutable)
{
  return mImpl->GetMutable(aMutable);
}

NS_IMETHODIMP
DOMFile::SetMutable(bool aMutable)
{
  return mImpl->SetMutable(aMutable);
}

NS_IMETHODIMP_(bool)
DOMFile::IsMemoryFile()
{
  return mImpl->IsMemoryFile();
}

////////////////////////////////////////////////////////////////////////////
// mozilla::dom::DOMFileImpl implementation

nsresult
DOMFileImpl::Slice(int64_t aStart, int64_t aEnd,
                   const nsAString& aContentType, uint8_t aArgc,
                   nsIDOMBlob **aBlob)
{
  *aBlob = nullptr;

  // Truncate aStart and aEnd so that we stay within this file.
  uint64_t thisLength;
  nsresult rv = GetSize(&thisLength);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aArgc < 2) {
    aEnd = (int64_t)thisLength;
  }

  ParseSize((int64_t)thisLength, aStart, aEnd);

  // Create the new file
  nsCOMPtr<nsIDOMBlob> blob =
    CreateSlice((uint64_t)aStart, (uint64_t)(aEnd - aStart), aContentType);

  blob.forget(aBlob);
  return *aBlob ? NS_OK : NS_ERROR_UNEXPECTED;
}

////////////////////////////////////////////////////////////////////////////
// DOMFileImpl implementation

NS_IMPL_ISUPPORTS(DOMFileImpl, PIDOMFileImpl)

////////////////////////////////////////////////////////////////////////////
// DOMFileImplFile implementation

NS_IMPL_ISUPPORTS_INHERITED0(DOMFileImplFile, DOMFileImpl)

nsresult
DOMFileImplBase::GetName(nsAString& aName)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  aName = mName;
  return NS_OK;
}

nsresult
DOMFileImplBase::GetPath(nsAString& aPath)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  aPath = mPath;
  return NS_OK;
}

nsresult
DOMFileImplBase::GetLastModifiedDate(JSContext* aCx,
                                     JS::MutableHandle<JS::Value> aDate)
{
  JS::Rooted<JSObject*> date(aCx, JS_NewDateObjectMsec(aCx, JS_Now() / PR_USEC_PER_MSEC));
  if (!date) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  aDate.setObject(*date);
  return NS_OK;
}

nsresult
DOMFileImplBase::GetMozFullPath(nsAString &aFileName)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");

  // It is unsafe to call IsCallerChrome on a non-main thread. If
  // you hit the following assertion you need to figure out some other way to
  // determine privileges and call GetMozFullPathInternal.
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (nsContentUtils::IsCallerChrome()) {
    return GetMozFullPathInternal(aFileName);
  }
  aFileName.Truncate();
  return NS_OK;
}

nsresult
DOMFileImplBase::GetMozFullPathInternal(nsAString& aFileName)
{
  if (!mIsFile) {
    return NS_ERROR_FAILURE;
  }

  aFileName.Truncate();
  return NS_OK;
}

nsresult
DOMFileImplBase::GetSize(uint64_t* aSize)
{
  *aSize = mLength;
  return NS_OK;
}

nsresult
DOMFileImplBase::GetType(nsAString& aType)
{
  aType = mContentType;
  return NS_OK;
}

nsresult
DOMFileImplBase::GetMozLastModifiedDate(uint64_t* aLastModifiedDate)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  if (IsDateUnknown()) {
    mLastModificationDate = PR_Now();
  }
  *aLastModifiedDate = mLastModificationDate;
  return NS_OK;
}

already_AddRefed<nsIDOMBlob>
DOMFileImplBase::CreateSlice(uint64_t aStart, uint64_t aLength,
                             const nsAString& aContentType)
{
  return nullptr;
}

nsresult
DOMFileImplBase::GetInternalStream(nsIInputStream** aStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

int64_t
DOMFileImplBase::GetFileId()
{
  int64_t id = -1;

  if (IsStoredFile() && IsWholeFile() && !IsSnapshot()) {
    if (!indexedDB::IndexedDatabaseManager::IsClosed()) {
      indexedDB::IndexedDatabaseManager::FileMutex().Lock();
    }

    NS_ASSERTION(!mFileInfos.IsEmpty(),
                 "A stored file must have at least one file info!");

    nsRefPtr<indexedDB::FileInfo>& fileInfo = mFileInfos.ElementAt(0);
    if (fileInfo) {
      id =  fileInfo->Id();
    }

    if (!indexedDB::IndexedDatabaseManager::IsClosed()) {
      indexedDB::IndexedDatabaseManager::FileMutex().Unlock();
    }
  }

  return id;
}

void
DOMFileImplBase::AddFileInfo(indexedDB::FileInfo* aFileInfo)
{
  if (indexedDB::IndexedDatabaseManager::IsClosed()) {
    NS_ERROR("Shouldn't be called after shutdown!");
    return;
  }

  nsRefPtr<indexedDB::FileInfo> fileInfo = aFileInfo;

  MutexAutoLock lock(indexedDB::IndexedDatabaseManager::FileMutex());

  NS_ASSERTION(!mFileInfos.Contains(aFileInfo),
               "Adding the same file info agan?!");

  nsRefPtr<indexedDB::FileInfo>* element = mFileInfos.AppendElement();
  element->swap(fileInfo);
}

indexedDB::FileInfo*
DOMFileImplBase::GetFileInfo(indexedDB::FileManager* aFileManager)
{
  if (indexedDB::IndexedDatabaseManager::IsClosed()) {
    NS_ERROR("Shouldn't be called after shutdown!");
    return nullptr;
  }

  // A slice created from a stored file must keep the file info alive.
  // However, we don't support sharing of slices yet, so the slice must be
  // copied again. That's why we have to ignore the first file info.
  // Snapshots are handled in a similar way (they have to be copied).
  uint32_t startIndex;
  if (IsStoredFile() && (!IsWholeFile() || IsSnapshot())) {
    startIndex = 1;
  }
  else {
    startIndex = 0;
  }

  MutexAutoLock lock(indexedDB::IndexedDatabaseManager::FileMutex());

  for (uint32_t i = startIndex; i < mFileInfos.Length(); i++) {
    nsRefPtr<indexedDB::FileInfo>& fileInfo = mFileInfos.ElementAt(i);
    if (fileInfo->Manager() == aFileManager) {
      return fileInfo;
    }
  }

  return nullptr;
}

nsresult
DOMFileImplBase::GetSendInfo(nsIInputStream** aBody,
                             uint64_t* aContentLength,
                             nsACString& aContentType,
                             nsACString& aCharset)
{
  nsresult rv;

  nsCOMPtr<nsIInputStream> stream;
  rv = this->GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = this->GetSize(aContentLength);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString contentType;
  rv = this->GetType(contentType);
  NS_ENSURE_SUCCESS(rv, rv);

  CopyUTF16toUTF8(contentType, aContentType);

  aCharset.Truncate();

  stream.forget(aBody);
  return NS_OK;
}

nsresult
DOMFileImplBase::GetMutable(bool* aMutable) const
{
  *aMutable = !mImmutable;
  return NS_OK;
}

nsresult
DOMFileImplBase::SetMutable(bool aMutable)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG(!mImmutable || !aMutable);

  if (!mImmutable && !aMutable) {
    // Force the content type and size to be cached
    nsString dummyString;
    rv = this->GetType(dummyString);
    NS_ENSURE_SUCCESS(rv, rv);

    uint64_t dummyInt;
    rv = this->GetSize(&dummyInt);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mImmutable = !aMutable;
  return rv;
}

////////////////////////////////////////////////////////////////////////////
// DOMFileImplFile implementation

already_AddRefed<nsIDOMBlob>
DOMFileImplFile::CreateSlice(uint64_t aStart, uint64_t aLength,
                             const nsAString& aContentType)
{
  nsCOMPtr<nsIDOMBlob> blob =
    new DOMFile(new DOMFileImplFile(this, aStart, aLength, aContentType));
  return blob.forget();
}

nsresult
DOMFileImplFile::GetMozFullPathInternal(nsAString& aFilename)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  return mFile->GetPath(aFilename);
}

nsresult
DOMFileImplFile::GetLastModifiedDate(JSContext* aCx,
                                     JS::MutableHandle<JS::Value> aLastModifiedDate)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");

  PRTime msecs;
  if (IsDateUnknown()) {
    nsresult rv = mFile->GetLastModifiedTime(&msecs);
    NS_ENSURE_SUCCESS(rv, rv);
    mLastModificationDate = msecs;
  } else {
    msecs = mLastModificationDate;
  }

  JSObject* date = JS_NewDateObjectMsec(aCx, msecs);
  if (date) {
    aLastModifiedDate.setObject(*date);
  }
  else {
    date = JS_NewDateObjectMsec(aCx, JS_Now() / PR_USEC_PER_MSEC);
    aLastModifiedDate.setObject(*date);
  }

  return NS_OK;
}

nsresult
DOMFileImplFile::GetSize(uint64_t* aFileSize)
{
  if (IsSizeUnknown()) {
    NS_ASSERTION(mWholeFile,
                 "Should only use lazy size when using the whole file");
    int64_t fileSize;
    nsresult rv = mFile->GetFileSize(&fileSize);
    NS_ENSURE_SUCCESS(rv, rv);

    if (fileSize < 0) {
      return NS_ERROR_FAILURE;
    }

    mLength = fileSize;
  }

  *aFileSize = mLength;

  return NS_OK;
}

nsresult
DOMFileImplFile::GetType(nsAString& aType)
{
  if (mContentType.IsVoid()) {
    NS_ASSERTION(mWholeFile,
                 "Should only use lazy ContentType when using the whole file");
    nsresult rv;
    nsCOMPtr<nsIMIMEService> mimeService =
      do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString mimeType;
    rv = mimeService->GetTypeFromFile(mFile, mimeType);
    if (NS_FAILED(rv)) {
      mimeType.Truncate();
    }

    AppendUTF8toUTF16(mimeType, mContentType);
    mContentType.SetIsVoid(false);
  }

  aType = mContentType;

  return NS_OK;
}

nsresult
DOMFileImplFile::GetMozLastModifiedDate(uint64_t* aLastModifiedDate)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  if (IsDateUnknown()) {
    PRTime msecs;
    nsresult rv = mFile->GetLastModifiedTime(&msecs);
    NS_ENSURE_SUCCESS(rv, rv);
    mLastModificationDate = msecs;
  }
  *aLastModifiedDate = mLastModificationDate;
  return NS_OK;
}

const uint32_t sFileStreamFlags =
  nsIFileInputStream::CLOSE_ON_EOF |
  nsIFileInputStream::REOPEN_ON_REWIND |
  nsIFileInputStream::DEFER_OPEN;

nsresult
DOMFileImplFile::GetInternalStream(nsIInputStream** aStream)
{
  return mWholeFile ?
    NS_NewLocalFileInputStream(aStream, mFile, -1, -1, sFileStreamFlags) :
    NS_NewPartialLocalFileInputStream(aStream, mFile, mStart, mLength,
                                      -1, -1, sFileStreamFlags);
}

void
DOMFileImplFile::SetPath(const nsAString& aPath)
{
  MOZ_ASSERT(aPath.IsEmpty() ||
             aPath[aPath.Length() - 1] == char16_t('/'),
             "Path must end with a path separator");
  mPath = aPath;
}

////////////////////////////////////////////////////////////////////////////
// DOMFileImplMemory implementation

NS_IMPL_ISUPPORTS_INHERITED0(DOMFileImplMemory, DOMFileImpl)

already_AddRefed<nsIDOMBlob>
DOMFileImplMemory::CreateSlice(uint64_t aStart, uint64_t aLength,
                               const nsAString& aContentType)
{
  nsCOMPtr<nsIDOMBlob> blob =
    new DOMFile(new DOMFileImplMemory(this, aStart, aLength, aContentType));
  return blob.forget();
}

nsresult
DOMFileImplMemory::GetInternalStream(nsIInputStream** aStream)
{
  if (mLength > INT32_MAX)
    return NS_ERROR_FAILURE;

  return DataOwnerAdapter::Create(mDataOwner, mStart, mLength, aStream);
}

/* static */ StaticMutex
DOMFileImplMemory::DataOwner::sDataOwnerMutex;

/* static */ StaticAutoPtr<LinkedList<DOMFileImplMemory::DataOwner>>
DOMFileImplMemory::DataOwner::sDataOwners;

/* static */ bool
DOMFileImplMemory::DataOwner::sMemoryReporterRegistered = false;

MOZ_DEFINE_MALLOC_SIZE_OF(DOMMemoryFileDataOwnerMallocSizeOf)

class DOMFileImplMemoryDataOwnerMemoryReporter MOZ_FINAL
  : public nsIMemoryReporter
{
  ~DOMFileImplMemoryDataOwnerMemoryReporter() {}

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD CollectReports(nsIMemoryReporterCallback *aCallback,
                            nsISupports *aClosure, bool aAnonymize)
  {
    typedef DOMFileImplMemory::DataOwner DataOwner;

    StaticMutexAutoLock lock(DataOwner::sDataOwnerMutex);

    if (!DataOwner::sDataOwners) {
      return NS_OK;
    }

    const size_t LARGE_OBJECT_MIN_SIZE = 8 * 1024;
    size_t smallObjectsTotal = 0;

    for (DataOwner *owner = DataOwner::sDataOwners->getFirst();
         owner; owner = owner->getNext()) {

      size_t size = DOMMemoryFileDataOwnerMallocSizeOf(owner->mData);

      if (size < LARGE_OBJECT_MIN_SIZE) {
        smallObjectsTotal += size;
      } else {
        SHA1Sum sha1;
        sha1.update(owner->mData, owner->mLength);
        uint8_t digest[SHA1Sum::kHashSize]; // SHA1 digests are 20 bytes long.
        sha1.finish(digest);

        nsAutoCString digestString;
        for (size_t i = 0; i < sizeof(digest); i++) {
          digestString.AppendPrintf("%02x", digest[i]);
        }

        nsresult rv = aCallback->Callback(
          /* process */ NS_LITERAL_CSTRING(""),
          nsPrintfCString(
            "explicit/dom/memory-file-data/large/file(length=%llu, sha1=%s)",
            owner->mLength, aAnonymize ? "<anonymized>" : digestString.get()),
          KIND_HEAP, UNITS_BYTES, size,
          nsPrintfCString(
            "Memory used to back a memory file of length %llu bytes.  The file "
            "has a sha1 of %s.\n\n"
            "Note that the allocator may round up a memory file's length -- "
            "that is, an N-byte memory file may take up more than N bytes of "
            "memory.",
            owner->mLength, digestString.get()),
          aClosure);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    if (smallObjectsTotal > 0) {
      nsresult rv = aCallback->Callback(
        /* process */ NS_LITERAL_CSTRING(""),
        NS_LITERAL_CSTRING("explicit/dom/memory-file-data/small"),
        KIND_HEAP, UNITS_BYTES, smallObjectsTotal,
        nsPrintfCString(
          "Memory used to back small memory files (less than %d bytes each).\n\n"
          "Note that the allocator may round up a memory file's length -- "
          "that is, an N-byte memory file may take up more than N bytes of "
          "memory."),
        aClosure);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(DOMFileImplMemoryDataOwnerMemoryReporter, nsIMemoryReporter)

/* static */ void
DOMFileImplMemory::DataOwner::EnsureMemoryReporterRegistered()
{
  sDataOwnerMutex.AssertCurrentThreadOwns();
  if (sMemoryReporterRegistered) {
    return;
  }

  RegisterStrongMemoryReporter(new DOMFileImplMemoryDataOwnerMemoryReporter());

  sMemoryReporterRegistered = true;
}

////////////////////////////////////////////////////////////////////////////
// DOMFileImplTemporaryFileBlob implementation

NS_IMPL_ISUPPORTS_INHERITED0(DOMFileImplTemporaryFileBlob, DOMFileImpl)

already_AddRefed<nsIDOMBlob>
DOMFileImplTemporaryFileBlob::CreateSlice(uint64_t aStart, uint64_t aLength,
                                          const nsAString& aContentType)
{
  if (aStart + aLength > mLength)
    return nullptr;

  nsCOMPtr<nsIDOMBlob> blob =
    new DOMFile(new DOMFileImplTemporaryFileBlob(this, aStart + mStartPos,
                                                 aLength, aContentType));
  return blob.forget();
}

nsresult
DOMFileImplTemporaryFileBlob::GetInternalStream(nsIInputStream** aStream)
{
  nsCOMPtr<nsIInputStream> stream =
    new nsTemporaryFileInputStream(mFileDescOwner, mStartPos, mStartPos + mLength);
  stream.forget(aStream);
  return NS_OK;
}

} // dom namespace
} // mozilla namespace

////////////////////////////////////////////////////////////////////////////
// nsDOMFileList implementation

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(nsDOMFileList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMFileList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFileList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFileList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMFileList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMFileList)

JSObject*
nsDOMFileList::WrapObject(JSContext *cx)
{
  return mozilla::dom::FileListBinding::Wrap(cx, this);
}

NS_IMETHODIMP
nsDOMFileList::GetLength(uint32_t* aLength)
{
  *aLength = Length();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileList::Item(uint32_t aIndex, nsIDOMFile **aFile)
{
  NS_IF_ADDREF(*aFile = Item(aIndex));

  return NS_OK;
}
