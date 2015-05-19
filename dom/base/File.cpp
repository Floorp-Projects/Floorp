/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/File.h"

#include "MultipartBlobImpl.h"
#include "nsCExternalHandlerService.h"
#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsICharsetDetector.h"
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
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/BlobSet.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "nsThreadUtils.h"

#include "mozilla/dom/FileListBinding.h"

namespace mozilla {
namespace dom {

// XXXkhuey the input stream that we pass out of a File
// can outlive the actual File object.  Thus, we must
// ensure that the buffer underlying the stream we get
// from NS_NewByteInputStream is held alive as long as the
// stream is.  We do that by passing back this class instead.
class DataOwnerAdapter final : public nsIInputStream,
                               public nsISeekableStream,
                               public nsIIPCSerializableInputStream
{
  typedef BlobImplMemory::DataOwner DataOwner;
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
// mozilla::dom::Blob implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(Blob)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Blob)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Blob)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Blob)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Blob)
  // This class should not receive any nsIRemoteBlob QI!
  MOZ_ASSERT(!aIID.Equals(NS_GET_IID(nsIRemoteBlob)));

  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMBlob)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBlob)
  NS_INTERFACE_MAP_ENTRY(nsIXHRSendable)
  NS_INTERFACE_MAP_ENTRY(nsIMutable)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Blob)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Blob)

/* static */ Blob*
Blob::Create(nsISupports* aParent, BlobImpl* aImpl)
{
  MOZ_ASSERT(aImpl);

  return aImpl->IsFile() ? new File(aParent, aImpl)
                         : new Blob(aParent, aImpl);
}

/* static */ already_AddRefed<Blob>
Blob::Create(nsISupports* aParent, const nsAString& aContentType,
             uint64_t aLength)
{
  nsRefPtr<Blob> blob = Blob::Create(aParent,
    new BlobImplBase(aContentType, aLength));
  MOZ_ASSERT(!blob->mImpl->IsFile());
  return blob.forget();
}

/* static */ already_AddRefed<Blob>
Blob::Create(nsISupports* aParent, const nsAString& aContentType,
             uint64_t aStart, uint64_t aLength)
{
  nsRefPtr<Blob> blob = Blob::Create(aParent,
    new BlobImplBase(aContentType, aStart, aLength));
  MOZ_ASSERT(!blob->mImpl->IsFile());
  return blob.forget();
}

/* static */ already_AddRefed<Blob>
Blob::CreateMemoryBlob(nsISupports* aParent, void* aMemoryBuffer,
                       uint64_t aLength, const nsAString& aContentType)
{
  nsRefPtr<Blob> blob = Blob::Create(aParent,
    new BlobImplMemory(aMemoryBuffer, aLength, aContentType));
  MOZ_ASSERT(!blob->mImpl->IsFile());
  return blob.forget();
}

/* static */ already_AddRefed<Blob>
Blob::CreateTemporaryBlob(nsISupports* aParent, PRFileDesc* aFD,
                          uint64_t aStartPos, uint64_t aLength,
                          const nsAString& aContentType)
{
  nsRefPtr<Blob> blob = Blob::Create(aParent,
    new BlobImplTemporaryBlob(aFD, aStartPos, aLength, aContentType));
  MOZ_ASSERT(!blob->mImpl->IsFile());
  return blob.forget();
}

Blob::Blob(nsISupports* aParent, BlobImpl* aImpl)
  : mImpl(aImpl)
  , mParent(aParent)
{
  MOZ_ASSERT(mImpl);

#ifdef DEBUG
  {
    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aParent);
    if (win) {
      MOZ_ASSERT(win->IsInnerWindow());
    }
  }
#endif
}

bool
Blob::IsFile() const
{
  return mImpl->IsFile();
}

const nsTArray<nsRefPtr<BlobImpl>>*
Blob::GetSubBlobImpls() const
{
  return mImpl->GetSubBlobImpls();
}

already_AddRefed<File>
Blob::ToFile()
{
  if (!mImpl->IsFile()) {
    return nullptr;
  }

  nsRefPtr<File> file;
  if (HasFileInterface()) {
    file = static_cast<File*>(this);
  } else {
    file = new File(mParent, mImpl);
  }

  return file.forget();
}

already_AddRefed<File>
Blob::ToFile(const nsAString& aName) const
{
  nsAutoTArray<nsRefPtr<BlobImpl>, 1> blobImpls;
  blobImpls.AppendElement(mImpl);

  nsAutoString contentType;
  mImpl->GetType(contentType);

  nsRefPtr<MultipartBlobImpl> impl =
    new MultipartBlobImpl(blobImpls, aName, contentType);

  nsRefPtr<File> file = new File(mParent, impl);
  return file.forget();
}

already_AddRefed<Blob>
Blob::CreateSlice(uint64_t aStart, uint64_t aLength,
                  const nsAString& aContentType,
                  ErrorResult& aRv)
{
  nsRefPtr<BlobImpl> impl = mImpl->CreateSlice(aStart, aLength,
                                               aContentType, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<Blob> blob = Blob::Create(mParent, impl);
  return blob.forget();
}

uint64_t
Blob::GetSize(ErrorResult& aRv)
{
  return mImpl->GetSize(aRv);
}

void
Blob::GetType(nsAString &aType)
{
  mImpl->GetType(aType);
}

already_AddRefed<Blob>
Blob::Slice(const Optional<int64_t>& aStart,
            const Optional<int64_t>& aEnd,
            const nsAString& aContentType,
            ErrorResult& aRv)
{
  nsRefPtr<BlobImpl> impl =
    mImpl->Slice(aStart, aEnd, aContentType, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<Blob> blob = Blob::Create(mParent, impl);
  return blob.forget();
}

NS_IMETHODIMP
Blob::GetSendInfo(nsIInputStream** aBody,
                  uint64_t* aContentLength,
                  nsACString& aContentType,
                  nsACString& aCharset)
{
  return mImpl->GetSendInfo(aBody, aContentLength, aContentType, aCharset);
}

NS_IMETHODIMP
Blob::GetMutable(bool* aMutable)
{
  return mImpl->GetMutable(aMutable);
}

NS_IMETHODIMP
Blob::SetMutable(bool aMutable)
{
  return mImpl->SetMutable(aMutable);
}

JSObject*
Blob::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return BlobBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<Blob>
Blob::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsRefPtr<MultipartBlobImpl> impl = new MultipartBlobImpl();

  impl->InitializeBlob();
  MOZ_ASSERT(!impl->IsFile());

  nsRefPtr<Blob> blob = Blob::Create(aGlobal.GetAsSupports(), impl);
  return blob.forget();
}

/* static */ already_AddRefed<Blob>
Blob::Constructor(
        const GlobalObject& aGlobal,
        const Sequence<OwningArrayBufferOrArrayBufferViewOrBlobOrString>& aData,
        const BlobPropertyBag& aBag,
        ErrorResult& aRv)
{
  nsRefPtr<MultipartBlobImpl> impl = new MultipartBlobImpl();

  impl->InitializeBlob(aGlobal.Context(), aData, aBag.mType,
                       aBag.mEndings == EndingTypes::Native, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(!impl->IsFile());

  nsRefPtr<Blob> blob = Blob::Create(aGlobal.GetAsSupports(), impl);
  return blob.forget();
}

int64_t
Blob::GetFileId()
{
  return mImpl->GetFileId();
}

void
Blob::AddFileInfo(indexedDB::FileInfo* aFileInfo)
{
  mImpl->AddFileInfo(aFileInfo);
}

indexedDB::FileInfo*
Blob::GetFileInfo(indexedDB::FileManager* aFileManager)
{
  return mImpl->GetFileInfo(aFileManager);
}

bool
Blob::IsMemoryFile() const
{
  return mImpl->IsMemoryFile();
}

void
Blob::GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv)
{
  mImpl->GetInternalStream(aStream, aRv);
}

////////////////////////////////////////////////////////////////////////////
// mozilla::dom::File implementation

File::File(nsISupports* aParent, BlobImpl* aImpl)
  : Blob(aParent, aImpl)
{
  MOZ_ASSERT(aImpl->IsFile());
}

/* static */ File*
File::Create(nsISupports* aParent, BlobImpl* aImpl)
{
  MOZ_ASSERT(aImpl);
  MOZ_ASSERT(aImpl->IsFile());

  return new File(aParent, aImpl);
}

/* static */ already_AddRefed<File>
File::Create(nsISupports* aParent, const nsAString& aName,
             const nsAString& aContentType, uint64_t aLength,
             int64_t aLastModifiedDate)
{
  nsRefPtr<File> file = new File(aParent,
    new BlobImplBase(aName, aContentType, aLength, aLastModifiedDate));
  return file.forget();
}

/* static */ already_AddRefed<File>
File::Create(nsISupports* aParent, const nsAString& aName,
             const nsAString& aContentType, uint64_t aLength)
{
  nsRefPtr<File> file = new File(aParent,
    new BlobImplBase(aName, aContentType, aLength));
  return file.forget();
}

/* static */ already_AddRefed<File>
File::CreateMemoryFile(nsISupports* aParent, void* aMemoryBuffer,
                       uint64_t aLength, const nsAString& aName,
                       const nsAString& aContentType,
                       int64_t aLastModifiedDate)
{
  nsRefPtr<File> file = new File(aParent,
    new BlobImplMemory(aMemoryBuffer, aLength, aName,
                       aContentType, aLastModifiedDate));
  return file.forget();
}

/* static */ already_AddRefed<File>
File::CreateFromFile(nsISupports* aParent, nsIFile* aFile, bool aTemporary)
{
  nsRefPtr<File> file = new File(aParent, new BlobImplFile(aFile, aTemporary));
  return file.forget();
}

/* static */ already_AddRefed<File>
File::CreateFromFile(nsISupports* aParent, const nsAString& aContentType,
                     uint64_t aLength, nsIFile* aFile,
                     indexedDB::FileInfo* aFileInfo)
{
  nsRefPtr<File> file = new File(aParent,
    new BlobImplFile(aContentType, aLength, aFile, aFileInfo));
  return file.forget();
}

/* static */ already_AddRefed<File>
File::CreateFromFile(nsISupports* aParent, const nsAString& aName,
                     const nsAString& aContentType,
                     uint64_t aLength, nsIFile* aFile,
                     indexedDB::FileInfo* aFileInfo)
{
  nsRefPtr<File> file = new File(aParent,
    new BlobImplFile(aName, aContentType, aLength, aFile, aFileInfo));
  return file.forget();
}

/* static */ already_AddRefed<File>
File::CreateFromFile(nsISupports* aParent, nsIFile* aFile,
                     indexedDB::FileInfo* aFileInfo)
{
  nsRefPtr<File> file = new File(aParent,
    new BlobImplFile(aFile, aFileInfo));
  return file.forget();
}

/* static */ already_AddRefed<File>
File::CreateFromFile(nsISupports* aParent, nsIFile* aFile,
                     const nsAString& aName, const nsAString& aContentType)
{
  nsRefPtr<File> file = new File(aParent,
    new BlobImplFile(aFile, aName, aContentType));
  return file.forget();
}

JSObject*
File::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FileBinding::Wrap(aCx, this, aGivenProto);
}

void
File::GetName(nsAString& aFileName)
{
  mImpl->GetName(aFileName);
}

void
File::GetPath(nsAString& aPath, ErrorResult& aRv)
{
  mImpl->GetPath(aPath, aRv);
}

Date
File::GetLastModifiedDate(ErrorResult& aRv)
{
  int64_t value = GetLastModified(aRv);
  if (aRv.Failed()) {
    return Date();
  }

  return Date(value);
}

int64_t
File::GetLastModified(ErrorResult& aRv)
{
  return mImpl->GetLastModified(aRv);
}

void
File::GetMozFullPath(nsAString& aFilename, ErrorResult& aRv)
{
  mImpl->GetMozFullPath(aFilename, aRv);
}

void
File::GetMozFullPathInternal(nsAString& aFileName, ErrorResult& aRv)
{
  mImpl->GetMozFullPathInternal(aFileName, aRv);
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

/* static */ already_AddRefed<File>
File::Constructor(
        const GlobalObject& aGlobal,
        const Sequence<OwningArrayBufferOrArrayBufferViewOrBlobOrString>& aData,
        const nsAString& aName,
        const FilePropertyBag& aBag,
        ErrorResult& aRv)
{
  nsRefPtr<MultipartBlobImpl> impl = new MultipartBlobImpl(aName);

  impl->InitializeBlob(aGlobal.Context(), aData, aBag.mType, false, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(impl->IsFile());

  if (aBag.mLastModified.WasPassed()) {
    impl->SetLastModified(aBag.mLastModified.Value());
  }

  nsRefPtr<File> file = new File(aGlobal.GetAsSupports(), impl);
  return file.forget();
}

/* static */ already_AddRefed<File>
File::Constructor(const GlobalObject& aGlobal,
                  Blob& aData,
                  const ChromeFilePropertyBag& aBag,
                  ErrorResult& aRv)
{
  if (!nsContentUtils::ThreadsafeIsCallerChrome()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<MultipartBlobImpl> impl = new MultipartBlobImpl(EmptyString());
  impl->InitializeChromeFile(aData, aBag, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(impl->IsFile());

  if (aBag.mLastModified.WasPassed()) {
    impl->SetLastModified(aBag.mLastModified.Value());
  }

  nsRefPtr<File> domFile = new File(aGlobal.GetAsSupports(), impl);
  return domFile.forget();
}

/* static */ already_AddRefed<File>
File::Constructor(const GlobalObject& aGlobal,
                  nsIFile* aData,
                  const ChromeFilePropertyBag& aBag,
                  ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!nsContentUtils::IsCallerChrome()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());

  nsRefPtr<MultipartBlobImpl> impl = new MultipartBlobImpl(EmptyString());
  impl->InitializeChromeFile(window, aData, aBag, true, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(impl->IsFile());

  if (aBag.mLastModified.WasPassed()) {
    impl->SetLastModified(aBag.mLastModified.Value());
  }

  nsRefPtr<File> domFile = new File(aGlobal.GetAsSupports(), impl);
  return domFile.forget();
}

/* static */ already_AddRefed<File>
File::Constructor(const GlobalObject& aGlobal,
                  const nsAString& aData,
                  const ChromeFilePropertyBag& aBag,
                  ErrorResult& aRv)
{
  if (!nsContentUtils::ThreadsafeIsCallerChrome()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());

  nsRefPtr<MultipartBlobImpl> impl = new MultipartBlobImpl(EmptyString());
  impl->InitializeChromeFile(window, aData, aBag, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(impl->IsFile());

  if (aBag.mLastModified.WasPassed()) {
    impl->SetLastModified(aBag.mLastModified.Value());
  }

  nsRefPtr<File> domFile = new File(aGlobal.GetAsSupports(), impl);
  return domFile.forget();
}

////////////////////////////////////////////////////////////////////////////
// mozilla::dom::BlobImpl implementation

already_AddRefed<BlobImpl>
BlobImpl::Slice(const Optional<int64_t>& aStart,
                const Optional<int64_t>& aEnd,
                const nsAString& aContentType,
                ErrorResult& aRv)
{
  // Truncate aStart and aEnd so that we stay within this file.
  uint64_t thisLength = GetSize(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  int64_t start = aStart.WasPassed() ? aStart.Value() : 0;
  int64_t end = aEnd.WasPassed() ? aEnd.Value() : (int64_t)thisLength;

  ParseSize((int64_t)thisLength, start, end);

  return CreateSlice((uint64_t)start, (uint64_t)(end - start),
                     aContentType, aRv);
}

////////////////////////////////////////////////////////////////////////////
// BlobImpl implementation

NS_IMPL_ISUPPORTS(BlobImpl, BlobImpl)

////////////////////////////////////////////////////////////////////////////
// BlobImplFile implementation

NS_IMPL_ISUPPORTS_INHERITED0(BlobImplFile, BlobImpl)

void
BlobImplBase::GetName(nsAString& aName)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  aName = mName;
}

void
BlobImplBase::GetPath(nsAString& aPath, ErrorResult& aRv)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  aPath = mPath;
}

void
BlobImplBase::GetMozFullPath(nsAString& aFileName, ErrorResult& aRv)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");

  aFileName.Truncate();

  if (NS_IsMainThread()) {
    if (nsContentUtils::IsCallerChrome()) {
      GetMozFullPathInternal(aFileName, aRv);
    }

    return;
  }

  workers::WorkerPrivate* workerPrivate =
    workers::GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  if (workerPrivate->UsesSystemPrincipal()) {
    GetMozFullPathInternal(aFileName, aRv);
  }
}

void
BlobImplBase::GetMozFullPathInternal(nsAString& aFileName, ErrorResult& aRv)
{
  if (!mIsFile) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aFileName.Truncate();
}

void
BlobImplBase::GetType(nsAString& aType)
{
  aType = mContentType;
}

int64_t
BlobImplBase::GetLastModified(ErrorResult& aRv)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  if (IsDateUnknown()) {
    mLastModificationDate = PR_Now();
  }

  return mLastModificationDate / PR_USEC_PER_MSEC;
}

void
BlobImplBase::SetLastModified(int64_t aLastModified)
{
  mLastModificationDate = aLastModified * PR_USEC_PER_MSEC;
}

int64_t
BlobImplBase::GetFileId()
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
BlobImplBase::AddFileInfo(indexedDB::FileInfo* aFileInfo)
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
BlobImplBase::GetFileInfo(indexedDB::FileManager* aFileManager)
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
BlobImplBase::GetSendInfo(nsIInputStream** aBody, uint64_t* aContentLength,
                          nsACString& aContentType, nsACString& aCharset)
{
  MOZ_ASSERT(aContentLength);

  ErrorResult rv;

  nsCOMPtr<nsIInputStream> stream;
  GetInternalStream(getter_AddRefs(stream), rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  *aContentLength = GetSize(rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  nsAutoString contentType;
  GetType(contentType);

  CopyUTF16toUTF8(contentType, aContentType);

  aCharset.Truncate();

  stream.forget(aBody);
  return NS_OK;
}

nsresult
BlobImplBase::GetMutable(bool* aMutable) const
{
  *aMutable = !mImmutable;
  return NS_OK;
}

nsresult
BlobImplBase::SetMutable(bool aMutable)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG(!mImmutable || !aMutable);

  if (!mImmutable && !aMutable) {
    // Force the content type and size to be cached
    nsAutoString dummyString;
    GetType(dummyString);

    ErrorResult error;
    GetSize(error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  mImmutable = !aMutable;
  return rv;
}

////////////////////////////////////////////////////////////////////////////
// BlobImplFile implementation

already_AddRefed<BlobImpl>
BlobImplFile::CreateSlice(uint64_t aStart, uint64_t aLength,
                          const nsAString& aContentType,
                          ErrorResult& aRv)
{
  nsRefPtr<BlobImpl> impl =
    new BlobImplFile(this, aStart, aLength, aContentType);
  return impl.forget();
}

void
BlobImplFile::GetMozFullPathInternal(nsAString& aFilename, ErrorResult& aRv)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  aRv = mFile->GetPath(aFilename);
}

uint64_t
BlobImplFile::GetSize(ErrorResult& aRv)
{
  if (IsSizeUnknown()) {
    NS_ASSERTION(mWholeFile,
                 "Should only use lazy size when using the whole file");
    int64_t fileSize;
    aRv = mFile->GetFileSize(&fileSize);
    if (NS_WARN_IF(aRv.Failed())) {
      return 0;
    }

    if (fileSize < 0) {
      aRv.Throw(NS_ERROR_FAILURE);
      return 0;
    }

    mLength = fileSize;
  }

  return mLength;
}

void
BlobImplFile::GetType(nsAString& aType)
{
  if (mContentType.IsVoid()) {
    NS_ASSERTION(mWholeFile,
                 "Should only use lazy ContentType when using the whole file");
    nsresult rv;
    nsCOMPtr<nsIMIMEService> mimeService =
      do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aType.Truncate();
      return;
    }

    nsAutoCString mimeType;
    rv = mimeService->GetTypeFromFile(mFile, mimeType);
    if (NS_FAILED(rv)) {
      mimeType.Truncate();
    }

    AppendUTF8toUTF16(mimeType, mContentType);
    mContentType.SetIsVoid(false);
  }

  aType = mContentType;
}

int64_t
BlobImplFile::GetLastModified(ErrorResult& aRv)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  if (IsDateUnknown()) {
    PRTime msecs;
    aRv = mFile->GetLastModifiedTime(&msecs);
    if (NS_WARN_IF(aRv.Failed())) {
      return 0;
    }

    mLastModificationDate = msecs;
  }

  return mLastModificationDate;
}

void
BlobImplFile::SetLastModified(int64_t aLastModified)
{
  MOZ_CRASH("SetLastModified of a real file is not allowed!");
}

const uint32_t sFileStreamFlags =
  nsIFileInputStream::CLOSE_ON_EOF |
  nsIFileInputStream::REOPEN_ON_REWIND |
  nsIFileInputStream::DEFER_OPEN |
  nsIFileInputStream::SHARE_DELETE;

void
BlobImplFile::GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv)
{
  if (mWholeFile) {
    aRv = NS_NewLocalFileInputStream(aStream, mFile, -1, -1, sFileStreamFlags);
    return;
  }

  aRv = NS_NewPartialLocalFileInputStream(aStream, mFile, mStart, mLength,
                                          -1, -1, sFileStreamFlags);
}

void
BlobImplFile::SetPath(const nsAString& aPath)
{
  MOZ_ASSERT(aPath.IsEmpty() ||
             aPath[aPath.Length() - 1] == char16_t('/'),
             "Path must end with a path separator");
  mPath = aPath;
}

////////////////////////////////////////////////////////////////////////////
// BlobImplMemory implementation

NS_IMPL_ISUPPORTS_INHERITED0(BlobImplMemory, BlobImpl)

already_AddRefed<BlobImpl>
BlobImplMemory::CreateSlice(uint64_t aStart, uint64_t aLength,
                            const nsAString& aContentType,
                            ErrorResult& aRv)
{
  nsRefPtr<BlobImpl> impl =
    new BlobImplMemory(this, aStart, aLength, aContentType);
  return impl.forget();
}

void
BlobImplMemory::GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv)
{
  if (mLength > INT32_MAX) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aRv = DataOwnerAdapter::Create(mDataOwner, mStart, mLength, aStream);
}

/* static */ StaticMutex
BlobImplMemory::DataOwner::sDataOwnerMutex;

/* static */ StaticAutoPtr<LinkedList<BlobImplMemory::DataOwner>>
BlobImplMemory::DataOwner::sDataOwners;

/* static */ bool
BlobImplMemory::DataOwner::sMemoryReporterRegistered = false;

MOZ_DEFINE_MALLOC_SIZE_OF(MemoryFileDataOwnerMallocSizeOf)

class BlobImplMemoryDataOwnerMemoryReporter final
  : public nsIMemoryReporter
{
  ~BlobImplMemoryDataOwnerMemoryReporter() {}

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD CollectReports(nsIMemoryReporterCallback *aCallback,
                            nsISupports *aClosure, bool aAnonymize) override
  {
    typedef BlobImplMemory::DataOwner DataOwner;

    StaticMutexAutoLock lock(DataOwner::sDataOwnerMutex);

    if (!DataOwner::sDataOwners) {
      return NS_OK;
    }

    const size_t LARGE_OBJECT_MIN_SIZE = 8 * 1024;
    size_t smallObjectsTotal = 0;

    for (DataOwner *owner = DataOwner::sDataOwners->getFirst();
         owner; owner = owner->getNext()) {

      size_t size = MemoryFileDataOwnerMallocSizeOf(owner->mData);

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

NS_IMPL_ISUPPORTS(BlobImplMemoryDataOwnerMemoryReporter, nsIMemoryReporter)

/* static */ void
BlobImplMemory::DataOwner::EnsureMemoryReporterRegistered()
{
  sDataOwnerMutex.AssertCurrentThreadOwns();
  if (sMemoryReporterRegistered) {
    return;
  }

  RegisterStrongMemoryReporter(new BlobImplMemoryDataOwnerMemoryReporter());

  sMemoryReporterRegistered = true;
}

////////////////////////////////////////////////////////////////////////////
// BlobImplTemporaryBlob implementation

NS_IMPL_ISUPPORTS_INHERITED0(BlobImplTemporaryBlob, BlobImpl)

already_AddRefed<BlobImpl>
BlobImplTemporaryBlob::CreateSlice(uint64_t aStart, uint64_t aLength,
                                   const nsAString& aContentType,
                                   ErrorResult& aRv)
{
  if (aStart + aLength > mLength) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<BlobImpl> impl =
    new BlobImplTemporaryBlob(this, aStart + mStartPos,
                              aLength, aContentType);
  return impl.forget();
}

void
BlobImplTemporaryBlob::GetInternalStream(nsIInputStream** aStream,
                                         ErrorResult& aRv)
{
  nsCOMPtr<nsIInputStream> stream =
    new nsTemporaryFileInputStream(mFileDescOwner, mStartPos, mStartPos + mLength);
  stream.forget(aStream);
}

////////////////////////////////////////////////////////////////////////////
// FileList implementation

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FileList, mFiles)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFileList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFileList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FileList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileList)

JSObject*
FileList::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::FileListBinding::Wrap(cx, this, aGivenProto);
}

NS_IMETHODIMP
FileList::GetLength(uint32_t* aLength)
{
  *aLength = Length();

  return NS_OK;
}

NS_IMETHODIMP
FileList::Item(uint32_t aIndex, nsISupports** aFile)
{
  nsCOMPtr<nsIDOMBlob> file = Item(aIndex);
  file.forget(aFile);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////
// BlobSet implementation

already_AddRefed<Blob>
BlobSet::GetBlobInternal(nsISupports* aParent, const nsACString& aContentType)
{
  nsRefPtr<Blob> blob = Blob::Create(aParent,
    new MultipartBlobImpl(GetBlobImpls(),
                          NS_ConvertASCIItoUTF16(aContentType)));
  return blob.forget();
}

nsresult
BlobSet::AppendVoidPtr(const void* aData, uint32_t aLength)
{
  NS_ENSURE_ARG_POINTER(aData);

  uint64_t offset = mDataLen;

  if (!ExpandBufferSize(aLength))
    return NS_ERROR_OUT_OF_MEMORY;

  memcpy((char*)mData + offset, aData, aLength);
  return NS_OK;
}

nsresult
BlobSet::AppendString(const nsAString& aString, bool nativeEOL, JSContext* aCx)
{
  nsCString utf8Str = NS_ConvertUTF16toUTF8(aString);

  if (nativeEOL) {
    if (utf8Str.FindChar('\r') != kNotFound) {
      utf8Str.ReplaceSubstring("\r\n", "\n");
      utf8Str.ReplaceSubstring("\r", "\n");
    }
#ifdef XP_WIN
    utf8Str.ReplaceSubstring("\n", "\r\n");
#endif
  }

  return AppendVoidPtr((void*)utf8Str.Data(),
                       utf8Str.Length());
}

nsresult
BlobSet::AppendBlobImpl(BlobImpl* aBlobImpl)
{
  NS_ENSURE_ARG_POINTER(aBlobImpl);

  Flush();
  mBlobImpls.AppendElement(aBlobImpl);

  return NS_OK;
}

nsresult
BlobSet::AppendBlobImpls(const nsTArray<nsRefPtr<BlobImpl>>& aBlobImpls)
{
  Flush();
  mBlobImpls.AppendElements(aBlobImpls);

  return NS_OK;
}

} // dom namespace
} // mozilla namespace
