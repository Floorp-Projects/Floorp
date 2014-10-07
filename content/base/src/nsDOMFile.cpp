/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMFile.h"

#include "nsCExternalHandlerService.h"
#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsDOMBlobBuilder.h"
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
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "nsThreadUtils.h"

#include "mozilla/dom/FileListBinding.h"

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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMFile)
  MOZ_ASSERT(tmp->mImpl);
  tmp->mImpl->Traverse(cb);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DOMFile)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMFile)
  // This class should not receive any nsIRemoteBlob QI!
  MOZ_ASSERT(!aIID.Equals(NS_GET_IID(nsIRemoteBlob)));

  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFile)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBlob)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIDOMFile, IsFile())
  NS_INTERFACE_MAP_ENTRY(nsIXHRSendable)
  NS_INTERFACE_MAP_ENTRY(nsIMutable)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMFile)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMFile)

/* static */ already_AddRefed<DOMFile>
DOMFile::Create(nsISupports* aParent, const nsAString& aName,
                const nsAString& aContentType, uint64_t aLength,
                uint64_t aLastModifiedDate)
{
  nsRefPtr<DOMFile> file = new DOMFile(aParent,
    new DOMFileImplBase(aName, aContentType, aLength, aLastModifiedDate));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::Create(nsISupports* aParent, const nsAString& aName,
                const nsAString& aContentType, uint64_t aLength)
{
  nsRefPtr<DOMFile> file = new DOMFile(aParent,
    new DOMFileImplBase(aName, aContentType, aLength));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::Create(nsISupports* aParent, const nsAString& aContentType,
                uint64_t aLength)
{
  nsRefPtr<DOMFile> file = new DOMFile(aParent,
    new DOMFileImplBase(aContentType, aLength));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::Create(nsISupports* aParent, const nsAString& aContentType,
                uint64_t aStart, uint64_t aLength)
{
  nsRefPtr<DOMFile> file = new DOMFile(aParent,
    new DOMFileImplBase(aContentType, aStart, aLength));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateMemoryFile(nsISupports* aParent, void* aMemoryBuffer,
                          uint64_t aLength, const nsAString& aName,
                          const nsAString& aContentType,
                          uint64_t aLastModifiedDate)
{
  nsRefPtr<DOMFile> file = new DOMFile(aParent,
    new DOMFileImplMemory(aMemoryBuffer, aLength, aName,
                          aContentType, aLastModifiedDate));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateMemoryFile(nsISupports* aParent, void* aMemoryBuffer,
                          uint64_t aLength, const nsAString& aContentType)
{
  nsRefPtr<DOMFile> file = new DOMFile(aParent,
    new DOMFileImplMemory(aMemoryBuffer, aLength, aContentType));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateTemporaryFileBlob(nsISupports* aParent, PRFileDesc* aFD,
                                 uint64_t aStartPos, uint64_t aLength,
                                 const nsAString& aContentType)
{
  nsRefPtr<DOMFile> file = new DOMFile(aParent,
    new DOMFileImplTemporaryFileBlob(aFD, aStartPos, aLength, aContentType));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateFromFile(nsISupports* aParent, nsIFile* aFile)
{
  nsRefPtr<DOMFile> file = new DOMFile(aParent, new DOMFileImplFile(aFile));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateFromFile(nsISupports* aParent, const nsAString& aContentType,
                        uint64_t aLength, nsIFile* aFile,
                        indexedDB::FileInfo* aFileInfo)
{
  nsRefPtr<DOMFile> file = new DOMFile(aParent,
    new DOMFileImplFile(aContentType, aLength, aFile, aFileInfo));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateFromFile(nsISupports* aParent, const nsAString& aName,
                        const nsAString& aContentType,
                        uint64_t aLength, nsIFile* aFile,
                        indexedDB::FileInfo* aFileInfo)
{
  nsRefPtr<DOMFile> file = new DOMFile(aParent,
    new DOMFileImplFile(aName, aContentType, aLength, aFile, aFileInfo));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateFromFile(nsISupports* aParent, nsIFile* aFile,
                        indexedDB::FileInfo* aFileInfo)
{
  nsRefPtr<DOMFile> file = new DOMFile(aParent,
    new DOMFileImplFile(aFile, aFileInfo));
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::CreateFromFile(nsISupports* aParent, nsIFile* aFile,
                        const nsAString& aName, const nsAString& aContentType)
{
  nsRefPtr<DOMFile> file = new DOMFile(aParent,
    new DOMFileImplFile(aFile, aName, aContentType));
  return file.forget();
}

DOMFile::DOMFile(nsISupports* aParent, DOMFileImpl* aImpl)
  : mImpl(aImpl)
  , mParent(aParent)
{
  MOZ_ASSERT(mImpl);
  SetIsDOMBinding();

#ifdef DEBUG
  {
    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aParent);
    if (win) {
      MOZ_ASSERT(win->IsInnerWindow());
    }
  }
#endif
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

already_AddRefed<DOMFile>
DOMFile::CreateSlice(uint64_t aStart, uint64_t aLength,
                     const nsAString& aContentType,
                     ErrorResult& aRv)
{
  nsRefPtr<DOMFileImpl> impl = mImpl->CreateSlice(aStart, aLength,
                                                  aContentType, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DOMFile> file = new DOMFile(mParent, impl);
  return file.forget();
}

NS_IMETHODIMP
DOMFile::GetName(nsAString& aFileName)
{
  mImpl->GetName(aFileName);
  return NS_OK;
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
  ErrorResult rv;
  Date value = GetLastModifiedDate(rv);
  if (rv.Failed()) {
    return rv.ErrorCode();
  }

  if (!value.ToDateObject(aCx, aDate)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

Date
DOMFile::GetLastModifiedDate(ErrorResult& aRv)
{
  int64_t value = GetLastModified(aRv);
  if (aRv.Failed()) {
    return Date();
  }

  return Date(value);
}

int64_t
DOMFile::GetLastModified(ErrorResult& aRv)
{
  return mImpl->GetLastModified(aRv);
}

NS_IMETHODIMP
DOMFile::GetMozFullPath(nsAString& aFileName)
{
  ErrorResult rv;
  GetMozFullPath(aFileName, rv);
  return rv.ErrorCode();
}

void
DOMFile::GetMozFullPath(nsAString& aFilename, ErrorResult& aRv)
{
  mImpl->GetMozFullPath(aFilename, aRv);
}

NS_IMETHODIMP
DOMFile::GetMozFullPathInternal(nsAString& aFileName)
{
  ErrorResult rv;
  mImpl->GetMozFullPathInternal(aFileName, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
DOMFile::GetSize(uint64_t* aSize)
{
  MOZ_ASSERT(aSize);

  ErrorResult rv;
  *aSize = GetSize(rv);
  return rv.ErrorCode();
}

uint64_t
DOMFile::GetSize(ErrorResult& aRv)
{
  return mImpl->GetSize(aRv);
}

NS_IMETHODIMP
DOMFile::GetType(nsAString &aType)
{
  mImpl->GetType(aType);
  return NS_OK;
}

NS_IMETHODIMP
DOMFile::GetMozLastModifiedDate(uint64_t* aDate)
{
  MOZ_ASSERT(aDate);

  ErrorResult rv;
  *aDate = GetLastModified(rv);
  return rv.ErrorCode();
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
  Optional<int64_t> start;
  if (aArgc > 0) {
    start.Construct(aStart);
  }

  Optional<int64_t> end;
  if (aArgc > 1) {
    end.Construct(aEnd);
  }

  ErrorResult rv;
  nsRefPtr<DOMFile> file = Slice(start, end, aContentType, rv);
  if (rv.Failed()) {
    return rv.ErrorCode();
  }

  file.forget(aBlob);
  return NS_OK;
}

already_AddRefed<DOMFile>
DOMFile::Slice(const Optional<int64_t>& aStart,
               const Optional<int64_t>& aEnd,
               const nsAString& aContentType,
               ErrorResult& aRv)
{
  nsRefPtr<DOMFileImpl> impl =
    mImpl->Slice(aStart, aEnd, aContentType, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<DOMFile> file = new DOMFile(mParent, impl);
  return file.forget();
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

JSObject*
DOMFile::WrapObject(JSContext* aCx)
{
  return IsFile() ? FileBinding::Wrap(aCx, this)
                  : BlobBinding::Wrap(aCx, this);
}

/* static */ already_AddRefed<DOMFile>
DOMFile::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsRefPtr<DOMMultipartFileImpl> impl = new DOMMultipartFileImpl();

  impl->InitializeBlob();
  MOZ_ASSERT(!impl->IsFile());

  nsRefPtr<DOMFile> file = new DOMFile(aGlobal.GetAsSupports(), impl);
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::Constructor(
        const GlobalObject& aGlobal,
        const Sequence<OwningArrayBufferOrArrayBufferViewOrBlobOrString>& aData,
        const BlobPropertyBag& aBag,
        ErrorResult& aRv)
{
  nsRefPtr<DOMMultipartFileImpl> impl = new DOMMultipartFileImpl();

  impl->InitializeBlob(aGlobal.Context(), aData, aBag.mType,
                       aBag.mEndings == EndingTypes::Native, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(!impl->IsFile());

  nsRefPtr<DOMFile> file = new DOMFile(aGlobal.GetAsSupports(), impl);
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::Constructor(
        const GlobalObject& aGlobal,
        const Sequence<OwningArrayBufferOrArrayBufferViewOrBlobOrString>& aData,
        const nsAString& aName,
        const FilePropertyBag& aBag,
        ErrorResult& aRv)
{
  nsRefPtr<DOMMultipartFileImpl> impl = new DOMMultipartFileImpl(aName);

  impl->InitializeBlob(aGlobal.Context(), aData, aBag.mType, false, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(impl->IsFile());

  nsRefPtr<DOMFile> file = new DOMFile(aGlobal.GetAsSupports(), impl);
  return file.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::Constructor(const GlobalObject& aGlobal,
                     DOMFile& aData,
                     const FilePropertyBag& aBag,
                     ErrorResult& aRv)
{
  if (!nsContentUtils::IsCallerChrome()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMMultipartFileImpl> impl = new DOMMultipartFileImpl(EmptyString());
  impl->InitializeChromeFile(aData, aBag, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(impl->IsFile());

  nsRefPtr<DOMFile> domFile = new DOMFile(aGlobal.GetAsSupports(), impl);
  return domFile.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::Constructor(const GlobalObject& aGlobal,
                     nsIFile* aData,
                     const FilePropertyBag& aBag,
                     ErrorResult& aRv)
{
  if (!nsContentUtils::IsCallerChrome()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());

  nsRefPtr<DOMMultipartFileImpl> impl = new DOMMultipartFileImpl(EmptyString());
  impl->InitializeChromeFile(window, aData, aBag, true, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(impl->IsFile());

  nsRefPtr<DOMFile> domFile = new DOMFile(aGlobal.GetAsSupports(), impl);
  return domFile.forget();
}

/* static */ already_AddRefed<DOMFile>
DOMFile::Constructor(const GlobalObject& aGlobal,
                     const nsAString& aData,
                     const FilePropertyBag& aBag,
                     ErrorResult& aRv)
{
  if (!nsContentUtils::IsCallerChrome()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());

  nsRefPtr<DOMMultipartFileImpl> impl = new DOMMultipartFileImpl(EmptyString());
  impl->InitializeChromeFile(window, aData, aBag, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(impl->IsFile());

  nsRefPtr<DOMFile> domFile = new DOMFile(aGlobal.GetAsSupports(), impl);
  return domFile.forget();
}

////////////////////////////////////////////////////////////////////////////
// mozilla::dom::DOMFileImpl implementation

already_AddRefed<DOMFileImpl>
DOMFileImpl::Slice(const Optional<int64_t>& aStart,
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
// DOMFileImpl implementation

NS_IMPL_ISUPPORTS(DOMFileImpl, PIDOMFileImpl)

////////////////////////////////////////////////////////////////////////////
// DOMFileImplFile implementation

NS_IMPL_ISUPPORTS_INHERITED0(DOMFileImplFile, DOMFileImpl)

void
DOMFileImplBase::GetName(nsAString& aName)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  aName = mName;
}

nsresult
DOMFileImplBase::GetPath(nsAString& aPath)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  aPath = mPath;
  return NS_OK;
}

void
DOMFileImplBase::GetMozFullPath(nsAString& aFileName, ErrorResult& aRv)
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
DOMFileImplBase::GetMozFullPathInternal(nsAString& aFileName, ErrorResult& aRv)
{
  if (!mIsFile) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aFileName.Truncate();
}

void
DOMFileImplBase::GetType(nsAString& aType)
{
  aType = mContentType;
}

int64_t
DOMFileImplBase::GetLastModified(ErrorResult& aRv)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  if (IsDateUnknown()) {
    mLastModificationDate = PR_Now();
  }

  return mLastModificationDate / PR_USEC_PER_MSEC;
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
  MOZ_ASSERT(aContentLength);

  nsresult rv;

  nsCOMPtr<nsIInputStream> stream;
  rv = GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  ErrorResult error;
  *aContentLength = GetSize(error);
  if (NS_WARN_IF(error.Failed())) {
    return error.ErrorCode();
  }

  nsAutoString contentType;
  GetType(contentType);

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
    nsAutoString dummyString;
    GetType(dummyString);

    ErrorResult error;
    GetSize(error);
    if (NS_WARN_IF(error.Failed())) {
      return error.ErrorCode();
    }
  }

  mImmutable = !aMutable;
  return rv;
}

////////////////////////////////////////////////////////////////////////////
// DOMFileImplFile implementation

already_AddRefed<DOMFileImpl>
DOMFileImplFile::CreateSlice(uint64_t aStart, uint64_t aLength,
                             const nsAString& aContentType,
                             ErrorResult& aRv)
{
  nsRefPtr<DOMFileImpl> impl =
    new DOMFileImplFile(this, aStart, aLength, aContentType);
  return impl.forget();
}

void
DOMFileImplFile::GetMozFullPathInternal(nsAString& aFilename, ErrorResult& aRv)
{
  NS_ASSERTION(mIsFile, "Should only be called on files");
  aRv = mFile->GetPath(aFilename);
}

uint64_t
DOMFileImplFile::GetSize(ErrorResult& aRv)
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
DOMFileImplFile::GetType(nsAString& aType)
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
DOMFileImplFile::GetLastModified(ErrorResult& aRv)
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

const uint32_t sFileStreamFlags =
  nsIFileInputStream::CLOSE_ON_EOF |
  nsIFileInputStream::REOPEN_ON_REWIND |
  nsIFileInputStream::DEFER_OPEN |
  nsIFileInputStream::SHARE_DELETE;

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

already_AddRefed<DOMFileImpl>
DOMFileImplMemory::CreateSlice(uint64_t aStart, uint64_t aLength,
                               const nsAString& aContentType,
                               ErrorResult& aRv)
{
  nsRefPtr<DOMFileImpl> impl =
    new DOMFileImplMemory(this, aStart, aLength, aContentType);
  return impl.forget();
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

already_AddRefed<DOMFileImpl>
DOMFileImplTemporaryFileBlob::CreateSlice(uint64_t aStart, uint64_t aLength,
                                          const nsAString& aContentType,
                                          ErrorResult& aRv)
{
  if (aStart + aLength > mLength) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<DOMFileImpl> impl =
    new DOMFileImplTemporaryFileBlob(this, aStart + mStartPos,
                                     aLength, aContentType);
  return impl.forget();
}

nsresult
DOMFileImplTemporaryFileBlob::GetInternalStream(nsIInputStream** aStream)
{
  nsCOMPtr<nsIInputStream> stream =
    new nsTemporaryFileInputStream(mFileDescOwner, mStartPos, mStartPos + mLength);
  stream.forget(aStream);
  return NS_OK;
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
FileList::WrapObject(JSContext *cx)
{
  return mozilla::dom::FileListBinding::Wrap(cx, this);
}

NS_IMETHODIMP
FileList::GetLength(uint32_t* aLength)
{
  *aLength = Length();

  return NS_OK;
}

NS_IMETHODIMP
FileList::Item(uint32_t aIndex, nsIDOMFile **aFile)
{
  nsRefPtr<DOMFile> file = Item(aIndex);
  file.forget(aFile);
  return NS_OK;
}

} // dom namespace
} // mozilla namespace
